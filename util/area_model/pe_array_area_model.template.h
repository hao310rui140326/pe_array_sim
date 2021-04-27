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

#ifndef __PE_ARRAY_AREA_MODEL_H__
#define __PE_ARRAY_AREA_MODEL_H__

#ifndef INTELFPGA_CL

typedef struct __attribute__((packed)) {
  struct __attribute__((packed)) {
    struct __attribute__((packed)) {
      int num_tenth_alms;
      int num_aluts;
      int num_regs;
      int num_mlabs;
    } accum[2];
  } dot[2];

  int num_extra_tenth_alms;
  int num_extra_aluts;
  int num_extra_regs;
  int num_extra_mlabs;
#if PE_ARRAY_ENABLE_AREA_MODEL_FMAX
  int model_fmax;
#endif
} pe_array_area_model_t;
static const pe_array_area_model_t zero_pe_array_area_model = {0};

#if PE_ARRAY_ENABLE_AREA_MODEL_FMAX
#define PE_ARRAY_AREA_MODEL_FMAX(fmax) fmax
#else
#define PE_ARRAY_AREA_MODEL_FMAX(fmax)
#endif

__GENERATE_MODEL_HERE__

#endif // !INTELFPGA_CL

#endif // __PE_ARRAY_AREA_MODEL_H__
