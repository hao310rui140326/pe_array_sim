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

package pe_types;

  localparam FP32_MANTISSA_WIDTH = 23;
  localparam FP32_EXPONENT_WIDTH = 8;
  localparam FP32_EXPONENT_BIAS  = 127;

  typedef struct packed {
    int ENABLE_FP32_MODE;
    int ENABLE_FIXED_MODE;
    int ENABLE_BLOCKFP_MODE;

    int ENABLE_1X_DSP_PACKING;
    int ENABLE_2X_DSP_PACKING;
    int ENABLE_4X_DSP_PACKING;

    int FEATURE_WIDTH;
    int FILTER_WIDTH;
    int EXPONENT_WIDTH;
    int EXPONENT_BIAS;

    int RESULT_MANTISSA_WIDTH;
    int RESULT_EXPONENT_WIDTH;
    int RESULT_EXPONENT_BIAS;
    int RESULT_WIDTH;
    int NUM_RESULTS_PER_CYCLE;

    int NUM_CHAINS;
    int NUM_PES;
    int NUM_DOTS;
    int NUM_FEATURES;
    int NUM_FILTERS;

    int PE_ID_WIDTH;
    int FILTER_ID_WIDTH;

    int RAM_LATENCY;
    int RAM_ADDR_WIDTH;
    int RAM_DEPTH;
    int RAM_WIDTH;

    int MULT_OUTPUT_WIDTH;

    int DOT_SIZE;
    int DOT_OUTPUT_WIDTH;
    int DOT_LATENCY_DSP_MULT;
    int DOT_LATENCY_ALM_MULT;
    int DOT_LATENCY_ADDER_TREE;
    int DOT_LATENCY;

    int ENABLE_FRACTAL_MULT;
    int ENABLE_PRECOMPILED_DOTS;

    int ACCUM_LATENCY;
    int ALM_ACCUM_WIDTH;
    int ALM_ACCUM_FRACTION_WIDTH;

    int FIXED_ACCUM_WIDTH;

    int DOT_RATIO_DSP;
    int DOT_RATIO_ALM;
    int ACCUM_RATIO_DSP;
    int ACCUM_RATIO_ALM;

    int TOTAL_LATENCY;

    int DEVICE_A10;
    int DEVICE_S10;

    int DRAIN_LATENCY;
    int NUM_ACCUM;
    int NUM_FLUSH_CYCLES;

    int IDATA_WIDTH;
    int ODATA_WIDTH;
  } pe_cfg_t;

  virtual class sign_mag_t #(int WIDTH);
    typedef struct packed {
      logic             sign;
      logic [WIDTH-2:0] magnitude;
    } t;
  endclass

  virtual class block_feature_mantissa_t #(pe_cfg_t cfg);
    typedef logic [cfg.DOT_SIZE-1:0][cfg.FEATURE_WIDTH-1:0] t;
  endclass

  virtual class block_filter_mantissa_t #(pe_cfg_t cfg);
    typedef logic [cfg.DOT_SIZE-1:0][cfg.FILTER_WIDTH-1:0] t;
  endclass

  virtual class exponent_t #(pe_cfg_t cfg);
    typedef logic [cfg.EXPONENT_WIDTH-1:0] t;
  endclass

  virtual class block_feature_t #(pe_cfg_t cfg);
    typedef struct packed {
      block_feature_mantissa_t#(cfg)::t mantissa;
      exponent_t#(cfg)::t               exponent;
    } t;
  endclass

  virtual class block_filter_t #(pe_cfg_t cfg);
    typedef struct packed {
      block_filter_mantissa_t#(cfg)::t mantissa;
      exponent_t#(cfg)::t              exponent;
    } t;
  endclass

  virtual class feature_bus_t #(pe_cfg_t cfg);
    typedef block_feature_t#(cfg)::t [cfg.NUM_DOTS-1:0][cfg.NUM_FEATURES-1:0] t;
  endclass

  virtual class filter_cache_read_data_t #(pe_cfg_t cfg);
    typedef block_filter_t#(cfg)::t [cfg.NUM_DOTS-1:0][cfg.NUM_FILTERS-1:0] t;
  endclass

  virtual class filter_cache_write_data_t #(pe_cfg_t cfg);
    typedef block_filter_t#(cfg)::t [cfg.NUM_DOTS-1:0] t;
  endclass

  virtual class ram_addr_t #(pe_cfg_t cfg);
    typedef logic [cfg.RAM_ADDR_WIDTH-1:0] t;
  endclass

  virtual class ram_write_request_t #(pe_cfg_t cfg);
    typedef struct packed {
      logic                              enable;
      logic [cfg.PE_ID_WIDTH-1:0]        pe_id;
      logic [cfg.FILTER_ID_WIDTH-1:0]    filter_id;
      ram_addr_t#(cfg)::t                addr;
      filter_cache_write_data_t#(cfg)::t data;
    } t;
  endclass

  virtual class pe_bus_request_t #(pe_cfg_t cfg);
    typedef struct packed {
      logic                        feature_valid;
      feature_bus_t#(cfg)::t       feature_data;
      ram_addr_t#(cfg)::t          filter_read_addr;
      logic                        flush_accumulator;
      ram_write_request_t#(cfg)::t ram_write_request;

      logic                        result_valid;
      logic                        result_token;
      logic [cfg.NUM_RESULTS_PER_CYCLE-1:0][cfg.RESULT_WIDTH-1:0] result;
    } t;
  endclass

  virtual class pe_bus_response_t #(pe_cfg_t cfg);
    typedef struct packed {
      logic [cfg.NUM_RESULTS_PER_CYCLE-1:0][cfg.RESULT_WIDTH-1:0] result;
    } t;
  endclass

  virtual class pe_array_input_t #(pe_cfg_t cfg);
    typedef struct packed {
      struct packed {
        struct packed {
          logic [7:0] exponent;
          logic [cfg.DOT_SIZE-1:0][31:0] mantissa;
        } [cfg.NUM_DOTS-1:0][cfg.NUM_FEATURES-1:0] feature_data;

        logic  [7:0] feature_valid;

        logic [31:0] filter_read_addr;

        // filter cache write interface
        struct packed {
          struct packed {
            logic [7:0] exponent;
            logic [cfg.DOT_SIZE-1:0][31:0] mantissa;
          } [cfg.NUM_DOTS-1:0] data;

          logic [31:0] addr;
          logic [31:0] filter_id;
          logic [31:0] pe_id;
          logic  [7:0] enable;
        } ram_write;
      } [cfg.NUM_CHAINS-1:0] chain;

      logic [7:0] flush_accumulator;
      logic [7:0] send_output;
    } t;
  endclass

  virtual class pe_array_output_t #(pe_cfg_t cfg);
    typedef struct packed {
      logic [cfg.NUM_CHAINS-1:0][cfg.NUM_RESULTS_PER_CYCLE-1:0][31:0] result;
    } t;
  endclass

  // This function calculates the number of bits required to represent
  // a value. It assumes the input is signed, and so it will include the bit
  // required to represent the sign when it calculates the number of bits.
  function int num_bits(input int value);
    num_bits = 1;
    for (int i = 0; i < $bits(value); i++) begin
      if ((value > 0 && value[i]) || (value < 0 && !value[i])) begin
        num_bits = (i+2);
      end
    end
  endfunction

  function int max(input int a, input int b);
    max = (a > b) ? a : b;
  endfunction

endpackage

