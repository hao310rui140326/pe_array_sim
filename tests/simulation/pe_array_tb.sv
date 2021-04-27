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

`timescale 1 ns / 1 ps

`include "pe_inc.sv"

module pe_array_tb import pe_types::*; #(
  // testbench parameters are overridden with "-g" options to vsim
  int NUM_TEST_CYCLES,

  `pe_cfg_top
);

logic                      clock = 0;
logic                      resetn = 0;

logic                      ivalid;
logic                      ovalid;

pe_array_input_t#(cfg)::t  idata;
pe_array_output_t#(cfg)::t odata;

always #10 clock = ~clock;

pe_array #(
  `pe_cfg_fwd
  //.cfg(cfg)
) pe_array_inst (
  .clock (clock),
  .resetn(resetn),

  .ivalid(ivalid),
  .ovalid(ovalid),

  .iready(),
  .oready(),

  .idata(idata),
  .odata(odata)
);

int test_data_fd = 0;

typedef struct {
  logic                      ivalid;
  pe_array_input_t#(cfg)::t  idata;
  pe_array_output_t#(cfg)::t odata_ref;
} test_vector_t;

function test_vector_t read_test_vector;
  automatic test_vector_t ret;
  automatic int values_found;
  automatic byte ivalid_char;

  values_found = $fscanf(test_data_fd, "%c %x %x\n", ivalid_char,
    ret.idata, ret.odata_ref);

  if (values_found != 3) begin
    $display("Error: could not read test data from file");
    $finish;
  end

  ret.ivalid = (ivalid_char == "v");

  return ret;
endfunction


logic do_init = 1'b1;
int   init_count = 0;

logic run_tests = 1'b0;
logic test_failed = 1'b0;

pe_array_output_t#(cfg)::t odata_ref = 1'b0;

typedef struct packed {
  pe_array_input_t#(cfg)::t  idata;
  logic                      ivalid;
  pe_array_output_t#(cfg)::t odata_ref;
  int                        test_count;
  logic                      test_active;
} test_info_t;

test_info_t test_info_reg [TOTAL_LATENCY+1];

int test_count = 0;
int passed_tests = 0;

//always_ff @(posedge clock) begin
always @(posedge clock) begin
  if (~resetn) begin
    if (test_data_fd != 0) begin
      $fclose(test_data_fd);
      test_data_fd = 0;
    end

    // TODO: [shaneoco] use array initializer somehow?
    for (int i = 0; i < $size(test_info_reg); i++) begin
      test_info_reg[i] <= 1'b0;
    end

  end
  else begin
    if (test_data_fd == 0) begin
      test_data_fd = $fopen("pe_array_test_data.txt", "r");
    end

    if (run_tests) begin

      if (do_init) begin
        idata  <= 1'b0;
        ivalid <= 1'b1;

        if (init_count == 0) begin
          idata.flush_accumulator <= 1'b1;
        end

        init_count <= init_count + 1;

        if (init_count >= cfg.NUM_FLUSH_CYCLES) begin
          do_init <= 1'b0;
        end
      end
      else begin

        automatic test_info_t output_test_info;

        if (test_count < NUM_TEST_CYCLES) begin
          automatic test_vector_t test_vector = read_test_vector();

          idata  <= test_vector.idata;
          ivalid <= test_vector.ivalid;

          test_info_reg[0].idata       <= test_vector.idata;
          test_info_reg[0].ivalid      <= test_vector.ivalid;
          test_info_reg[0].odata_ref   <= test_vector.odata_ref;
        end
        else begin
          idata  <= 1'b0;
          ivalid <= 1'b0;

          test_info_reg[0].idata       <= 1'b0;
          test_info_reg[0].ivalid      <= 1'b0;
          test_info_reg[0].odata_ref   <= 1'b0;
        end

        test_info_reg[0].test_count  <= test_count;
        test_info_reg[0].test_active <= 1'b1;

        for (int i = 1; i < $size(test_info_reg); i++) begin
          test_info_reg[i] <= test_info_reg[i-1];
        end

        output_test_info = test_info_reg[$high(test_info_reg)];

        $display("cycle %d: ovalid=[match=%0d/rtl=%X/ref=%X] odata=[match=%0d/rtl=%X/ref=%X] ivalid=%0d idata=%X",
          output_test_info.test_count,
          ovalid === output_test_info.ivalid,
          ovalid, output_test_info.ivalid,
          ovalid === 1'b0 || odata === output_test_info.odata_ref,
          odata, output_test_info.odata_ref,
          ivalid, idata);

        if (!output_test_info.test_active) begin
          $display("Waiting for first test output...");
        end
        else begin

          if (ovalid !== output_test_info.ivalid) begin
            $display("Error: mismatch found! test_count=%0d ovalid=%0X ovalid_ref=%0X",
              output_test_info.test_count, ovalid, output_test_info.ivalid);
          end
          else if (ovalid && odata !== output_test_info.odata_ref) begin
            $display("Error: mismatch found! test_count=%0d odata=%0X odata_ref=%0X",
              output_test_info.test_count, odata, output_test_info.odata_ref);
          end
          else begin
            passed_tests++;
          end

          if (output_test_info.test_count == (NUM_TEST_CYCLES-1)) begin
            $display("Test finished!");
            if (passed_tests == NUM_TEST_CYCLES) begin
              $display("PASSED %0d/%0d", passed_tests, NUM_TEST_CYCLES);
            end
            else begin
              $display("FAILED! %0d/%0d", passed_tests, NUM_TEST_CYCLES);
            end
            $finish;
          end
        end

        test_count++;
      end
    end
  end
end


initial begin

  @(negedge clock);
  resetn = 1;
  @(negedge clock);
  @(negedge clock);
  resetn = 0;
  @(negedge clock);
  resetn = 1;

  for (int i = 0; i < 100; i++) begin
    @(negedge clock);
  end

  run_tests = 1'b1;
end




`ifdef DUMP
initial begin
	#5 ;

	$fsdbDumpon;
	$fsdbAutoSwitchDumpfile(500,"pe_array.fsdb",100);
	$fsdbDumpvars(0,pe_array_tb);

	#5000000
	$finish;
end

initial begin
	#5 ;
    $vcdpluson;
    $vcdplusmemon;
    $vcdplusglitchon;
    $vcdplusflush;
        
end

`endif



endmodule
