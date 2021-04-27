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

// -------------------------------------------------------------------------- //
// Most of the code from this file is copied from Sergey Gribok's A10 Fractal
// Synthesis reference design
// -------------------------------------------------------------------------- //

// Add two numbers in sign-magnitude format
// and generate the result in two's complement format:
// dout = {sign_a, din_a} + {sign_b, din_b}
module pe_a10_adder1 #(
	parameter SIZE = 4
)(
	input [SIZE-1:0] din_a,
	input sign_a,
	input [SIZE-1:0] din_b,
	input sign_b,
	output [SIZE+1:0] dout	
);

wire [SIZE+1:0] sum_w;
wire [SIZE+1:0] cout_w;
wire [SIZE+1:0] shareout_w;

//////////////// ALM 1
// f0 = (a^c)^(b^d)^(c^d) = a^b
// f2 = maj(a^c, b^d, c^d)
twentynm_lcell_comb #(
    .lut_mask(64'h0000_1BD8_0000_6666),
    .shared_arith("on"),
    .extended_lut("off")
) wc0 (
    .dataa (din_a[0]),
    .datab (din_b[0]),
    .datac (sign_a),
    .datad (sign_b),
    .datae (1'b0),
    .dataf (1'b0),
    .datag(1'b1),
    .cin(1'b0),
    .sharein(1'b0),
    .sumout(sum_w[0]),
    .cout(cout_w[0]),
    .shareout(shareout_w[0]),
    .combout()
);

// f0 = (a^c)^(b^d)^(c&d)
// f2 = maj(a^c, b^d, cd)
twentynm_lcell_comb #(
    .lut_mask(64'h0000_7248_0000_9996),
    .shared_arith("on"),
    .extended_lut("off")
) wc1 (
    .dataa (din_a[1]),
    .datab (din_b[1]),
    .datac (sign_a),
    .datad (sign_b),
    .datae (1'b0),
    .dataf (1'b0),
    .datag(1'b1),
    .cin(cout_w[0]),
    .sharein(shareout_w[0]),
    .sumout(sum_w[1]),
    .cout(cout_w[1]),
    .shareout(shareout_w[1]),
    .combout()
);

genvar i;
generate
for (i=2; i<SIZE; i=i+1)
begin: gen
//////////////// ALM 2
// f0 = (a^c)^(b^d)
// f2 = (a^c)&(b^d)
twentynm_lcell_comb #(
    .lut_mask(64'h0000_1248_0000_6996),
    .shared_arith("on"),
    .extended_lut("off")
) wc (
    .dataa (din_a[i]),
    .datab (din_b[i]),
    .datac (sign_a),
    .datad (sign_b),
    .datae (1'b0),
    .dataf (1'b0),
    .datag(1'b1),
    .cin(cout_w[i-1]),
    .sharein(shareout_w[i-1]),
    .sumout(sum_w[i]),
    .cout(cout_w[i]),
    .shareout(shareout_w[i]),
    .combout()
);
end
endgenerate

// f0 = c^d
// f2 = cd
twentynm_lcell_comb #(
    .lut_mask(64'h0000_F000_0000_0FF0),
    .shared_arith("on"),
    .extended_lut("off")
) wc_n (
    .dataa (1'b0),
    .datab (1'b0),
    .datac (sign_a),
    .datad (sign_b),
    .datae (1'b0),
    .dataf (1'b0),
    .datag(1'b1),
    .cin(cout_w[SIZE-1]),
    .sharein(shareout_w[SIZE-1]),
    .sumout(sum_w[SIZE]),
    .cout(cout_w[SIZE]),
    .shareout(shareout_w[SIZE]),
    .combout()
);

// f0 = c^d
// f2 = cd
twentynm_lcell_comb #(
    .lut_mask(64'h0000_F000_0000_0FF0),
    .shared_arith("on"),
    .extended_lut("off")
) wc_n_plus_1 (
    .dataa (1'b0),
    .datab (1'b0),
    .datac (sign_a),
    .datad (sign_b),
    .datae (1'b0),
    .dataf (1'b0),
    .datag(1'b1),
    .cin(cout_w[SIZE]),
    .sharein(shareout_w[SIZE]),
    .sumout(sum_w[SIZE+1]),
    .cout(cout_w[SIZE+1]),
    .shareout(shareout_w[SIZE+1]),
    .combout()
);

assign dout = sum_w;

endmodule

