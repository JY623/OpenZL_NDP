// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/codecs/zigzag/encode_zigzag_binding.h"
#include "openzl/codecs/zigzag/encode_zigzag_kernel.h" // ZS_zigzagEncodeXX
#include "openzl/common/assertion.h"

#include <stdio.h>
#include <time.h>
static double EI_zigzag_num_time_ms = 0.0;

// ZL_TypedEncoderFn
ZL_Report EI_zigzag_num(ZL_Encoder* eictx, const ZL_Input* ins[], size_t nbIns)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    ZL_ASSERT_NN(eictx);
    ZL_ASSERT_EQ(nbIns, 1);
    ZL_ASSERT_NN(ins);
    const ZL_Input* const in = ins[0];
    ZL_ASSERT_NN(in);
    ZL_ASSERT_EQ(ZL_Input_type(in), ZL_Type_numeric);
    size_t const numWidth = ZL_Input_eltWidth(in);
    size_t const nbInts   = ZL_Input_numElts(in);
    ZL_Output* const out =
            ZL_Encoder_createTypedStream(eictx, 0, nbInts, numWidth);
    ZL_RET_R_IF_NULL(allocation, out);
    ZL_ASSERT(numWidth == 1 || numWidth == 2 || numWidth == 4 || numWidth == 8);
    switch (numWidth) {
        case 1:
            ZL_zigzagEncode8(ZL_Output_ptr(out), ZL_Input_ptr(in), nbInts);
            break;
        case 2:
            ZL_zigzagEncode16(ZL_Output_ptr(out), ZL_Input_ptr(in), nbInts);
            break;
        case 4:
            ZL_zigzagEncode32(ZL_Output_ptr(out), ZL_Input_ptr(in), nbInts);
            break;
        case 8:
            ZL_zigzagEncode64(ZL_Output_ptr(out), ZL_Input_ptr(in), nbInts);
            break;
        default:
            ZL_ASSERT_FAIL("Unreachable");
    }
    ZL_RET_R_IF_ERR(ZL_Output_commit(out, nbInts));

    clock_gettime(CLOCK_MONOTONIC, &end);
    double start_time_ms = (double)start.tv_sec * 1000.0 + (double)start.tv_nsec / 1000000.0;
    double end_time_ms = (double)end.tv_sec * 1000.0 + (double)end.tv_nsec / 1000000.0;

    double current_duration = end_time_ms - start_time_ms;
    EI_zigzag_num_time_ms += current_duration;
    fprintf(stderr, "[EI_zigzag_num] Start: %.4f ms | End: %.4f ms | Accumulated: %.4f ms | Current_duration: %.4f\n", start_time_ms, end_time_ms, EI_zigzag_num_time_ms, current_duration);
    return ZL_returnValue(1);
}
