/* SPDX-License-Identifier: MIT */


#include "tpm2.hpp"
#include "fd.hpp"
#include "main.hpp"
#include "parse.hpp"

#include <algorithm>
#include <inttypes.h>
#include <time.h>


template <class F>
static int try_or_passphrase(const char * what, const char * what_for, ESYS_CONTEXT * tpm2_ctx, TPM2_RC valid_error, ESYS_TR passphrased_object, F && func) {
	auto err = func();
	for(int i = 0; err == TPM2_RC_9 + valid_error && i < 3; ++i) {
		if(i)
			fprintf(stderr, "Couldn't %s: %s\n", what, Tss2_RC_Decode(err));

		uint8_t * pass{};
		size_t pass_len{};
		TRY_MAIN(read_known_passphrase(what_for, pass, pass_len, sizeof(TPM2B_AUTH::buffer)));
		quickscope_wrapper pass_deleter{[&] { free(pass); }};

		TPM2B_AUTH auth{};
		auth.size = pass_len;
		memcpy(auth.buffer, pass, auth.size);

		TRY_TPM2("set passphrase", Esys_TR_SetAuth(tpm2_ctx, passphrased_object, &auth));
		err = func();
	}

	// TRY_TPM2() unrolled because no constexpr/string-literal-template arguments until C++20, which is not supported by GCC 8, which we need for Buster
	if(err != TPM2_RC_SUCCESS)
		return fprintf(stderr, "Couldn't %s: %s\n", what, Tss2_RC_Decode(err)), __LINE__;
	return 0;
}


TPM2B_DATA tpm2_creation_metadata(const char * dataset_name) {
	TPM2B_DATA metadata{}; // 64 bytesish

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	metadata.size = snprintf((char *)metadata.buffer, sizeof(metadata.buffer), "%" PRIu64 ".%09ld %s %s", ts.tv_sec, ts.tv_nsec, dataset_name, TZPFMS_VERSION) + 1;
	metadata.size = metadata.size > sizeof(metadata.buffer) ? sizeof(metadata.buffer) : metadata.size;

	// fprintf(stderr, "%" PRIu16 "/%zu: \"%s\"\n", metadata.size, sizeof(metadata.buffer), metadata.buffer);
	return metadata;
}


int tpm2_parse_handle(const char * dataset_name, const char * handle_s, TPMI_DH_PERSISTENT & handle) {
	if(parse_int(handle_s, handle))
		return fprintf(stderr, "Dataset %s's handle %s not valid.\n", dataset_name, handle_s), __LINE__;

	return 0;
}


int tpm2_generate_rand(ESYS_CONTEXT * tpm2_ctx, void * into, size_t length) {
	TPM2B_DIGEST * rand{};
	TRY_TPM2("get random data from TPM", Esys_GetRandom(tpm2_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, length, &rand));
	quickscope_wrapper rand_deleter{[=] { Esys_Free(rand); }};

	if(rand->size != length)
		return fprintf(stderr, "Wrong random size: wanted %zu, got %" PRIu16 " bytes.\n", length, rand->size), __LINE__;

	memcpy(into, rand->buffer, length);
	return 0;
}


static int tpm2_find_unused_persistent_non_platform(ESYS_CONTEXT * tpm2_ctx, TPMI_DH_PERSISTENT & persistent_handle) {
	TPMS_CAPABILITY_DATA * cap;  // TODO: check for more data?
	TRY_TPM2("Read used persistent TPM handles", Esys_GetCapability(tpm2_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, TPM2_CAP_HANDLES, TPM2_PERSISTENT_FIRST,
	                                                                TPM2_MAX_CAP_HANDLES, nullptr, &cap));
	quickscope_wrapper cap_deleter{[=] { Esys_Free(cap); }};

	persistent_handle = 0;
	switch(cap->data.handles.count) {
		case 0:
			persistent_handle = TPM2_PERSISTENT_FIRST;
			break;
		case TPM2_MAX_CAP_HANDLES:
			break;
		default:
			for(TPM2_HC i = TPM2_PERSISTENT_FIRST; i <= TPM2_PERSISTENT_LAST && i <= TPM2_PLATFORM_PERSISTENT; ++i)
				if(std::find(std::begin(cap->data.handles.handle), std::end(cap->data.handles.handle), i) == std::end(cap->data.handles.handle)) {
					persistent_handle = i;
					break;
				}
	}

	if(!persistent_handle)
		return fprintf(stderr, "All %zu persistent handles allocated! We're fucked!\n", TPM2_MAX_CAP_HANDLES), __LINE__;
	return 0;
}

int tpm2_seal(const char * dataset, ESYS_CONTEXT * tpm2_ctx, ESYS_TR tpm2_session, TPMI_DH_PERSISTENT & persistent_handle, const TPM2B_DATA & metadata,
              void * data, size_t data_len) {
	ESYS_TR primary_handle = ESYS_TR_NONE;
	quickscope_wrapper primary_handle_deleter{[&] { Esys_FlushContext(tpm2_ctx, primary_handle); }};

	{
		const TPM2B_SENSITIVE_CREATE primary_sens{};

		// Adapted from tpm2-tss-3.0.1/test/integration/esys-create-primary-hmac.int.c
		TPM2B_PUBLIC pub{};
		pub.publicArea.type             = TPM2_ALG_RSA;
		pub.publicArea.nameAlg          = TPM2_ALG_SHA1;
		pub.publicArea.objectAttributes = TPMA_OBJECT_USERWITHAUTH | TPMA_OBJECT_RESTRICTED | TPMA_OBJECT_DECRYPT | TPMA_OBJECT_FIXEDTPM | TPMA_OBJECT_FIXEDPARENT |
		                                  TPMA_OBJECT_SENSITIVEDATAORIGIN;
		pub.publicArea.parameters.rsaDetail.symmetric.algorithm   = TPM2_ALG_AES;
		pub.publicArea.parameters.rsaDetail.symmetric.keyBits.aes = 128;
		pub.publicArea.parameters.rsaDetail.symmetric.mode.aes    = TPM2_ALG_CFB;
		pub.publicArea.parameters.rsaDetail.scheme.scheme         = TPM2_ALG_NULL;
		pub.publicArea.parameters.rsaDetail.keyBits               = 2048;
		pub.publicArea.parameters.rsaDetail.exponent              = 0;

		const TPML_PCR_SELECTION pcrs{};

		TPM2B_PUBLIC * public_ret{};
		TPM2B_CREATION_DATA * creation_data{};
		TPM2B_DIGEST * creation_hash{};
		TPMT_TK_CREATION * creation_ticket{};
		TRY_MAIN(try_or_passphrase("create primary encryption key", "TPM2 owner hierarchy", tpm2_ctx, TPM2_RC_BAD_AUTH, ESYS_TR_RH_OWNER, [&] {
			return Esys_CreatePrimary(tpm2_ctx, ESYS_TR_RH_OWNER, tpm2_session, ESYS_TR_NONE, ESYS_TR_NONE, &primary_sens, &pub, &metadata, &pcrs, &primary_handle,
			                          &public_ret, &creation_data, &creation_hash, &creation_ticket);
		}));
		quickscope_wrapper creation_ticket_deleter{[=] { Esys_Free(creation_ticket); }};
		quickscope_wrapper creation_hash_deleter{[=] { Esys_Free(creation_hash); }};
		quickscope_wrapper creation_data_deleter{[=] { Esys_Free(creation_data); }};
		quickscope_wrapper public_ret_deleter{[=] { Esys_Free(public_ret); }};

		// TSS2_RC Esys_CertifyCreation 	( 	ESYS_CONTEXT *  	esysContext,
		//		ESYS_TR  	signHandle,
		//		ESYS_TR  	objectHandle,
		//		ESYS_TR  	shandle1,
		//		ESYS_TR  	shandle2,
		//		ESYS_TR  	shandle3,
		//		const TPM2B_DATA *  	qualifyingData,
		//		const TPM2B_DIGEST *  	creationHash,
		//		const TPMT_SIG_SCHEME *  	inScheme,
		//		const TPMT_TK_CREATION *  	creationTicket,
		//		TPM2B_ATTEST **  	certifyInfo,
		//		TPMT_SIGNATURE **  	signature
		//	)
	}


	TPM2B_PRIVATE * sealant_private{};
	TPM2B_PUBLIC * sealant_public{};
	quickscope_wrapper sealant_public_deleter{[=] { Esys_Free(sealant_public); }};
	quickscope_wrapper sealant_private_deleter{[=] { Esys_Free(sealant_private); }};

	/// This is the object with the actual sealed data in it
	{
		TPM2B_SENSITIVE_CREATE secret_sens{};
		secret_sens.sensitive.data.size = data_len;
		memcpy(secret_sens.sensitive.data.buffer, data, secret_sens.sensitive.data.size);

		{
			char what_for[ZFS_MAX_DATASET_NAME_LEN + 38 + 1];
			snprintf(what_for, sizeof(what_for), "%s TPM2 wrapping key (or empty for none)", dataset);

			uint8_t * passphrase{};
			size_t passphrase_len{};
			TRY_MAIN(read_new_passphrase("%s TPM2 wrapping key (or empty for none)", passphrase, passphrase_len,
			                             sizeof(TPM2B_SENSITIVE_CREATE::sensitive.userAuth.buffer)));
			quickscope_wrapper passphrase_deleter{[&] { free(passphrase); }};

			secret_sens.sensitive.userAuth.size = passphrase_len;
			memcpy(secret_sens.sensitive.userAuth.buffer, passphrase, secret_sens.sensitive.userAuth.size);
		}

		// Same args as tpm2-tools' tpm2_create(1)
		TPM2B_PUBLIC pub{};
		pub.publicArea.type                                     = TPM2_ALG_KEYEDHASH;
		pub.publicArea.nameAlg                                  = TPM2_ALG_SHA256;
		pub.publicArea.objectAttributes                         = TPMA_OBJECT_FIXEDTPM | TPMA_OBJECT_FIXEDPARENT | TPMA_OBJECT_USERWITHAUTH;
		pub.publicArea.parameters.keyedHashDetail.scheme.scheme = TPM2_ALG_NULL;

		const TPML_PCR_SELECTION pcrs{};

		TPM2B_CREATION_DATA * creation_data{};
		TPM2B_DIGEST * creation_hash{};
		TPMT_TK_CREATION * creation_ticket{};
		TRY_TPM2("create key seal", Esys_Create(tpm2_ctx, primary_handle, tpm2_session, ESYS_TR_NONE, ESYS_TR_NONE, &secret_sens, &pub, &metadata, &pcrs,
		                                        &sealant_private, &sealant_public, &creation_data, &creation_hash, &creation_ticket));
		quickscope_wrapper creation_ticket_deleter{[=] { Esys_Free(creation_ticket); }};
		quickscope_wrapper creation_hash_deleter{[=] { Esys_Free(creation_hash); }};
		quickscope_wrapper creation_data_deleter{[=] { Esys_Free(creation_data); }};
	}

	ESYS_TR sealed_handle = ESYS_TR_NONE;
	quickscope_wrapper sealed_handle_deleter{[&] { Esys_FlushContext(tpm2_ctx, sealed_handle); }};

	/// Load the sealed object (keyedhash) into a transient handle
	TRY_TPM2("load key seal", Esys_Load(tpm2_ctx, primary_handle, tpm2_session, ESYS_TR_NONE, ESYS_TR_NONE, sealant_private, sealant_public, &sealed_handle));

	/// Find lowest unused persistent handle
	TRY_MAIN(tpm2_find_unused_persistent_non_platform(tpm2_ctx, persistent_handle));

	/// Persist the loaded handle in the TPM — this will make it available as $persistent_handle until we explicitly evict it back to the transient store
	{
		// Can't be flushed (tpm:parameter(1):value is out of range or is not correct for the context), plus, that's kinda the point
		ESYS_TR new_handle;
		TRY_TPM2("persist key seal",
		         Esys_EvictControl(tpm2_ctx, ESYS_TR_RH_OWNER, sealed_handle, tpm2_session, ESYS_TR_NONE, ESYS_TR_NONE, persistent_handle, &new_handle));
	}

	return 0;
}

int tpm2_unseal(const char * dataset, ESYS_CONTEXT * tpm2_ctx, ESYS_TR tpm2_session, TPMI_DH_PERSISTENT persistent_handle, void * data, size_t data_len) {
	char what_for[ZFS_MAX_DATASET_NAME_LEN + 18 + 1];
	snprintf(what_for, sizeof(what_for), "%s TPM2 wrapping key", dataset);

	// Entirely fake and not flushable (tpm:parameter(1):value is out of range or is not correct for the context)
	ESYS_TR pandle;
	TRY_TPM2("convert persistent handle to object", Esys_TR_FromTPMPublic(tpm2_ctx, persistent_handle, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, &pandle));

	TPM2B_SENSITIVE_DATA * unsealed{};
	quickscope_wrapper unsealed_deleter{[=] { Esys_Free(unsealed); }};
	TRY_MAIN(try_or_passphrase("unseal wrapping key", what_for, tpm2_ctx, TPM2_RC_AUTH_FAIL, pandle,
	                           [&] { return Esys_Unseal(tpm2_ctx, pandle, tpm2_session, ESYS_TR_NONE, ESYS_TR_NONE, &unsealed); }));

	if(unsealed->size != data_len)
		return fprintf(stderr, "Unsealed data has wrong length %" PRIu16 ", expected %zu!\n", unsealed->size, data_len), __LINE__;
	memcpy(data, unsealed->buffer, data_len);
	return 0;
}

int tpm2_free_persistent(ESYS_CONTEXT * tpm2_ctx, ESYS_TR tpm2_session, TPMI_DH_PERSISTENT persistent_handle) {
	// Neither of these are flushable (tpm:parameter(1):value is out of range or is not correct for the context)
	ESYS_TR pandle;
	TRY_TPM2("convert persistent handle to object", Esys_TR_FromTPMPublic(tpm2_ctx, persistent_handle, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, &pandle));

	ESYS_TR new_handle;
	TRY_MAIN(try_or_passphrase("unpersist object", "TPM2 owner hierarchy", tpm2_ctx, TPM2_RC_BAD_AUTH, ESYS_TR_RH_OWNER,
	                           [&] { return Esys_EvictControl(tpm2_ctx, ESYS_TR_RH_OWNER, pandle, tpm2_session, ESYS_TR_NONE, ESYS_TR_NONE, 0, &new_handle); }));

	return 0;
}
