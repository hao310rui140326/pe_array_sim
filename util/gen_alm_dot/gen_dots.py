#!/usr/bin/env python
import sys
import os
import shutil
import subprocess
import textwrap
import re

build_dir = 'build'

devices = ('a10','s10')

dots = [
    # 12x12 (FP16/INT12)
    {'feature_width': 12, 'filter_width': 12, 'dot_size': 4},
    {'feature_width': 12, 'filter_width': 12, 'dot_size': 8},
    {'feature_width': 12, 'filter_width': 12, 'dot_size': 16},
    {'feature_width': 12, 'filter_width': 12, 'dot_size': 32},

    # # 11x11 (FP15/INT11)
    # {'feature_width': 7, 'filter_width': 7, 'dot_size': 4},
    # {'feature_width': 7, 'filter_width': 7, 'dot_size': 8},
    # {'feature_width': 7, 'filter_width': 7, 'dot_size': 16},
    # {'feature_width': 7, 'filter_width': 7, 'dot_size': 32},

    # # 10x10 (FP14/INT10)
    # {'feature_width': 7, 'filter_width': 7, 'dot_size': 4},
    # {'feature_width': 7, 'filter_width': 7, 'dot_size': 8},
    # {'feature_width': 7, 'filter_width': 7, 'dot_size': 16},
    # {'feature_width': 7, 'filter_width': 7, 'dot_size': 32},

    # # 9x9 (FP13/INT9)
    # {'feature_width': 7, 'filter_width': 7, 'dot_size': 4},
    # {'feature_width': 7, 'filter_width': 7, 'dot_size': 8},
    # {'feature_width': 7, 'filter_width': 7, 'dot_size': 16},
    # {'feature_width': 7, 'filter_width': 7, 'dot_size': 32},

    # 8x8 (FP12/INT8)
    {'feature_width': 8, 'filter_width': 8, 'dot_size': 4},
    {'feature_width': 8, 'filter_width': 8, 'dot_size': 8},
    {'feature_width': 8, 'filter_width': 8, 'dot_size': 16},
    {'feature_width': 8, 'filter_width': 8, 'dot_size': 32},

    # 7x7 (FP11/INT7)
    {'feature_width': 7, 'filter_width': 7, 'dot_size': 4},
    {'feature_width': 7, 'filter_width': 7, 'dot_size': 8},
    {'feature_width': 7, 'filter_width': 7, 'dot_size': 16},
    {'feature_width': 7, 'filter_width': 7, 'dot_size': 32},

    # 6x6 (FP10/INT6)
    {'feature_width': 6, 'filter_width': 6, 'dot_size': 4},
    {'feature_width': 6, 'filter_width': 6, 'dot_size': 8},
    {'feature_width': 6, 'filter_width': 6, 'dot_size': 16},
    {'feature_width': 6, 'filter_width': 6, 'dot_size': 32},

    # 5x5 (FP9/INT5)
    {'feature_width': 5, 'filter_width': 5, 'dot_size': 4},
    {'feature_width': 5, 'filter_width': 5, 'dot_size': 8},
    {'feature_width': 5, 'filter_width': 5, 'dot_size': 16},
    {'feature_width': 5, 'filter_width': 5, 'dot_size': 32},

    # 5x4 (mixed)
    {'feature_width': 5, 'filter_width': 4, 'dot_size': 4},
    {'feature_width': 5, 'filter_width': 4, 'dot_size': 8},
    {'feature_width': 5, 'filter_width': 4, 'dot_size': 16},
    {'feature_width': 5, 'filter_width': 4, 'dot_size': 32},

    # 4x5 (mixed)
    {'feature_width': 4, 'filter_width': 5, 'dot_size': 4},
    {'feature_width': 4, 'filter_width': 5, 'dot_size': 8},
    {'feature_width': 4, 'filter_width': 5, 'dot_size': 16},
    {'feature_width': 4, 'filter_width': 5, 'dot_size': 32},

    # 4x4 (FP8/INT4)
    {'feature_width': 4, 'filter_width': 4, 'dot_size': 4},
    {'feature_width': 4, 'filter_width': 4, 'dot_size': 8},
    {'feature_width': 4, 'filter_width': 4, 'dot_size': 16},
    {'feature_width': 4, 'filter_width': 4, 'dot_size': 32},
]


def main():
  if os.path.exists(build_dir):
    sys.exit('Error: build directory already exists, delete it first')
  os.mkdir(build_dir)

  for device in devices:
    for dot_info in dots:
      gen_dot_product(device, dot_info['feature_width'], dot_info['filter_width'], dot_info['dot_size'])

  gen_dot_wrapper()


def gen_dot_product(device, feature_width, filter_width, dot_size):
  output_module_name = 'pe_dot_alm_{0}_{1}x{2}x{3}'.format(device,
      feature_width, filter_width, dot_size)

  gen_dot_product_dir = os.path.join(build_dir, 'gen-{0}'.format(output_module_name))

  if not os.path.exists(gen_dot_product_dir):
    os.mkdir(gen_dot_product_dir)

    verilog_filename = 'dot_product_{0}.sv'.format(device)
    verilog_relpath = os.path.relpath(verilog_filename, gen_dot_product_dir)

    tcl_filename = 'gen_dot.tcl'
    tcl_filepath = os.path.join(gen_dot_product_dir, tcl_filename)

    if device == 'a10':
      device_name = '10AX115S2F45I1SG'
      device_family = 'Arria 10'
    elif device == 's10':
      device_name = '1SG280LU2F50E2VG'
      device_family = 'Stratix 10'
    else:
      assert False

    quartusini_filepath = os.path.join(gen_dot_product_dir, 'quartus.ini')
    with open(quartusini_filepath, 'w') as f:
      f.write(textwrap.dedent('''\
        # Turn on fractal synthesis
        opt_fractal_synthesis = on

        # Turn off DSP inference
        opt_dont_use_mac = on
        opt_use_old_infer_dsp = on
        '''))

        # package require ::quartus::project
    with open(tcl_filepath, 'w') as f:
      f.write(textwrap.dedent('''\
        load_package flow
        project_new -revision top top
        set_global_assignment -name PROJECT_OUTPUT_DIRECTORY output_files
        set_global_assignment -name MIN_CORE_JUNCTION_TEMP "-40"
        set_global_assignment -name MAX_CORE_JUNCTION_TEMP 100
        set_global_assignment -name DEVICE {device_name}
        set_global_assignment -name FAMILY "{device_family}"
        set_global_assignment -name ERROR_CHECK_FREQUENCY_DIVISOR 1
        set_global_assignment -name EDA_SIMULATION_TOOL "ModelSim-Altera (Verilog)"
        set_global_assignment -name EDA_OUTPUT_DATA_FORMAT "VERILOG HDL" -section_id eda_simulation
        set_global_assignment -name SYSTEMVERILOG_FILE {verilog_relpath}
        set_global_assignment -name POWER_AUTO_COMPUTE_TJ ON
        set_global_assignment -name POWER_PRESET_COOLING_SOLUTION "23 MM HEAT SINK WITH 200 LFPM AIRFLOW"
        set_global_assignment -name POWER_BOARD_THERMAL_MODEL "NONE (CONSERVATIVE)"
        set_global_assignment -name FRACTAL_SYNTHESIS ON -entity dot_product

        set_instance_assignment -name VIRTUAL_PIN ON -to dout -entity top
        set_instance_assignment -name VIRTUAL_PIN ON -to din_a -entity top
        set_instance_assignment -name VIRTUAL_PIN ON -to din_b -entity top

        set_parameter -name DOT {dot_size}
        set_parameter -name SIZEA {feature_width}
        set_parameter -name SIZEB {filter_width}
        set_parameter -name NUM 1
        set_parameter -name PIPELINE 1

        export_assignments

        puts "INFO: Running quartus_syn"
        if {{[catch {{execute_module -tool syn}} result]}} {{
          puts "$result"
          puts "ERROR: quartus_syn failed"
          exit 1
        }} else {{
          puts "INFO: quartus_syn succeeded"
        }}

        puts "INFO: Running quartus_eda"
        if {{[catch {{execute_module -tool eda -args "--simulation --tool=modelsim --format=verilog --functional"}} result]}} {{
          puts "$result"
          puts "ERROR: quartus_eda failed"
          exit 1
        }} else {{
          puts "INFO: quartus_eda succeeded"
        }}

        project_close
        '''.format(
          verilog_relpath = verilog_relpath,
          device_name = device_name,
          device_family = device_family,
          dot_size = dot_size,
          feature_width = feature_width,
          filter_width = filter_width)))

    subprocess.check_call(['quartus_sh', '-t', tcl_filename], cwd=gen_dot_product_dir)

  simulation_model_filepath = os.path.join(gen_dot_product_dir, 'simulation', 'modelsim', 'top.vo')
  output_filepath = os.path.join(build_dir, '{0}.sv'.format(output_module_name))
  transform_simulation_model(simulation_model_filepath, output_filepath, output_module_name)

def transform_simulation_model(input_filename, output_filename, output_module_name):
  inst_lines = []
  inst_module = None
  inst_name = None
  inst_done = False
  inst_params = []

  with open(input_filename, 'rU') as input_file:
    with open(output_filename, 'w') as output_file:
      for line in input_file:
        line = line.replace('.devclrn(devclrn),', '.devclrn(),')
        line = line.replace('.devpor(devpor),', '.devpor(),')
        line = line.replace('module top (', 'module {0} ('.format(output_module_name))

        replace_bad_chars = False
        for idx, char in enumerate(line):
          if char == '\\':
            line = line[:idx] + 'X' + line[idx+1:]
            replace_bad_chars = True
          elif replace_bad_chars:
            if char in ('[', ']', '.', '~', '|'):
              line = line[:idx] + '_' + line[idx+1:]
            if char.isspace():
              replace_bad_chars = False

        if not inst_name:
          m = re.match(r'^(fourteennm_ff|dffeas|fourteennm_lcell_comb|twentynm_lcell_comb|twentynm_mac) (\S+) \($', line)
          if m != None:
            inst_module = m.group(1)
            inst_name = m.group(2)
          else:
            output_file.write(line)

        else:
          if not inst_done:
            inst_lines.append(line)
            if line.endswith(');\n'):
              inst_done = True

          else:
            if line.strip():
              m = re.match(r'^defparam (\S+) \.(\w+) = ([^;]+);$', line)
              assert m != None
              assert m.group(1) == inst_name
              inst_params.append((m.group(2), m.group(3)))

            else:
              output_file.write('{0} #(\n'.format(inst_module))

              first = True
              for param_name, param_value in inst_params:
                if (inst_module == 'fourteennm_ff' and param_name == 'power_up'
                    and param_value == '"dont_care"'):
                  param_value = ''

                if not first:
                  output_file.write(',\n')
                first = False
                output_file.write('	.{0}({1})'.format(param_name, param_value))

              output_file.write('\n) {0} (\n'.format(inst_name))

              for line in inst_lines:
                if (inst_module == 'fourteennm_lcell_comb'
                    and line == '\t.sharein(gnd),\n'):
                  output_file.write('\t.sharein(),\n')
                else:
                  output_file.write(line)

              output_file.write('\n')

              inst_lines = []
              inst_name = None
              inst_done = False
              inst_params = []

def gen_dot_wrapper():
  output_module_name = 'pe_dot_alm'

  template_filepath = output_module_name + '.template.sv'
  output_filepath = os.path.join(build_dir, output_module_name + '.sv')

  with open(template_filepath, 'rU') as template:
    with open(output_filepath, 'w') as output:

      for line in template:

        if '__INSTANTIATE_DOTS_HERE__' not in line:
          output.write(line)
        else:

          for device in devices:
            for dot_info in dots:
              module_name = 'pe_dot_alm_{0}_{1}x{2}x{3}'.format(device,
                  dot_info['feature_width'], dot_info['filter_width'], dot_info['dot_size'])

              condition = 'else if (cfg.DEVICE_{0} == 1 && cfg.FEATURE_WIDTH == {1} && cfg.FILTER_WIDTH == {2} && cfg.DOT_SIZE == {3})'.format(
                  device.upper(), dot_info['feature_width'], dot_info['filter_width'], dot_info['dot_size'])

              output.write(textwrap.dedent('''\
                  {condition} begin : {module_name}_gen
                    {module_name} {module_name}_inst (
                      .clk(clock),
                      .din_a(i_feature),
                      .din_b(i_filter),
                      .dout(result_pre)
                    );
                  end
                  '''.format(condition = condition, module_name = module_name)))

          output.write('else $fatal(1, "Invalid ALM dot configuration");\n')


if __name__ == '__main__':
  main()
