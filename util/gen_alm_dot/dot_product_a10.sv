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

// Pipelined ternary adder tree
module adder_tree #(
	parameter SIZE = 5,
	parameter OUT_SIZE = 7,
	parameter NUM = 2
)(
   input clk,
   input signed [SIZE-1:0] din[0:NUM-1],
   output reg signed [OUT_SIZE-1:0] dout
);

	genvar i;
	generate
	if (NUM == 2)
	begin
		always @(posedge clk)
		begin
			dout <= din[0] + din[1];
		end
	end
	else if (NUM == 3)
	begin
		always @(posedge clk)
		begin
			dout <= din[0] + din[1] + din[2];
		end
	end
	else
	begin
		localparam THIRD_NUM = (NUM + 2) / 3;
		reg signed [SIZE+1:0] first_level[0:THIRD_NUM-1];
		for (i = 0; i < NUM/3; i = i+1)
		begin: level
			always @(posedge clk)
			begin
				first_level[i] <= din[3*i] + din[3*i+1] + din[3*i+2];
			end
		end
		if (NUM % 3 == 2)
		begin
			always @(posedge clk)
			begin
				first_level[THIRD_NUM-1] <= din[NUM-1] + din[NUM-2];
			end
		end
		else if (NUM % 3 == 1)
		begin
			always @(posedge clk)
			begin
				first_level[THIRD_NUM-1] <= din[NUM-1];
			end
		end
		adder_tree #(SIZE+2, OUT_SIZE, THIRD_NUM) inst(.clk(clk), .din(first_level), .dout(dout));	
	end
	endgenerate
endmodule

// Add two numbers in sign-magnitude format
// and generate the result in two's complement format:
// dout = {sign_a, din_a} + {sign_b, din_b}
module adder1 #(
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

// Calculate DOT products of SIGNA-bit by SIGNB-bit signed numbers
// in sign-magnitude format and add them all together.
// The result is in two's complement format
// DOT must be 1, 2, or a multiple of 4
module dot_product #(
	parameter SIZEA = 6,
	parameter SIZEB = 6,
	parameter DOT = 16,
	parameter PIPELINE = 1
)(
	input clk,
	input [SIZEA*DOT-1:0] din_a,
	input [SIZEB*DOT-1:0] din_b,
	output signed [$clog2(DOT)+SIZEA+SIZEB-2:0] dout
);

	wire [SIZEA-2:0] u_din_a[0:DOT-1];
	wire [SIZEB-2:0] u_din_b[0:DOT-1];
	wire sign_a[0:DOT-1];
	wire sign_b[0:DOT-1];

	reg [SIZEA+SIZEB-3:0] product[0:DOT-1];
	reg [SIZEA+SIZEB-3:0] product1[0:DOT-1];

	reg sign[0:DOT-1];
	reg sign1[0:DOT-1];
			
	genvar i;
	generate
	for (i = 0; i < DOT; i = i+1)
	begin: a0	
		assign {sign_a[i], u_din_a[i]} = din_a[SIZEA*(i+1)-1:SIZEA*i];
		assign {sign_b[i], u_din_b[i]} = din_b[SIZEB*(i+1)-1:SIZEB*i];
		always @(posedge clk)
		begin
			if (PIPELINE)
			begin
				sign1[i] <= sign_a[i] ^ sign_b[i];
				sign[i] <= sign1[i];
				product1[i] <= u_din_a[i] * u_din_b[i];
				product[i] <= product1[i];
			end
			else
			begin
				sign[i] <= sign_a[i] ^ sign_b[i];
				product[i] <= u_din_a[i] * u_din_b[i];
			end
		end		
	end

	if (DOT == 1)
		assign dout = {sign[0], product[0]};
	else
	begin		
		wire signed [SIZEA+SIZEB-1:0] sum1[0:DOT/2-1];
		reg signed [SIZEA+SIZEB-1:0] r_sum1[0:DOT/2-1];
		reg r_sign[0:DOT/2-1];

		for (i = 0; i < DOT/2; i = i+1)
		begin: a1
			adder1 #(SIZEA+SIZEB-2) adder1_inst(.din_a(product[2*i]), .din_b(product[2*i+1]), .sign_a(sign[2*i]), .sign_b(sign[2*i+1]), .dout(sum1[i]));
			
			always @(posedge clk)
			begin
				r_sum1[i] <= sum1[i];
			end
		end

		if (DOT == 2)
			assign dout = r_sum1[0];
		else
			adder_tree #(SIZEA+SIZEB, $clog2(DOT/2)+SIZEA+SIZEB, DOT/2) inst(.clk(clk), .din(r_sum1), .dout(dout));	
	end
	endgenerate

	
endmodule

// Top level module.
// Instantiates NUM instances of dot_product module.
// DOT must be 1 or a multiple of 4.
module top #(
	parameter SIZEA = 6,
	parameter SIZEB = 6,
	parameter PIPELINE = 1,
	parameter DOT = 16,
	parameter NUM = 1,
	parameter SIZEO = $clog2(DOT)+SIZEA+SIZEB-1
)(
   input clk,
   input [SIZEA*DOT*NUM-1:0] din_a,
   input [SIZEB*DOT*NUM-1:0] din_b,
   output signed [SIZEO*NUM-1:0] dout
);

	genvar i;
	generate
	for (i = 0; i < NUM; i = i+1)
	begin: d
		dot_product #(SIZEA, SIZEB, DOT, PIPELINE) inst(
			.clk(clk), 
			.din_a(din_a[SIZEA*DOT*(i+1)-1:SIZEA*DOT*i]), 
			.din_b(din_b[SIZEB*DOT*(i+1)-1:SIZEB*DOT*i]), 
			.dout(dout[SIZEO*(i+1)-1:SIZEO*i])
		);
	end
	endgenerate

	
endmodule
