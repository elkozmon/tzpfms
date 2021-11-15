/* SPDX-License-Identifier: MIT */


#include <libzfs.h>
// #include <sys/zio_crypt.h>
#define WRAPPING_KEY_LEN 32

#include <inttypes.h>
#include <stdio.h>

#include "../fd.hpp"
#include "../main.hpp"
#include "../parse.hpp"
#include "../tpm2.hpp"
#include "../zfs.hpp"


#define THIS_BACKEND "TPM2"


int main(int argc, char ** argv) {
	const char * backup{};
	return do_main(
	    argc, argv, "b:", "[-b backup-file]", [&](auto) { backup = optarg; },
	    [&](auto dataset) {
		    REQUIRE_KEY_LOADED(dataset);


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

		    return with_tpm2_session([&](auto tpm2_ctx, auto tpm2_session) {
			    TRY_MAIN(verify_backend(dataset, THIS_BACKEND, [&](auto previous_handle_s) {
				    TPMI_DH_PERSISTENT previous_handle{};
				    if(parse_int(previous_handle_s, previous_handle))
					    fprintf(stderr, "Couldn't parse previous persistent handle for dataset %s. You might need to run \"tpm2_evictcontrol -c %s\" or equivalent!\n",
					            zfs_get_name(dataset), previous_handle_s);
				    else {
					    if(tpm2_free_persistent(tpm2_ctx, tpm2_session, previous_handle))
						    fprintf(stderr,
						            "Couldn't free previous persistent handle for dataset %s. You might need to run \"tpm2_evictcontrol -c 0x%" PRIX32
						            "\" or equivalent!\n",
						            zfs_get_name(dataset), previous_handle);
				    }
			    }));

			    uint8_t wrap_key[WRAPPING_KEY_LEN];
			    TPMI_DH_PERSISTENT persistent_handle{};

			    TRY_MAIN(tpm2_generate_rand(tpm2_ctx, wrap_key, sizeof(wrap_key)));
			    if(backup)
				    TRY_MAIN(write_exact(backup, wrap_key, sizeof(wrap_key), 0400));

			    TRY_MAIN(tpm2_seal(zfs_get_name(dataset), tpm2_ctx, tpm2_session, persistent_handle, tpm2_creation_metadata(zfs_get_name(dataset)), wrap_key,
			                       sizeof(wrap_key)));
			    bool ok = false;  // Try to free the persistent handle if we're unsuccessful in actually using it later on
			    quickscope_wrapper persistent_clearer{[&] {
				    if(!ok && tpm2_free_persistent(tpm2_ctx, tpm2_session, persistent_handle))
					    fprintf(stderr, "Couldn't free persistent handle. You might need to run \"tpm2_evictcontrol -c 0x%" PRIX32 "\" or equivalent!\n",
					            persistent_handle);
				    if(!ok)
					    clear_key_props(dataset);
			    }};

			    {
				    char persistent_handle_s[2 + sizeof(persistent_handle) * 2 + 1];
				    if(auto written = snprintf(persistent_handle_s, sizeof(persistent_handle_s), "0x%" PRIX32, persistent_handle);
				       written < 0 || written >= static_cast<int>(sizeof(persistent_handle_s))) {
					    return fprintf(stderr, "Truncated persistent_handle name? %d/%zu\n", written, sizeof(persistent_handle_s)), __LINE__;
				    }
				    TRY_MAIN(set_key_props(dataset, THIS_BACKEND, persistent_handle_s));
			    }

			    TRY_MAIN(change_key(dataset, wrap_key));

			    ok = true;
			    return 0;
		    });
	    });
}
