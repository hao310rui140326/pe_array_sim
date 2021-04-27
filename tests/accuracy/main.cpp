// INTEL CONFIDENTIAL
// Copyright 2015-2017 Intel Corporation
//
// The source code contained or described herein and all documents related to
// the source code ("Material") are owned by Intel Corporation or its suppliers
// or licensors. Title to the Material remains with Intel Corporation or its
// suppliers and licensors. The Material contains trade secrets and proprietary
// and confidential information of Intel or its suppliers and licensors. The
// Material is protected by worldwide copyright and trade secret laws and treaty
// provisions. No part of the Material may be used, copied, reproduced,
// modified, published, uploaded, posted, transmitted, distributed, or disclosed
// in any way without Intel's prior express written permission.
//
// No license under any patent, copyright, trade secret or other intellectual
// property right is granted to or conferred upon you by disclosure or delivery
// of the Materials, either expressly, by implication, inducement, estoppel or
// otherwise. Any license under such intellectual property rights must be
// express and approved by Intel in writing.

#include "pe_array.h"

#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

typedef struct {
  pe_array_cfg_t cfg;
  int test_num;
  float ref_result;
  float model_result;
  float diff;
  float diff_percent;
} test_result_t;

typedef struct {
  pe_array_cfg_t cfg;
  float avg_diff;
  float avg_diff_percent;
  float max_diff;
  float max_diff_percent;
} cfg_result_t;

const int NUM_TESTS_PER_CONFIG = 1000;
const int NUM_ACCUM_CYCLES = 3;

static char rand_char() {
  return rand() & 0xFF;
}

static short rand_short() {
  return ((short)rand_char() << 8) | (short)rand_char();
}

static int rand_int() {
  return ((int)rand_short() << 16) | (int)rand_short();
}

static long long rand_long_long() {
  return ((long long)rand_int() << 32) | (long long)rand_int();
}

static float fp16_to_fp32(ushort orig_fp16) {
  // This implementation was written to be essentially exactly the same as the
  // Verilog implementation that's part of the compiler. I used that Verilog as
  // a starting point and re-wrote it in OpenCL C.

  const uint FP32_EXPONENT_BIAS = 112;

  uint sign     = PE_ARRAY_BIT_SEL(orig_fp16, 15, 15);
  uint exponent = PE_ARRAY_BIT_SEL(orig_fp16, 14, 10);
  uint mantissa = PE_ARRAY_BIT_SEL(orig_fp16, 9, 0);

  // Examples for leading 1 count
  // 10 0000 0000 => 0
  // 01 0000 0000 => 1
  // 00 1000 0000 => 2
  if (exponent == 0) {
    int count_bit_3 = PE_ARRAY_BIT_SEL(mantissa, 9, 2) == 0;
    int count_bit_2 = (PE_ARRAY_BIT_SEL(mantissa, 9, 2) != 0 && PE_ARRAY_BIT_SEL(mantissa, 9, 6) == 0);
    int count_bit_1 =
       (count_bit_3 && PE_ARRAY_BIT_SEL(mantissa, 1, 0) == 0) ||
        (!count_bit_3 &&
        ((!count_bit_2 && PE_ARRAY_BIT_SEL(mantissa, 9, 8) == 0) || (count_bit_2 && PE_ARRAY_BIT_SEL(mantissa, 5, 4) == 0)));
    int count_bit_0 =
        (PE_ARRAY_BIT_SEL(mantissa, 9, 8) == 1) ||
        (PE_ARRAY_BIT_SEL(mantissa, 9, 6) == 1) ||
        (PE_ARRAY_BIT_SEL(mantissa, 9, 4) == 1) ||
        (PE_ARRAY_BIT_SEL(mantissa, 9, 2) == 1) ||
        (PE_ARRAY_BIT_SEL(mantissa, 9, 0) == 1);
    int count =
        (count_bit_3 << 3) | (count_bit_2 << 2) |
        (count_bit_1 << 1) | (count_bit_0 << 0);

    int shifted_mantissa = PE_ARRAY_BIT_SEL(mantissa << count, 8, 0);
    mantissa = shifted_mantissa << 14;

    exponent = FP32_EXPONENT_BIAS - count;
  } else {
    mantissa = mantissa << 13;
    exponent = FP32_EXPONENT_BIAS + exponent;
  }

  uint result = (sign << 31) | (exponent << 23) | (mantissa << 0);
  return *(float*)&result;
}

static float ref_dot_accum(pe_array_cfg_t* cfg, int pe_idx,
    pe_array_block_t feature[NUM_ACCUM_CYCLES],
    pe_array_block_t filter[NUM_ACCUM_CYCLES]) {

  // TODO: [shaneoco] make this value calculated based on the ALM accumulator
  // width and fraction width from the pe array config
  const ushort max_fp16 = (PE_ARRAY_BIT_MASK(4) << 11) | PE_ARRAY_BIT_MASK(10);
  const float max_fp16_as_float = fp16_to_fp32(max_fp16);

  bool use_alm_accum = (pe_idx % (cfg->accum_ratio_dsp +
    cfg->accum_ratio_alm)) >= cfg->accum_ratio_dsp;

  float accum_value;
  bool pos_saturate = false;
  bool neg_saturate = false;

  for (int accum_cycle = 0; accum_cycle < NUM_ACCUM_CYCLES; accum_cycle++) {

    // compute dot product output
    pe_array_int64_t dot_result = 0;
    for (int dot_value_idx = 0; dot_value_idx < cfg->dot_size; dot_value_idx++) {
      uint raw_feature = feature[accum_cycle].mantissa[dot_value_idx];
      uint raw_filter  = filter[accum_cycle].mantissa[dot_value_idx];

      bool feature_sign = PE_ARRAY_BIT_IS_SET(raw_feature, cfg->feature_width-1);
      bool filter_sign  = PE_ARRAY_BIT_IS_SET(raw_filter, cfg->filter_width-1);

      pe_array_int64_t feature_magnitude = PE_ARRAY_BIT_MASK_VAL(raw_feature, cfg->feature_width-1);
      pe_array_int64_t filter_magnitude  = PE_ARRAY_BIT_MASK_VAL(raw_filter, cfg->filter_width-1);

      dot_result += (feature_sign == filter_sign ? 1LL : -1LL) * feature_magnitude * filter_magnitude;
    }

    // convert dot product output to FP32
    int shift =
      PE_ARRAY_BIT_MASK_VAL(feature[accum_cycle].exponent, cfg->exponent_width) +
      PE_ARRAY_BIT_MASK_VAL(filter[accum_cycle].exponent, cfg->exponent_width)
      - (2*cfg->exponent_bias) - (cfg->feature_width-2) - (cfg->filter_width-2);

    float dot_result_fp32 = (shift >= 0) ?
      (double)dot_result * (double)(1ULL << shift) :
      (double)dot_result / (double)(1ULL << -shift);

    // The ALM accumulator saturates at the min/max FP16 values after every
    // accumulator
    if (use_alm_accum && !pos_saturate && !neg_saturate) {
      if (dot_result_fp32 > max_fp16_as_float) pos_saturate = true;
      if (dot_result_fp32 < -max_fp16_as_float) neg_saturate = true;
    }

    // accumulate
    accum_value += dot_result_fp32;
  }

  // The DSP accumulator saturates at the min/max FP16 values at the end of
  // the dot product accumulation
  if (!use_alm_accum) {
    if (accum_value > max_fp16_as_float) pos_saturate = true;
    if (accum_value < -max_fp16_as_float) neg_saturate = true;
  }

  if (pos_saturate) accum_value = max_fp16_as_float;
  if (neg_saturate) accum_value = -max_fp16_as_float;

  return accum_value;
}

static test_result_t run_test(pe_array_cfg_t* cfg, int test_num,
    pe_array_block_t feature[NUM_ACCUM_CYCLES],
    pe_array_block_t filter[NUM_ACCUM_CYCLES]) {

  test_result_t result;
  result.cfg = *cfg;
  result.test_num = test_num;

  // All PEs will have the same alm/dsp dot/accumulator settings for the accuracy
  // test, so it doesn't match which PE we pick. We might as well go with 0.
  int chain_idx = 0;
  int pe_idx = 0;

  // run the model
  uint model_result_fp16;
  pe_array_accum_state_t accum_state = zero_pe_array_accum_state;

  for (int accum_cycle = 0; accum_cycle < NUM_ACCUM_CYCLES; accum_cycle++) {
    bool flush_accumulator = (accum_cycle == NUM_ACCUM_CYCLES-1);
    model_result_fp16 = pe_array_dot_accum(cfg, chain_idx, pe_idx,
        feature[accum_cycle], filter[accum_cycle], flush_accumulator,
        &accum_state);
  }
  result.model_result = fp16_to_fp32(model_result_fp16);

  // run the reference
  result.ref_result = ref_dot_accum(cfg, pe_idx, feature, filter);

  result.diff = fabs(result.model_result - result.ref_result);

  if (result.ref_result == 0.0) {
    result.diff_percent = result.diff == 0.0 ? 0.0 : 100.0;
  } else {
    result.diff_percent = fabs((result.diff/result.ref_result)*100.0);
  }

  return result;
}

static cfg_result_t run_tests_for_cfg(pe_array_cfg_t* cfg, int test_num_filter,
    std::vector<test_result_t>& test_results) {

  static int test_num = 1;

  pe_array_block_t feature[NUM_ACCUM_CYCLES] = {zero_pe_array_block};
  pe_array_block_t filter[NUM_ACCUM_CYCLES] = {zero_pe_array_block};

  cfg_result_t cfg_result = {0};
  cfg_result.cfg = *cfg;

  float total_diff = 0.0;
  float total_diff_percent = 0.0;

  bool all_passed = true;
  for (int i = 0; i < NUM_TESTS_PER_CONFIG; i++) {
    for (int accum_cycle = 0; accum_cycle < NUM_ACCUM_CYCLES; accum_cycle++) {
      for (int dot_value_idx = 0; dot_value_idx < cfg->dot_size; dot_value_idx++) {
        feature[accum_cycle].mantissa[dot_value_idx] = rand_int();
        filter[accum_cycle].mantissa[dot_value_idx] = rand_int();
      }
      feature[accum_cycle].exponent = rand_char();
      filter[accum_cycle].exponent = rand_char();
    }

    if (test_num_filter <= 0 || test_num_filter == test_num) {
      test_result_t test_result = run_test(cfg, test_num, feature, filter);
      test_results.push_back(test_result);

      total_diff += test_result.diff;
      total_diff_percent += test_result.diff_percent;
      cfg_result.max_diff = std::max(cfg_result.max_diff, test_result.diff);
      cfg_result.max_diff_percent = std::max(cfg_result.max_diff_percent, test_result.diff_percent);
    }

    test_num++;
  }

  cfg_result.avg_diff = test_num_filter > 0 ? total_diff : total_diff / NUM_TESTS_PER_CONFIG;
  cfg_result.avg_diff_percent = test_num_filter > 0 ? total_diff_percent : total_diff_percent / NUM_TESTS_PER_CONFIG;

  return cfg_result;
}

static bool compare_files(const std::string& filename1,
    const std::string& filename2) {
  std::ifstream file1(filename1.c_str(), std::ifstream::ate | std::ifstream::binary);
  std::ifstream file2(filename2.c_str(), std::ifstream::ate | std::ifstream::binary);

  if (!file1.is_open()) {
    std::cerr << "Error: could not open " << filename1 << std::endl;
    return false;
  }

  if (!file2.is_open()) {
    std::cerr << "Error: could not open " << filename2 << std::endl;
    return false;
  }

  // first check file size
  if (file1.tellg() != file2.tellg()) return false;

  // then compare contents
  file1.seekg(0);
  file2.seekg(0);
  std::istreambuf_iterator<char> begin1(file1);
  std::istreambuf_iterator<char> begin2(file2);
  return std::equal(begin1, std::istreambuf_iterator<char>(), begin2);
}

static void generate_output_files(std::vector<test_result_t>& test_results,
    std::vector<cfg_result_t>& cfg_results) {

  std::ofstream results_unsorted("accuracy_results.txt");
  results_unsorted.setf(std::ios_base::fixed);
  results_unsorted << "test num,feature width,filter width,dot size,reference,model,diff,diff percent\n";

  for (auto&& test_result : test_results) {
    results_unsorted << test_result.test_num
      << "," << test_result.cfg.feature_width
      << "," << test_result.cfg.filter_width
      << "," << test_result.cfg.dot_size
      << "," << test_result.ref_result
      << "," << test_result.model_result
      << "," << test_result.diff
      << "," << test_result.diff_percent << "%"
      << "\n";
  }

  std::sort(test_results.begin(), test_results.end(),
      [&] (const test_result_t& r1, const test_result_t& r2) {
        return r1.diff_percent < r2.diff_percent;
      });

  std::ofstream results_sorted("accuracy_results.sorted.txt");
  results_sorted.setf(std::ios_base::fixed);
  results_sorted << "test num,feature width,filter width,dot size,reference,model,diff,diff percent\n";

  for (auto&& test_result : test_results) {
    results_sorted << test_result.test_num
      << "," << test_result.cfg.feature_width
      << "," << test_result.cfg.filter_width
      << "," << test_result.cfg.dot_size
      << "," << test_result.ref_result
      << "," << test_result.model_result
      << "," << test_result.diff
      << "," << test_result.diff_percent << "%"
      << "\n";
  }

  std::ofstream results_summary("accuracy_results.summary.txt");
  results_summary.setf(std::ios_base::fixed);
  results_summary << "feature width,filter width,dot size,avg diff,avg diff percent,max diff,max diff percent\n";

  for (auto&& cfg_result : cfg_results) {
    results_summary << cfg_result.cfg.feature_width
      << "," << cfg_result.cfg.filter_width
      << "," << cfg_result.cfg.dot_size
      << "," << cfg_result.avg_diff
      << "," << cfg_result.avg_diff_percent << "%"
      << "," << cfg_result.max_diff
      << "," << cfg_result.max_diff_percent << "%"
      << "\n";
  }
}

int main(int argc, char** argv) {

  int test_num_filter = 0;
  if (argc >= 2) {
    char* strtol_end;
    int test_num = strtol(argv[1], &strtol_end, 10);
    if (isdigit(argv[1][0]) && *strtol_end == 0 && test_num > 0) {
      test_num_filter = test_num;
    }
  }

  // Re-initialize the random number generator with a constant seed so that we
  // always get the same random numbers for this test.
  srand(0);

  std::vector<test_result_t> test_results;
  std::vector<cfg_result_t> cfg_results;

  bool all_passed = true;

  for (int feature_width : {4, 5, 6, 7, 8, 12}) {
    for (int filter_width : {4, 5, 6, 7, 8, 12}) {
      for (int dot_size : {4, 8, 16, 32}) {
        for (int use_alm_accum : {true, false}) {

          pe_array_cfg_options_t cfg_options = zero_pe_array_cfg_options;
          cfg_options.enable_blockfp_mode   = true;
          cfg_options.enable_fixed_mode     = false;
          cfg_options.feature_width         = feature_width;
          cfg_options.filter_width          = filter_width;
          cfg_options.num_results_per_cycle = 1;
          cfg_options.num_chains            = 1;
          cfg_options.num_pes               = 1;
          cfg_options.num_dots              = 1;
          cfg_options.num_features          = 1;
          cfg_options.num_filters           = 1;
          cfg_options.ram_depth             = 512;
          cfg_options.dot_size              = dot_size;
          cfg_options.enable_ratios         = true;
          cfg_options.dot_ratio_alm         = 0;
          cfg_options.dot_ratio_dsp         = 1;
          cfg_options.accum_ratio_alm       = use_alm_accum ? 1 : 0;
          cfg_options.accum_ratio_dsp       = use_alm_accum ? 0 : 1;
          // For now, we don't need to set the device correctly here, because only the
          // Verilog RTL itself currently depends on it being set correctly.
          cfg_options.device_a10            = 0;
          cfg_options.device_s10            = 0;
          pe_array_cfg_t cfg = pe_array_cfg_init(cfg_options);

          cfg_result_t cfg_result = run_tests_for_cfg(&cfg, test_num_filter,
              test_results);
          cfg_results.push_back(cfg_result);
        }
      }
    }
  }

  generate_output_files(test_results, cfg_results);

  if (test_num_filter != 0) {
    std::cout << "Warning: by selecting a test filter, the results of this "
      "test cannot be compared to accuracy_results.reference.txt. Please "
      "inspect the test results in accuracy_results.txt yourself to check if "
      "the test passed. The return value of this test does not indicate pass "
      "or failure.\n";
    return 0;
  } else {
    bool results_match_reference = compare_files("accuracy_results.txt",
        "accuracy_results.reference.txt");
    return results_match_reference ? 0 : 1;
  }
}
