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

`define pe_cfg_init '{ \
  ENABLE_FP32_MODE: ENABLE_FP32_MODE, \
  ENABLE_FIXED_MODE: ENABLE_FIXED_MODE, \
  ENABLE_BLOCKFP_MODE: ENABLE_BLOCKFP_MODE, \
\
  ENABLE_1X_DSP_PACKING: ENABLE_1X_DSP_PACKING, \
  ENABLE_2X_DSP_PACKING: ENABLE_2X_DSP_PACKING, \
  ENABLE_4X_DSP_PACKING: ENABLE_4X_DSP_PACKING, \
\
  FEATURE_WIDTH: FEATURE_WIDTH, \
  FILTER_WIDTH: FILTER_WIDTH, \
  EXPONENT_WIDTH: EXPONENT_WIDTH, \
  EXPONENT_BIAS: EXPONENT_BIAS, \
\
  RESULT_MANTISSA_WIDTH: RESULT_MANTISSA_WIDTH, \
  RESULT_EXPONENT_WIDTH: RESULT_EXPONENT_WIDTH, \
  RESULT_EXPONENT_BIAS: RESULT_EXPONENT_BIAS, \
  RESULT_WIDTH: RESULT_WIDTH, \
  NUM_RESULTS_PER_CYCLE: NUM_RESULTS_PER_CYCLE, \
\
  NUM_CHAINS: NUM_CHAINS, \
  NUM_PES: NUM_PES, \
  NUM_DOTS: NUM_DOTS, \
  NUM_FEATURES: NUM_FEATURES, \
  NUM_FILTERS: NUM_FILTERS, \
\
  PE_ID_WIDTH: PE_ID_WIDTH, \
  FILTER_ID_WIDTH: FILTER_ID_WIDTH, \
\
  RAM_LATENCY: RAM_LATENCY, \
  RAM_ADDR_WIDTH: RAM_ADDR_WIDTH, \
  RAM_DEPTH: RAM_DEPTH, \
  RAM_WIDTH: RAM_WIDTH, \
\
  MULT_OUTPUT_WIDTH: MULT_OUTPUT_WIDTH, \
\
  DOT_SIZE: DOT_SIZE, \
  DOT_OUTPUT_WIDTH: DOT_OUTPUT_WIDTH, \
  DOT_LATENCY_DSP_MULT: DOT_LATENCY_DSP_MULT, \
  DOT_LATENCY_ALM_MULT: DOT_LATENCY_ALM_MULT, \
  DOT_LATENCY_ADDER_TREE: DOT_LATENCY_ADDER_TREE, \
  DOT_LATENCY: DOT_LATENCY, \
\
  ENABLE_FRACTAL_MULT: ENABLE_FRACTAL_MULT, \
  ENABLE_PRECOMPILED_DOTS: ENABLE_PRECOMPILED_DOTS, \
\
  ACCUM_LATENCY: ACCUM_LATENCY, \
  ALM_ACCUM_WIDTH: ALM_ACCUM_WIDTH, \
  ALM_ACCUM_FRACTION_WIDTH: ALM_ACCUM_FRACTION_WIDTH, \
\
  FIXED_ACCUM_WIDTH: FIXED_ACCUM_WIDTH, \
\
  DOT_RATIO_DSP: DOT_RATIO_DSP, \
  DOT_RATIO_ALM: DOT_RATIO_ALM, \
  ACCUM_RATIO_DSP: ACCUM_RATIO_DSP, \
  ACCUM_RATIO_ALM: ACCUM_RATIO_ALM, \
\
  TOTAL_LATENCY: TOTAL_LATENCY, \
\
  DEVICE_A10: DEVICE_A10, \
  DEVICE_S10: DEVICE_S10, \
\
  DRAIN_LATENCY: DRAIN_LATENCY, \
  NUM_ACCUM: NUM_ACCUM, \
  NUM_FLUSH_CYCLES: NUM_FLUSH_CYCLES, \
\
  IDATA_WIDTH: IDATA_WIDTH, \
  ODATA_WIDTH: ODATA_WIDTH \
}


`define pe_cfg_top \
  int ENABLE_FP32_MODE, \
  int ENABLE_FIXED_MODE, \
  int ENABLE_BLOCKFP_MODE, \
\
  int ENABLE_1X_DSP_PACKING, \
  int ENABLE_2X_DSP_PACKING, \
  int ENABLE_4X_DSP_PACKING, \
\
  int FEATURE_WIDTH, \
  int FILTER_WIDTH, \
  int EXPONENT_WIDTH, \
  int EXPONENT_BIAS, \
\
  int RESULT_MANTISSA_WIDTH, \
  int RESULT_EXPONENT_WIDTH, \
  int RESULT_EXPONENT_BIAS, \
  int RESULT_WIDTH, \
  int NUM_RESULTS_PER_CYCLE, \
\
  int NUM_CHAINS, \
  int NUM_PES, \
  int NUM_DOTS, \
  int NUM_FEATURES, \
  int NUM_FILTERS, \
\
  int PE_ID_WIDTH, \
  int FILTER_ID_WIDTH, \
\
  int RAM_LATENCY, \
  int RAM_ADDR_WIDTH, \
  int RAM_DEPTH, \
  int RAM_WIDTH, \
\
  int MULT_OUTPUT_WIDTH, \
\
  int DOT_SIZE, \
  int DOT_OUTPUT_WIDTH, \
  int DOT_LATENCY_DSP_MULT, \
  int DOT_LATENCY_ALM_MULT, \
  int DOT_LATENCY_ADDER_TREE, \
  int DOT_LATENCY, \
\
  int ENABLE_FRACTAL_MULT, \
  int ENABLE_PRECOMPILED_DOTS, \
\
  int ACCUM_LATENCY, \
  int ALM_ACCUM_WIDTH, \
  int ALM_ACCUM_FRACTION_WIDTH, \
\
  int FIXED_ACCUM_WIDTH, \
\
  int DOT_RATIO_DSP, \
  int DOT_RATIO_ALM, \
  int ACCUM_RATIO_DSP, \
  int ACCUM_RATIO_ALM, \
\
  int TOTAL_LATENCY, \
\
  int DEVICE_A10, \
  int DEVICE_S10, \
\
  int DRAIN_LATENCY, \
  int NUM_ACCUM, \
  int NUM_FLUSH_CYCLES, \
\
  int IDATA_WIDTH, \
  int ODATA_WIDTH, \
\
  pe_cfg_t cfg = `pe_cfg_init


`define pe_cfg_fwd \
  .ENABLE_FP32_MODE(cfg.ENABLE_FP32_MODE), \
  .ENABLE_FIXED_MODE(cfg.ENABLE_FIXED_MODE), \
  .ENABLE_BLOCKFP_MODE(cfg.ENABLE_BLOCKFP_MODE), \
\
  .ENABLE_1X_DSP_PACKING(cfg.ENABLE_1X_DSP_PACKING), \
  .ENABLE_2X_DSP_PACKING(cfg.ENABLE_2X_DSP_PACKING), \
  .ENABLE_4X_DSP_PACKING(cfg.ENABLE_4X_DSP_PACKING), \
\
  .FEATURE_WIDTH(cfg.FEATURE_WIDTH), \
  .FILTER_WIDTH(cfg.FILTER_WIDTH), \
  .EXPONENT_WIDTH(cfg.EXPONENT_WIDTH), \
  .EXPONENT_BIAS(cfg.EXPONENT_BIAS), \
\
  .RESULT_MANTISSA_WIDTH(cfg.RESULT_MANTISSA_WIDTH), \
  .RESULT_EXPONENT_WIDTH(cfg.RESULT_EXPONENT_WIDTH), \
  .RESULT_EXPONENT_BIAS(cfg.RESULT_EXPONENT_BIAS), \
  .RESULT_WIDTH(cfg.RESULT_WIDTH), \
  .NUM_RESULTS_PER_CYCLE(cfg.NUM_RESULTS_PER_CYCLE), \
\
  .NUM_CHAINS(cfg.NUM_CHAINS), \
  .NUM_PES(cfg.NUM_PES), \
  .NUM_DOTS(cfg.NUM_DOTS), \
  .NUM_FEATURES(cfg.NUM_FEATURES), \
  .NUM_FILTERS(cfg.NUM_FILTERS), \
\
  .PE_ID_WIDTH(cfg.PE_ID_WIDTH), \
  .FILTER_ID_WIDTH(cfg.FILTER_ID_WIDTH), \
\
  .RAM_LATENCY(cfg.RAM_LATENCY), \
  .RAM_ADDR_WIDTH(cfg.RAM_ADDR_WIDTH), \
  .RAM_DEPTH(cfg.RAM_DEPTH), \
  .RAM_WIDTH(cfg.RAM_WIDTH), \
\
  .MULT_OUTPUT_WIDTH(cfg.MULT_OUTPUT_WIDTH), \
\
  .DOT_SIZE(cfg.DOT_SIZE), \
  .DOT_OUTPUT_WIDTH(cfg.DOT_OUTPUT_WIDTH), \
  .DOT_LATENCY_DSP_MULT(cfg.DOT_LATENCY_DSP_MULT), \
  .DOT_LATENCY_ALM_MULT(cfg.DOT_LATENCY_ALM_MULT), \
  .DOT_LATENCY_ADDER_TREE(cfg.DOT_LATENCY_ADDER_TREE), \
  .DOT_LATENCY(cfg.DOT_LATENCY), \
\
  .ENABLE_FRACTAL_MULT(cfg.ENABLE_FRACTAL_MULT), \
  .ENABLE_PRECOMPILED_DOTS(cfg.ENABLE_PRECOMPILED_DOTS), \
\
  .ACCUM_LATENCY(cfg.ACCUM_LATENCY), \
  .ALM_ACCUM_WIDTH(cfg.ALM_ACCUM_WIDTH), \
  .ALM_ACCUM_FRACTION_WIDTH(cfg.ALM_ACCUM_FRACTION_WIDTH), \
\
  .FIXED_ACCUM_WIDTH(cfg.FIXED_ACCUM_WIDTH), \
\
  .DOT_RATIO_DSP(cfg.DOT_RATIO_DSP), \
  .DOT_RATIO_ALM(cfg.DOT_RATIO_ALM), \
  .ACCUM_RATIO_DSP(cfg.ACCUM_RATIO_DSP), \
  .ACCUM_RATIO_ALM(cfg.ACCUM_RATIO_ALM), \
\
  .TOTAL_LATENCY(cfg.TOTAL_LATENCY), \
\
  .DEVICE_A10(cfg.DEVICE_A10), \
  .DEVICE_S10(cfg.DEVICE_S10), \
\
  .DRAIN_LATENCY(cfg.DRAIN_LATENCY), \
  .NUM_ACCUM(cfg.NUM_ACCUM), \
  .NUM_FLUSH_CYCLES(cfg.NUM_FLUSH_CYCLES), \
\
  .IDATA_WIDTH(cfg.IDATA_WIDTH), \
  .ODATA_WIDTH(cfg.ODATA_WIDTH)
