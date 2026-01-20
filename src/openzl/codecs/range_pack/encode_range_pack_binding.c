// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/codecs/range_pack/encode_range_pack_binding.h"
#include "openzl/codecs/range_pack/encode_range_pack_kernel.h"

#include "openzl/common/errors_internal.h"
#include "openzl/shared/estimate.h"
#include "openzl/shared/mem.h"
#include "openzl/shared/numeric_operations.h" // NUMOP_*
#include "openzl/zl_ctransform.h"
#include "openzl/zl_data.h"
#include "openzl/zl_errors.h"

#include <stdio.h>
#include <time.h>
static double EI_rangePack_time_ms = 0.0;

ZL_Report EI_rangePack(ZL_Encoder* eictx, const ZL_Input* ins[], size_t nbIns)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    ZL_ASSERT_EQ(nbIns, 1);
    ZL_ASSERT_NN(ins);
    const ZL_Input* in    = ins[0];
    void const* src       = ZL_Input_ptr(in);
    size_t const srcWidth = ZL_Input_eltWidth(in);
    size_t const nbElts   = ZL_Input_numElts(in);
    ZL_ElementRange const range =
            ZL_computeUnsignedRange(src, nbElts, srcWidth);
    const size_t dstWidth = NUMOP_numericWidthForValue(range.max - range.min);

    ZL_Output* const dstStream =
            ZL_Encoder_createTypedStream(eictx, 0, nbElts, dstWidth);
    ZL_RET_R_IF(allocation, !dstStream);
    void* dst = ZL_Output_ptr(dstStream);
    rangePackEncode(dst, dstWidth, src, srcWidth, nbElts, range.min);
    ZL_RET_R_IF_ERR(ZL_Output_commit(dstStream, nbElts));

    /* Header has the source size and the min value if needed */
    uint8_t header[9];
    size_t header_size = 1;
    header[0]          = (uint8_t)srcWidth;
    if (range.min != 0) {
        ZL_writeLE64_N(header + 1, (uint64_t)range.min, srcWidth);
        header_size += srcWidth;
    }
    ZL_Encoder_sendCodecHeader(eictx, &header, header_size);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double start_time_ms = (double)start.tv_sec * 1000.0 + (double)start.tv_nsec / 1000000.0;
    double end_time_ms = (double)end.tv_sec * 1000.0 + (double)end.tv_nsec / 1000000.0;

    double current_duration = end_time_ms - start_time_ms;
    EI_rangePack_time_ms += current_duration;
    fprintf(stderr, "[EI_rangePack] Start: %.4f ms | End: %.4f ms | Accumulated: %.4f ms | Current_duration: %.4f\n", start_time_ms, end_time_ms, EI_rangePack_time_ms, current_duration);

    return ZL_returnValue(1);
}
