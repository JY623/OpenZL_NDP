// Copyright (c) Meta Platforms, Inc. and affiliates.
#include "openzl/codecs/interleave/encode_interleave_binding.h"

#include "openzl/codecs/interleave/common_interleave.h"
#include "openzl/zl_errors.h"

#include <stdio.h>
#include <time.h>
static double EI_interleave_time_ms = 0.0;

ZL_Report EI_interleave(ZL_Encoder* eictx, const ZL_Input* ins[], size_t nbIns)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    ZL_RESULT_DECLARE_SCOPE_REPORT(eictx);

    ZL_ERR_IF_EQ(nbIns, 0, node_invalid_input, "Need at least one input");
    ZL_ERR_IF_GT(
            nbIns,
            ZL_INTERLEAVE_MAX_INPUTS,
            node_invalid_input,
            "Too many inputs. Only support up to 512 inputs for now");
    for (size_t i = 0; i < nbIns; i++) {
        ZL_RET_R_IF_NULL(node_invalid_input, ins[i]);
        ZL_ERR_IF_NE(
                ZL_Input_type(ins[i]),
                ZL_Type_string,
                temporaryLibraryLimitation,
                "Only string inputs are supported");
    }
    size_t totSize        = 0;
    size_t nbStrsPerInput = ZL_Input_numElts(ins[0]);

    for (size_t i = 0; i < nbIns; i++) {
        totSize += ZL_Input_contentSize(ins[i]);
        ZL_ERR_IF_NE(
                ZL_Input_numElts(ins[i]),
                nbStrsPerInput,
                node_invalid_input,
                "All inputs must have the same number of strings");
    }
    const uint32_t nbInsU32 = (uint32_t)nbIns;
    ZL_Encoder_sendCodecHeader(eictx, &nbInsU32, sizeof(nbInsU32));
    ZL_Output* out = ZL_Encoder_createStringStream(
            eictx, 0, nbStrsPerInput * nbIns, totSize);
    ZL_RET_R_IF_NULL(allocation, out);

    char* ptr           = ZL_Output_ptr(out);
    uint32_t* strLenPtr = ZL_Output_stringLens(out);

    const char** inPtrs =
            ZL_Encoder_getScratchSpace(eictx, nbIns * sizeof(char*));
    const uint32_t** inStrLens =
            ZL_Encoder_getScratchSpace(eictx, nbIns * sizeof(uint32_t*));
    for (size_t i = 0; i < nbIns; i++) {
        inPtrs[i]    = ZL_Input_ptr(ins[i]);
        inStrLens[i] = ZL_Input_stringLens(ins[i]);
    }
    for (size_t i = 0; i < nbStrsPerInput; ++i) {
        for (size_t j = 0; j < nbIns; ++j) {
            memcpy(ptr, inPtrs[j], *inStrLens[j]);
            *strLenPtr = *inStrLens[j];
            ptr += *inStrLens[j];
            inPtrs[j] += *inStrLens[j];
            ++inStrLens[j];
            ++strLenPtr;
        }
    }
    ZL_RET_R_IF_ERR(ZL_Output_commit(out, nbStrsPerInput * nbIns));

    clock_gettime(CLOCK_MONOTONIC, &end);
    double start_time_ms = (double)start.tv_sec * 1000.0 + (double)start.tv_nsec / 1000000.0;
    double end_time_ms = (double)end.tv_sec * 1000.0 + (double)end.tv_nsec / 1000000.0;

    double current_duration = end_time_ms - start_time_ms;
    EI_interleave_time_ms += current_duration;
    fprintf(stderr, "[EI_interleave] Start: %.4f ms | End: %.4f ms | Accumulated: %.4f ms | Current_duration: %.4f\n", start_time_ms, end_time_ms, EI_interleave_time_ms, current_duration);

    return ZL_returnSuccess();
}
