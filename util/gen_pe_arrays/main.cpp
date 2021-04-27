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

#include <cmath>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <sstream>

using namespace pe_array;

#define PROJECT_TEMPLATE_DIR "../../common/quartus_project_template"
#define RTL_DIR "../../../rtl"

struct data_type_info_t {
  const char *name;
  bool        enable_blockfp_mode;
  int         feature_filter_width;
};

struct device_info_t {
  DeviceFamily               device;
  std::string                device_name;
  int                        device_alms;
  int                        device_dsps;
  int                        device_m20ks;
  std::vector<DeviceVoltage> device_voltages;
  std::vector<int>           device_speed_grades;
};

struct arch_info_t {
  const char *name;
  bool        enable_blockfp_mode;
  int         feature_filter_width;
  int         exponent_width;
  int         filters_per_pe;
  int         p_vector;
  int         q_vector;
  int         c_vector;
  int         k_vector;
  int         activation_k_vector;
  int         num_filter_ddr_banks;
  int         ddr_bandwidth_in_bytes;
  int         filter_cache_depth;
};

// ARCH Parameters

const std::vector<arch_info_t> arch_infos = {
    {/* name */ "fp11",
     /* enable_blockfp_mode */ true,
     /* feature_filter_width */ 7,
     /* exponent_width */ 5,
     /* filter_per_pe */ 2,
     /* p_vector */ 2,
     /* q_vector */ 4,
     /* c_vector */ 8,
     /* k_vector */ 16,
     /* activation_k_vector */ 8,
     /* num_filter_ddr_banks */ 2,
     /* ddr_bandwidth_in_bytes */ 64,
     /* filter_cache_depth */ 2048},
    {/* name */ "fp11",
     /* enable_blockfp_mode */ true,
     /* feature_filter_width */ 7,
     /* exponent_width */ 5,
     /* filter_per_pe */ 2,
     /* p_vector */ 3,
     /* q_vector */ 6,
     /* c_vector */ 16,
     /* k_vector */ 64,
     /* activation_k_vector */ 8,
     /* num_filter_ddr_banks */ 2,
     /* ddr_bandwidth_in_bytes */ 64,
     /* filter_cache_depth */ 2048},
};

// PROJECT & CSV Parameters

const std::vector<data_type_info_t> all_data_types = {
    {"fp8", /* enable_blockfp_mode */ true, /* feature_filter_width */ 4},
    {"fp9", /* enable_blockfp_mode */ true, /* feature_filter_width */ 5},
    {"fp11", /* enable_blockfp_mode */ true, /* feature_filter_width */ 7},
    {"fp16", /* enable_blockfp_mode */ true, /* feature_filter_width */ 12},
};

const std::vector<DeviceFamily> all_device_families = {DEVICE_A10, DEVICE_S10};
const std::vector<int>          all_dsp_percents = {90, 100};
const std::vector<int>          all_alm_percents = {50, 80, 90, 100};
const std::vector<int>          all_m20k_percents = {50, 90, 100};
const std::vector<int>          all_dot_sizes = {4, 8, 16, 32};
const std::vector<int>          all_ram_depths = {1024, 2048};
const std::vector<int>          all_num_features = {1, 8, 16, 24, 32, 40, 48, 56, 64};

const int num_chains = 1;
const int num_seeds = 3;

const char *get_voltage_name(DeviceVoltage voltage) {
  switch (voltage) {
  case VOLTAGE_090V: return "0.9v"; break;
  case VOLTAGE_095V: return "0.95v"; break;
  default: assert(false);
  }
}

// Helper function to check if the given value is a power of 2
static bool is_power_of_two(uint32_t x) { return (x != 0) && ((x & (x - 1)) == 0); }

int find_num_pes(pe_array_cfg_options_t cfg_options, int alm_limit, int m20k_limit, bool *error) {
  bool      init_active = true;
  const int init_num_pes_step = 1024;
  int       init_num_pes = init_num_pes_step;

  int under_limit_num_pes = 0;
  int over_limit_num_pes = 0;

  while (init_active || over_limit_num_pes != (under_limit_num_pes + 1)) {
    int test_num_pes = init_active ? init_num_pes : (over_limit_num_pes + under_limit_num_pes) / 2;

    cfg_options.num_pes = test_num_pes;

    pe_array_cfg_t           cfg = pe_array_cfg_init(cfg_options);
    pe_array_area_estimate_t area = pe_array_estimate_area(&cfg, error);
    if (*error)
      return 0;

    if (area.num_alms <= alm_limit && area.num_m20ks <= m20k_limit) {
      if (init_active) {
        init_num_pes += init_num_pes_step;
      } else {
        under_limit_num_pes = test_num_pes;
      }
    } else {
      if (init_active) {
        over_limit_num_pes = init_num_pes;
        under_limit_num_pes = init_num_pes - init_num_pes_step;
        init_active = false;
      } else {
        over_limit_num_pes = test_num_pes;
      }
    }
  }

  return under_limit_num_pes;
}

void run_mode_arch(device_info_t &device_info) {
  pe_array_cfg_options_t cfg_options = zero_pe_array_cfg_options;

  for (auto arch_info : arch_infos) {
    bool enable_blockfp_mode = arch_info.enable_blockfp_mode;
    bool enable_fixed_mode = !arch_info.enable_blockfp_mode;
    int  feature_width = arch_info.feature_filter_width;
    int  filter_width = arch_info.feature_filter_width;
    int  exponent_width = arch_info.exponent_width;
    int  filters_per_pe = arch_info.filters_per_pe;
    int  p_vector = arch_info.p_vector;
    int  q_vector = arch_info.q_vector;
    int  c_vector = arch_info.c_vector;
    int  k_vector = arch_info.k_vector;
    int  activation_k_vector = arch_info.activation_k_vector;
    int  num_filter_ddr_banks = arch_info.num_filter_ddr_banks;
    int  ddr_bandwidth_in_bytes = arch_info.ddr_bandwidth_in_bytes;
    int  filter_cache_depth = arch_info.filter_cache_depth;

    int num_pes = k_vector / filters_per_pe;

    int packed_filter_block_size = (filter_width * c_vector + exponent_width + 7) / 8;
    int num_packed_filters_lsu = ddr_bandwidth_in_bytes / packed_filter_block_size;
    while (num_packed_filters_lsu * num_filter_ddr_banks > activation_k_vector ||
           num_packed_filters_lsu * num_filter_ddr_banks > PE_ARRAY_MAX_NUM_CHAINS ||
           !is_power_of_two(num_packed_filters_lsu) || k_vector % num_packed_filters_lsu != 0 ||
           num_pes % (num_packed_filters_lsu * num_filter_ddr_banks) != 0)
      num_packed_filters_lsu--;

    int num_pe_arrays = num_packed_filters_lsu * num_filter_ddr_banks;

    cfg_options.enable_blockfp_mode = enable_blockfp_mode;
    cfg_options.enable_fixed_mode = enable_fixed_mode;
    cfg_options.feature_width = feature_width;
    cfg_options.filter_width = filter_width;
    cfg_options.num_results_per_cycle = (activation_k_vector / num_pe_arrays) * p_vector * q_vector;
    cfg_options.num_chains = num_pe_arrays;
    cfg_options.num_pes = num_pes / num_pe_arrays;
    cfg_options.num_dots = 1;
    cfg_options.num_features = p_vector * q_vector;
    cfg_options.num_filters = filters_per_pe;
    cfg_options.ram_depth = filter_cache_depth;
    cfg_options.dot_size = c_vector;
    cfg_options.dsp_limit = device_info.device_dsps;
    cfg_options.device_a10 = device_info.device == DEVICE_A10;
    cfg_options.device_s10 = device_info.device == DEVICE_S10;
    pe_array_cfg_t cfg = pe_array_cfg_init(cfg_options);

    for (int seed = 1; seed <= num_seeds; seed++) {
      std::ostringstream project_name_ss;
      project_name_ss << "pe_array_" << device_info.device_name << "_" << p_vector << "x"
                      << q_vector << "x" << c_vector << "x" << k_vector << "_actk"
                      << activation_k_vector << "_" << arch_info.name << "_ram"
                      << filter_cache_depth << "_seed" << seed;
      std::string project_name = project_name_ss.str();

      std::cout << "Generating " << project_name << "...";
      std::cout.flush();

      generate_quartus_project(&cfg, PROJECT_TEMPLATE_DIR, RTL_DIR, project_name, VOLTAGE_090V, 1);

      std::cout << " done\n";
    }
  }
}

void print_usage_and_exit(int argc, char **argv) {
  assert(argc >= 1);
  std::cout << "Usage: " << argv[0] << " <csv|projects>\n";
  exit(1);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage_and_exit(argc, argv);
  }

  enum { MODE_CSV, MODE_PROJECTS, MODE_ARCH } mode;

  std::string mode_str = argv[1];
  if (mode_str.compare("csv") == 0) {
    mode = MODE_CSV;
  } else if (mode_str.compare("projects") == 0) {
    mode = MODE_PROJECTS;
  } else if (mode_str.compare("arch") == 0) {
    mode = MODE_ARCH;
  } else {
    print_usage_and_exit(argc, argv);
  }

  for (DeviceFamily device : all_device_families) {
    device_info_t device_info;

    switch (device) {
    case DEVICE_A10:
      device_info.device = DEVICE_A10;
      device_info.device_name = "a10";
      device_info.device_alms = 427200;
      device_info.device_dsps = 1518;
      device_info.device_m20ks = 2713;
      device_info.device_voltages = {VOLTAGE_090V, VOLTAGE_095V};
      device_info.device_speed_grades = {1};
      break;
    case DEVICE_S10:
      device_info.device = DEVICE_S10;
      device_info.device_name = "s10";
      device_info.device_alms = 933120;
      device_info.device_dsps = 5760;
      device_info.device_m20ks = 11721;
      device_info.device_voltages = {VOLTAGE_090V};
      device_info.device_speed_grades = {1, 2};
      break;
    default: assert(false);
    }

    if (mode == MODE_ARCH) {
      run_mode_arch(device_info);
    } else {
      for (int dsp_percent : all_dsp_percents) {
        for (int alm_percent : all_alm_percents) {
          for (int m20k_percent : all_m20k_percents) {
            int dsp_limit = (device_info.device_dsps * dsp_percent) / 100;
            int alm_limit = (device_info.device_alms * alm_percent) / 100;
            int m20k_limit = (device_info.device_m20ks * m20k_percent) / 100;

            std::ostringstream csv_filename_ss;
            csv_filename_ss << "area_" << device_info.device_name << "_dsp" << dsp_percent << "_alm"
                            << alm_percent << "_m20k" << m20k_percent << ".csv";
            std::ofstream csv;
            if (mode == MODE_CSV) {
              csv.open(csv_filename_ss.str());
            }

            csv << "type,dot size,ram depth,num_features,ops per cycle,"
                << "ops per cycle per alm,number of dot products,number of PEs,"
                << "average alms per PE,";

            std::ostringstream real_alms_headers;
            std::ostringstream real_dsps_headers;
            std::ostringstream real_m20ks_headers;
            std::ostringstream real_fmax_headers;
            std::ostringstream real_tops_headers;

            for (int speed_grade : device_info.device_speed_grades) {
              for (DeviceVoltage voltage : device_info.device_voltages) {
                std::string build_name = std::string("speed ") + std::to_string(speed_grade) +
                                         " - " + get_voltage_name(voltage);
                real_alms_headers << "real alms (" << build_name << "),";
                real_dsps_headers << "real dsps (" << build_name << "),";
                real_m20ks_headers << "real m20ks (" << build_name << "),";
                real_fmax_headers << "real fmax (" << build_name << "),";
                real_tops_headers << "TOPS (" << build_name << "),";
              }
            }

            csv << "estimated alms," << real_alms_headers.str();
            csv << "estimated dsps," << real_dsps_headers.str();
            csv << "estimated m20ks," << real_m20ks_headers.str();
            csv << "small model fmax (MHz)," << real_fmax_headers.str();
            csv << "TOPS (small model)," << real_tops_headers.str();
            csv << "\n";

            for (auto data_type : all_data_types) {
              bool enable_blockfp_mode = data_type.enable_blockfp_mode;
              bool enable_fixed_mode = !data_type.enable_blockfp_mode;
              int  feature_width = data_type.feature_filter_width;
              int  filter_width = data_type.feature_filter_width;
              int  num_filters = feature_width <= 7 && filter_width <= 7 ? 2 : 1;

              for (int dot_size : all_dot_sizes) {
                for (int ram_depth : all_ram_depths) {
                  for (int num_features : all_num_features) {
                    pe_array_cfg_options_t cfg_options = zero_pe_array_cfg_options;
                    cfg_options.enable_blockfp_mode = enable_blockfp_mode;
                    cfg_options.enable_fixed_mode = enable_fixed_mode;
                    cfg_options.feature_width = feature_width;
                    cfg_options.filter_width = filter_width;
                    cfg_options.num_results_per_cycle = num_features * num_filters;
                    cfg_options.num_chains = num_chains;
                    cfg_options.num_pes = 1;
                    cfg_options.num_dots = 1;
                    cfg_options.num_features = num_features;
                    cfg_options.num_filters = num_filters;
                    cfg_options.ram_depth = ram_depth;
                    cfg_options.dot_size = dot_size;
                    cfg_options.dsp_limit = dsp_limit;
                    cfg_options.device_a10 = device_info.device == DEVICE_A10;
                    cfg_options.device_s10 = device_info.device == DEVICE_S10;

                    bool error = false;
                    int  num_pes = find_num_pes(cfg_options, alm_limit, m20k_limit, &error);

                    if (error) {
                      csv << data_type.name << "," << dot_size << "," << ram_depth << ","
                          << num_features << "\n";
                    } else {
                      cfg_options.num_pes = num_pes;
                      pe_array_cfg_t cfg = pe_array_cfg_init(cfg_options);

                      pe_array_area_estimate_t area = pe_array_estimate_area(&cfg, &error);
                      // it should be impossible to have an error after this
                      // function call, if we didn't have an error when
                      // running find_num_pes
                      assert(!error);

                      int   total_num_pes = num_chains * num_pes;
                      int   avg_alms_per_pe = PE_ARRAY_CEIL(area.num_alms, total_num_pes);
                      int   num_dot_accum = total_num_pes * num_features * num_filters;
                      int   ops_per_cycle = num_dot_accum * dot_size * 2;
                      float ops_per_cycle_per_alm = (float)ops_per_cycle / area.num_alms;

                      std::ostringstream real_alms;
                      std::ostringstream real_dsps;
                      std::ostringstream real_m20ks;
                      std::ostringstream real_fmax;
                      std::ostringstream real_tops;

                      for (int speed_grade : device_info.device_speed_grades) {
                        for (DeviceVoltage voltage : device_info.device_voltages) {
                          std::string voltage_name;
                          switch (voltage) {
                          case VOLTAGE_090V: voltage_name = "090v"; break;
                          case VOLTAGE_095V: voltage_name = "095v"; break;
                          default: assert(false);
                          }

                          for (int seed = 1; seed <= num_seeds; seed++) {
                            std::ostringstream project_name_ss;
                            project_name_ss << "pe_array_" << device_info.device_name << "_"
                                            << voltage_name << "_sg" << speed_grade << "_dsp"
                                            << dsp_percent << "_alm" << alm_percent << "_m20k"
                                            << m20k_percent << "_" << data_type.name << "_dot"
                                            << dot_size << "_ram" << ram_depth << "_nfe"
                                            << num_features << "_seed" << seed;
                            std::string project_name = project_name_ss.str();

                            // TODO: [shaneoco] use the data from the other
                            // seeds as well
                            if (mode == MODE_CSV && seed == 1) {
                              // attempt to get information from the existing
                              // quartus project, if there is one
                              std::string fit_report_filename =
                                  project_name + "/output_files/pe_array.fit.rpt";
                              std::string sta_report_filename =
                                  project_name + "/output_files/pe_array.sta.rpt";

                              bool          fit_report_error = false;
                              FitReportInfo fit_info = parse_pe_array_info_from_fit_report(
                                  fit_report_filename, &fit_report_error);

                              if (fit_report_error) {
                                real_alms << ",";
                                real_dsps << ",";
                                real_m20ks << ",";
                              } else {
                                real_alms << PE_ARRAY_CEIL(fit_info.num_tenth_alms, 10) << ",";
                                real_dsps << fit_info.num_dsps << ",";
                                real_m20ks << fit_info.num_m20ks << ",";
                              }

                              int fmax = parse_fmax_from_sta_report(sta_report_filename);
                              if (fmax == -1) {
                                real_fmax << ",";
                                real_tops << ",";
                              } else {
                                real_fmax << (fmax / 100.0) << ",";
                                real_tops << ((float)ops_per_cycle * (fmax / 100.0) / 1000000.0)
                                          << ",";
                              }
                            }

                            if (mode == MODE_PROJECTS) {
                              std::cout << "Generating " << project_name << "...";
                              std::cout.flush();

                              generate_quartus_project(&cfg, PROJECT_TEMPLATE_DIR, RTL_DIR,
                                                       project_name, voltage, speed_grade);

                              std::cout << " done\n";
                            }
                          }
                        }
                      }

                      csv << data_type.name << "," << dot_size << "," << ram_depth << ","
                          << num_features << "," << ops_per_cycle << "," << ops_per_cycle_per_alm
                          << "," << num_dot_accum << "," << total_num_pes << "," << avg_alms_per_pe
                          << "," << area.num_alms << "," << real_alms.str() << area.num_dsps << ","
                          << real_dsps.str() << area.num_m20ks << "," << real_m20ks.str();

                      if (!std::isnan(area.model_fmax)) {
                        csv << area.model_fmax;
                      }

                      csv << "," << real_fmax.str();

                      if (!std::isnan(area.model_fmax)) {
                        csv << ((float)ops_per_cycle * area.model_fmax / 1000000.0);
                      }

                      csv << "," << real_tops.str() << "\n";
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  return 0;
}
