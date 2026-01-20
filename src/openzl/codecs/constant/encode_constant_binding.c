// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/codecs/constant/encode_constant_binding.h"
#include "openzl/codecs/constant/encode_constant_kernel.h"

#include "openzl/common/assertion.h"
#include "openzl/common/errors_internal.h"
#include "openzl/shared/varint.h"
#include "openzl/zl_data.h"
#include "openzl/zl_errors.h"

#include <stdio.h>
#include <time.h>
static double EI_constant_typed_serial_time_ms = 0.0;
static double EI_constant_typed_struct_time_ms = 0.0;

static size_t EI_constant_typed_serial_contentsize = 0;
static size_t EI_constant_typed_struct_contentsize = 0;

ZL_Report
EI_constant_typed(ZL_Encoder* eictx, const ZL_Input* ins[], size_t nbIns)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    ZL_ASSERT_EQ(nbIns, 1);
    ZL_ASSERT_NN(ins);
    const ZL_Input* in = ins[0];
    ZL_ASSERT_NN(eictx);
    ZL_ASSERT_NN(in);
    ZL_ASSERT(
            ZL_Input_type(in) == ZL_Type_serial
            || ZL_Input_type(in) == ZL_Type_struct);

    const uint8_t* const src = ZL_Input_ptr(in);
    size_t const nbElts      = ZL_Input_numElts(in);
    size_t const eltWidth    = ZL_Input_eltWidth(in);
    ZL_RET_R_IF_LT(srcSize_tooSmall, nbElts, 1);
    ZL_ASSERT_GE(eltWidth, 1);
    ZL_RET_R_IF_EQ(
            node_invalid_input, ZS_isConstantStream(src, nbElts, eltWidth), 0);

    ZL_Output* const out = ZL_Encoder_createTypedStream(eictx, 0, 1, eltWidth);
    ZL_RET_R_IF_NULL(allocation, out);
    uint8_t* const outPtr = (uint8_t*)ZL_Output_ptr(out);
    ZL_ASSERT_NN(outPtr);

    uint8_t header[ZL_VARINT_LENGTH_64];
    size_t const varintSize = ZL_varintEncode((uint64_t)nbElts, header);
    ZL_Encoder_sendCodecHeader(eictx, header, varintSize);

    ZS_encodeConstant(outPtr, src, eltWidth);
    ZL_RET_R_IF_ERR(ZL_Output_commit(out, 1));

    clock_gettime(CLOCK_MONOTONIC, &end);
    double start_time_ms = (double)start.tv_sec * 1000.0 + (double)start.tv_nsec / 1000000.0;
    double end_time_ms = (double)end.tv_sec * 1000.0 + (double)end.tv_nsec / 1000000.0;

    double current_duration = end_time_ms - start_time_ms;

    ZL_Type currentType = ZL_Input_type(in);
    switch (currentType){
        case ZL_Type_serial:
            EI_constant_typed_serial_time_ms += current_duration;
            EI_constant_typed_serial_contentsize += ZL_Input_contentSize(in);
            fprintf(stderr, "[EI_constant_typed_serial] Start: %.4f ms | End: %.4f ms | Accumulated: %.4f ms | Current_duration: %.4f | content_size: %zu\n", start_time_ms, end_time_ms, EI_constant_typed_serial_time_ms, current_duration, EI_constant_typed_serial_contentsize);
            break;
        case ZL_Type_struct:
            EI_constant_typed_struct_time_ms += current_duration;
            EI_constant_typed_struct_contentsize += ZL_Input_contentSize(in);
            fprintf(stderr, "[EI_constant_typed_struct] Start: %.4f ms | End: %.4f ms | Accumulated: %.4f ms | Current_duration: %.4f | content_size: %zu\n", start_time_ms, end_time_ms, EI_constant_typed_struct_time_ms, current_duration, EI_constant_typed_struct_contentsize);
            break;
        case ZL_Type_numeric:
            break;
        case ZL_Type_string:
            break;
    }

    return ZL_returnSuccess();
}
