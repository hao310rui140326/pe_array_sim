// Copyright 2015-2017 Intel Corporation.
//
// The source code, information and material ("Material") contained herein is
// owned by Intel Corporation or its suppliers or licensors, and title to such
// Material remains with Intel Corporation or its suppliers or licensors. The
// Material contains proprietary information of Intel or its suppliers and
// licensors. The Material is protected by worldwide copyright laws and treaty
// provisions. No part of the Material may be used, copied, reproduced,
// modified, published, uploaded, posted, transmitted, distributed or disclosed
// in any way without Intel's prior express written permission. No license under
// any patent, copyright or other intellectual property rights in the Material
// is granted to or conferred upon you, either expressly, by implication,
// inducement, estoppel or otherwise. Any license under such intellectual
// property rights must be express and approved by Intel in writing.
//
// Unless otherwise agreed by Intel in writing, you may not remove or alter this
// notice or any other notice embedded in Materials by Intel or Intel's
// suppliers or licensors in any way.

#include "pe_array_quartus.h"

#include <iostream>
#include <fstream>

using namespace pe_array;

void pe_array::generate_quartus_project(const pe_array_cfg_t* cfg,
    const std::string& template_dir, const std::string& rtl_dir,
    const std::string& output_dir, DeviceVoltage voltage, int speed_grade) {

  // This assert makes sure that output_dir doesn't add extra levels of
  // directory hierarchy aside from the name of the output directory itself.
  // This is required because the generated quartus project qsf uses relative
  // pathnames to the files in the RTL directory,
  for (char c : output_dir) { assert(c != '/'); }

  // This complicated assert is here because I pass these directories to shell
  // commands, without quotes and unescaped. There's probably all kinds of
  // things you can do to break that, like spaces and blashslashes.. but you
  // know, there really doesn't seem to be an easy way in C++ to copy a
  // directory, so shell command it is!
  // TODO: [shaneoco] Surely there's an escape from this madness
  for (const std::string& dir : {template_dir, output_dir}) {
    for (char c : dir) {
      assert(
          (c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') ||
          c == '.' || c == '/' || c == '_');
    }
  }

  // One and only one of these device bools should be set
  assert(cfg->device_a10 + cfg->device_s10 == 1);
  DeviceFamily device = cfg->device_a10 ? DEVICE_A10 : DEVICE_S10;
  std::string device_name = cfg->device_a10 ? "a10" : "s10";

  // copy template directory
  std::string cp_template_cmd = std::string("rm -rf ") + output_dir +
    " && cp -R " + template_dir + "/" + device_name + " " + output_dir +
    " && chmod -R +w " + output_dir;
  int ret = system(cp_template_cmd.c_str());
  if (ret != 0) {
    std::cerr << "Error: could not copy quartus project template directory\n";
    exit(1);
  }

  // modify qsf
  std::ofstream qsf(output_dir + "/pe_array.qsf", std::ofstream::app);

  // add device and voltage assignments
  std::string voltage_qsf_value;
  switch (voltage) {
    case VOLTAGE_090V: voltage_qsf_value = "0.9V"; break;
    case VOLTAGE_095V: voltage_qsf_value = "0.95V"; break;
    default: assert(false);
  }

  switch (device) {
    case DEVICE_A10:
      switch (speed_grade) {
        case 1:
          // This is the device in the A10 devkit (I believe)
          qsf << "set_global_assignment -name DEVICE 10AX115S2F45I1SG\n";
          break;
        case 2:
          qsf << "set_global_assignment -name DEVICE 10AX115S2F45I2SG\n";
          break;
        default: assert(false);
      }

      qsf << "set_global_assignment -name VCCP_USER_VOLTAGE " << voltage_qsf_value << "\n";
      qsf << "set_global_assignment -name NOMINAL_CORE_SUPPLY_VOLTAGE " << voltage_qsf_value << "\n";
      qsf << "set_global_assignment -name VCCERAM_USER_VOLTAGE " << voltage_qsf_value << "\n";
      break;

    case DEVICE_S10:
      switch (speed_grade) {
        case 1:
          qsf << "set_global_assignment -name DEVICE 1SG280HU1F50E1VG\n";
          break;
        case 2:
          // This is the device in the S10 devkit (I believe)
          qsf << "set_global_assignment -name DEVICE 1SG280LU2F50E2VG\n";
          break;
        default:
          assert(false);
      }

      assert(voltage == VOLTAGE_090V);
      break;

    default:
      assert(false);
  }

  // add RTL files
  qsf << "\n";
  for (const std::string& filename : pe_array_files) {
    qsf << "set_global_assignment -name SYSTEMVERILOG_FILE ../" << rtl_dir << "/" << filename << "\n";
  }
  qsf << "set_global_assignment -name SYSTEMVERILOG_FILE pe_array_wrapper.sv\n";
  qsf.close();

  // generate wrapper modules
  std::ofstream wrapper(output_dir + "/pe_array_wrapper.sv");
  wrapper << "`include \"../" << rtl_dir << "/pe_inc.sv\"\n";
  wrapper << "module pe_array_wrapper import pe_types::*; #(\n";
  pe_array_cfg_iterate(cfg, [&](const char* name, int value) {
    wrapper << "  int " << name << " = " << value << ",\n";
  });
  wrapper << "  pe_cfg_t cfg = `pe_cfg_init\n";
  wrapper << ") (\n";
  wrapper << "  input        clock,\n";
  wrapper << "  input        resetn,\n";
  wrapper << "\n";
  wrapper << "  input        ivalid,\n";
  wrapper << "  output logic ovalid,\n";
  wrapper << "\n";
  // for some reason, Quartus will ignore a virtual pin assignment if the type
  // of the port is a struct, but is fine if it's port with the same width as
  // the struct using $bits()
  wrapper << "  input        [$bits(pe_array_input_t#(cfg)::t)-1:0]  idata,\n";
  wrapper << "  output logic [$bits(pe_array_output_t#(cfg)::t)-1:0] odata\n";
  wrapper << ");\n";
  wrapper << "\n";
  wrapper << "// register inputs\n";
  wrapper << "logic                     ivalid_reg;\n";
  wrapper << "pe_array_input_t#(cfg)::t idata_reg;\n";
  wrapper << "\n";
  wrapper << "always_ff @(posedge clock) begin\n";
  wrapper << "  ivalid_reg <= ivalid;\n";
  wrapper << "  idata_reg <= idata;\n";
  wrapper << "end\n";
  wrapper << "\n";
  wrapper << "// register outputs\n";
  wrapper << "logic                      ovalid_pre;\n";
  wrapper << "pe_array_output_t#(cfg)::t odata_pre;\n";
  wrapper << "\n";
  wrapper << "always_ff @(posedge clock) begin\n";
  wrapper << "  ovalid <= ovalid_pre;\n";
  wrapper << "  odata <= odata_pre;\n";
  wrapper << "end\n";
  wrapper << "\n";
  wrapper << "// instantiate pe_array\n";
  wrapper << "pe_array #(\n";
  wrapper << "  `pe_cfg_fwd\n";
  wrapper << ") pe_array (\n";
  wrapper << "  .clock(clock),\n";
  wrapper << "  .resetn(resetn),\n";
  wrapper << "\n";
  wrapper << "  .ivalid(ivalid_reg),\n";
  wrapper << "  .ovalid(ovalid_pre),\n";
  wrapper << "\n";
  wrapper << "  .idata(idata_reg),\n";
  wrapper << "  .odata(odata_pre)\n";
  wrapper << ");\n";
  wrapper << "\n";
  wrapper << "endmodule\n";
  wrapper.close();
}

std::string parse_fit_report_string(const std::string& line,
    size_t offset) {
  size_t end = line.find(" ", offset);
  assert(end != std::string::npos);
  return line.substr(offset, end - offset);
}

int parse_fixed(const std::string& line, size_t offset,
    int num_fraction_digits) {
  assert(offset < line.size());

  int result = 0;
  int point_found = false;
  int fraction_count = 0;
  for (size_t index = offset; index < line.size(); index++) {
    char c = line[index];
    if (c == '.') {
      point_found = true;
    } else if (c >= '0' && c <= '9') {
      result *= 10;
      result += (c - '0');
      if (point_found) fraction_count++;
    } else {
      break;
    }
  }

  while (fraction_count < num_fraction_digits) {
    result *= 10;
    fraction_count++;
  }

  assert(fraction_count == num_fraction_digits);

  return result;
}

void pe_array::iterate_over_fit_report(const std::string& filename,
    std::function<bool(const FitReportInfo&)> callback) {

  enum {
    BEFORE_TABLE,
    INSIDE_HEADER,
    INSIDE_TABLE
  } parse_state = BEFORE_TABLE;

  bool exit_loop = false;

  size_t npos = std::string::npos;
  size_t num_alms_offset    = npos;
  size_t num_mlabs_offset   = npos;
  size_t num_aluts_offset   = npos;
  size_t num_regs_offset    = npos;
  size_t num_m20ks_offset   = npos;
  size_t num_dsps_offset    = npos;
  size_t entity_path_offset = npos;
  size_t entity_name_offset = npos;

  std::ifstream fit_report(filename);
  for (std::string line; !exit_loop && std::getline(fit_report, line);) {

    // Skip any horizontal dividers, to make the rest of the code simpler
    if (line[0] == '+') continue;

    switch (parse_state) {
      case BEFORE_TABLE:
        if (0 == line.compare(0, 39, "; Fitter Resource Utilization by Entity")) {
          parse_state = INSIDE_HEADER;
        }
        break;

      case INSIDE_HEADER:
        assert(line[0] == ';');

        num_alms_offset    = line.find("ALMs needed [=A-B+C]");
        num_mlabs_offset   = line.find("ALMs used for memory");
        num_aluts_offset   = line.find("Combinational ALUTs");
        num_regs_offset    = line.find("Dedicated Logic Registers");
        num_m20ks_offset   = line.find("M20Ks");
        num_dsps_offset    = line.find("DSP Blocks");
        entity_path_offset = line.find("Full Hierarchy Name");
        entity_name_offset = line.find("Entity Name");

        assert(num_alms_offset    != npos);
        assert(num_mlabs_offset   != npos);
        assert(num_aluts_offset   != npos);
        assert(num_regs_offset    != npos);
        assert(num_m20ks_offset   != npos);
        assert(num_dsps_offset    != npos);
        assert(entity_path_offset != npos);
        assert(entity_name_offset != npos);

        parse_state = INSIDE_TABLE;
        break;

      case INSIDE_TABLE:
        if (line[0] != ';') {
          exit_loop = true;
        } else {

          std::string entity_path = parse_fit_report_string(line, entity_path_offset);
          std::string entity_name = parse_fit_report_string(line, entity_name_offset);

          bool is_pe       = entity_name == "pe";
          bool is_pe_array = entity_name == "pe_array";

          if (is_pe || is_pe_array) {
            FitReportInfo info;
            info.num_tenth_alms = parse_fixed(line, num_alms_offset, /* num_fraction_digits */ 1);
            info.num_aluts = parse_fixed(line, num_aluts_offset, /* num_fraction_digits */ 0);
            info.num_regs = parse_fixed(line, num_regs_offset, /* num_fraction_digits */ 0);
            info.num_m20ks = parse_fixed(line, num_m20ks_offset, /* num_fraction_digits */ 0);
            info.num_dsps = parse_fixed(line, num_dsps_offset, /* num_fraction_digits */ 0);

            int num_tenth_alms_for_mlabs = parse_fixed(line, num_mlabs_offset,
                /* num_fraction_digits */ 1);
            assert(num_tenth_alms_for_mlabs % 100 == 0);
            info.num_mlabs = num_tenth_alms_for_mlabs / 100;

            info.is_pe_array = is_pe_array;
            if (!is_pe_array) {
              assert(line.find("GEN_CHAIN[0]") != npos);
              size_t gen_pe_offset = line.find("GEN_PE[");
              assert(gen_pe_offset != npos);
              info.pe_id = parse_fixed(line, gen_pe_offset + 7,
                  /* num_fraction_digits */ 0);
            }

            if (!callback(info)) {
              exit_loop = true;
            }
          }
        }
        break;
    }
  }
}

FitReportInfo pe_array::parse_pe_array_info_from_fit_report(
    const std::string& filename, bool* error) {
  assert(!*error);

  FitReportInfo result = {0};
  bool found = false;

  iterate_over_fit_report(filename, [&] (const FitReportInfo& fit_info) {
    if (fit_info.is_pe_array) {
      result = fit_info;
      found = true;
      return false;
    }
    return true;
  });

  if (!found) *error = true;
  return result;
}

int pe_array::parse_fmax_from_sta_report(const std::string& filename) {
  enum {
    BEFORE_TABLE,
    INSIDE_HEADER,
    INSIDE_TABLE
  } parse_state = BEFORE_TABLE;

  bool exit_loop = false;

  int min_fmax = std::numeric_limits<int>::max();
  bool found_fmax = false;

  std::ifstream sta_report(filename);
  for (std::string line; !exit_loop && std::getline(sta_report, line);) {

    // Skip any horizontal dividers, to make the rest of the code simpler
    if (line[0] == '+') continue;

    switch (parse_state) {
      case BEFORE_TABLE:
        if (line[0] == ';' && line.find("Fmax Summary") != std::string::npos) {
          parse_state = INSIDE_HEADER;
        }
        break;

      case INSIDE_HEADER:
        assert(0 == line.compare(0, 6, "; Fmax"));
        parse_state = INSIDE_TABLE;
        break;

      case INSIDE_TABLE:
        min_fmax = std::min(min_fmax, parse_fixed(line, /* offset */ 2,
              /* num_fraction_digits */ 2));
        found_fmax = true;
        parse_state = BEFORE_TABLE;
        break;
    }
  }

  if (found_fmax) {
    return min_fmax;
  } else {
    return -1;
  }
}

