/* SPDX-License-Identifier: MIT */


#include <libzfs.h>
// #include <sys/zio_crypt.h>
#define WRAPPING_KEY_LEN 32

#include <stdio.h>

#include "../fd.hpp"
#include "../main.hpp"
#include "../parse.hpp"
#include "../tpm1x.hpp"
#include "../zfs.hpp"


#define THIS_BACKEND "TPM1.X"

// ./src/swtpm_setup/swtpm_setup --tpmstate /home/nabijaczleweli/code/tzpfms/tpm1.x-state --createek --display --logfile /dev/stdout --overwrite
// swtpm cuse -n tpm --tpmstate dir=/home/nabijaczleweli/code/tzpfms/tpm1.x-state --seccomp action=none --log level=10,file=/dev/fd/4 4>&1; sleep 0.5;
// swtpm_ioctl -i /dev/tpm; sleep 0.5; TPM_DEVICE=/dev/tpm swtpm_bios; sleep 0.5; tcsd -f swtpm_ioctl -s /dev/tpm


int main(int argc, char ** argv) {
	const char * backup{};
	return do_main(
	    argc, argv, "b:", "[-b backup-file]", [&](auto) { backup = optarg; },
	    [&](auto dataset) {
		    REQUIRE_KEY_LOADED(dataset);

		    /// We don't use Tspi_Context_RegisterKey() and friends, so we don't touch the persistent storage database; as such, there is nothing to clean up.
		    TRY_MAIN(verify_backend(dataset, THIS_BACKEND, [](auto) {}));

		    /// Mostly based on tpm_sealdata(1) from tpm-tools.
		    return with_tpm1x_session([&](auto ctx, auto srk, auto srk_policy) {
			    TSS_HTPM tpm_h{};
			    TRY_TPM1X("extract TPM from context", Tspi_Context_GetTpmObject(ctx, &tpm_h));


			    uint8_t * wrap_key{};
			    TRY_TPM1X("get random data from TPM", Tspi_TPM_GetRandom(tpm_h, WRAPPING_KEY_LEN, &wrap_key));
			    if(backup)
				    TRY_MAIN(write_exact(backup, wrap_key, WRAPPING_KEY_LEN, 0400));


			    TSS_HOBJECT parent_key{};
			    TRY_TPM1X("prepare sealant key",
			              Tspi_Context_CreateObject(ctx, TSS_OBJECT_TYPE_RSAKEY, TSS_KEY_SIZE_2048 | TSS_KEY_VOLATILE | TSS_KEY_NOT_MIGRATABLE, &parent_key));
			    quickscope_wrapper parent_key_deleter{[&] {
				    Tspi_Key_UnloadKey(parent_key);
				    Tspi_Context_CloseObject(ctx, parent_key);
			    }};

			    TSS_HPOLICY parent_key_policy{};
			    TRY_TPM1X("create sealant key policy", Tspi_Context_CreateObject(ctx, TSS_OBJECT_TYPE_POLICY, TSS_POLICY_USAGE, &parent_key_policy));
			    TRY_TPM1X("assign policy to sealant key", Tspi_Policy_AssignToObject(parent_key_policy, parent_key));
			    quickscope_wrapper parent_key_policy_deleter{[&] {
				    Tspi_Policy_FlushSecret(parent_key_policy);
				    Tspi_Context_CloseObject(ctx, parent_key_policy);
			    }};

			    {
				    char what_for[ZFS_MAX_DATASET_NAME_LEN + 40 + 1];
				    snprintf(what_for, sizeof(what_for), "%s TPM1.X wrapping key (or empty for none)", zfs_get_name(dataset));

				    uint8_t * parent_key_passphrase{};
				    size_t parent_key_passphrase_len{};
				    TRY_MAIN(read_new_passphrase(what_for, parent_key_passphrase, parent_key_passphrase_len));
				    quickscope_wrapper parent_key_passphrase_deleter{[&] { free(parent_key_passphrase); }};

				    if(parent_key_passphrase_len)
					    TRY_TPM1X("assign passphrase to parent_key key",
					              Tspi_Policy_SetSecret(parent_key_policy, TSS_SECRET_MODE_PLAIN, parent_key_passphrase_len, parent_key_passphrase));
				    else
					    TRY_TPM1X("assign default sealant key secret",
					              Tspi_Policy_SetSecret(parent_key_policy, TSS_SECRET_MODE_SHA1, sizeof(parent_key_secret), (BYTE *)parent_key_secret));
			    }

			    TRY_MAIN(try_policy_or_passphrase("create sealant key (did you take ownership?)", "SRK", srk_policy,
			                                      [&] { return Tspi_Key_CreateKey(parent_key, srk, 0); }));

			    TRY_TPM1X("load sealant key", Tspi_Key_LoadKey(parent_key, srk));


			    TSS_HOBJECT sealed_object{};
			    TSS_HPOLICY sealed_object_policy{};
			    TRY_MAIN(tpm1x_prep_sealed_object(ctx, sealed_object, sealed_object_policy));
			    quickscope_wrapper sealed_object_deleter{[&] {
				    Tspi_Policy_FlushSecret(sealed_object_policy);
				    Tspi_Context_CloseObject(ctx, sealed_object_policy);
				    Tspi_Context_CloseObject(ctx, sealed_object);
			    }};

			    // This would need to replace the 0 below to handle PCRs
			    // TSS_HOBJECT bound_pcrs{};  // See tpm_sealdata.c from src:tpm-tools for more on flags here
			    // TRY_TPM1X("create PCR list", Tspi_Context_CreateObject(ctx, TSS_OBJECT_TYPE_PCRS, 0, &bound_pcrs));
			    // quickscope_wrapper bound_pcrs_deleter{[&] { Tspi_Context_CloseObject(ctx, bound_pcrs); }};

			    TRY_TPM1X("seal wrapping key data", Tspi_Data_Seal(sealed_object, parent_key, WRAPPING_KEY_LEN, wrap_key, 0));


			    uint8_t * parent_key_blob{};
			    uint32_t parent_key_blob_len{};
			    TRY_TPM1X("get sealant key blob",
			              Tspi_GetAttribData(parent_key, TSS_TSPATTRIB_KEY_BLOB, TSS_TSPATTRIB_KEYBLOB_BLOB, &parent_key_blob_len, &parent_key_blob));

			    uint8_t * sealed_object_blob{};
			    uint32_t sealed_object_blob_len{};
			    TRY_TPM1X("get sealed object blob", Tspi_GetAttribData(sealed_object, TSS_TSPATTRIB_ENCDATA_BLOB, TSS_TSPATTRIB_ENCDATABLOB_BLOB,
			                                                           &sealed_object_blob_len, &sealed_object_blob));


			    {
				    // 1740 in testing; we probably have a lot of stack to spare, but don't try our luck
				    auto handle = reinterpret_cast<char *>(TRY_PTR("allocate handle string", malloc(parent_key_blob_len * 2 + 1 + sealed_object_blob_len * 2 + 1)));
				    quickscope_wrapper handle_deleter{[=] { free(handle); }};

				    {
					    auto cur = handle;
					    for(auto i = 0u; i < parent_key_blob_len; ++i, cur += 2)
						    sprintf(cur, "%02X", parent_key_blob[i]);
					    *cur++ = ':';
					    for(auto i = 0u; i < sealed_object_blob_len; ++i, cur += 2)
						    sprintf(cur, "%02X", sealed_object_blob[i]);
					    *cur++ = '\0';
				    }

				    TRY_MAIN(set_key_props(dataset, THIS_BACKEND, handle));
			    }


			    if(auto err = change_key(dataset, wrap_key)) {
				    clear_key_props(dataset);
				    return err;
			    }

			    return 0;
		    });
	    });
}
