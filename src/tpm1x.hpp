/* SPDX-License-Identifier: MIT */


#pragma once


#include "common.hpp"
#include "fd.hpp"
#include "main.hpp"

#include <stdlib.h>

#include <tss/platform.h>
#include <tss/tspi.h>
#include <tss/tss_structs.h>
#include <tss/tss_typedef.h>

#include <trousers/trousers.h>


#define TRY_TPM1X(what, ...) TRY_GENERIC(what, , != TPM_SUCCESS, _try_ret, __LINE__, Trspi_Error_String, __VA_ARGS__)


/// Used as default secret if passphrase wasn't provided for wrapping key for the sealed object
// I just got this out of /dev/random, for greppers: CE4CF677875B5EB8993591D5A9AF1ED24A3A8736
static const constexpr uint8_t parent_key_secret[TPM_SHA1_160_HASH_LEN]{0xCE, 0x4C, 0xF6, 0x77, 0x87, 0x5B, 0x5E, 0xB8, 0x99, 0x35,
                                                                        0x91, 0xD5, 0xA9, 0xAF, 0x1E, 0xD2, 0x4A, 0x3A, 0x87, 0x36};


template <class F>
int with_tpm1x_session(F && func) {
	TSS_HCONTEXT ctx{};  // All memory lives as long as this does
	TRY_TPM1X("create TPM context", Tspi_Context_Create(&ctx));

	{
		UNICODE * tcs_address{};
		quickscope_wrapper tcs_address_deleter{[&] { free(tcs_address); }};
		if(auto addr = getenv("TZPFMS_TPM1X"))
			tcs_address = reinterpret_cast<UNICODE *>(TRY_PTR("allocate remote TPM address", Trspi_Native_To_UNICODE(reinterpret_cast<BYTE *>(addr), nullptr)));
		TRY_TPM1X("connect TPM context to TPM", Tspi_Context_Connect(ctx, tcs_address));
	}
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

/// Try to run func() with the current policy; if it fails, prompt for passphrase and reattempt up to three total times.
template <class F>
int try_policy_or_passphrase(const char * what, const char * what_for, TSS_HPOLICY policy, F && func) {
	auto err = func();
	// Equivalent to TSS_ERROR_LAYER(err) == TSS_LAYER_TPM && TSS_ERROR_CODE(err) == TPM_E_AUTHFAIL
	for(int i = 0; ((err & TSS_LAYER_TSP) == TSS_LAYER_TPM && (err & TSS_MAX_ERROR) == TPM_E_AUTHFAIL) && i < 3; ++i) {
		if(i)
			fprintf(stderr, "Couldn't %s: %s\n", what, Trspi_Error_String(err));

		BYTE * pass{};
		size_t pass_len{};
		TRY_MAIN(read_known_passphrase(what_for, pass, pass_len));
		quickscope_wrapper pass_deleter{[&] { free(pass); }};

		TRY_TPM1X("set passphrase secret on policy", Tspi_Policy_SetSecret(policy, TSS_SECRET_MODE_PLAIN, pass_len, pass));
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

/// Create sealed object, assign a policy and a known secret to it.
extern int tpm1x_prep_sealed_object(TSS_HCONTEXT ctx, TSS_HOBJECT & sealed_object, TSS_HPOLICY & sealed_object_policy);
