// Copyright (c) Meta Platforms, Inc. and affiliates.
#include "openzl/codecs/flatpack/encode_flatpack_binding.h"

#include "openzl/codecs/flatpack/encode_flatpack_kernel.h"
#include "openzl/zl_errors.h"

#include <stdio.h>
#include <time.h>
static double EI_flatpack_time_ms = 0.0;

ZL_Report EI_flatpack(ZL_Encoder* eictx, const ZL_Input* ins[], size_t nbIns)
{

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    ZL_ASSERT_EQ(nbIns, 1);
    ZL_ASSERT_NN(ins);
    const ZL_Input* in = ins[0];
    ZL_ASSERT_EQ(ZL_Input_type(in), ZL_Type_serial);

    size_t const nbElts = ZL_Input_numElts(in);

    ZL_Output* alphabet = ZL_Encoder_createTypedStream(eictx, 0, 256, 1);

    size_t const packedCapacity = ZS_flatpackEncodeBound(nbElts);
    ZL_Output* packed =
            ZL_Encoder_createTypedStream(eictx, 1, packedCapacity, 1);

    if (alphabet == NULL || packed == NULL) {
        ZL_RET_R_ERR(allocation);
    }

    ZS_FlatPackSize const size = ZS_flatpackEncode(
            (uint8_t*)ZL_Output_ptr(alphabet),
            256,
            (uint8_t*)ZL_Output_ptr(packed),
            packedCapacity,
            (uint8_t const*)ZL_Input_ptr(in),
            nbElts);
    ZL_ASSERT(!ZS_FlatPack_isError(size));

    ZL_RET_R_IF_ERR(ZL_Output_commit(alphabet, ZS_FlatPack_alphabetSize(size)));
    ZL_RET_R_IF_ERR(
            ZL_Output_commit(packed, ZS_FlatPack_packedSize(size, nbElts)));

    clock_gettime(CLOCK_MONOTONIC, &end);
    double start_time_ms = (double)start.tv_sec * 1000.0 + (double)start.tv_nsec / 1000000.0;
    double end_time_ms = (double)end.tv_sec * 1000.0 + (double)end.tv_nsec / 1000000.0;

    double current_duration = end_time_ms - start_time_ms;
    EI_flatpack_time_ms += current_duration;
    fprintf(stderr, "[EI_flatpack] Start: %.4f ms | End: %.4f ms | Accumulated: %.4f ms | Current_duration: %.4f\n", start_time_ms, end_time_ms, EI_flatpack_time_ms, current_duration);

    return ZL_returnValue(2);
}
