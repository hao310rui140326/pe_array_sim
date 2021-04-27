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

// Turn on model fmax information in area estimates
#define PE_ARRAY_ENABLE_AREA_MODEL_FMAX 1

#include "pe_array.h"
#include "pe_array_quartus.h"

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <initializer_list>
#include <limits>

using namespace pe_array;

#define PROJECT_TEMPLATE_DIR         "../../common/quartus_project_template"
#define RTL_DIR                      "../../../rtl"
#define AREA_MODEL_FILENAME          "pe_array_area_model.h"
#define AREA_MODEL_TEMPLATE_FILENAME "../pe_array_area_model.template.h"

const std::vector<DeviceFamily> all_device_families = {DEVICE_A10, DEVICE_S10};
const std::vector<int>          all_feature_filter_widths = {/* fp8 */ 4, /* fp9 */ 5, /* fp11 */ 7, /* fp16 */ 12};
const std::vector<int>          all_dot_sizes = {4, 8, 16, 32};
const std::vector<int>          all_num_features = {1, 32, 64};
const std::vector<int>          all_ram_depths = {512, 4096, 8192};

const DeviceVoltage model_voltage = VOLTAGE_090V;
const int model_speed_grade = 1;

pe_array_cfg_t generate_cfg(DeviceFamily device, int feature_filter_width,
    int dot_size, int num_features, int ram_depth) {

  int num_filters = feature_filter_width <= 7 ? 2 : 1;

  pe_array_cfg_options_t cfg_options = zero_pe_array_cfg_options;
  cfg_options.enable_blockfp_mode   = true;
  cfg_options.enable_fixed_mode     = false;
  cfg_options.feature_width         = feature_filter_width;
  cfg_options.filter_width          = feature_filter_width;
  cfg_options.num_results_per_cycle = num_features*num_filters;
  cfg_options.num_chains            = 1;
  cfg_options.num_pes               = 4;
  cfg_options.num_dots              = 1; // TODO: [shaneoco] model this
  cfg_options.num_features          = num_features;
  cfg_options.num_filters           = num_filters;
  cfg_options.ram_depth             = ram_depth;
  cfg_options.dot_size              = dot_size;
  cfg_options.enable_ratios         = true;
  cfg_options.dot_ratio_dsp         = 1;
  cfg_options.dot_ratio_alm         = 1;
  cfg_options.accum_ratio_dsp       = 2;
  cfg_options.accum_ratio_alm       = 2;
  cfg_options.device_a10            = device == DEVICE_A10;
  cfg_options.device_s10            = device == DEVICE_S10;

  return pe_array_cfg_init(cfg_options);
}

std::string get_project_name(const pe_array_cfg_t* cfg) {
  std::string device_name;
  if (cfg->device_a10) device_name = "a10";
  if (cfg->device_s10) device_name = "s10";
  assert(!device_name.empty());

  std::ostringstream project_name_ss;
  project_name_ss << "pe_array_fw" << cfg->feature_width << "_dot"
    << cfg->dot_size << "_fe" << cfg->num_features << "_ram" << cfg->ram_depth <<
    "_" << device_name;

  return project_name_ss.str();
}

void iterate_over_cfgs(
    std::function<void(const pe_array_cfg_t*, const std::string&)> callback) {

  // TODO: [shaneoco] model feature and filter width separately

  for (DeviceFamily device : all_device_families) {
    for (int feature_filter_width : all_feature_filter_widths) {
      for (int dot_size : all_dot_sizes) {
        for (int num_features : all_num_features) {
          for (int ram_depth : all_ram_depths) {

            int num_filters = feature_filter_width <= 7 ? 2 : 1;

            pe_array_cfg_options_t cfg_options = zero_pe_array_cfg_options;
            cfg_options.enable_blockfp_mode   = true;
            cfg_options.enable_fixed_mode     = false;
            cfg_options.feature_width         = feature_filter_width;
            cfg_options.filter_width          = feature_filter_width;
            cfg_options.num_results_per_cycle = num_features*num_filters;
            cfg_options.num_chains            = 1;
            cfg_options.num_pes               = 4;
            cfg_options.num_dots              = 1; // TODO: [shaneoco] model this
            cfg_options.num_features          = num_features;
            cfg_options.num_filters           = num_filters;
            cfg_options.ram_depth             = ram_depth;
            cfg_options.dot_size              = dot_size;
            cfg_options.enable_ratios         = true;
            cfg_options.dot_ratio_dsp         = 1;
            cfg_options.dot_ratio_alm         = 1;
            cfg_options.accum_ratio_dsp       = 2;
            cfg_options.accum_ratio_alm       = 2;
            cfg_options.device_a10            = device == DEVICE_A10;
            cfg_options.device_s10            = device == DEVICE_S10;
            pe_array_cfg_t cfg = pe_array_cfg_init(cfg_options);

            std::string device_name;
            if (device == DEVICE_A10) device_name = "a10";
            if (device == DEVICE_S10) device_name = "s10";
            assert(!device_name.empty());

            std::ostringstream project_name_ss;
            project_name_ss << "pe_array_fw" << feature_filter_width << "_dot"
              << dot_size << "_fe" << num_features << "_ram" << ram_depth <<
              "_" << device_name;

            callback(&cfg, project_name_ss.str());
          }
        }
      }
    }
  }
}

void generate_projects() {
  iterate_over_cfgs([&] (const pe_array_cfg_t* cfg, const std::string& project_name) {
    generate_quartus_project(cfg, PROJECT_TEMPLATE_DIR, RTL_DIR, project_name,
      model_voltage, model_speed_grade);

    bool area_error = false;
    pe_array_area_estimate_t area = pe_array_estimate_area(cfg, &area_error);

    std::cout << "Project " << project_name <<
      " successfully generated (estimated area: ";

    if (area_error) {
      std::cout << "unknown";
    } else {
      std::cout << "alms=" << area.num_alms << " aluts=" << area.num_aluts <<
        " regs=" << area.num_regs << " mlabs=" << area.num_mlabs << " m20ks="
        << area.num_m20ks << " dsps=" << area.num_dsps;
    }

    std::cout << ")\n";
    std::cout.flush();
  });
}

const char* get_device_name(DeviceFamily device) {
  switch (device) {
    case DEVICE_A10: return "a10";
    case DEVICE_S10: return "s10";
  };
  assert(false);
  return nullptr;
}

void generate_model(std::ofstream& output) {

  output << "static int pe_array_area_model_db_lookup_device_index(const pe_array_cfg_t* cfg, bool* error) {\n";
  output << "  *error = false;\n";
  for (size_t device_index = 0; device_index < all_device_families.size(); device_index++) {
    output << "  if (cfg->device_" << get_device_name(all_device_families[device_index])
      << ") return " << device_index << ";\n";
  }
  output << "  *error = true;\n";
  output << "  return 0;\n";
  output << "}\n";
  output << "\n";

  auto generate_int_array = [&](const std::vector<int>& values, const std::string& name) {
    output << "static std::array<int, " << values.size() <<
      "> pe_array_area_model_db_" << name << " = {";
    for (int value : values) {
      output << value << ",";
    }
    output << "};\n";
    output << "\n";
  };

  generate_int_array(all_feature_filter_widths, "feature_filter_width_corners");
  generate_int_array(all_dot_sizes, "dot_size_corners");
  generate_int_array(all_num_features, "num_features_corners");
  generate_int_array(all_ram_depths, "ram_depth_corners");

  output << "typedef struct {\n";
  output << "  struct {\n";
  output << "    struct {\n";
  output << "      struct {\n";
  output << "        struct {\n";
  output << "          struct {\n";
  output << "            pe_array_area_model_t area_model;\n";
  output << "          } ram_depth[" << all_ram_depths.size() << "];\n";
  output << "        } num_features[" << all_num_features.size() << "];\n";
  output << "      } dot_size[" << all_dot_sizes.size() << "];\n";
  output << "    } feature_filter_width[" << all_feature_filter_widths.size() << "];\n";
  output << "  } device[" << all_device_families.size() << "];\n";
  output << "} pe_array_area_model_db_t;\n";
  output << "\n";

  size_t db_size = all_ram_depths.size() * all_num_features.size() *
    all_dot_sizes.size() * all_feature_filter_widths.size() *
    all_device_families.size() * sizeof(pe_array_area_model_t);

  std::string db_size_str = (db_size < 1024) ?
    std::to_string(db_size) + " bytes" : std::to_string(db_size/1024.0) + " KB";

  output << "// Database size: " << db_size_str << "\n";
  output << "static pe_array_area_model_db_t pe_array_area_model_db = {{\n";

  for (DeviceFamily device : all_device_families) {
    output << "  {{ // device = " << get_device_name(device) << "\n";

    for (int feature_filter_width : all_feature_filter_widths) {
      output << "    {{ // feature_filter_width = " << feature_filter_width << "\n";

      for (int dot_size : all_dot_sizes) {
        output << "      {{ // dot_size = " << dot_size << "\n";

        for (int num_features : all_num_features) {
          output << "        {{ // num_features = " << num_features << "\n";

          for (int ram_depth : all_ram_depths) {
            output << "        {{ // ram_depth = " << ram_depth << "\n";

            pe_array_cfg_t cfg = generate_cfg(device, feature_filter_width,
                dot_size, num_features, ram_depth);
            std::string project_name = get_project_name(&cfg);

            std::cout << "Processing " << project_name << "...";
            std::cout.flush();

            std::string fit_report_filename = project_name +
              "/output_files/pe_array.fit.rpt";
            std::string sta_report_filename = project_name +
              "/output_files/pe_array.sta.rpt";

            pe_array_area_model_t area_model = {0};

            area_model.model_fmax = parse_fmax_from_sta_report(
                sta_report_filename);

            // Initialize values to -1 so that if we don't find an entry in the
            // fit report, possibly due to a Quartus crash or error, then the
            // missing data in the generated model is obvious and code using
            // the model can check for it.
            for (bool use_alm_dot : {false, true}) {
              for (bool use_alm_accum : {false, true}) {
                area_model.dot[use_alm_dot].accum[use_alm_accum].num_tenth_alms = -1;
                area_model.dot[use_alm_dot].accum[use_alm_accum].num_aluts = -1;
                area_model.dot[use_alm_dot].accum[use_alm_accum].num_regs = -1;
                area_model.dot[use_alm_dot].accum[use_alm_accum].num_mlabs = -1;
              }
            }

            iterate_over_fit_report(fit_report_filename, [&](const FitReportInfo& pe_info) {
              // Keep track of how much area was used in the PE array outside
              // the PEs
              if (pe_info.is_pe_array) {
                area_model.num_extra_tenth_alms += pe_info.num_tenth_alms;
                area_model.num_extra_aluts += pe_info.num_aluts;
                area_model.num_extra_regs += pe_info.num_regs;
                area_model.num_extra_mlabs += pe_info.num_mlabs;
              } else {
                area_model.num_extra_tenth_alms -= pe_info.num_tenth_alms;
                area_model.num_extra_aluts -= pe_info.num_aluts;
                area_model.num_extra_regs -= pe_info.num_regs;
                area_model.num_extra_mlabs -= pe_info.num_mlabs;
              }

              if (!pe_info.is_pe_array) {
                bool use_alm_dot = (pe_info.pe_id % 2) == 1;
                bool use_alm_accum = pe_info.pe_id >= 2;

                // We should only find one PE of each type, so the current
                // value should be -1
                assert(area_model.dot[use_alm_dot].accum[use_alm_accum].num_tenth_alms == -1);
                assert(area_model.dot[use_alm_dot].accum[use_alm_accum].num_aluts == -1);
                assert(area_model.dot[use_alm_dot].accum[use_alm_accum].num_regs == -1);
                assert(area_model.dot[use_alm_dot].accum[use_alm_accum].num_mlabs == -1);

                area_model.dot[use_alm_dot].accum[use_alm_accum].num_tenth_alms = pe_info.num_tenth_alms;
                area_model.dot[use_alm_dot].accum[use_alm_accum].num_aluts = pe_info.num_aluts;
                area_model.dot[use_alm_dot].accum[use_alm_accum].num_regs = pe_info.num_regs;
                area_model.dot[use_alm_dot].accum[use_alm_accum].num_mlabs = pe_info.num_mlabs;
              }

              // continue iterating
              return true;
            });


            output << "          {{{{"
              << area_model.dot[0].accum[0].num_tenth_alms << ","
              << area_model.dot[0].accum[0].num_aluts << ","
              << area_model.dot[0].accum[0].num_regs << ","
              << area_model.dot[0].accum[0].num_mlabs << "},{"
              << area_model.dot[0].accum[1].num_tenth_alms << ","
              << area_model.dot[0].accum[1].num_aluts << ","
              << area_model.dot[0].accum[1].num_regs << ","
              << area_model.dot[0].accum[1].num_mlabs << "}}},{{{"
              << area_model.dot[1].accum[0].num_tenth_alms << ","
              << area_model.dot[1].accum[0].num_aluts << ","
              << area_model.dot[1].accum[0].num_regs << ","
              << area_model.dot[1].accum[0].num_mlabs << "},{"
              << area_model.dot[1].accum[1].num_tenth_alms << ","
              << area_model.dot[1].accum[1].num_aluts << ","
              << area_model.dot[1].accum[1].num_regs << ","
              << area_model.dot[1].accum[1].num_mlabs << "}}}},"
              << area_model.num_extra_tenth_alms << ","
              << area_model.num_extra_aluts << ","
              << area_model.num_extra_regs << ","
              << area_model.num_extra_mlabs << ",PE_ARRAY_AREA_MODEL_FMAX("
              << area_model.model_fmax << ")\n";

            std::cout << " done\n";
            std::cout.flush();

            output << "        }}, // ram_depth = " << ram_depth << "\n";
          }

          output << "        }}, // num_features = " << num_features << "\n";
        }

        output << "      }}, // dot_size = " << dot_size << "\n";
      }

      output << "    }}, // feature_filter_width = " << feature_filter_width << "\n";
    }

    output << "  }}, // device = " << get_device_name(device) << "\n";
  }

  output << "}};\n";

  std::cout << "In-memory area model database size: " << db_size_str << "\n";
}

size_t get_file_size(const std::string& filename) {
  std::ifstream file(filename, std::ifstream::ate | std::ifstream::binary);
  return file.tellg();
}

void parse_projects() {
  std::ifstream area_model_template(AREA_MODEL_TEMPLATE_FILENAME);
  std::ofstream area_model_output(AREA_MODEL_FILENAME);

  for (std::string line; std::getline(area_model_template, line);) {
    if (line.find("__GENERATE_MODEL_HERE__") != std::string::npos) {
      generate_model(area_model_output);
    }
    else {
      area_model_output << line << "\n";
    }
  }

  area_model_template.close();
  area_model_output.close();

  size_t file_size = get_file_size(AREA_MODEL_FILENAME);
  std::string file_size_str = (file_size < 1024) ?
    std::to_string(file_size) + " bytes" : std::to_string(file_size/1024.0) + " KB";

  std::cout << "Succesfully generated " AREA_MODEL_FILENAME ", total file size: "
    << file_size_str << "\n";
}

void check_projects() {
  iterate_over_cfgs([&] (const pe_array_cfg_t* cfg, const std::string& project_name) {
    bool area_error = false;
    pe_array_area_estimate_t area = pe_array_estimate_area(cfg, &area_error);

    std::string fit_report_filename = project_name +
      "/output_files/pe_array.fit.rpt";

    std::cout << project_name << ": ";

    if (area_error) {
      std::cout << "area estimate error\n";
    } else {
      bool found_pe_array = false;

      iterate_over_fit_report(fit_report_filename, [&](const FitReportInfo& fit_info) {
        if (fit_info.is_pe_array) {
          assert(found_pe_array == false);
          found_pe_array = true;

          int fit_alms = PE_ARRAY_CEIL(fit_info.num_tenth_alms, 10);
          std::cout << "num_alms[est=" << area.num_alms << " fit=" <<
          fit_alms << " diff=" << (fit_alms - area.num_alms) <<
          " (" << ((((float)fit_alms - area.num_alms) * 100.0)/area.num_alms) <<
          "%)] num_aluts[est=" << area.num_aluts << " fit=" << fit_info.num_aluts
          << " diff=" << (fit_info.num_aluts - area.num_aluts) <<
          "] num_regs=[est=" << area.num_regs << " fit=" << fit_info.num_regs
          << " diff=" << (fit_info.num_regs - area.num_regs) <<
          "] num_mlabs=[est=" << area.num_mlabs << " fit=" << fit_info.num_mlabs
          << " diff=" << (fit_info.num_mlabs - area.num_mlabs) <<
          "] num_dsps[est=" << area.num_dsps << " fit=" << fit_info.num_dsps
          << " match=" << (fit_info.num_dsps == area.num_dsps) <<
          "] num_m20ks[est=" << area.num_m20ks << " fit=" << fit_info.num_m20ks
          << " match=" << (fit_info.num_m20ks == area.num_m20ks) << "] fmax="
          << area.model_fmax << "\n";
        }

        // continue iterating
        return true;
      });

      if (!found_pe_array) {
        std::cout << "cound not find PE array in fit report (estimated: num_alms="
          << area.num_alms << " num_dsps=" << area.num_dsps << " num_m20ks=" <<
          area.num_m20ks << " model_fmax=" << area.model_fmax << ")\n";
      }
    }

    std::cout.flush();

  });
}

void print_usage_and_exit(int argc, char** argv) {
  assert(argc >= 1);
  std::cout << "Usage: " << argv[0] << " <projects|model|check>\n";
  exit(1);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage_and_exit(argc, argv);
  }

  std::string mode_str = argv[1];
  if (mode_str.compare("projects") == 0) { generate_projects(); }
  else if (mode_str.compare("model") == 0) { parse_projects(); }
  else if (mode_str.compare("check") == 0) { check_projects(); }
  else {
    print_usage_and_exit(argc, argv);
  }

  return 0;
}
