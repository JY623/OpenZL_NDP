// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/codecs/concat/encode_concat_binding.h"

#include "openzl/common/assertion.h"
#include "openzl/shared/mem.h" // ZL_memcpy
#include "openzl/zl_data.h"
#include "openzl/zl_errors.h"

#include <stdio.h>
#include <time.h>
static double EI_concat_serial_time_ms = 0.0;
static double EI_concat_struct_time_ms = 0.0;
static double EI_concat_numeric_time_ms = 0.0;
static double EI_concat_string_time_ms = 0.0;

static size_t EI_concat_serial_contentsize = 0;
static size_t EI_concat_struct_contentsize = 0;
static size_t EI_concat_numeric_contentsize = 0;
static size_t EI_concat_string_contentsize = 0;


ZL_Report EI_concat(ZL_Encoder* eictx, const ZL_Input* ins[], size_t nbIns)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    ZL_ASSERT_GE(nbIns, 1);
    ZL_ASSERT_NN(ins);
    size_t nbElts   = 0;
    ZL_Type type    = ZL_Input_type(ins[0]);
    size_t eltWidth = ZL_Input_eltWidth(ins[0]);
    for (size_t n = 0; n < nbIns; n++) {
        ZL_ASSERT_NN(ins[n]);
        ZL_RET_R_IF_NE(
                node_unexpected_input_type,
                ZL_Input_type(ins[n]),
                type,
                "Concat types must be homogenous");
        ZL_RET_R_IF_NE(
                node_unexpected_input_type,
                ZL_Input_eltWidth(ins[n]),
                eltWidth,
                "Concat widths must be homogenous");

        ZL_RET_R_IF_GE(
                node_invalid_input, ZL_Input_numElts(ins[n]), UINT32_MAX);
        ZL_RET_R_IF(
                node_invalid_input,
                ZL_overflowAddST(nbElts, ZL_Input_numElts(ins[n]), &nbElts));
    }
    size_t eltsCapacity = nbElts;
    if (type == ZL_Type_string) {
        eltWidth     = 1;
        eltsCapacity = 0;
        for (size_t n = 0; n < nbIns; n++) {
            ZL_RET_R_IF(
                    node_invalid_input,
                    ZL_overflowAddST(
                            eltsCapacity,
                            ZL_Input_contentSize(ins[n]),
                            &eltsCapacity));
        }
    }

    ZL_Output* const sizes = ZL_Encoder_createTypedStream(eictx, 0, nbIns, 4);
    ZL_RET_R_IF_NULL(allocation, sizes);
    ZL_Output* const out =
            ZL_Encoder_createTypedStream(eictx, 1, eltsCapacity, eltWidth);
    ZL_RET_R_IF_NULL(allocation, out);

    uint8_t* outPtr = (uint8_t*)ZL_Output_ptr(out);
    ZL_ASSERT_NN(outPtr);
    uint8_t* const outEnd    = outPtr + eltsCapacity * eltWidth;
    uint32_t* const sizesPtr = (uint32_t*)ZL_Output_ptr(sizes);
    ZL_ASSERT_NN(sizesPtr);

    for (size_t n = 0; n < nbIns; n++) {
        sizesPtr[n] = (uint32_t)ZL_Input_numElts(ins[n]);
        size_t size = sizesPtr[n] * eltWidth;
        if (type == ZL_Type_string) {
            size = (uint32_t)ZL_Input_contentSize(ins[n]);
        }
        if (size > 0) {
            ZL_ASSERT_NN(ZL_Input_ptr(ins[n]));
            ZL_memcpy(outPtr, ZL_Input_ptr(ins[n]), size);
        }
        outPtr += size;
    }

    if (type == ZL_Type_string) {
        uint32_t* wptr = ZL_Output_reserveStringLens(out, nbElts);
        size_t rPos    = 0;
        for (size_t n = 0; n < nbIns; n++) {
            size_t size = ZL_Input_numElts(ins[n]);
            if (size > 0) {
                ZL_ASSERT_NN(ZL_Input_stringLens(ins[n]));
                ZL_memcpy(
                        wptr + rPos,
                        ZL_Input_stringLens(ins[n]),
                        size * sizeof(uint32_t));
            }
            rPos += size;
        }
    }
    ZL_ASSERT_EQ(outPtr, outEnd);
    (void)outEnd;

    ZL_RET_R_IF_ERR(ZL_Output_commit(out, nbElts));
    ZL_RET_R_IF_ERR(ZL_Output_commit(sizes, nbIns));

    clock_gettime(CLOCK_MONOTONIC, &end);
    double start_time_ms = (double)start.tv_sec * 1000.0 + (double)start.tv_nsec / 1000000.0;
    double end_time_ms = (double)end.tv_sec * 1000.0 + (double)end.tv_nsec / 1000000.0;

    double current_duration = end_time_ms - start_time_ms;

    switch (type){
        case ZL_Type_serial:
            EI_concat_serial_time_ms += current_duration;
            EI_concat_serial_contentsize += nbElts * eltWidth;
            fprintf(stderr, "[EI_concat_serial] Start: %.4f ms | End: %.4f ms | Accumulated: %.4f ms | Current_duration: %.4f | content_size: %zu\n", start_time_ms, end_time_ms, EI_concat_serial_time_ms, current_duration, EI_concat_serial_contentsize);
            break;
        case ZL_Type_struct:
            EI_concat_struct_time_ms += current_duration;
            EI_concat_struct_contentsize += nbElts * eltWidth;
            fprintf(stderr, "[EI_concat_struct] Start: %.4f ms | End: %.4f ms | Accumulated: %.4f ms | Current_duration: %.4f | content_size: %zu\n", start_time_ms, end_time_ms, EI_concat_struct_time_ms, current_duration, EI_concat_struct_contentsize);
            break;
        case ZL_Type_numeric:
            EI_concat_numeric_time_ms += current_duration;
            EI_concat_numeric_contentsize += nbElts * eltWidth;
            fprintf(stderr, "[EI_concat_numeric] Start: %.4f ms | End: %.4f ms | Accumulated: %.4f ms | Current_duration: %.4f | content_size: %zu\n", start_time_ms, end_time_ms, EI_concat_numeric_time_ms, current_duration, EI_concat_numeric_contentsize);
            break;
        case ZL_Type_string:
            EI_concat_string_time_ms += current_duration;
            EI_concat_string_contentsize += eltsCapacity;
            fprintf(stderr, "[EI_concat_string] Start: %.4f ms | End: %.4f ms | Accumulated: %.4f ms | Current_duration: %.4f | content_size: %zu\n", start_time_ms, end_time_ms, EI_concat_string_time_ms, current_duration, EI_concat_string_contentsize);
            break;
    }
    return ZL_returnSuccess();
}
