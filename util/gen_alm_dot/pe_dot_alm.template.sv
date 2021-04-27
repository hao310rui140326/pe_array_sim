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

module pe_dot_alm import pe_types::*; #(
  pe_cfg_t cfg
) (
  input                                   clock,

  input block_feature_mantissa_t#(cfg)::t i_feature,
  input block_filter_mantissa_t#(cfg)::t  i_filter,

  output signed [cfg.DOT_OUTPUT_WIDTH-1:0] o_result
);

logic [cfg.DOT_OUTPUT_WIDTH-1:0] result_pre;

localparam ALM_DOT_LATENCY = cfg.DOT_LATENCY_ALM_MULT + cfg.DOT_LATENCY_ADDER_TREE;

delay #(
  .WIDTH(cfg.DOT_OUTPUT_WIDTH),
  .DELAY(cfg.DOT_LATENCY - ALM_DOT_LATENCY)
) result_reg (
  .clock(clock),
  .i_data(result_pre),
  .o_data(o_result)
);

if (!cfg.ENABLE_PRECOMPILED_DOTS) begin : GEN_DOT

  logic [cfg.MULT_OUTPUT_WIDTH-1:0] mult_output [cfg.DOT_SIZE];

  for (genvar dot_value_idx = 0; dot_value_idx < cfg.DOT_SIZE; dot_value_idx++) begin : GEN_MULT
    logic [cfg.MULT_OUTPUT_WIDTH-2:0] mult_result;
    logic                             sign_result;

    if (cfg.ENABLE_FRACTAL_MULT) begin : GEN_FRACTAL_MULT
      always_ff @(posedge clock) begin
        mult_result <= i_feature[dot_value_idx][cfg.FEATURE_WIDTH-2:0] *
          i_filter [dot_value_idx][cfg.FILTER_WIDTH-2:0];
      end
    end
    else begin : GEN_LPM_MULT
      lpm_mult #(
        .lpm_hint("DEDICATED_MULTIPLIER_CIRCUITRY=NO,MAXIMIZE_SPEED=5"),
        .lpm_pipeline(1),
        .lpm_representation("UNSIGNED"),
        .lpm_type("LPM_MULT"),
        .lpm_widtha(cfg.FEATURE_WIDTH-1),
        .lpm_widthb(cfg.FILTER_WIDTH-1),
        .lpm_widthp(cfg.MULT_OUTPUT_WIDTH-1)
      ) lpm_mult (
        .clock (clock),
        .clken (1'b1),
        .aclr  (1'b0),
        .sclr  (1'b0),
        .dataa (i_feature[dot_value_idx][cfg.FEATURE_WIDTH-2:0]),
        .datab (i_filter [dot_value_idx][cfg.FILTER_WIDTH-2:0]),
        .result(mult_result),
        .sum   (1'b0)
      );
    end

    always_ff @(posedge clock) begin
      sign_result <= i_feature[dot_value_idx][cfg.FEATURE_WIDTH-1] ^
        i_filter [dot_value_idx][cfg.FILTER_WIDTH-1];

      mult_output[dot_value_idx] <= {sign_result, mult_result};
    end
  end

  pe_dot_adder_tree #(
    .cfg(cfg)
  ) adder_tree (
    .clock(clock),
    .i_mult_output(mult_output),
    .o_result(result_pre)
  );

end
__INSTANTIATE_DOTS_HERE__

endmodule
