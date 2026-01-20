// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/codecs/bitpack/encode_bitpack_binding.h"

#include "openzl/codecs/bitpack/common_bitpack_kernel.h"
#include "openzl/common/assertion.h"
#include "openzl/common/errors_internal.h"
#include "openzl/compress/private_nodes.h"
#include "openzl/shared/bits.h"
#include "openzl/shared/utils.h"
#include "openzl/zl_ctransform.h"
#include "openzl/zl_data.h"
#include "openzl/zl_errors.h"

#include <stdio.h>
#include <time.h>
static double EI_bitpack_typed_int_time_ms = 0.0;
static double EI_bitpack_typed_serial_time_ms = 0.0;

static size_t EI_bitpack_typed_int_contentsize = 0;
static size_t EI_bitpack_typed_serial_contentsize = 0;

static uint32_t computeMaxValue8(uint8_t const* src, size_t nbElts)
{
    uint32_t max = 0;
    for (size_t i = 0; i < nbElts; ++i) {
        max = ZL_MAX(max, (uint32_t)src[i]);
    }
    return max;
}

static uint32_t computeMaxValue16(uint16_t const* src, size_t nbElts)
{
    uint32_t max = 0;
    for (size_t i = 0; i < nbElts; ++i) {
        max = ZL_MAX(max, (uint32_t)src[i]);
    }
    return max;
}

static uint32_t computeMaxValue32(uint32_t const* src, size_t nbElts)
{
    uint32_t max = 0;
    for (size_t i = 0; i < nbElts; ++i) {
        max = ZL_MAX(max, (uint32_t)src[i]);
    }
    return max;
}

static uint64_t computeMaxValue64(uint64_t const* src, size_t nbElts)
{
    uint64_t max = 0;
    for (size_t i = 0; i < nbElts; ++i) {
        max = ZL_MAX(max, (uint64_t)src[i]);
    }
    return max;
}

static int computeNbBits(ZL_Input const* in)
{
    const void* const src = ZL_Input_ptr(in);
    size_t const nbElts   = ZL_Input_numElts(in);
    size_t const eltWidth = ZL_Input_eltWidth(in);
    uint64_t maxValue;
    switch (eltWidth) {
        default:
            ZL_ASSERT_FAIL("Impossible");
            ZL_FALLTHROUGH;
        case 1:
            maxValue = computeMaxValue8((uint8_t const*)src, nbElts);
            break;
        case 2:
            maxValue = computeMaxValue16((uint16_t const*)src, nbElts);
            break;
        case 4:
            maxValue = computeMaxValue32((uint32_t const*)src, nbElts);
            break;
        case 8:
            maxValue = computeMaxValue64((uint64_t const*)src, nbElts);
            break;
    }
    // Wastes bits when maxValue == 0...
    return 1 + (maxValue == 0 ? 0 : (int)ZL_highbit64(maxValue));
}

static ZL_Report checkEtlWidth(size_t eltWidth)
{
    switch (eltWidth) {
        case 1:
        case 2:
        case 4:
        case 8:
            return ZL_returnSuccess();
        default:
            ZL_RET_R_ERR(
                    GENERIC, "Bitpack expects element width of 1, 2, 4 or 8");
    }
}

ZL_Report
EI_bitpack_typed(ZL_Encoder* eictx, const ZL_Input* ins[], size_t nbIns)
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
            || ZL_Input_type(in) == ZL_Type_numeric);
    const void* const src = ZL_Input_ptr(in);
    size_t const nbElts   = ZL_Input_numElts(in);
    size_t const eltWidth = ZL_Input_eltWidth(in);

    // Check that eltWidth is one we support
    ZL_RET_R_IF_ERR(checkEtlWidth(eltWidth));

    int const nbBits = computeNbBits(in);
    ZL_ASSERT_EQ(ZS_bitpackEncodeVerify(src, nbElts, eltWidth, nbBits), 1);

    size_t const dstCapacity = ZS_bitpackEncodeBound(nbElts, nbBits);
    ZL_Output* const out =
            ZL_Encoder_createTypedStream(eictx, 0, dstCapacity, 1);
    ZL_RET_R_IF_NULL(allocation, out);

    uint8_t* const ostart = (uint8_t*)ZL_Output_ptr(out);
    size_t const dstSize  = ZS_bitpackEncode(
            ostart, dstCapacity, src, nbElts, eltWidth, nbBits);

    size_t const maxNbElts = dstSize * 8 / (size_t)nbBits;
    ZL_ASSERT_GE(maxNbElts, nbElts);
    ZL_ASSERT_LT(maxNbElts - nbElts, 8);
    ZL_ASSERT_GT(nbBits, 0);
    ZL_ASSERT_LE(nbBits, 64);
    uint8_t const hasExtraSpace = maxNbElts > nbElts;

    // Header is up to 2 bytes, the first byte encodes the nbBits and eltWidth.
    // The second byte, if exists, encodes how many extra elements are there in
    // the stream that shouldn't be decoded. Header's first byte is encoded as
    // following [0, 6) = nbBits [6, 8) = log2(eltWidth)
    uint8_t header[2];
    uint8_t const log2EltWidth[9] = { 0, 0, 1, 0, 2, 0, 0, 0, 3 };
    ZL_ASSERT(nbBits - 1 <= 0x3F);
    header[0] = (uint8_t)((log2EltWidth[eltWidth] << 6) | (nbBits - 1));
    if (hasExtraSpace) {
        ZL_ASSERT(maxNbElts - nbElts < 256);
        header[1] = (uint8_t)(maxNbElts - nbElts);
        ZL_Encoder_sendCodecHeader(eictx, &header, 2);
    } else {
        ZL_Encoder_sendCodecHeader(eictx, &header, 1);
    }

    ZL_RET_R_IF_ERR(ZL_Output_commit(out, dstSize));

    clock_gettime(CLOCK_MONOTONIC, &end);
    double start_time_ms = (double)start.tv_sec * 1000.0 + (double)start.tv_nsec / 1000000.0;
    double end_time_ms = (double)end.tv_sec * 1000.0 + (double)end.tv_nsec / 1000000.0;

    double current_duration = end_time_ms - start_time_ms;

    ZL_Type currentType = ZL_Input_type(in);
    switch (currentType){
        case ZL_Type_serial:
            EI_bitpack_typed_serial_time_ms += current_duration;
            EI_bitpack_typed_serial_contentsize += ZL_Input_contentSize(in);
            fprintf(stderr, "[EI_bitpack_typed_serial] Start: %.4f ms | End: %.4f ms | Accumulated: %.4f ms | Current_duration: %.4f | contentsize: %zu\n", start_time_ms, end_time_ms, EI_bitpack_typed_serial_time_ms, current_duration, EI_bitpack_typed_serial_contentsize);
            break;
        case ZL_Type_struct:
            break;
        case ZL_Type_numeric:
            EI_bitpack_typed_int_time_ms += current_duration;
            EI_bitpack_typed_int_contentsize += ZL_Input_contentSize(in);
            fprintf(stderr, "[EI_bitpack_typed_numeric] Start: %.4f ms | End: %.4f ms | Accumulated: %.4f ms | Current_duration: %.4f | contentsize: %zu\n", start_time_ms, end_time_ms, EI_bitpack_typed_int_time_ms, current_duration, EI_bitpack_typed_int_contentsize);
            break;
        case ZL_Type_string:
            break;
    }

    return ZL_returnValue(1);
}

ZL_GraphID SI_selector_bitpack(
        ZL_Selector const* selCtx,
        const ZL_Input* in,
        ZL_GraphID const* customSuccessors,
        size_t nbCustomSuccessors)
{
    (void)selCtx;
    (void)customSuccessors;
    (void)nbCustomSuccessors;
    ZL_Type inType = ZL_Input_type(in);
    ZL_ASSERT(inType == ZL_Type_serial || inType == ZL_Type_numeric);
    if (inType == ZL_Type_numeric) {
        return ZL_GRAPH_BITPACK_INT;
    } else {
        return ZL_GRAPH_BITPACK_SERIAL;
    }
}
