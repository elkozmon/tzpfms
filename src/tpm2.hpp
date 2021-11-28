/* SPDX-License-Identifier: MIT */


#pragma once


#include "common.hpp"

#include <tss2/tss2_common.h>
#include <tss2/tss2_esys.h>
#include <tss2/tss2_rc.h>


#define TRY_TPM2(what, ...) TRY_GENERIC(what, , != TPM2_RC_SUCCESS, _try_ret, __LINE__, Tss2_RC_Decode, __VA_ARGS__)


// https://github.com/tpm2-software/tpm2-tss/blob/49146d926ccb0fd3c3ee064455eb02356e0cdf90/test/integration/esys-create-session-auth.int.c#L218
static const constexpr TPMT_SYM_DEF tpm2_session_key{.algorithm = TPM2_ALG_AES, .keyBits = {.aes = 128}, .mode = {.aes = TPM2_ALG_CFB}};


template <class F>
int with_tpm2_session(F && func) {
	// https://trustedcomputinggroup.org/wp-content/uploads/TSS_ESAPI_v1p00_r05_pubrev.pdf
	// mainly "3.4. The ESAPI Session" and "3.5. ESAPI Use Model"
	// https://tpm2-tss.readthedocs.io/en/latest/group___e_s_y_s___c_o_n_t_e_x_t.html

	ESYS_CONTEXT * tpm2_ctx{};
	TRY_TPM2("initialise TPM connection", Esys_Initialize(&tpm2_ctx, nullptr, nullptr));
	quickscope_wrapper tpm2_ctx_deleter{[&] { Esys_Finalize(&tpm2_ctx); }};

	TRY_TPM2("start TPM", Esys_Startup(tpm2_ctx, TPM2_SU_CLEAR));

	ESYS_TR tpm2_session = ESYS_TR_NONE;
	quickscope_wrapper tpm2_session_deleter{[&] { Esys_FlushContext(tpm2_ctx, tpm2_session); }};

	TRY_TPM2("authenticate with TPM", Esys_StartAuthSession(tpm2_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, nullptr, TPM2_SE_HMAC,
	                                                        &tpm2_session_key, TPM2_ALG_SHA256, &tpm2_session));

	return func(tpm2_ctx, tpm2_session);
}

extern TPM2B_DATA tpm2_creation_metadata(const char * dataset_name);

/// Parse a persistent handle name as stored in a ZFS property
extern int tpm2_parse_prop(const char * dataset_name, char * handle_s, TPMI_DH_PERSISTENT & handle, TPML_PCR_SELECTION * pcrs);
extern int tpm2_unparse_prop(TPMI_DH_PERSISTENT persistent_handle, const TPML_PCR_SELECTION & pcrs, char ** prop);

/// `alg:PCR[,PCR]...[+alg:PCR[,PCR]...]...`; all separators can have spaces
extern int tpm2_parse_pcrs(char * arg, TPML_PCR_SELECTION & pcrs);

extern int tpm2_generate_rand(ESYS_CONTEXT * tpm2_ctx, void * into, size_t length);
extern int tpm2_seal(const char * dataset, ESYS_CONTEXT * tpm2_ctx, ESYS_TR tpm2_session, TPMI_DH_PERSISTENT & persistent_handle, const TPM2B_DATA & metadata,
                     const TPML_PCR_SELECTION & pcrs, bool allow_PCR_or_pass, void * data, size_t data_len);
extern int tpm2_unseal(const char * dataset, ESYS_CONTEXT * tpm2_ctx, ESYS_TR tpm2_session, TPMI_DH_PERSISTENT persistent_handle,
                       const TPML_PCR_SELECTION & pcrs, void * data, size_t data_len);
extern int tpm2_free_persistent(ESYS_CONTEXT * tpm2_ctx, ESYS_TR tpm2_session, TPMI_DH_PERSISTENT persistent_handle);
