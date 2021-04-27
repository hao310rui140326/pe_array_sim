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
// Most of the code from this file is copied from Sergey Gribok's S10 Fractal
// Synthesis reference design
// -------------------------------------------------------------------------- //

module adder_tree #(
	parameter SIZE = 5,
	parameter NUM = 40
)(
   input clk,
   input signed [SIZE-1:0] din[0:NUM-1],
   output reg signed [$clog2(NUM)+SIZE-1:0] dout
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
	else
	begin
		localparam HALF_NUM = (NUM + 1) / 2;
		reg signed [SIZE:0] first_level[0:HALF_NUM-1];
		for (i = 0; i < NUM/2; i = i+1)
		begin: level
			always @(posedge clk)
			begin
				first_level[i] <= din[2*i] + din[2*i+1];
			end
		end
		if (NUM & 1 == 1)
		begin
			always @(posedge clk)
			begin
				first_level[HALF_NUM-1] <= din[NUM-1];
			end
		end
		adder_tree #(SIZE+1, HALF_NUM) inst(.clk(clk), .din(first_level), .dout(dout));	
	end
	endgenerate
endmodule


module adder2 #(
	parameter SIZE = 5
)(
	input signed [SIZE-1:0] a,
	input signed [SIZE-1:0] b,
	input s0,
	input s1,
	output signed [SIZE:0] out
);
	wire [SIZE+1:0] left;
	wire [SIZE+1:0] right;
	wire temp;
	
	assign left = {a[SIZE-1] ^ b[SIZE-1], a ^ b, s0};
	assign right = {a & b, s1, s0};
	assign {out, temp} = left + right;
	
endmodule


module adder1 #(
	parameter SIZE = 5
)(
	input [SIZE-1:0] a,
	input [SIZE-1:0] b,
	input sa,
	input sb,
	output signed [SIZE+1:0] out
);
	wire [SIZE+2:0] left;
	wire [SIZE+2:0] right;
	wire temp;
	
	assign left = {sa, sa, a ^ {SIZE{sa}}, sa};
	assign right = {sb, sb, b ^ {SIZE{sb}}, sa};
	assign {out, temp} = left + right;
	
endmodule


module dot_product #(
	parameter SIZEA = 5,
	parameter SIZEB = 5,
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

	wire signed [SIZEA+SIZEB-1:0] sum1[0:DOT/2-1];
	reg signed [SIZEA+SIZEB-1:0] r_sum1[0:DOT/2-1];
	reg r_sign[0:DOT/2-1];

	for (i = 0; i < DOT/2; i = i+1)
	begin: a1
		adder1 #(SIZEA+SIZEB-2) adder1_inst(.a(product[2*i]), .b(product[2*i+1]), .sa(sign[2*i]), .sb(sign[2*i+1]), .out(sum1[i]));
		always @(posedge clk)
		begin
			r_sum1[i] <= sum1[i];
			r_sign[i] <= sign[2*i+1];
		end
	end

	wire signed [SIZEA+SIZEB:0] sum2[0:DOT/4-1];
	reg signed [SIZEA+SIZEB:0] r_sum2[0:DOT/4-1];
	
	for (i = 0; i < DOT/4; i = i+1)
	begin: a2
		adder2 #(SIZEA+SIZEB) adder2_inst(.a(r_sum1[2*i]), .b(r_sum1[2*i+1]), .s0(r_sign[2*i]), .s1(r_sign[2*i+1]), .out(sum2[i]));
		always @(posedge clk)
		begin
			r_sum2[i] <= sum2[i];
		end
	end
	
	if (DOT == 4)
		assign dout = r_sum2[0];
	else
		adder_tree #(SIZEA+SIZEB+1, DOT/4) inst(.clk(clk), .din(r_sum2), .dout(dout));	
	
	endgenerate	
endmodule


// Top level module.
// Instantiates NUM instances of dot_product module.
// DOT must be 1 or a multiple of 4.
module top #(
    parameter SIZEA = 4,
    parameter SIZEB = 4,
    parameter PIPELINE = 0,
    parameter DOT = 16,
    parameter NUM = 4,
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
