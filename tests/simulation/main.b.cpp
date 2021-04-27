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

#include "pe_array.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <list>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <unistd.h>
#include <algorithm>

#define DO_FILENAME "pe_array_test.do"
#define TEST_DATA_FILENAME "pe_array_test_data.txt"
#define LOG_FILENAME "output.log"

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

int num_test_cycles = 0;
bool compile_sim_lib = false;

uchar rand_uchar() {
  return rand() & 0xFF;
}

uint rand_uint() {
  return ((uint)rand() << 30) | ((uint)rand() << 15) | (uint)rand();
}

void write_pe_input(pe_array_cfg_t* cfg, std::ofstream& f, pe_array_input_t pe_input) {
  std::ios_base::fmtflags old_flags = f.flags();
  f << std::hex << std::uppercase << std::setfill('0');
  int width_count = 0;

#define WRITE_PE_INPUT_MEMBER(member_name) do { \
  width_count += sizeof(pe_input.member_name)*8; \
  f << std::setw(sizeof(pe_input.member_name)*2) << (uint)pe_input.member_name; \
} while (0)

  for (int chain_idx = cfg->num_chains-1; chain_idx >= 0; chain_idx--) {
    for (int dot_idx = cfg->num_dots-1; dot_idx >= 0; dot_idx--) {
      for (int feature_idx = cfg->num_features-1; feature_idx >= 0; feature_idx--) {
        WRITE_PE_INPUT_MEMBER(chain[chain_idx].feature_data
            [dot_idx][feature_idx].exponent);
        for (int dot_value_idx = cfg->dot_size-1; dot_value_idx >= 0; dot_value_idx--) {
          WRITE_PE_INPUT_MEMBER(chain[chain_idx].feature_data
              [dot_idx][feature_idx].mantissa[dot_value_idx]);
        }
      }
    }

    WRITE_PE_INPUT_MEMBER(chain[chain_idx].feature_valid);

    WRITE_PE_INPUT_MEMBER(chain[chain_idx].filter_read_addr);

    for (int dot_idx = cfg->num_dots-1; dot_idx >= 0; dot_idx--) {
      WRITE_PE_INPUT_MEMBER(chain[chain_idx].ram_write.data.data
          [dot_idx].exponent);
      for (int dot_value_idx = cfg->dot_size-1; dot_value_idx >= 0; dot_value_idx--) {
        WRITE_PE_INPUT_MEMBER(chain[chain_idx].ram_write.data.data
            [dot_idx].mantissa[dot_value_idx]);
      }
    }

    WRITE_PE_INPUT_MEMBER(chain[chain_idx].ram_write.addr);
    WRITE_PE_INPUT_MEMBER(chain[chain_idx].ram_write.filter_id);
    WRITE_PE_INPUT_MEMBER(chain[chain_idx].ram_write.pe_id);
    WRITE_PE_INPUT_MEMBER(chain[chain_idx].ram_write.enable);
  }

  WRITE_PE_INPUT_MEMBER(flush_accumulator);
  WRITE_PE_INPUT_MEMBER(send_output);

#undef WRITE_PE_INPUT_MEMBER

  assert(width_count == cfg->idata_width);
  f.flags(old_flags);
}

void write_pe_output(pe_array_cfg_t* cfg, std::ofstream& f, pe_array_output_t pe_output) {
  std::ios_base::fmtflags old_flags = f.flags();
  f << std::hex << std::uppercase << std::setfill('0');
  int width_count = 0;

#define WRITE_PE_OUTPUT_MEMBER(member_name) do { \
  width_count += sizeof(pe_output.member_name)*8; \
  f << std::setw(sizeof(pe_output.member_name)*2) << (uint)pe_output.member_name; \
} while (0)

  for (int chain_idx = cfg->num_chains-1; chain_idx >= 0; chain_idx--) {
    for (int result_idx = cfg->num_results_per_cycle-1; result_idx >= 0; result_idx--) {
      WRITE_PE_OUTPUT_MEMBER(result[chain_idx][result_idx]);
    }
  }

#undef WRITE_PE_OUTPUT_MEMBER

  assert(width_count == cfg->odata_width);
  f.flags(old_flags);
}

void run_model_and_save_result(pe_array_cfg_t* cfg, std::ofstream& f,
    bool pe_input_valid, pe_array_input_t pe_input,
    std::vector<uchar>& state_buffer) {

  pe_array_output_t pe_output_ref = zero_pe_array_output;

  if (pe_input_valid) {
    pe_output_ref = pe_array_model(cfg, pe_input, state_buffer.data(),
        state_buffer.size());
  }

  f << (pe_input_valid ? "v " : ". ");
  write_pe_input(cfg, f, pe_input);
  f << " ";
  write_pe_output(cfg, f, pe_output_ref);
  f << "\n";

  num_test_cycles++;
}

void run_model_for_one_set_of_weights(pe_array_cfg_t* cfg, std::ofstream& f,
    std::vector<uchar>& state_buffer, int filter_write_offset, int filter_size,
    int filter_read_offset, int dot_product_size, int num_dot_products) {

  int filter_write_pe_id = 0;
  int filter_write_filter_id = 0;
  int filter_write_addr = 0;

  int dot_product_cycle = 0;
  int dot_product_count = 0;

  // initialize in an already-drained state
  int accum_flush_cycle = cfg->num_flush_cycles;

  while (1) {

    pe_array_input_t pe_input = zero_pe_array_input;
    bool exit_loop = true;

    // initialize the entire pe_input struct to random data
    for (size_t i = 0; i < sizeof(pe_input); i++) {
      ((uchar*)&pe_input)[i] = rand_uchar();
    }

    // initialize a few specific control signals to 0
    for (int chain_idx = 0; chain_idx < cfg->num_chains; chain_idx++) {
      pe_input.chain[chain_idx].ram_write.enable = false;
      pe_input.chain[chain_idx].feature_valid = false;
    }
    pe_input.send_output = false;
    pe_input.flush_accumulator = false;

    // write random weights to the filter cache if there are still weights left
    // to write
    if (filter_size > 0 && filter_write_pe_id < cfg->num_pes) {
      exit_loop = false;

      // insert filter write stalls randomly 25% of the time
      if (rand() >= (RAND_MAX/4)) {
        for (int chain_idx = 0; chain_idx < cfg->num_chains; chain_idx++) {
          pe_input.chain[chain_idx].ram_write.enable    = true;
          pe_input.chain[chain_idx].ram_write.pe_id     = filter_write_pe_id;
          pe_input.chain[chain_idx].ram_write.filter_id = filter_write_filter_id;
          pe_input.chain[chain_idx].ram_write.addr      = filter_write_offset + filter_write_addr;
        }

        filter_write_addr++;
        if (filter_write_addr == filter_size) {
          filter_write_addr = 0;
          filter_write_filter_id++;
        }
        if (filter_write_filter_id == cfg->num_filters) {
          filter_write_filter_id = 0;
          filter_write_pe_id++;
        }
      }
    }

    // compute dot products if there are still dot products left to compute
    if (dot_product_count < num_dot_products) {
      exit_loop = false;

      bool send_feature = true;

      // insert feature sending stalls randomly 25% of the time
      if (rand() < (RAND_MAX/4)) send_feature = false;

      // don't send feature data if we need to flush but we're already
      // currently flushing
      bool last_dot_cycle = dot_product_cycle == (dot_product_size-1);
      if (last_dot_cycle && accum_flush_cycle < cfg->num_flush_cycles) send_feature = false;

      if (send_feature) {
        for (int chain_idx = 0; chain_idx < cfg->num_chains; chain_idx++) {
          pe_input.chain[chain_idx].feature_valid = true;
          pe_input.chain[chain_idx].filter_read_addr = filter_read_offset + dot_product_cycle;
        }

        dot_product_cycle++;
        if (last_dot_cycle) {
          dot_product_cycle = 0;
          dot_product_count++;
          accum_flush_cycle = 0;
          pe_input.flush_accumulator = true;
        }
      }
    }

    // ask for more output data if we are still flushing the accumulators
    if (accum_flush_cycle < cfg->num_flush_cycles) {
      exit_loop = false;

      // insert stalls randomly 25% of the time
      pe_input.send_output = rand() >= (RAND_MAX/4);

      if (pe_input.send_output) {
        accum_flush_cycle++;
      }
    }

    if (exit_loop) {
      break;
    } else {
      run_model_and_save_result(cfg, f, /* pe_input_valid */ true, pe_input,
          state_buffer);

      // insert an invalid cycle randomly 25% of the time
      if (rand() < (RAND_MAX/4)) {
        run_model_and_save_result(cfg, f, /* pe_input_valid */ false, pe_input,
            state_buffer);
      }

    }
  }
}

bool gen_test_data(pe_array_cfg_t* cfg, int test_num, const std::string& test_name) {
  // Re-initialize the random number generator with a test-specific constant
  // seed so that we always get the same random numbers for this test.
  srand(test_num);

  // TODO: [shaneoco] this probably shouldn't be a global variable
  num_test_cycles = 0;

  std::string test_data_path = test_name + "/" TEST_DATA_FILENAME;
  std::ofstream test_data(test_data_path, std::ios::trunc);

  std::vector<uchar> state_buffer(pe_array_calc_state_buffer_size(cfg));

  // --- write filter of size 1 to offset 0 ---
  // * prepares filter data for next step
  run_model_for_one_set_of_weights(cfg, test_data, state_buffer,
      /* filter_write_offset */ 0, /* filter_size */ 1,
      /* filter_read_offset */ 0, /* dot_product_size */ 0,
      /* num_dot_products */ 0);

  // --- perform 1 dot product of size 1 using weights at offset 0 ---
  // --- write filter of size 1 to offset cfg->ram_depth-1 ---
  // * tests 1-cycle dot product from offset 0 (edge cases)
  run_model_for_one_set_of_weights(cfg, test_data, state_buffer,
      /* filter_write_offset */ cfg->ram_depth-1, /* filter_size */ 1,
      /* filter_read_offset */ 0, /* dot_product_size */ 1,
      /* num_dot_products */ 1);

  // --- perform 1 dot product of size 1 using weights at offset cfg->ram_depth-1 ---
  // --- write filter of size 10 to offset 0 ---
  // * tests 1-cycle dot product from offset cfg->ram_depth-1 (edge cases)
  // * tests taking longer to load weights than to compute
  run_model_for_one_set_of_weights(cfg, test_data, state_buffer,
      /* filter_write_offset */ 0, /* filter_size */ 10,
      /* filter_read_offset */ cfg->ram_depth-1, /* dot_product_size */ 1,
      /* num_dot_products */ 1);

  // --- perform 4 dot products of size 10 using weights at offset 0 ---
  // * tests taking longer to compute than to load weights
  run_model_for_one_set_of_weights(cfg, test_data, state_buffer,
      /* filter_write_offset */ 0, /* filter_size */ 0,
      /* filter_read_offset */ 0, /* dot_product_size */ 10,
      /* num_dot_products */ 4);

  // --- perform 2 dot products of size 10 using weights at offset 0 ---
  // --- write filter of size 32 to offset cfg->ram_depth/2 ---
  // * tests taking longer to load weights than to compute (probably)
  run_model_for_one_set_of_weights(cfg, test_data, state_buffer,
      /* filter_write_offset */ cfg->ram_depth/2, /* filter_size */ 32,
      /* filter_read_offset */ 0, /* dot_product_size */ 10,
      /* num_dot_products */ 2);

  for (int dot_product_size : {1, 2, 3, 4, 8, 16}) {
    // --- perform dot products of selected size using weights at offset cfg->ram_depth/2 ---
    run_model_for_one_set_of_weights(cfg, test_data, state_buffer,
        /* filter_write_offset */ 0, /* filter_size */ 0,
        /* filter_read_offset */ cfg->ram_depth/2, dot_product_size,
        /* num_dot_products */ 8);
  }

  test_data.close();
  return true;
}

void check_if_compiling_sim_lib() {
  std::cout << "Checking ModelSim version...\n";

  FILE* stream = popen("vsim -version", "r");
  if (!stream) {
    std::cerr << "Error: could not run vsim command to find the version being used\n";
    exit(1);
  }

  std::string command_output;
  while (!feof(stream)) {
    const int buffer_size = 256;
    char buffer[buffer_size];
    if (fgets(buffer, buffer_size, stream)) {
      command_output.append(buffer);
    }
  }

  int retval = pclose(stream);
  if (retval != 0) {
    std::cerr << "Error: vsim command to find version being used failed (return value was " << retval << ")\n";
    exit(1);
  }

  auto case_insensitive_compare = [](char a, char b) {
    return std::tolower(a) == std::tolower(b);
  };

  std::string intel_str = "intel";
  std::string altera_str = "altera";

  bool intel_found = std::search(command_output.begin(),
      command_output.end(), intel_str.begin(), intel_str.end(),
      case_insensitive_compare) != command_output.end();

  bool altera_found = std::search(command_output.begin(),
      command_output.end(), altera_str.begin(), altera_str.end(),
      case_insensitive_compare) != command_output.end();

  compile_sim_lib = !(intel_found || altera_found);

  // The command_output string already has a newline from the vsim output
  std::cout << "ModelSim version: " << command_output;

  if (compile_sim_lib) {
    std::cout << "ModelSim version detected that does NOT have built-in "
      "simulation libraries (i.e. ModelSim SE), they will be compiled\n";
  } else {
    std::cout << "ModelSim version detected that has built-in simulation "
      "libraries (i.e. ModelSim Intel edition), they will not be compiled\n";
  }

  std::cout << "\n";
}

bool run_vsim(pe_array_cfg_t* cfg, const std::string& test_name) {

  std::string do_path = test_name + "/" DO_FILENAME;
  //std::string define_path = test_name + "/reg_define.sv" ;
  //std::string param_path  = test_name + "/param.sv" ;
  std::string filelist_path  = test_name + "/tb.vc" ;
  std::string makefile_path  = test_name + "/makefile" ;
  std::ofstream do_file(do_path, std::ios::trunc);
  //std::ofstream define_file(define_path, std::ios::trunc);
  //std::ofstream param_file(param_path, std::ios::trunc);
  std::ofstream filelist_file(filelist_path, std::ios::trunc);
  std::ofstream makefile_file(makefile_path, std::ios::trunc);

  //// TODO: [shaneoco] log /r * and -O0 should probably not be the default as
  //// they might make things slower
  //pe_array_cfg_iterate(cfg, [&](const char* name, int value) {
  //  	define_file << " `define " << name << " " << value << "\n";
  //  });

  //param_file << "assign " << num_test_cycles << " NUM_TEST_CYCLES" << "\n";
  //pe_array_cfg_iterate(cfg, [&](const char* name, int value) {
  //  	param_file << " assign " << value << " " << name << "\n";
  //  });
  //define_file.close();
  //param_file.close();

//////////////////////////////////////////////////////////////////////////////////////////////////////
//makefile
  makefile_file << "DUMP=NONE "  << "\n";

  makefile_file << "VALUE= "  ;
  makefile_file << " -pvalue+NUM_TEST_CYCLES=" << num_test_cycles;
  pe_array_cfg_iterate(cfg, [&](const char* name, int value) {
    makefile_file << " -pvalue+" << name << "=" << value;
  });
  makefile_file << "\n";

  makefile_file << "PDEFINE= "  ;
  pe_array_cfg_iterate(cfg, [&](const char* name, int value) {
    makefile_file << " +define+" << name << "=" << value;
  });
  makefile_file << "\n";

  makefile_file <<"verdi_path = /usr/synopsys/Verdi3_L-2016.06-1"<<"\n";
  makefile_file <<"DEBUG_ELAB_OPTS += -debug_all"<<"\n";
  makefile_file <<"ELAB_OPTS += $(DEBUG_ELAB_OPTS)"<<"\n";
  makefile_file <<"ELAB_OPTS += +lint=TFIPC-L"<<"\n";
  makefile_file <<"default:"<<"\n";
  makefile_file <<"	";
  makefile_file <<"echo \" \\`define  $(DUMP)       \"  >   define.v"<<"\n";
  makefile_file <<"	";
  makefile_file <<"/bin/cp   ../../../../rtl/pe_inc.sv  ./"<<"\n";
  makefile_file <<"	";
  makefile_file <<"vcs -assert svaext  +error+10000 -full64  -ntb_opts dtm -lca -timescale=1ns/1ps  -P  $(verdi_path)/share/PLI/VCS/LINUX64/novas.tab   $(verdi_path)/share/PLI/VCS/LINUX64/pli.a   +vcs+lic+wait +vcd+vcdpluson  -sverilog +verilog2001ext+.v  +lint=TFIPC-L    -debug_all $(PDEFINE)  $(VALUE)   -f tb.vc -top pe_array_tb    -cm line+cond+fsm+tgl -cm_dir cov_result/my_cov_info     -l vcs_com.log"<<"\n";
  makefile_file <<"	";
  makefile_file <<"./simv  +notimingcheck -debug_all   -cm line+cond+fsm+tgl -cm_dir cov_result/my_cov_info      -l vcs_sim.log"<<"\n";
  makefile_file <<"verdi:"<<"\n";
  makefile_file <<"	";
  makefile_file <<"verdi -sv -f tb.vc  -top  pe_array_tb  -nologo  &"<<"\n";
  makefile_file <<"wave:"<<"\n";
  makefile_file <<"	";
  makefile_file <<"dve -full64 -vpd vcdplus.vpd &"<<"\n";
  makefile_file.close();

////////////////////////////////////////////////////////////////////////////////////////////////////////
  do_file << "vlib work\n";
  do_file << "vmap work work\n";

    filelist_file << "./define.v" << "\n";
    filelist_file << "./pe_inc.sv" << "\n";
    filelist_file << "../../../tb/220model.v" << "\n";
    filelist_file << "../../../tb/altera_lnsim.sv" << "\n";
    filelist_file << "../../../tb/twentynm_atoms.v" << "\n";
    filelist_file << "../../../tb/fourteennm_atoms.sv" << "\n";
    filelist_file << "../../../tb/fourteennm_atoms_ncrypt.sv" << "\n";
  
  for (const std::string& filename : pe_array_files) {
    //do_file << "vlog -O0 ../../../../rtl/" << filename << "\n";
    do_file << "vlog ";
    pe_array_cfg_iterate(cfg, [&](const char* name, int value) {
    	do_file << " +define+" << name << "=" << value;
    });
    do_file << "-O0 ../../../../rtl/" << filename << "\n";
    filelist_file << "../../../../rtl/" << filename << "\n";
  }
  do_file << "vlog -O0 ../../pe_array_tb.sv +incdir+../../../../rtl\n";
  filelist_file << "../../pe_array_tb.sv" << "\n";
  filelist_file.close();

  if (!compile_sim_lib) {
    do_file << "vsim work.pe_array_tb";
    do_file << " -L 220model -L altera_ver -L altera_mf_ver -L altera_lnsim_ver";

    if (cfg->device_a10) do_file << " -L twentynm";
    if (cfg->device_s10) do_file << " -L fourteennm";
    assert(cfg->device_a10 || cfg->device_s10);

  } else {
    const char* quartus_dir = std::getenv("QUARTUS_DIR");
    if (!quartus_dir) {
      std::cerr << "Error: Environment variable QUARTUS_DIR is not set, which "
        "is required to find Quartus simulation libraries\n";
      exit(1);
    }

    // There are extra simulation libraries that Quartus has but are missing
    // from the lists below. That's because they are for modules that we don't
    // currently use in the PE array, and so we avoid compiling them to save
    // time. In case we need one someday, here are the ones that are missing as
    // of writing this comment:
    // * Common: 220model.v sgate.v altera_mf.v
    // * A10: twentynm_hssi_atoms.v mentor/twentynm_hip_atoms_ncrypt.v
    //        twentynm_hip_atoms.v
    // * S10: ct1_hssi_atoms.sv mentor/ct1_hssi_atoms_ncrypt.sv
    //        mentor/cr3v0_serdes_models_ncrypt.sv ct1_hip_atoms.sv
    //        mentor/ct1_hip_atoms_ncrypt.sv

    std::vector<std::string> sim_lib_files = {"altera_primitives.v", "altera_lnsim.sv"};
    std::initializer_list<std::string> sim_lib_files_a10 = {"twentynm_atoms.v", "mentor/twentynm_atoms_ncrypt.v"};
    std::initializer_list<std::string> sim_lib_files_s10 = {"fourteennm_atoms.sv", "mentor/fourteennm_atoms_ncrypt.sv"};

    if (cfg->device_a10) { sim_lib_files.insert(sim_lib_files.end(), sim_lib_files_a10); }
    if (cfg->device_s10) { sim_lib_files.insert(sim_lib_files.end(), sim_lib_files_s10); }
    assert(cfg->device_a10 || cfg->device_s10);

    for (const std::string& filename : sim_lib_files) {
      std::string sv_ext = ".sv";
      bool is_sv = (filename.compare(filename.length() - sv_ext.length(), sv_ext.length(), sv_ext) == 0);
      std::string vlog_opt = is_sv ? "" : "-vlog01compat";
      do_file << "vlog " << vlog_opt << " " << quartus_dir << "/eda/sim_lib/" << filename << "\n";
    }

    do_file << "vsim work.pe_array_tb";
  }

  do_file << " -gNUM_TEST_CYCLES=" << num_test_cycles;

  pe_array_cfg_iterate(cfg, [&](const char* name, int value) {
    do_file << " -g" << name << "=" << value;
  });

  do_file << "\n";

  do_file << "log -r /*\n";
  do_file << "run -all\n";

  do_file.close();

  std::string vsim_cmd = std::string("cd ") + test_name +
    " && vsim <" DO_FILENAME " >" + LOG_FILENAME + " 2>&1";
  int vsim_exit_code = system(vsim_cmd.c_str());
  if (vsim_exit_code != 0) {
    std::cerr << "Error: modelsim returned non-zero exit code " << vsim_exit_code << "\n";
    exit(1);
  }

  std::ifstream transcript(test_name + "/transcript");
  if (!transcript) {
    std::cerr << "Error: could not open the transcript\n";
    exit(1);
  }

  std::ostringstream pass_line;
  pass_line << "# PASSED " << num_test_cycles << "/" << num_test_cycles;

  bool passed = false;
  while (!transcript.eof()) {
    std::string line;
    std::getline(transcript, line);
    if (line == pass_line.str()) {
      passed = true;
    }
  }

  // free up disk space since the work directory is huge and there's no use for
  // it anymore
  std::string rm_cmd = std::string("rm -rf ") + test_name + "/work";
  int rm_exit_code = system(rm_cmd.c_str());
  if (rm_exit_code != 0) {
    std::cerr << "Error: could not delete work directory\n";
    exit(1);
  }

  return passed;
}

bool run_config(int test_num_filter, const std::string& test_name_filter,
    bool* test_ran, pe_array_cfg_t cfg) {
  static int test_num = 1;

  std::string mode_str;
  if (cfg.enable_fixed_mode) mode_str = "fixed";
  if (cfg.enable_blockfp_mode) mode_str = "blockfp";
  PE_ARRAY_ASSERT(!mode_str.empty());

  std::ostringstream test_name_ss;
  test_name_ss << mode_str
    << "_fe" << cfg.feature_width
    << "_fi" << cfg.filter_width
    << "_nr" << cfg.num_results_per_cycle
    << "_nc" << cfg.num_chains
    << "_np" << cfg.num_pes
    << "_nd" << cfg.num_dots
    << "_nfe" << cfg.num_features
    << "_nfi" << cfg.num_filters
    << "_ram" << cfg.ram_depth
    << "_dot" << cfg.dot_size
    << "_dr" << cfg.dot_ratio_dsp << "x" << cfg.dot_ratio_alm
    << "_ar" << cfg.accum_ratio_dsp << "x" << cfg.accum_ratio_alm
    << "_a" << cfg.device_a10
    << "_s" << cfg.device_s10;
  std::string test_name = test_name_ss.str();

  bool run_test = true;
  if (test_num_filter > 0 && test_num != test_num_filter) run_test = false;
  if (!test_name_filter.empty() && test_name != test_name_filter) run_test = false;

  bool passed = true;
  if (run_test) {
    std::string rm_cmd = std::string("rm -rf ") + test_name + " && mkdir " + test_name;
    if (system(rm_cmd.c_str()) != 0) {
      std::cerr << "Error: unable to create simulation directory\n";
      exit(1);
    }

    std::cout << "* Executing test " << test_num << " (" << test_name << ")... ";
    std::cout.flush();
    passed = gen_test_data(&cfg, test_num, test_name) && run_vsim(&cfg, test_name);
    std::cout << (passed ? "PASSED" : "FAILED!") << "\n";

    *test_ran = true;
  }

  test_num++;
  return passed;
}

int main(int argc, char** argv) {

  bool launch_arc = argc >= 2 && std::string(argv[1]) == "launch-arc";
  bool launch_arc_subjobs = argc >= 2 && std::string(argv[1]) == "launch-arc-subjobs";
  std::string arc_priority;
  std::string arc_group;
  if (launch_arc || launch_arc_subjobs) {
    if (argc < 4) {
      std::cerr << "Error: you must specify a group and priority\n";
      return 1;
    }
    arc_group = argv[2];
    arc_priority = argv[3];
  }

  if (launch_arc) {
    std::ostringstream arc_cmd;
    arc_cmd << "arc submit --local --test name=pe_array_tests -- ./test_runner "
      "launch-arc-subjobs " << arc_group << " " << arc_priority;
    int ret = system(arc_cmd.str().c_str());
    if (ret != 0) {
      std::cerr << "Error: arc submit returned error code " << ret << "\n";
      return 1;
    }
    return 0;
  }

  check_if_compiling_sim_lib();

  int test_num_filter = 0;
  std::string test_name_filter;
  std::string cfg_filename;

  if (!launch_arc && !launch_arc_subjobs && argc >= 2) {
    if (strncmp(argv[1], "cfg=", 4) == 0) {
      cfg_filename = &argv[1][4];
    } else {
      char* strtol_end;
      int test_num = strtol(argv[1], &strtol_end, 10);
      if (isdigit(argv[1][0]) && *strtol_end == 0 && test_num > 0) {
        test_num_filter = test_num;
      } else {
        test_name_filter = argv[1];
      }
    }
  }

  if (!cfg_filename.empty()) {
    pe_array_cfg_t cfg;

    FILE* cfg_file = fopen(cfg_filename.c_str(), "rb");
    if (cfg_file == NULL) {
      std::cerr << "Error: could not open " << cfg_filename << "\n";
      return 1;
    }
    assert(cfg_file != NULL);

    size_t elements_read = fread(&cfg, sizeof(cfg), 1, cfg_file);
    fclose(cfg_file);
    assert(elements_read == 1);

    bool test_ran = false;
    bool success = run_config(test_num_filter, test_name_filter, &test_ran, cfg);

    if (!test_ran) {
      std::cout << "Error: no test was run\n";
    }

    return (test_ran && success) ? 0 : 1;

  } else {

    int num_split_arrays = 1;
    int pe_output_width = 1;
    int activation_k_vector = 2;

    int num_pes = 2;

    bool success = true;
    bool test_ran = false;

    // TODO: [shaneoco] It might be possible to reduce the number of tests, or
    // at least change the regtest to run them all in parallel on the farm so
    // that the test isn't so long.

    for (bool enable_blockfp_mode : {true, false}) {
      bool enable_fixed_mode = !enable_blockfp_mode;

      std::vector<std::pair<int, int>> test_widths;
      std::vector<std::pair<int, int>> test_num_features_filters;

      if (enable_blockfp_mode) {
        test_widths = {
          std::make_pair(4,   4), // FP8, 4x packing
          std::make_pair(5,   5), // FP9, 4x packing with overlap
          std::make_pair(5,   4), // FP9/FP8, 4x packing with overlap
          std::make_pair(4,   5), // FP8/FP9, 4x packing with overlap
          std::make_pair(6,   6), // FP10, 2x packing
          std::make_pair(7,   7), // FP11, 2x packing
          std::make_pair(8,   8), // FP12, 1x packing
          std::make_pair(12, 12)  // FP16, 1x packing
        };

        test_num_features_filters = {
          std::make_pair(1, 1), // 1 feature, 1 filter
          std::make_pair(2, 1), // 2 feature, 1 filter
          std::make_pair(1, 2), // 1 feature, 2 filter
          std::make_pair(2, 2), // 2 feature, 2 filter
          std::make_pair(3, 2), // 3 feature, 2 filter
          std::make_pair(4, 2)  // 4 feature, 2 filter
        };
      } else {
        test_widths = {
          std::make_pair(7,   7), // INT7,  2x packing
          std::make_pair(8,   8), // INT8,  1x packing
        };

        // we assume all the other combinations of num_features/num_filters are
        // tested during the blockfp tests
        test_num_features_filters = {
          std::make_pair(1, 1)  // 1 feature, 1 filter
        };
      }

      for (auto&& width : test_widths) {
        int feature_width = width.first;
        int filter_width = width.second;

        for (auto&& num_features_filters : test_num_features_filters) {
          int num_features = num_features_filters.first;
          int num_filters = num_features_filters.second;

          for (int dot_size : {4, 8, 16, 32}) {
            for (int use_a10 : {true, false}) {

              pe_array_cfg_options_t cfg_options = zero_pe_array_cfg_options;
              cfg_options.enable_blockfp_mode   = enable_blockfp_mode;
              cfg_options.enable_fixed_mode     = enable_fixed_mode;
              cfg_options.feature_width         = feature_width;
              cfg_options.filter_width          = filter_width;
              cfg_options.num_results_per_cycle = (activation_k_vector / num_split_arrays) * num_features;
              cfg_options.num_chains            = num_split_arrays * pe_output_width;
              cfg_options.num_pes               = num_pes;
              cfg_options.num_dots              = 1;
              cfg_options.num_features          = num_features;
              cfg_options.num_filters           = num_filters;
              cfg_options.ram_depth             = 512;
              cfg_options.dot_size              = dot_size;
              cfg_options.enable_ratios         = true;
              cfg_options.dot_ratio_alm         = 1;
              cfg_options.dot_ratio_dsp         = 1;
              cfg_options.accum_ratio_alm       = 1;
              cfg_options.accum_ratio_dsp       = 1;
              cfg_options.device_a10            = use_a10 ? 1 : 0;
              cfg_options.device_s10            = use_a10 ? 0 : 1;
              pe_array_cfg_t cfg = pe_array_cfg_init(cfg_options);

              success &= run_config(test_num_filter, test_name_filter,
                  &test_ran, cfg);
            }
          }
        }
      }
    }

    if (!test_ran) {
      std::cout << "Error: no test was run\n";
    }

    return (test_ran && success) ? 0 : 1;
  }
}
