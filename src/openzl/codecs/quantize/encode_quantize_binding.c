// Copyright (c) Meta Platforms, Inc. and affiliates.
#include "openzl/codecs/quantize/encode_quantize_binding.h"

#include "openzl/codecs/quantize/common_quantize.h"
#include "openzl/codecs/quantize/encode_quantize_kernel.h"
#include "openzl/common/debug.h"
#include "openzl/common/errors_internal.h" // ZS2_RET_IF*
#include "openzl/zl_ctransform.h"

#include <stdio.h>
#include <time.h>
static double EI_quantizeLengths_time_ms = 0.0;
static double EI_quantizeOffsets_time_ms = 0.0;

static ZL_Report EI_quantize(
        ZL_Encoder* eictx,
        const ZL_Input* in,
        ZL_Quantize32Params const* params)
{
    ZL_RET_R_IF_NE(GENERIC, ZL_Input_type(in), ZL_Type_numeric);
    ZL_RET_R_IF_NE(GENERIC, ZL_Input_eltWidth(in), 4);

    size_t const nbElts = ZL_Input_numElts(in);

    ZL_Output* codes = ZL_Encoder_createTypedStream(eictx, 0, nbElts, 1);
    // TODO(terrelln): Bound this allocation tighter
    size_t const bitsCapacity = 4 * nbElts + 9;
    ZL_Output* bits = ZL_Encoder_createTypedStream(eictx, 1, bitsCapacity, 1);

    ZL_RET_R_IF_NULL(allocation, codes);
    ZL_RET_R_IF_NULL(allocation, bits);

    ZL_Report const bitsSize = ZS2_quantize32Encode(
            (uint8_t*)ZL_Output_ptr(bits),
            bitsCapacity,
            (uint8_t*)ZL_Output_ptr(codes),
            (uint32_t const*)ZL_Input_ptr(in),
            nbElts,
            params);
    ZL_RET_R_IF_ERR(bitsSize);

    ZL_RET_R_IF_ERR(ZL_Output_commit(codes, nbElts));
    ZL_RET_R_IF_ERR(ZL_Output_commit(bits, ZL_validResult(bitsSize)));

    return ZL_returnValue(2);
}

ZL_Report
EI_quantizeOffsets(ZL_Encoder* eictx, const ZL_Input* ins[], size_t nbIns)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    ZL_ASSERT_EQ(nbIns, 1);
    ZL_ASSERT_NN(ins);
    const ZL_Input* in = ins[0];
    ZL_Report EI_quantizeOffsets_report = EI_quantize(eictx, in, &ZL_quantizeOffsetsParams);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double start_time_ms = (double)start.tv_sec * 1000.0 + (double)start.tv_nsec / 1000000.0;
    double end_time_ms = (double)end.tv_sec * 1000.0 + (double)end.tv_nsec / 1000000.0;

    double current_duration = end_time_ms - start_time_ms;
    EI_quantizeOffsets_time_ms += current_duration;
    fprintf(stderr, "[EI_quantizeOffsets] Start: %.4f ms | End: %.4f ms | Accumulated: %.4f ms | Current_duration: %.4f\n", start_time_ms, end_time_ms, EI_quantizeOffsets_time_ms, current_duration);
    return EI_quantizeOffsets_report;
}

ZL_Report
EI_quantizeLengths(ZL_Encoder* eictx, const ZL_Input* ins[], size_t nbIns)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    ZL_ASSERT_EQ(nbIns, 1);
    ZL_ASSERT_NN(ins);
    const ZL_Input* in = ins[0];
    ZL_Report EI_quantizeLengths_report = EI_quantize(eictx, in, &ZL_quantizeLengthsParams);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double start_time_ms = (double)start.tv_sec * 1000.0 + (double)start.tv_nsec / 1000000.0;
    double end_time_ms = (double)end.tv_sec * 1000.0 + (double)end.tv_nsec / 1000000.0;

    double current_duration = end_time_ms - start_time_ms;
    EI_quantizeLengths_time_ms += current_duration;
    fprintf(stderr, "[EI_quantizeLengths] Start: %.4f ms | End: %.4f ms | Accumulated: %.4f ms | Current_duration: %.4f\n", start_time_ms, end_time_ms, EI_quantizeLengths_time_ms, current_duration);
    return EI_quantizeLengths_report;
}
