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


int main(int argc, char ** argv) {
	const char * backup{};
	return do_main(
	    argc, argv, "b:", "[-b backup-file]", [&](auto) { backup = optarg; },
	    [&](auto dataset) {
		    REQUIRE_KEY_LOADED(dataset);


		    // TSS_RESULT Tspi_Context_Create(TSS_HCONTEXT* phContext);
		    // TSS_RESULT Tspi_Context_Connect(TSS_HCONTEXT hLocalContext, UNICODE* wszDestination);
		    // TSS_RESULT Tspi_Context_GetTpmObject(TSS_HCONTEXT hContext, TSS_HTPM* phTPM);
		    //
		    // TSS_RESULT Tspi_TPM_GetRandom(TSS_HTPM hTPM, UINT32 size, BYTE** random);
		    //
		    //
		    // TSS_RESULT Tspi_Data_Seal(TSS_HENCDATA hEncData,     TSS_HKEY hEncKey,
		    //                           UINT32       ulDataLength, BYTE*    rgbDataToSeal,
		    //                           TSS_HPCRS    hPcrComposite);
		    //
		    //
		    // TSS_RESULT Tspi_Context_Close(TSS_HCONTEXT hLocalContext);


		    TSS_HCONTEXT ctx{};
		    fprintf(stderr, "Tspi_Context_Create() = %s\n", Trspi_Error_String(Tspi_Context_Create(&ctx)));
		    fprintf(stderr, "Tspi_Context_Connect() = %s\n", Trspi_Error_String(Tspi_Context_Connect(ctx, nullptr)));
		    TSS_HTPM tpm_h{};
		    fprintf(stderr, "Tspi_Context_GetTpmObject() = %s\n", Trspi_Error_String(Tspi_Context_GetTpmObject(ctx, &tpm_h)));
		    quickscope_wrapper ctx_deleter{[&] { fprintf(stderr, "Tspi_Context_Close() = %s\n", Trspi_Error_String(Tspi_Context_Close(ctx))); }};

		    return 0;
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
			    }};

			    TRY_MAIN(set_key_props(dataset, THIS_BACKEND, persistent_handle));

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

			    ok = true;
			    return 0;
		    }));

		    return 0;
	    });
}
