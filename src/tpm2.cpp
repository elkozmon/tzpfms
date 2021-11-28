/* SPDX-License-Identifier: MIT */


#include "tpm2.hpp"
#include "fd.hpp"
#include "main.hpp"
#include "parse.hpp"

#include <algorithm>
#include <inttypes.h>
#include <openssl/sha.h>
#include <optional>
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
	TPM2B_DATA metadata{};  // 64 bytesish

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	metadata.size = snprintf((char *)metadata.buffer, sizeof(metadata.buffer), "%" PRIu64 ".%09" PRId64 " %s %s", ts.tv_sec, static_cast<int64_t>(ts.tv_nsec),
	                         dataset_name, TZPFMS_VERSION) +
	                1;
	metadata.size = metadata.size > sizeof(metadata.buffer) ? sizeof(metadata.buffer) : metadata.size;

	// fprintf(stderr, "%" PRIu16 "/%zu: \"%s\"\n", metadata.size, sizeof(metadata.buffer), metadata.buffer);
	return metadata;
}


int tpm2_parse_prop(const char * dataset_name, char * handle_s, TPMI_DH_PERSISTENT & handle, TPML_PCR_SELECTION * pcrs) {
	char * sv{};
	if(!parse_uint(handle_s = strtok_r(handle_s, ";", &sv), handle))
		return fprintf(stderr, "Dataset %s's handle %s: %s.\n", dataset_name, handle_s, strerror(errno)), __LINE__;

	if(auto p = strtok_r(nullptr, ";", &sv); p && pcrs)
		TRY_MAIN(tpm2_parse_pcrs(p, *pcrs));

	return 0;
}


/// Extension of the table used by tpm2-tools (tpm2_create et al.), which only has "s{m,ha}3_XXX", not "s{m,ha}3-XXX", and does case-sentitive comparisons
#define TPM2_HASH_ALGS_MAX_NAME_LEN 8  // sha3_512
static const constexpr struct tpm2_hash_algs_t {
	TPM2_ALG_ID alg;
	const char * names[2];
} tpm2_hash_algs[] = {{TPM2_ALG_SHA1, {"sha1"}},
                      {TPM2_ALG_SHA256, {"sha256"}},
                      {TPM2_ALG_SHA384, {"sha384"}},
                      {TPM2_ALG_SHA512, {"sha512"}},
                      {TPM2_ALG_SM3_256, {"sm3_256", "sm3-256"}},
                      {TPM2_ALG_SHA3_256, {"sha3_256", "sha3-256"}},
                      {TPM2_ALG_SHA3_384, {"sha3_384", "sha3-384"}},
                      {TPM2_ALG_SHA3_512, {"sha3_512", "sha3-512"}}};

static constexpr bool is_tpm2_hash_algs_sorted() {
	for(auto itr = std::begin(tpm2_hash_algs); itr != std::end(tpm2_hash_algs) - 1; ++itr)
		if((itr + 1)->alg < itr->alg)
			return false;
	return true;
}
static_assert(is_tpm2_hash_algs_sorted());  // for the binary_search() below

/// Assuming always != end: we always parse first
static const char * tpm2_hash_alg_name(TPM2_ALG_ID id) {
	return std::lower_bound(std::begin(tpm2_hash_algs), std::end(tpm2_hash_algs), tpm2_hash_algs_t{id, {}},
	                        [&](auto && lhs, auto && rhs) { return lhs.alg < rhs.alg; })
	    ->names[0];
}


/// Nominally:
///   #define TPM2_MAX_PCRS       32
///   #define TPM2_PCR_SELECT_MAX ((TPM2_MAX_PCRS + 7) / 8)
/// and
///   struct TPMS_PCR_SELECT {
///       UINT8 sizeofSelect;                  /* the size in octets of the pcrSelect array */
///       BYTE pcrSelect[TPM2_PCR_SELECT_MAX]; /* the bit map of selected PCR */
///   };
///
/// This works out to TPM2_PCR_SELECT_MAX=4, but most (all?) TPM2s have only 24 PCRs, meaning *any* request with sizeofSelect=sizeof(pcrSelect)=4 fails with
///   WARNING:esys:src/tss2-esys/api/Esys_CreatePrimary.c:393:Esys_CreatePrimary_Finish() Received TPM Error
///   ERROR:esys:src/tss2-esys/api/Esys_CreatePrimary.c:135:Esys_CreatePrimary() Esys Finish ErrorCode (0x000004c4)
///   Couldn't create primary encryption key: tpm:parameter(4):value is out of range or is not correct for the context
///
/// Follow tpm2-tools and pretend TPM2_MAX_PCRS=24 => TPM2_PCR_SELECT_MAX=3 => sizeofSelect=3.
#define TPM2_MAX_PCRS_BUT_STRONGER 24
#define TPM2_PCR_SELECT_MAX_BUT_STRONGER ((TPM2_MAX_PCRS_BUT_STRONGER + 7) / 8)
static_assert(TPM2_PCR_SELECT_MAX_BUT_STRONGER <= sizeof(TPMS_PCR_SELECT::pcrSelect));

int tpm2_parse_pcrs(char * arg, TPML_PCR_SELECTION & pcrs) {
	TPMS_PCR_SELECTION * bank = pcrs.pcrSelections;

	char * ph_sv{};
	for(auto per_hash = strtok_r(arg, "+", &ph_sv); per_hash; per_hash = strtok_r(nullptr, "+", &ph_sv), ++bank) {
		while(*per_hash == ' ')
			++per_hash;

		if(bank == pcrs.pcrSelections + (sizeof(pcrs.pcrSelections) / sizeof(*pcrs.pcrSelections)))  // == TPM2_NUM_PCR_BANKS
			return fprintf(stderr, "Too many PCR banks specified! Can only have up to %zu\n", sizeof(pcrs.pcrSelections) / sizeof(*pcrs.pcrSelections)), __LINE__;

		if(auto sep = strchr(per_hash, ':')) {
			*sep        = '\0';
			auto values = sep + 1;

			if(auto alg = std::find_if(
			       std::begin(tpm2_hash_algs), std::end(tpm2_hash_algs),
			       [&](auto && alg) { return std::any_of(std::begin(alg.names), std::end(alg.names), [&](auto && nm) { return nm && !strcasecmp(per_hash, nm); }); });
			   alg != std::end(tpm2_hash_algs))
				bank->hash = alg->alg;
			else {
				if(!parse_uint(per_hash, bank->hash) || !std::binary_search(std::begin(tpm2_hash_algs), std::end(tpm2_hash_algs), tpm2_hash_algs_t{bank->hash, {}},
				                                                            [&](auto && lhs, auto && rhs) { return lhs.alg < rhs.alg; })) {
					fprintf(stderr,
					        "Unknown hash algorithm %s.\n"
					        "Can be any of case-insensitive ",
					        per_hash);
					auto first = true;
					for(auto && alg : tpm2_hash_algs)
						for(auto && nm : alg.names)
							if(nm)
								fprintf(stderr, "%s%s", first ? "" : ", ", nm), first = false;
					return fputs(".\n", stderr), __LINE__;
				}
			}

			bank->sizeofSelect = TPM2_PCR_SELECT_MAX_BUT_STRONGER;
			if(!strcasecmp(values, "all"))
				memset(bank->pcrSelect, 0xFF, bank->sizeofSelect);
			else if(!strcasecmp(values, "none"))
				;  // already 0
			else {
				char * sv{};
				for(values = strtok_r(values, ", ", &sv); values; values = strtok_r(nullptr, ", ", &sv)) {
					uint8_t pcr;
					if(!parse_uint(values, pcr))
						return fprintf(stderr, "PCR %s: %s\n", values, strerror(errno)), __LINE__;
					if(pcr > TPM2_MAX_PCRS_BUT_STRONGER - 1)
						return fprintf(stderr, "PCR %s: %s, max %u\n", values, strerror(ERANGE), TPM2_MAX_PCRS_BUT_STRONGER - 1), __LINE__;

					bank->pcrSelect[pcr / 8] |= 1 << (pcr % 8);
				}
			}
		} else
			return fprintf(stderr, "PCR bank \"%s\": no algorithm; need alg:PCR[,PCR]...\n", per_hash), __LINE__;
	}

	pcrs.count = bank - pcrs.pcrSelections;
	return 0;
}

int tpm2_unparse_prop(TPMI_DH_PERSISTENT persistent_handle, const TPML_PCR_SELECTION & pcrs, char ** prop) {
	// 0xFFFFFFFF;sha3_512:00,01,02,03,04,05,06,07,08,09,10,11,12,13,14,15,16,17,18,19,20,21,22+sha3_...
	*prop = TRY_PTR("allocate property value",
	                reinterpret_cast<char *>(malloc(2 + 8 + pcrs.count * (1 + TPM2_HASH_ALGS_MAX_NAME_LEN + (TPM2_MAX_PCRS_BUT_STRONGER - 1) * 3) + 1)));

	auto cur = *prop;
	cur += sprintf(cur, "0x%" PRIX32 "", persistent_handle);

	auto pre = ';';
	for(size_t i = 0; i < pcrs.count; ++i) {
		auto && sel = pcrs.pcrSelections[i];
		*cur++      = std::exchange(pre, '+');

		auto nm     = tpm2_hash_alg_name(sel.hash);
		auto nm_len = strlen(nm);
		memcpy(cur, nm, nm_len), cur += nm_len;

		if(std::all_of(sel.pcrSelect, sel.pcrSelect + sel.sizeofSelect, [](auto b) { return b == 0x00; }))
			memcpy(cur, ":none", strlen(":none")), cur += strlen(":none");
		else if(std::all_of(sel.pcrSelect, sel.pcrSelect + sel.sizeofSelect, [](auto b) { return b == 0xFF; }))
			memcpy(cur, ":all", strlen(":all")), cur += strlen(":all");
		else {
			bool first = true;
			for(size_t j = 0; j < sel.sizeofSelect; ++j)
				for(uint8_t b = 0; b < 8; ++b)
					if(sel.pcrSelect[j] & (1 << b))
						cur += sprintf(cur, "%c%zu", std::exchange(first, false) ? ':' : ',', j * 8 + b);
		}
	}

	*cur = '\0';
	return 0;
}


int tpm2_generate_rand(ESYS_CONTEXT * tpm2_ctx, void * into, size_t length) {
	TPM2B_DIGEST * rand{};
	TRY_TPM2("get random data from TPM", Esys_GetRandom(tpm2_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, length, &rand));
	quickscope_wrapper rand_deleter{[&] { Esys_Free(rand); }};

	if(rand->size != length)
		return fprintf(stderr, "Wrong random size: wanted %zu, got %" PRIu16 " bytes.\n", length, rand->size), __LINE__;

	memcpy(into, rand->buffer, length);
	return 0;
}


static int tpm2_find_unused_persistent_non_platform(ESYS_CONTEXT * tpm2_ctx, TPMI_DH_PERSISTENT & persistent_handle) {
	TPMS_CAPABILITY_DATA * cap{};  // TODO: check for more data?
	TRY_TPM2("Read used persistent TPM handles", Esys_GetCapability(tpm2_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, TPM2_CAP_HANDLES, TPM2_PERSISTENT_FIRST,
	                                                                TPM2_MAX_CAP_HANDLES, nullptr, &cap));
	quickscope_wrapper cap_deleter{[&] { Esys_Free(cap); }};

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

template <class F>
static int tpm2_police_pcrs(ESYS_CONTEXT * tpm2_ctx, const TPML_PCR_SELECTION & pcrs, TPM2_SE session_type, F && with_session) {
	if(!pcrs.count)
		return with_session(ESYS_TR_NONE);

	TPM2B_DIGEST digested_pcrs{};
	digested_pcrs.size = SHA256_DIGEST_LENGTH;
	static_assert(sizeof(TPM2B_DIGEST::buffer) >= SHA256_DIGEST_LENGTH);

	{
		SHA256_CTX ctx;
	new_pcrs:
		std::optional<uint32_t> update_count;
		SHA256_Init(&ctx);
		auto pcrs_left = pcrs;
		while(std::any_of(pcrs_left.pcrSelections, pcrs_left.pcrSelections + pcrs_left.count,
		                  [](auto && sel) { return std::any_of(sel.pcrSelect, sel.pcrSelect + sel.sizeofSelect, [](auto b) { return b; }); })) {
			uint32_t out_upcnt{};
			TPML_PCR_SELECTION * out_sel{};
			TPML_DIGEST * out_val{};
			TRY_TPM2("read PCRs", Esys_PCR_Read(tpm2_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, &pcrs_left, &out_upcnt, &out_sel, &out_val));
			quickscope_wrapper out_deleter{[&] { Esys_Free(out_val), Esys_Free(out_sel); }};

			if(update_count && update_count != out_upcnt)
				goto new_pcrs;
			update_count = out_upcnt;

			if(!out_val->count) {  // this can happen with SHA1 disabled, for example
				auto first = true;
				fputs("No PCRs when asking for ", stderr);
				for(size_t i = 0; i < pcrs_left.count; ++i)
					if(std::any_of(pcrs_left.pcrSelections[i].pcrSelect, pcrs_left.pcrSelections[i].pcrSelect + pcrs_left.pcrSelections[i].sizeofSelect,
					               [](auto b) { return b; }))
						fprintf(stderr, "%s%s", std::exchange(first, false) ? "" : ", ", tpm2_hash_alg_name(pcrs_left.pcrSelections[i].hash));
				return fputs(": does the TPM support the algorithm?\n", stderr), __LINE__;
			}

			for(size_t i = 0; i < out_val->count; ++i)
				SHA256_Update(&ctx, out_val->digests[i].buffer, out_val->digests[i].size);

			for(size_t i = 0; i < out_sel->count; ++i)
				for(size_t j = 0u; j < out_sel->pcrSelections[i].sizeofSelect; ++j)
					pcrs_left.pcrSelections[i].pcrSelect[j] &= ~out_sel->pcrSelections[i].pcrSelect[j];
		}

		SHA256_Final(digested_pcrs.buffer, &ctx);
	}


	ESYS_TR pcr_session = ESYS_TR_NONE;
	quickscope_wrapper tpm2_session_deleter{[&] { Esys_FlushContext(tpm2_ctx, pcr_session); }};

	TRY_TPM2("start PCR session", Esys_StartAuthSession(tpm2_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, nullptr, session_type,
	                                                    &tpm2_session_key, TPM2_ALG_SHA256, &pcr_session));


	TRY_TPM2("create PCR policy", Esys_PolicyPCR(tpm2_ctx, pcr_session, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, &digested_pcrs, &pcrs));

	return with_session(pcr_session);
}

int tpm2_seal(const char * dataset, ESYS_CONTEXT * tpm2_ctx, ESYS_TR tpm2_session, TPMI_DH_PERSISTENT & persistent_handle, const TPM2B_DATA & metadata,
              const TPML_PCR_SELECTION & pcrs, bool allow_PCR_or_pass, void * data, size_t data_len) {
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
		TRY_MAIN(try_or_passphrase("create primary encryption key", "TPM2 owner hierarchy", tpm2_ctx, TPM2_RC_BAD_AUTH, ESYS_TR_RH_OWNER, [&] {
			return Esys_CreatePrimary(tpm2_ctx, ESYS_TR_RH_OWNER, tpm2_session, ESYS_TR_NONE, ESYS_TR_NONE, &primary_sens, &pub, &metadata, &pcrs, &primary_handle,
			                          nullptr, nullptr, nullptr, nullptr);
		}));

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

	TPM2B_DIGEST policy_digest{};
	if(pcrs.count)
		TRY_MAIN(tpm2_police_pcrs(tpm2_ctx, pcrs, TPM2_SE_TRIAL, [&](auto pcr_session) {
			TPM2B_DIGEST * dgst{};
			TRY_TPM2("get PCR policy digest", Esys_PolicyGetDigest(tpm2_ctx, pcr_session, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, &dgst));
			quickscope_wrapper dgst_deleter{[&] { Esys_Free(dgst); }};
			policy_digest = *dgst;
			return 0;
		}));

	TPM2B_PRIVATE * sealant_private{};
	TPM2B_PUBLIC * sealant_public{};
	quickscope_wrapper sealant_deleter{[&] { Esys_Free(sealant_public), Esys_Free(sealant_private); }};

	/// This is the object with the actual sealed data in it
	{
		TPM2B_SENSITIVE_CREATE secret_sens{};
		secret_sens.sensitive.data.size = data_len;
		memcpy(secret_sens.sensitive.data.buffer, data, secret_sens.sensitive.data.size);

		if(!pcrs.count || allow_PCR_or_pass) {
			char what_for[ZFS_MAX_DATASET_NAME_LEN + 38 + 1];
			snprintf(what_for, sizeof(what_for), "%s TPM2 wrapping key (or empty for none)", dataset);

			uint8_t * passphrase{};
			size_t passphrase_len{};
			TRY_MAIN(read_new_passphrase(what_for, passphrase, passphrase_len, sizeof(TPM2B_SENSITIVE_CREATE::sensitive.userAuth.buffer)));
			quickscope_wrapper passphrase_deleter{[&] { free(passphrase); }};

			secret_sens.sensitive.userAuth.size = passphrase_len;
			memcpy(secret_sens.sensitive.userAuth.buffer, passphrase, secret_sens.sensitive.userAuth.size);
		}


		// Same args as tpm2-tools' tpm2_create(1)
		TPM2B_PUBLIC pub{};
		pub.publicArea.type    = TPM2_ALG_KEYEDHASH;
		pub.publicArea.nameAlg = TPM2_ALG_SHA256;
		pub.publicArea.objectAttributes =
		    TPMA_OBJECT_FIXEDTPM | TPMA_OBJECT_FIXEDPARENT | ((pcrs.count && !secret_sens.sensitive.userAuth.size) ? 0 : TPMA_OBJECT_USERWITHAUTH);
		pub.publicArea.parameters.keyedHashDetail.scheme.scheme = TPM2_ALG_NULL;
		pub.publicArea.authPolicy                               = policy_digest;

		TRY_TPM2("create key seal", Esys_Create(tpm2_ctx, primary_handle, tpm2_session, ESYS_TR_NONE, ESYS_TR_NONE, &secret_sens, &pub, &metadata, &pcrs,
		                                        &sealant_private, &sealant_public, nullptr, nullptr, nullptr));
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

int tpm2_unseal(const char * dataset, ESYS_CONTEXT * tpm2_ctx, ESYS_TR tpm2_session, TPMI_DH_PERSISTENT persistent_handle, const TPML_PCR_SELECTION & pcrs,
                void * data, size_t data_len) {
	// Esys_FlushContext(tpm2_ctx, tpm2_session);
	char what_for[ZFS_MAX_DATASET_NAME_LEN + 18 + 1];
	snprintf(what_for, sizeof(what_for), "%s TPM2 wrapping key", dataset);

	// Entirely fake and not flushable (tpm:parameter(1):value is out of range or is not correct for the context)
	ESYS_TR pandle;
	TRY_TPM2("convert persistent handle to object", Esys_TR_FromTPMPublic(tpm2_ctx, persistent_handle, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, &pandle));


	TPM2B_SENSITIVE_DATA * unsealed{};
	quickscope_wrapper unsealed_deleter{[&] { Esys_Free(unsealed); }};
	auto unseal = [&](auto sess) { return Esys_Unseal(tpm2_ctx, pandle, sess, ESYS_TR_NONE, ESYS_TR_NONE, &unsealed); };
	TRY_MAIN(tpm2_police_pcrs(tpm2_ctx, pcrs, TPM2_SE_POLICY, [&](auto pcr_session) {
		// In case there's (PCR policy || passphrase): try PCR once; if it fails, fall back to passphrase
		if(pcr_session != ESYS_TR_NONE) {
			if(auto err = unseal(pcr_session); err != TPM2_RC_SUCCESS)
				fprintf(stderr, "Couldn't %s with PCR policy: %s\n", "unseal wrapping key", Tss2_RC_Decode(err));
			else
				return 0;
		}

		return try_or_passphrase("unseal wrapping key", what_for, tpm2_ctx, TPM2_RC_AUTH_FAIL, pandle, [&] { return unseal(tpm2_session); });
	}));


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
