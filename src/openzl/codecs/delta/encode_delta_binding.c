// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/codecs/delta/encode_delta_binding.h"
#include "openzl/codecs/delta/encode_delta_kernel.h" // ZS_deltaEncodeXX
#include "openzl/common/assertion.h"
#include "openzl/common/errors_internal.h"
#include "openzl/zl_data.h"

#include <stdio.h>
#include <time.h>
static double EI_delta_int_time_ms = 0.0;

// ZL_TypedEncoderFn
// This variant is compatible with any allowed integer width
ZL_Report EI_delta_int(ZL_Encoder* eictx, const ZL_Input* ins[], size_t nbIns)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    ZL_ASSERT_NN(eictx);
    ZL_ASSERT_EQ(nbIns, 1);
    ZL_ASSERT_NN(ins);
    const ZL_Input* in = ins[0];
    ZL_ASSERT_NN(in);
    ZL_ASSERT_EQ(ZL_Input_type(in), ZL_Type_numeric);
    size_t const intWidth = ZL_Input_eltWidth(in);
    ZL_ASSERT(intWidth == 1 || intWidth == 2 || intWidth == 4 || intWidth == 8);
    size_t const nbInts = ZL_Input_numElts(in);
    ZL_Output* const out =
            ZL_Encoder_createTypedStream(eictx, 0, nbInts, intWidth);
    ZL_RET_R_IF_NULL(allocation, out);
    const void* src = ZL_Input_ptr(in);
    // Note : proper alignment is guaranteed by graph engine
    void* dst = ZL_Output_ptr(out);
    if (nbInts == 0) {
        // Special case: Zero ints behaves the same for both variants.
        ZL_RET_R_IF_ERR(ZL_Output_commit(out, 0));
    } else if (ZL_Encoder_getCParam(eictx, ZL_CParam_formatVersion) < 13) {
        // Old variant: Write the first element to the first value in the stream
        ZS_deltaEncode(dst, (char*)dst + intWidth, src, nbInts, intWidth);
        ZL_RET_R_IF_ERR(ZL_Output_commit(out, nbInts));
    } else {
        // New variant: Write the first element to the stream header
        uint8_t header[8];
        ZL_ASSERT_GT(nbInts, 0);
        ZL_ASSERT_LE(intWidth, sizeof(header));
        ZS_deltaEncode(header, dst, src, nbInts, intWidth);
        ZL_Encoder_sendCodecHeader(eictx, header, intWidth);
        ZL_RET_R_IF_ERR(ZL_Output_commit(out, nbInts - 1));
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double start_time_ms = (double)start.tv_sec * 1000.0 + (double)start.tv_nsec / 1000000.0;
    double end_time_ms = (double)end.tv_sec * 1000.0 + (double)end.tv_nsec / 1000000.0;

    double current_duration = end_time_ms - start_time_ms;
    EI_delta_int_time_ms += current_duration;

    fprintf(stderr, "[EI_delta_int] Start: %.4f ms | End: %.4f ms | Accumulated: %.4f ms | Current_duration: %.4f\n", start_time_ms, end_time_ms, EI_delta_int_time_ms, current_duration);
    return ZL_returnValue(1);
}
