/* SPDX-License-Identifier: MIT */


#pragma once


#include "common.hpp"

#include <tss2/tss2_common.h>
#include <tss2/tss2_esys.h>
#include <tss2/tss2_rc.h>


#define TRY_TPM2(what, ...) TRY_GENERIC(what, , != TPM2_RC_SUCCESS, _try_ret, __LINE__, Tss2_RC_Decode, __VA_ARGS__)


template <class F>
int with_tpm2_session(F && func) {
	// https://trustedcomputinggroup.org/wp-content/uploads/TSS_ESAPI_v1p00_r05_pubrev.pdf
	// mainly "3.4. The ESAPI Session" and "3.5. ESAPI Use Model"
	// https://tpm2-tss.readthedocs.io/en/latest/group___e_s_y_s___c_o_n_t_e_x_t.html

	ESYS_CONTEXT * tpm2_ctx{};
	TRY_TPM2("Esys_Initialize()", Esys_Initialize(&tpm2_ctx, nullptr, nullptr));
	quickscope_wrapper tpm2_ctx_deleter{[&] { Esys_Finalize(&tpm2_ctx); }};

	TRY_TPM2("Esys_Startup()", Esys_Startup(tpm2_ctx, TPM2_SU_CLEAR));

	ESYS_TR tpm2_session = ESYS_TR_NONE;
	quickscope_wrapper tpm2_session_deleter{[&] { Esys_FlushContext(tpm2_ctx, tpm2_session); }};

	{
		// https://github.com/tpm2-software/tpm2-tss/blob/master/test/integration/esys-create-session-auth.int.c#L218
		TPMT_SYM_DEF session_key{};
		session_key.algorithm   = TPM2_ALG_AES;
		session_key.keyBits.aes = 128;
		session_key.mode.aes    = TPM2_ALG_CFB;
		TRY_TPM2("Esys_StartAuthSession()", Esys_StartAuthSession(tpm2_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, nullptr,
		                                                          TPM2_SE_HMAC, &session_key, TPM2_ALG_SHA512, &tpm2_session));
	}

	return func(tpm2_ctx, tpm2_session);
}

extern TPM2B_DATA tpm2_creation_metadata(const char * dataset_name);

extern int tpm2_generate_rand(ESYS_CONTEXT * tpm2_ctx, void * into, size_t length);
extern int tpm2_seal(ESYS_CONTEXT * tpm2_ctx, ESYS_TR tpm2_session, TPMI_DH_PERSISTENT & persistent_handle, const TPM2B_DATA & metadata, void * data, size_t data_len);
extern int tpm2_unseal(ESYS_CONTEXT * tpm2_ctx, ESYS_TR tpm2_session, TPMI_DH_PERSISTENT persistent_handle, void * data, size_t data_len);
extern int tpm2_free_persistent(ESYS_CONTEXT * tpm2_ctx, ESYS_TR tpm2_session, TPMI_DH_PERSISTENT persistent_handle);
