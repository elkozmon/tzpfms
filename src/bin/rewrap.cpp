/* SPDX-License-Identifier: MIT */


#include <libzfs.h>
// #include <sys/zio_crypt.h>
#define WRAPPING_KEY_LEN 32

#include <algorithm>

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
		    if(zfs_prop_get_int(dataset, ZFS_PROP_KEYSTATUS) == ZFS_KEYSTATUS_UNAVAILABLE) {
			    fprintf(stderr, "Key change error: Key must be loaded.\n");  // mimic libzfs error output
			    return __LINE__;
		    }


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

		    fprintf(stderr, "Esys_Startup() = %s\n", Tss2_RC_Decode(Esys_Startup(tpm2_ctx, TPM2_SU_CLEAR)));


		    ESYS_TR tpm2_session;
		    quickscope_wrapper tpm2_session_deleter{
		        [&] { fprintf(stderr, "Esys_FlushContext(tpm2_session) = %s\n", Tss2_RC_Decode(Esys_FlushContext(tpm2_ctx, tpm2_session))); }};

		    {
			    // https://github.com/tpm2-software/tpm2-tss/blob/master/test/integration/esys-create-session-auth.int.c#L218
			    TPMT_SYM_DEF session_key{};
			    session_key.algorithm   = TPM2_ALG_AES;
			    session_key.keyBits.aes = 128;
			    session_key.mode.aes    = TPM2_ALG_CFB;
			    fprintf(stderr, "Esys_StartAuthSession() = %s\n",
			            Tss2_RC_Decode(Esys_StartAuthSession(tpm2_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, nullptr, TPM2_SE_HMAC,
			                                                 &session_key, TPM2_ALG_SHA512, &tpm2_session)));
		    }


		    uint8_t wrap_key[WRAPPING_KEY_LEN];
		    {
			    TPM2B_DIGEST * rand{};
			    fprintf(stderr, "Esys_GetRandom() = %s\n",
			            Tss2_RC_Decode(Esys_GetRandom(tpm2_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, WRAPPING_KEY_LEN, &rand)));
			    quickscope_wrapper rand_deleter{[=] { Esys_Free(rand); }};
			    if(rand->size != WRAPPING_KEY_LEN)
				    fprintf(stderr, "Wrong random size: %d\n", rand->size);
			    memcpy(wrap_key, rand->buffer, WRAPPING_KEY_LEN);
		    }

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
		    fprintf(stderr, "%d/%zu: \"%s\"\n", metadata.size, sizeof(metadata.buffer), metadata.buffer);

		    {
			    const TPM2B_SENSITIVE_CREATE primary_sens{};

			    // Adapted from tpm2-tss-3.0.1/test/integration/esys-create-primary-hmac.int.c
			    TPM2B_PUBLIC pub{};
			    pub.publicArea.type             = TPM2_ALG_RSA;
			    pub.publicArea.nameAlg          = TPM2_ALG_SHA1;
			    pub.publicArea.objectAttributes = TPMA_OBJECT_USERWITHAUTH | TPMA_OBJECT_RESTRICTED | TPMA_OBJECT_DECRYPT | TPMA_OBJECT_FIXEDTPM |
			                                      TPMA_OBJECT_FIXEDPARENT | TPMA_OBJECT_SENSITIVEDATAORIGIN;
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

		    /// This is the object with the actual sealed data in it
		    {
			    TPM2B_SENSITIVE_CREATE secret_sens{};
			    secret_sens.sensitive.data.size = sizeof(wrap_key);
			    memcpy(secret_sens.sensitive.data.buffer, wrap_key, secret_sens.sensitive.data.size);

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

		    /// Load the keyedhash/sealed object into a transient handle
		    {
			    fprintf(
			        stderr, "Esys_Load() = %s\n",
			        Tss2_RC_Decode(Esys_Load(tpm2_ctx, primary_handle, tpm2_session, ESYS_TR_NONE, ESYS_TR_NONE, sealant_private, sealant_public, &sealed_handle)));
		    }

		    TPMI_DH_PERSISTENT persistent_handle{};

		    /// Find lowest unused persistent handle
		    {
			    TPMS_CAPABILITY_DATA * cap;  // TODO: check for more data?
			    fprintf(stderr, "Esys_GetCapability() = %s\n",
			            Tss2_RC_Decode(Esys_GetCapability(tpm2_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, TPM2_CAP_HANDLES, TPM2_PERSISTENT_FIRST,
			                                              TPM2_MAX_CAP_HANDLES, nullptr, &cap)));
			    quickscope_wrapper cap_deleter{[=] { Esys_Free(cap); }};

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
				    fprintf(stderr, "All %zu persistent handles allocated! We're fucked!\n", TPM2_MAX_CAP_HANDLES);
		    }

		    fprintf(stderr, "0x%x\n", persistent_handle);

		    /// Persist the loaded handle in the TPM — this will make it available as $persistent_handle until we explicitly evict it back to the transient store
		    {
			    // Can't be flushed (tpm:parameter(1):value is out of range or is not correct for the context), plus, that's kinda the point
			    ESYS_TR new_handle;
			    fprintf(stderr, "Esys_EvictControl() = %s\n",
			            Tss2_RC_Decode(
			                Esys_EvictControl(tpm2_ctx, ESYS_TR_RH_OWNER, sealed_handle, tpm2_session, ESYS_TR_NONE, ESYS_TR_NONE, persistent_handle, &new_handle)));
		    }

		    /// Free the persistent slot
		    /*{
		      // Neither of these are flushable (tpm:parameter(1):value is out of range or is not correct for the context)
		      ESYS_TR pandle;
		      fprintf(stderr, "Esys_TR_FromTPMPublic() = %s\n",
		              Tss2_RC_Decode(Esys_TR_FromTPMPublic(tpm2_ctx, persistent_handle - 1, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, &pandle)));

		      ESYS_TR new_handle;
		      fprintf(stderr, "Esys_EvictControl() = %s\n",
		              Tss2_RC_Decode(Esys_EvictControl(tpm2_ctx, ESYS_TR_RH_OWNER, pandle, tpm2_session, ESYS_TR_NONE, ESYS_TR_NONE, 0, &new_handle)));
		    }*/


		    // uint8_t wrap_key[WRAPPING_KEY_LEN];
		    // TRY_MAIN(read_exact("/dev/random", wrap_key, sizeof(wrap_key)));
		    if(backup)
			    TRY_MAIN(write_exact(backup, wrap_key, sizeof(wrap_key), 0400));

		    char persistent_handle_s[2 + sizeof(persistent_handle) * 2 + 1];
		    if(auto written = snprintf(persistent_handle_s, sizeof(persistent_handle_s), "0x%02X", persistent_handle);
		       written < 0 || written >= static_cast<int>(sizeof(persistent_handle_s)))
			    fprintf(stderr, "oops, truncated: %d/%zu\n", written, sizeof(persistent_handle_s));
		    TRY_MAIN(zfs_prop_set(dataset, "xyz.nabijaczleweli:tzpfms.key", persistent_handle_s));


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
