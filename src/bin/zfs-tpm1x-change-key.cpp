/* SPDX-License-Identifier: MIT */


#include <libzfs.h>
// #include <sys/zio_crypt.h>
#define WRAPPING_KEY_LEN 32

#include <stdio.h>

#include <tss/platform.h>
#include <tss/tspi.h>
#include <tss/tss_structs.h>
#include <tss/tss_typedef.h>

#include <trousers/trousers.h>

#include "../fd.hpp"
#include "../main.hpp"
#include "../parse.hpp"
#include "../tpm2.hpp"
#include "../zfs.hpp"


#define THIS_BACKEND "TPM1.X"

// I just got this out of /dev/random
static const constexpr uint8_t sealing_secret[TPM_SHA1_160_HASH_LEN]{0xB9, 0xEE, 0x71, 0x5D, 0xBE, 0x4B, 0x24, 0x3F, 0xAA, 0x81,
                                                                     0xEA, 0x04, 0x30, 0x6E, 0x06, 0x37, 0x10, 0x38, 0x3E, 0x35};

// ./src/swtpm_setup/swtpm_setup --tpmstate /home/nabijaczleweli/code/tzpfms/tpm1.x-state --createek --display --logfile /dev/stdout --overwrite
// swtpm cuse -n tpm --tpmstate dir=/home/nabijaczleweli/code/tzpfms/tpm1.x-state --seccomp action=none --log level=10,file=/dev/fd/4 4>&1; sleep 0.5;
// swtpm_ioctl -i /dev/tpm; sleep 0.5; TPM_DEVICE=/dev/tpm swtpm_bios; sleep 0.5; tcsd -f swtpm_ioctl -s /dev/tpm


int main(int argc, char ** argv) {
	const char * backup{};
	return do_main(
	    argc, argv, "b:", "[-b backup-file]", [&](auto) { backup = optarg; },
	    [&](auto dataset) {
		    REQUIRE_KEY_LOADED(dataset);


		    /// Mostly based on tpm_sealdata(1) from tpm-tools.

		    // All memory lives as long as this does
		    TSS_HCONTEXT ctx{};
		    fprintf(stderr, "Tspi_Context_Create() = %s\n", Trspi_Error_String(Tspi_Context_Create(&ctx)));
		    fprintf(stderr, "Tspi_Context_Connect() = %s\n", Trspi_Error_String(Tspi_Context_Connect(ctx, nullptr)));
		    quickscope_wrapper ctx_deleter{[&] {
			    fprintf(stderr, "Tspi_Context_FreeMemory() = %s\n", Trspi_Error_String(Tspi_Context_FreeMemory(ctx, nullptr)));
			    fprintf(stderr, "Tspi_Context_Close() = %s\n", Trspi_Error_String(Tspi_Context_Close(ctx)));
		    }};

		    TSS_HTPM tpm_h{};
		    fprintf(stderr, "Tspi_Context_GetTpmObject() = %s\n", Trspi_Error_String(Tspi_Context_GetTpmObject(ctx, &tpm_h)));


		    uint8_t * wrap_key{};
		    fprintf(stderr, "Tspi_TPM_GetRandom() = %s\n", Trspi_Error_String(Tspi_TPM_GetRandom(tpm_h, WRAPPING_KEY_LEN, &wrap_key)));
		    fprintf(stderr, "%u\n", WRAPPING_KEY_LEN);
		    for(auto i = 0u; i < WRAPPING_KEY_LEN; ++i)
			    fprintf(stderr, "%02X", wrap_key[i]);
		    fprintf(stderr, "\n");


		    TSS_HKEY srk{};
		    fprintf(stderr, "Tspi_Context_LoadKeyByUUID() = %s\n",
		            Trspi_Error_String(Tspi_Context_LoadKeyByUUID(ctx, TSS_PS_TYPE_SYSTEM, TSS_UUID_SRK, &srk)));  // if fails: need to take ownership

		    TSS_HPOLICY srk_policy{};
		    fprintf(stderr, "Tspi_GetPolicyObject() = %s\n", Trspi_Error_String(Tspi_GetPolicyObject(srk, TSS_POLICY_USAGE, &srk_policy)));
		    fprintf(stderr, "Tspi_Policy_AssignToObject(srk_policy) = %s\n", Trspi_Error_String(Tspi_Policy_AssignToObject(srk_policy, srk)));
		    quickscope_wrapper srk_policy_deleter{
		        [&] { fprintf(stderr, "Tspi_Policy_FlushSecret() = %s\n", Trspi_Error_String(Tspi_Policy_FlushSecret(srk_policy))); }};

		    uint8_t well_known[TPM_SHA1_160_HASH_LEN]{};
		    fprintf(stderr, "Tspi_Policy_SetSecret(well known) = %s\n",
		            Trspi_Error_String(Tspi_Policy_SetSecret(srk_policy, TSS_SECRET_MODE_SHA1, sizeof(well_known), well_known)));

		    TSS_HKEY parent_key{};
		    fprintf(stderr, "Tspi_Context_CreateObject() = %s\n",
		            Trspi_Error_String(
		                Tspi_Context_CreateObject(ctx, TSS_OBJECT_TYPE_RSAKEY, TSS_KEY_SIZE_2048 | TSS_KEY_VOLATILE | TSS_KEY_NOT_MIGRATABLE, &parent_key)));

		    TSS_HKEY parent_key_policy{};
		    fprintf(stderr, "Tspi_Context_CreateObject() = %s\n",
		            Trspi_Error_String(Tspi_Context_CreateObject(ctx, TSS_OBJECT_TYPE_POLICY, TSS_POLICY_USAGE, &parent_key_policy)));
		    fprintf(stderr, "Tspi_Policy_AssignToObject(parent_key_policy) = %s\n", Trspi_Error_String(Tspi_Policy_AssignToObject(parent_key_policy, parent_key)));
		    quickscope_wrapper parent_key_policy_deleter{
		        [&] { fprintf(stderr, "Tspi_Policy_FlushSecret() = %s\n", Trspi_Error_String(Tspi_Policy_FlushSecret(parent_key_policy))); }};
		    fprintf(stderr, "Tspi_Policy_SetSecret(\"adenozynotrójfosforan\") = %s\n",
		            Trspi_Error_String(
		                Tspi_Policy_SetSecret(parent_key_policy, TSS_SECRET_MODE_PLAIN, strlen("adenozynotrójfosforan"), (BYTE *)"adenozynotrójfosforan")));


		    auto err = Tspi_Key_CreateKey(parent_key, srk, 0);
		    fprintf(stderr, "Tspi_Key_CreateKey() = %s\n", Trspi_Error_String(err));
		    // Equivalent to TSS_ERROR_LAYER(err) == TSS_LAYER_TPM && TSS_ERROR_CODE(err) == TPM_E_AUTHFAIL
		    if((err & TSS_LAYER_TSP) == TSS_LAYER_TPM && (err & TSS_MAX_ERROR) == TPM_E_AUTHFAIL) {
			    // TODO: read SRK password from stdin here
			    fprintf(stderr, "Tspi_Policy_SetSecret(\"dupanina\") = %s\n",
			            Trspi_Error_String(Tspi_Policy_SetSecret(srk_policy, TSS_SECRET_MODE_PLAIN, strlen("dupanina"), (BYTE *)"dupanina")));

			    fprintf(stderr, "Tspi_Key_CreateKey() = %s\n", Trspi_Error_String(Tspi_Key_CreateKey(parent_key, srk, 0)));
		    }

		    fprintf(stderr, "Tspi_Key_LoadKey() = %s\n", Trspi_Error_String(Tspi_Key_LoadKey(parent_key, srk)));


		    TSS_HKEY sealed_object{};
		    fprintf(stderr, "Tspi_Context_CreateObject() = %s\n",
		            Trspi_Error_String(Tspi_Context_CreateObject(ctx, TSS_OBJECT_TYPE_ENCDATA, TSS_ENCDATA_SEAL, &sealed_object)));
		    TSS_HPOLICY sealed_object_policy{};
		    fprintf(stderr, "Tspi_GetPolicyObject() = %s\n", Trspi_Error_String(Tspi_GetPolicyObject(srk, TSS_POLICY_USAGE, &sealed_object_policy)));

		    fprintf(stderr, "Tspi_Policy_AssignToObject(sealed_object) = %s\n",
		            Trspi_Error_String(Tspi_Policy_AssignToObject(sealed_object_policy, sealed_object)));
		    fprintf(stderr, "Tspi_Policy_SetSecret(sealing_secret) = %s\n",
		            Trspi_Error_String(Tspi_Policy_SetSecret(sealed_object_policy, TSS_SECRET_MODE_SHA1, sizeof(sealing_secret), (uint8_t *)sealing_secret)));

		    TSS_HKEY bound_pcrs{};
		    fprintf(stderr, "Tspi_Context_CreateObject() = %s\n",
		            Trspi_Error_String(
		                Tspi_Context_CreateObject(ctx, TSS_OBJECT_TYPE_PCRS, 0, &bound_pcrs)));  // See tpm_sealdata.c from src:tpm-tools for more on flags here
		    fprintf(stderr, "Tspi_Data_Seal() = %s\n", Trspi_Error_String(Tspi_Data_Seal(sealed_object, parent_key, WRAPPING_KEY_LEN, wrap_key, bound_pcrs)));

		    uint8_t * parent_key_blob{};
		    uint32_t parent_key_blob_len{};
		    fprintf(stderr, "Tspi_GetAttribData(parent_key) = %s\n",
		            Trspi_Error_String(Tspi_GetAttribData(parent_key, TSS_TSPATTRIB_KEY_BLOB, TSS_TSPATTRIB_KEYBLOB_BLOB, &parent_key_blob_len, &parent_key_blob)));
		    fprintf(stderr, "%u\n", parent_key_blob_len);
		    for(auto i = 0u; i < parent_key_blob_len; ++i)
			    fprintf(stderr, "%02X", parent_key_blob[i]);
		    fprintf(stderr, "\n");

		    uint8_t * sealed_object_blob{};
		    uint32_t sealed_object_blob_len{};
		    fprintf(stderr, "Tspi_GetAttribData(sealed_object) = %s\n",
		            Trspi_Error_String(Tspi_GetAttribData(sealed_object, TSS_TSPATTRIB_ENCDATA_BLOB, TSS_TSPATTRIB_ENCDATABLOB_BLOB, &sealed_object_blob_len,
		                                                  &sealed_object_blob)));
		    fprintf(stderr, "%u\n", sealed_object_blob_len);
		    for(auto i = 0u; i < sealed_object_blob_len; ++i)
			    fprintf(stderr, "%02X", sealed_object_blob[i]);
		    fprintf(stderr, "\n");


		    /*return 0;
		    TRY_MAIN(with_tpm2_session([&](auto tpm2_ctx, auto tpm2_session) {
		      char *previous_backend{}, *previous_handle_s{};
		      TRY_MAIN(lookup_userprop(dataset, PROPNAME_BACKEND, previous_backend));
		      TRY_MAIN(lookup_userprop(dataset, PROPNAME_KEY, previous_handle_s));
		      if(!!previous_backend ^ !!previous_handle_s)
		        fprintf(stderr, "Inconsistent tzpfms metadata for %s: back-end is %s, but handle is %s?\n", zfs_get_name(dataset), previous_backend,
		                previous_handle_s);
		      else if(previous_backend && previous_handle_s) {
		        if(strcmp(previous_backend, THIS_BACKEND))
		          fprintf(stderr,
		                  "Dataset %s was encrypted with tzpfms back-end %s before, but we are %s. You will have to free handle %s for back-end %s manually!\n",
		                  zfs_get_name(dataset), previous_backend, THIS_BACKEND, previous_handle_s, previous_backend);
		        else {
		          TPMI_DH_PERSISTENT previous_handle{};
		          if(parse_int(previous_handle_s, previous_handle))
		            fprintf(stderr, "Couldn't parse previous persistent handle for dataset %s. You might need to run \"tpm2_evictcontrol -c %s\" or equivalent!\n",
		                    zfs_get_name(dataset), previous_handle_s);
		          else {
		            if(tpm2_free_persistent(tpm2_ctx, tpm2_session, previous_handle))
		              fprintf(stderr,
		                      "Couldn't free previous persistent handle for dataset %s. You might need to run \"tpm2_evictcontrol -c 0x%X\" or equivalent!\n",
		                      zfs_get_name(dataset), previous_handle);
		          }
		        }
		      }

		      uint8_t wrap_key[WRAPPING_KEY_LEN];
		      TPMI_DH_PERSISTENT persistent_handle{};

		      TRY_MAIN(tpm2_generate_rand(tpm2_ctx, wrap_key, sizeof(wrap_key)));
		      if(backup)
		        TRY_MAIN(write_exact(backup, wrap_key, sizeof(wrap_key), 0400));

		      TRY_MAIN(tpm2_seal(tpm2_ctx, tpm2_session, persistent_handle, tpm2_creation_metadata(zfs_get_name(dataset)), wrap_key, sizeof(wrap_key)));
		      bool ok = false;  // Try to free the persistent handle if we're unsuccessful in actually using it later on
		      quickscope_wrapper persistent_clearer{[&] {
		        if(!ok && tpm2_free_persistent(tpm2_ctx, tpm2_session, persistent_handle))
		          fprintf(stderr, "Couldn't free persistent handle. You might need to run \"tpm2_evictcontrol -c 0x%X\" or equivalent!\n", persistent_handle);
		        if(!ok && clear_key_props(dataset))  // Sync with zfs-tpm2-clear-key
		          fprintf(stderr, "You might need to run \"zfs inherit %s %s\" and \"zfs inherit %s %s\"!\n", PROPNAME_BACKEND, zfs_get_name(dataset), PROPNAME_KEY,
		                  zfs_get_name(dataset));
		      }};*/


		    auto handle = reinterpret_cast<char *>(TRY_PTR("allocate handle string", alloca(parent_key_blob_len * 2 + 1 + sealed_object_blob_len * 2 + 1)));
		    {
			    auto cur = handle;
			    for(auto i = 0u; i < parent_key_blob_len; ++i, cur += 2)
				    sprintf(cur, "%02X", parent_key_blob[i]);
			    *cur++ = ':';
			    for(auto i = 0u; i < sealed_object_blob_len; ++i, cur += 2)
				    sprintf(cur, "%02X", sealed_object_blob[i]);
			    *cur++ = '\0';
		    }
		    fprintf(stderr, "%s\n", handle);
		    TRY_MAIN(set_key_props(dataset, THIS_BACKEND, handle));


		    TRY_MAIN(change_key(dataset, wrap_key));

		    // ok = true;
		    // return 0;
		    // }));

		    return 0;
	    });
}
