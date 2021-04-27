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

#ifndef __PE_ARRAY_QUARTUS_H__
#define __PE_ARRAY_QUARTUS_H__

#include "pe_array.h"

namespace pe_array {

enum DeviceFamily {
  DEVICE_A10,
  DEVICE_S10
};

enum DeviceVoltage {
  VOLTAGE_090V,
  VOLTAGE_095V
};

struct FitReportInfo {
  bool is_pe_array;
  int pe_id;
  int num_tenth_alms;
  int num_aluts;
  int num_regs;
  int num_mlabs;
  int num_m20ks;
  int num_dsps;
};

void generate_quartus_project(const pe_array_cfg_t* cfg,
    const std::string& template_dir, const std::string& rtl_dir,
    const std::string& output_dir, DeviceVoltage voltage, int speed_grade);

void iterate_over_fit_report(const std::string& filename,
    std::function<bool(const FitReportInfo&)> callback);

FitReportInfo parse_pe_array_info_from_fit_report(const std::string& filename,
    bool* error);

int parse_fmax_from_sta_report(const std::string& filename);

} // namespace pe_array

#endif // __PE_ARRAY_QUARTUS_H__
