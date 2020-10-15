/* SPDX-License-Identifier: MIT */


#include <libzfs.h>
// #include <sys/zio_crypt.h>
#define WRAPPING_KEY_LEN 32

#include <tss2/tss2_common.h>
#include <tss2/tss2_esys.h>
#include <tss2/tss2_rc.h>

#include <stdio.h>
#include <time.h>

#include "../fd.hpp"
#include "../main.hpp"
#include "../zfs.hpp"

// ./src/swtpm/swtpm socket --server port=2321 --ctrl type=tcp,port=2322 --tpm2


template <class T>
struct slice_iter {
	T * data;
	size_t len;

	T & operator*() { return *this->data; }
	bool operator!=(const slice_iter & other) { return this->data != other.data || this->len != other.len; }
	slice_iter operator++() {
		++this->data;
		--this->len;
		return *this;
	}
};

slice_iter<uint8_t> begin(TPM2B_DIGEST & dg) {
	return {&dg.buffer[0], dg.size};
}
slice_iter<uint8_t> end(TPM2B_DIGEST & dg) {
	return {dg.buffer + dg.size, 0};
}


int main(int argc, char ** argv) {
	const char * backup{};
	return do_main(
	    argc, argv, "b:", [&](auto) { backup = optarg; },
	    [&](auto dataset) {
		    ESYS_CONTEXT * tpm2_ctx{};
		    // https://software.intel.com/content/www/us/en/develop/articles/code-sample-protecting-secret-data-and-keys-using-intel-platform-trust-technology.html
		    // tssstartup
		    // tpm2_createprimary -Q --hierarchy=o --key-context=prim.ctx
		    // cat /tmp/sk |  tpm2_create --hash-algorithm=sha256 --public=seal.pub --private=seal.priv --sealing-input=- --parent-context=prim.ctx
		    // tpm2_flushcontext -t
		    // tpm2_load -Q --parent-context=prim.ctx --public=seal.pub --private=seal.priv --name=seal.name --key-context=seal.ctx
		    // tpm2_evictcontrol --hierarchy=o --object-context=seal.ctx
		    //   persistent-handle: 0x81000001
		    //
		    // tpm2_unseal -Q --object-context=0x81000000


		    // https://trustedcomputinggroup.org/wp-content/uploads/TSS_ESAPI_v1p00_r05_pubrev.pdf
		    // mainly "3.4. The ESAPI Session" and "3.5. ESAPI Use Model"
		    // https://tpm2-tss.readthedocs.io/en/latest/group___e_s_y_s___c_o_n_t_e_x_t.html
		    fprintf(stderr, "Esys_Initialize() = %s\n", Tss2_RC_Decode(Esys_Initialize(&tpm2_ctx, nullptr, nullptr)));
		    quickscope_wrapper tpm2_ctx_deleter{[&] { Esys_Finalize(&tpm2_ctx); }};
		    fprintf(stderr, "%p\n", (void *)tpm2_ctx);

		    fprintf(stderr, "Esys_Startup() = %s\n", Tss2_RC_Decode(Esys_Startup(tpm2_ctx, TPM2_SU_CLEAR)));


		    // https://github.com/tpm2-software/tpm2-tss/blob/master/test/integration/esys-create-session-auth.int.c#L218
		    const TPMT_SYM_DEF symmetric = {.algorithm = TPM2_ALG_AES, .keyBits = {.aes = 128}, .mode = {.aes = TPM2_ALG_CFB}};

		    ESYS_TR tpm2_session;
		    fprintf(stderr, "Esys_StartAuthSession() = %s\n",
		            Tss2_RC_Decode(Esys_StartAuthSession(tpm2_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, nullptr, TPM2_SE_HMAC,
		                                                 &symmetric, TPM2_ALG_SHA512, &tpm2_session)));
		    quickscope_wrapper tpm2_session_deleter{
		        [&] { fprintf(stderr, "Esys_FlushContext(tpm2_session) = %s\n", Tss2_RC_Decode(Esys_FlushContext(tpm2_ctx, tpm2_session))); }};


		    ESYS_TR primary_handle = ESYS_TR_NONE;
		    quickscope_wrapper primary_handle_deleter{
		        [&] { fprintf(stderr, "Esys_FlushContext(primary_handle) = %s\n", Tss2_RC_Decode(Esys_FlushContext(tpm2_ctx, primary_handle))); }};

		    TPM2B_DATA metadata{};
		    const auto now    = time(nullptr);
		    const auto now_tm = localtime(&now);
		    metadata.size     = snprintf((char *)metadata.buffer, sizeof(metadata.buffer), "%s %d-%02d-%02dT%02d:%02d:%02d %s", zfs_get_name(dataset),  //
                                 now_tm->tm_year + 1900, now_tm->tm_mon + 1, now_tm->tm_mday, now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec,  //
                                 TZPFMS_VERSION) +
		                    1;
		    metadata.size = metadata.size > sizeof(metadata.buffer) ? sizeof(metadata.buffer) : metadata.size;
		    fprintf(stderr, "%d/%d: \"%s\"\n", metadata.size, sizeof(metadata.buffer), metadata.buffer);

		    {
			    const TPM2B_SENSITIVE_CREATE primary_sens{};
			    // Adapted from tpm2-tss-3.0.1/test/integration/esys-create-primary-hmac.int.c
			    const TPM2B_PUBLIC pub = {.size       = 0,
			                              .publicArea = {.type             = TPM2_ALG_RSA,
			                                             .nameAlg          = TPM2_ALG_SHA1,
			                                             .objectAttributes = TPMA_OBJECT_USERWITHAUTH | TPMA_OBJECT_RESTRICTED | TPMA_OBJECT_DECRYPT |
			                                                                 TPMA_OBJECT_FIXEDTPM | TPMA_OBJECT_FIXEDPARENT | TPMA_OBJECT_SENSITIVEDATAORIGIN,
			                                             .authPolicy =
			                                                 {
			                                                     .size = 0,
			                                                 },
			                                             .parameters.rsaDetail =
			                                                 {
			                                                     .symmetric =
			                                                         {
			                                                             .algorithm   = TPM2_ALG_AES,
			                                                             .keyBits.aes = 128,
			                                                             .mode.aes    = TPM2_ALG_CFB,
			                                                         },
			                                                     .scheme =
			                                                         {
			                                                             .scheme = TPM2_ALG_NULL,
			                                                         },
			                                                     .keyBits  = 2048,
			                                                     .exponent = 0,
			                                                 },
			                                             .unique.rsa = {
			                                                 .size   = 0,
			                                                 .buffer = {},
			                                             }}};
			    const TPML_PCR_SELECTION pcrs{};

			    TPM2B_PUBLIC * public_ret{};
			    TPM2B_CREATION_DATA * creation_data{};
			    TPM2B_DIGEST * creation_hash{};
			    TPMT_TK_CREATION * creation_ticket{};
			    fprintf(stderr, "Esys_CreatePrimary() = %s\n",
			            Tss2_RC_Decode(Esys_CreatePrimary(tpm2_ctx, ESYS_TR_RH_OWNER, tpm2_session, ESYS_TR_NONE, ESYS_TR_NONE, &primary_sens, &pub, &metadata, &pcrs,
			                                              &primary_handle, &public_ret, &creation_data, &creation_hash, &creation_ticket)));
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

		    {
			    // Same args as tpm2-tools' tpm2_create(1)
			    const TPM2B_SENSITIVE_CREATE secret_sens{0, {{}, {8, "dupanina"}}};
			    const TPM2B_PUBLIC pub = {.size       = 0,
			                              .publicArea = {.type             = TPM2_ALG_KEYEDHASH,
			                                             .nameAlg          = TPM2_ALG_SHA256,
			                                             .objectAttributes = TPMA_OBJECT_FIXEDTPM | TPMA_OBJECT_FIXEDPARENT | TPMA_OBJECT_USERWITHAUTH,
			                                             .authPolicy =
			                                                 {
			                                                     .size = 0,
			                                                 },
			                                             .parameters.keyedHashDetail.scheme =
			                                                 {
			                                                     .scheme  = TPM2_ALG_NULL,
			                                                     .details = {},
			                                                 },
			                                             .unique.keyedHash = {
			                                                 .size   = 0,
			                                                 .buffer = {},
			                                             }}};
			    const TPML_PCR_SELECTION pcrs{};

			    TPM2B_CREATION_DATA * creation_data{};
			    TPM2B_DIGEST * creation_hash{};
			    TPMT_TK_CREATION * creation_ticket{};
			    fprintf(stderr, "Esys_Create() = %s\n",
			            Tss2_RC_Decode(Esys_Create(tpm2_ctx, primary_handle, tpm2_session, ESYS_TR_NONE, ESYS_TR_NONE, &secret_sens, &pub, &metadata, &pcrs,
			                                       &sealant_private, &sealant_public, &creation_data, &creation_hash, &creation_ticket)));
			    quickscope_wrapper creation_ticket_deleter{[=] { Esys_Free(creation_ticket); }};
			    quickscope_wrapper creation_hash_deleter{[=] { Esys_Free(creation_hash); }};
			    quickscope_wrapper creation_data_deleter{[=] { Esys_Free(creation_data); }};
		    }

		    ESYS_TR sealed_handle = ESYS_TR_NONE;
		    quickscope_wrapper sealed_handle_deleter{
		        [&] { fprintf(stderr, "Esys_FlushContext(sealed_handle) = %s\n", Tss2_RC_Decode(Esys_FlushContext(tpm2_ctx, sealed_handle))); }};

		    {
			    const TPM2B_SENSITIVE_CREATE secret_sens{0, {{}, {8, "dupanina"}}};  // TODO: this is the sealed data
			    const TPM2B_PUBLIC pub = {.size       = 0,
			                              .publicArea = {.type             = TPM2_ALG_KEYEDHASH,
			                                             .nameAlg          = TPM2_ALG_SHA256,
			                                             .objectAttributes = TPMA_OBJECT_FIXEDTPM | TPMA_OBJECT_FIXEDPARENT | TPMA_OBJECT_USERWITHAUTH,
			                                             .authPolicy =
			                                                 {
			                                                     .size = 0,
			                                                 },
			                                             .parameters.keyedHashDetail.scheme =
			                                                 {
			                                                     .scheme  = TPM2_ALG_NULL,
			                                                     .details = {},
			                                                 },
			                                             .unique.keyedHash = {
			                                                 .size   = 0,
			                                                 .buffer = {},
			                                             }}};
			    const TPML_PCR_SELECTION pcrs{};

			    TPM2B_PRIVATE * private_ret{};
			    TPM2B_PUBLIC * public_ret{};
			    TPM2B_CREATION_DATA * creation_data{};
			    TPM2B_DIGEST * creation_hash{};
			    TPMT_TK_CREATION * creation_ticket{};
			    fprintf(
			        stderr, "Esys_Load() = %s\n",
			        Tss2_RC_Decode(Esys_Load(tpm2_ctx, primary_handle, tpm2_session, ESYS_TR_NONE, ESYS_TR_NONE, sealant_private, sealant_public, &sealed_handle)));
			    quickscope_wrapper creation_ticket_deleter{[=] { Esys_Free(creation_ticket); }};
			    quickscope_wrapper creation_hash_deleter{[=] { Esys_Free(creation_hash); }};
			    quickscope_wrapper creation_data_deleter{[=] { Esys_Free(creation_data); }};
			    quickscope_wrapper public_ret_deleter{[=] { Esys_Free(public_ret); }};
			    quickscope_wrapper private_ret_deleter{[=] { Esys_Free(private_ret); }};
		    }

		    ESYS_TR persistent_handle = ESYS_TR_NONE;
		    quickscope_wrapper persistent_handle_deleter{
		        [&] { fprintf(stderr, "Esys_FlushContext(persistent_handle) = %s\n", Tss2_RC_Decode(Esys_FlushContext(tpm2_ctx, persistent_handle))); }};

		    {
			    const TPM2B_SENSITIVE_CREATE secret_sens{0, {{}, {8, "dupanina"}}};  // TODO: this is the sealed data
			    const TPM2B_PUBLIC pub = {.size       = 0,
			                              .publicArea = {.type             = TPM2_ALG_KEYEDHASH,
			                                             .nameAlg          = TPM2_ALG_SHA256,
			                                             .objectAttributes = TPMA_OBJECT_FIXEDTPM | TPMA_OBJECT_FIXEDPARENT | TPMA_OBJECT_USERWITHAUTH,
			                                             .authPolicy =
			                                                 {
			                                                     .size = 0,
			                                                 },
			                                             .parameters.keyedHashDetail.scheme =
			                                                 {
			                                                     .scheme  = TPM2_ALG_NULL,
			                                                     .details = {},
			                                                 },
			                                             .unique.keyedHash = {
			                                                 .size   = 0,
			                                                 .buffer = {},
			                                             }}};
			    const TPML_PCR_SELECTION pcrs{};

			    TPM2B_PRIVATE * private_ret{};
			    TPM2B_PUBLIC * public_ret{};
			    TPM2B_CREATION_DATA * creation_data{};
			    TPM2B_DIGEST * creation_hash{};
			    TPMT_TK_CREATION * creation_ticket{};
			    fprintf(stderr, "Esys_EvictControl() = %s\n",
			            Tss2_RC_Decode(
			                Esys_EvictControl(tpm2_ctx, ESYS_TR_RH_OWNER, sealed_handle, tpm2_session, ESYS_TR_NONE, ESYS_TR_NONE, 0x81000001, &persistent_handle)));
			    quickscope_wrapper creation_ticket_deleter{[=] { Esys_Free(creation_ticket); }};
			    quickscope_wrapper creation_hash_deleter{[=] { Esys_Free(creation_hash); }};
			    quickscope_wrapper creation_data_deleter{[=] { Esys_Free(creation_data); }};
			    quickscope_wrapper public_ret_deleter{[=] { Esys_Free(public_ret); }};
			    quickscope_wrapper private_ret_deleter{[=] { Esys_Free(private_ret); }};
		    }

		    fprintf(stderr, "0x%x\n", 0x81000001);  // TODO: find first unused
		                                            // TODO: remove previous


		    // const TPM2B_MAX_BUFFER to_hash{8, "dupanina"};
		    // TPM2B_DIGEST * hashed{};
		    // TPMT_TK_HASHCHECK * valid{};
		    // fprintf(stderr, "%s\n",
		    //         Tss2_RC_Decode(Esys_Hash(tpm2_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, &to_hash, TPM2_ALG_SHA256, ESYS_TR_RH_OWNER, &hashed, &valid)));
		    // quickscope_wrapper valid_deleter{[=] { Esys_Free(valid); }};
		    // quickscope_wrapper hashed_deleter{[=] { Esys_Free(hashed); }};
		    // fprintf(stderr, "hashed.len=%u\n", (unsigned)hashed->size);

		    // auto hashed_s = static_cast<char *>(TRY_PTR("hashed_s", alloca(WRAPPING_KEY_LEN * 2 + 1)));
		    // {
		    //  auto cur = hashed_s;
		    //  for(auto kb : *hashed) {
		    //   *cur++ = "0123456789ABCDEF"[(kb >> 4) & 0x0F];
		    //   *cur++ = "0123456789ABCDEF"[(kb >> 0) & 0x0F];
		    //  }
		    //  *cur = '\0';
		    // }
		    // fprintf(stderr, "%s\n", hashed_s);


		    if(zfs_prop_get_int(dataset, ZFS_PROP_KEYSTATUS) == ZFS_KEYSTATUS_UNAVAILABLE) {
			    fprintf(stderr, "Key change error: Key must be loaded.\n");  // mimic libzfs error output
			    return __LINE__;
		    }


		    uint8_t wrap_key[WRAPPING_KEY_LEN];
		    TRY_MAIN(read_exact("/dev/random", wrap_key, sizeof(wrap_key)));
		    if(backup)
			    TRY_MAIN(write_exact(backup, wrap_key, sizeof(wrap_key), 0400));


		    auto wrap_key_s = static_cast<char *>(TRY_PTR("wrap_key_s", alloca(WRAPPING_KEY_LEN * 2 + 1)));
		    {
			    auto cur = wrap_key_s;
			    for(auto kb : wrap_key) {
				    *cur++ = "0123456789ABCDEF"[(kb >> 4) & 0x0F];
				    *cur++ = "0123456789ABCDEF"[(kb >> 0) & 0x0F];
			    }
			    *cur = '\0';
		    }
		    TRY_MAIN(zfs_prop_set(dataset, "xyz.nabijaczleweli:tzpfms.key", wrap_key_s));


		    /// zfs_crypto_rewrap() with "prompt" reads from stdin, but not if it's a TTY;
		    /// this user-proofs the set-up, and means we don't have to touch the filesysten:
		    /// instead, get an FD, write the raw key data there, dup() it onto stdin,
		    /// let libzfs read it, then restore stdin

		    int key_fd;
		    TRY_MAIN(filled_fd(key_fd, wrap_key, WRAPPING_KEY_LEN));
		    quickscope_wrapper key_fd_deleter{[=] { close(key_fd); }};


		    TRY_MAIN(with_stdin_at(key_fd, [&] {
			    if(zfs_crypto_rewrap(dataset, TRY_PTR("get rewrap args", rewrap_args()), B_FALSE))
				    return __LINE__;  // Error printed by libzfs
			    else
				    printf("Key for %s changed\n", zfs_get_name(dataset));

			    return 0;
		    }));

		    return 0;
	    });
}
