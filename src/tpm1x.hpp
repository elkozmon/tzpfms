/* SPDX-License-Identifier: MIT */


#pragma once


#include "common.hpp"

#include <tss/platform.h>
#include <tss/tspi.h>
#include <tss/tss_structs.h>
#include <tss/tss_typedef.h>

#include <trousers/trousers.h>


#define TRY_TPM1X(what, ...) TRY_GENERIC(what, , != TPM_SUCCESS, _try_ret, __LINE__, Trspi_Error_String, __VA_ARGS__)


template <class F>
int with_tpm1x_session(F && func) {
	TSS_HCONTEXT ctx{};  // All memory lives as long as this does
	TRY_TPM1X("create TPM context", Tspi_Context_Create(&ctx));
	TRY_TPM1X("connect TPM context to TPM", Tspi_Context_Connect(ctx, nullptr));
	quickscope_wrapper ctx_deleter{[&] {
		Trspi_Error_String(Tspi_Context_FreeMemory(ctx, nullptr));
		Trspi_Error_String(Tspi_Context_Close(ctx));
	}};


	TSS_HOBJECT srk{};
	TRY_TPM1X("load SRK", Tspi_Context_LoadKeyByUUID(ctx, TSS_PS_TYPE_SYSTEM, TSS_UUID_SRK, &srk));

	TSS_HPOLICY srk_policy{};
	TRY_TPM1X("get SRK policy", Tspi_GetPolicyObject(srk, TSS_POLICY_USAGE, &srk_policy));
	TRY_TPM1X("assign SRK policy", Tspi_Policy_AssignToObject(srk_policy, srk));
	quickscope_wrapper srk_policy_deleter{[&] { Trspi_Error_String(Tspi_Policy_FlushSecret(srk_policy)); }};

	{
		uint8_t well_known[TPM_SHA1_160_HASH_LEN]{};
		TRY_TPM1X("set well-known secret on SRK policy", Tspi_Policy_SetSecret(srk_policy, TSS_SECRET_MODE_SHA1, sizeof(well_known), well_known));
	}

	return func(ctx, srk, srk_policy);
}

/// Try to run func() with the current SRK policy (well-known by default); if it fails, prompt for password and reattempt.
template <class F>
int try_srk(const char * what, TSS_HPOLICY srk_policy, F && func) {
	auto err = func();
	// Equivalent to TSS_ERROR_LAYER(err) == TSS_LAYER_TPM && TSS_ERROR_CODE(err) == TPM_E_AUTHFAIL
	if((err & TSS_LAYER_TSP) == TSS_LAYER_TPM && (err & TSS_MAX_ERROR) == TPM_E_AUTHFAIL) {
		// TODO: read SRK password from stdin here
		TRY_TPM1X("set password secret on SRK policy", Tspi_Policy_SetSecret(srk_policy, TSS_SECRET_MODE_PLAIN, strlen("dupanina"), (BYTE *)"dupanina"));

		err = func();
	}

	// TRY_TPM1X() unrolled because no constexpr/string-literal-template arguments until C++20, which is not supported by GCC 8, which we need for Buster
	if(err != TPM_SUCCESS) {
		fprintf(stderr, "Couldn't %s: %s\n", what, Trspi_Error_String(err));
		return __LINE__;
	}

	return 0;
}


struct tpm1x_handle {
	uint8_t * parent_key_blob;
	uint8_t * sealed_object_blob;
	size_t parent_key_blob_len;
	size_t sealed_object_blob_len;

	~tpm1x_handle();
};

/// Parse handle blobs as stored in a ZFS property
///
/// The stored handle is in the form [%X:%X] where the first blob is the parent key and the second is the sealed data.
extern int tpm1x_parse_handle(const char * dataset_name, char * handle_s, tpm1x_handle & handle);


extern int tpm1x_prep_sealed_object(TSS_HCONTEXT ctx, TSS_HOBJECT & sealed_object, TSS_HPOLICY & sealed_object_policy);
// extern int tpm2_seal(ESYS_CONTEXT * tpm2_ctx, ESYS_TR tpm2_session, TPMI_DH_PERSISTENT & persistent_handle, const TPM2B_DATA & metadata, void * data,
//                      size_t data_len);
// extern int tpm2_unseal(ESYS_CONTEXT * tpm2_ctx, ESYS_TR tpm2_session, TPMI_DH_PERSISTENT persistent_handle, void * data, size_t data_len);
// extern int tpm2_free_persistent(ESYS_CONTEXT * tpm2_ctx, ESYS_TR tpm2_session, TPMI_DH_PERSISTENT persistent_handle);
