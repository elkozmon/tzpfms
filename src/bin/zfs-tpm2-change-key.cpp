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
	TPML_PCR_SELECTION pcrs{};
	bool allow_PCR_or_pass{};
	return do_main(
	    argc, argv, "b:P:A", "[-b backup-file] [-P algorithm:PCR[,PCR]…[+algorithm:PCR[,PCR]…]… [-A]]",
	    [&](auto o) {
		    switch(o) {
			    case 'b':
				    return backup = optarg, 0;
			    case 'P':
				    return tpm2_parse_pcrs(optarg, pcrs);
			    case 'A':
				    return allow_PCR_or_pass = true, 0;
			    default:
				    __builtin_unreachable();
		    }
	    },
	    [&](auto dataset) {
		    REQUIRE_KEY_LOADED(dataset);


		    // https://software.intel.com/content/www/us/en/develop/articles/code-sample-protecting-secret-data-and-keys-using-intel-platform-trust-technology.html
		    // https://tpm2-software.github.io/2020/04/13/Disk-Encryption.html#pcr-policy-authentication---access-control-of-sealed-pass-phrase-on-tpm2-with-pcr-sealing
		    // tssstartup
		    // tpm2_createprimary -Q --hierarchy=o --key-context=prim.ctx
		    // cat /tmp/sk |  tpm2_create --hash-algorithm=sha256 --public=seal.pub --private=seal.priv --sealing-input=- --parent-context=prim.ctx
		    // tpm2_flushcontext -t
		    // tpm2_load -Q --parent-context=prim.ctx --public=seal.pub --private=seal.priv --name=seal.name --key-context=seal.ctx
		    // tpm2_evictcontrol --hierarchy=o --object-context=seal.ctx
		    //   persistent-handle: 0x81000001
		    //
		    // tpm2_unseal -Q --object-context=0x81000000
		    //
		    // For PCRs:
		    // tpm2_startauthsession --session=session.ctx
		    // tpm2_policypcr -S session.ctx -l 'sha512:7+sha256:10' -L 5-10.policy3
		    // tpm2_flushcontext session.ctx; rm session.ctx
		    // + tpm2_create{,primary} gain -l 'sha512:7+sha256:10', tpm2_create gains -L 5-10.policy3
		    //
		    // tpm2_unseal -p pcr:'sha512:7+sha256:10' --object-context=0x81000000
		    // or, longhand:
		    // tpm2_startauthsession --policy-session --session=session3.ctx
		    // tpm2_policypcr --session=session3.ctx --pcr-list='sha512:7+sha256:10'
		    // tpm2_unseal -p session:session3.ctx --object-context=0x81000000
		    // tpm2_flushcontext session3.ctx; rm session3.ctx

		    return with_tpm2_session([&](auto tpm2_ctx, auto tpm2_session) {
			    TRY_MAIN(verify_backend(dataset, THIS_BACKEND, [&](auto previous_handle_s) {
				    TPMI_DH_PERSISTENT previous_handle{};
				    if(tpm2_parse_prop(zfs_get_name(dataset), previous_handle_s, previous_handle, nullptr))
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

			    TRY_MAIN(tpm2_seal(zfs_get_name(dataset), tpm2_ctx, tpm2_session, persistent_handle, tpm2_creation_metadata(zfs_get_name(dataset)), pcrs,
			                       allow_PCR_or_pass, wrap_key, sizeof(wrap_key)));
			    bool ok = false;  // Try to free the persistent handle if we're unsuccessful in actually using it later on
			    quickscope_wrapper persistent_clearer{[&] {
				    if(!ok && tpm2_free_persistent(tpm2_ctx, tpm2_session, persistent_handle))
					    fprintf(stderr, "Couldn't free persistent handle. You might need to run \"tpm2_evictcontrol -c 0x%" PRIX32 "\" or equivalent!\n",
					            persistent_handle);
				    if(!ok)
					    clear_key_props(dataset);
			    }};

			    {
				    char * prop{};
				    TRY_MAIN(tpm2_unparse_prop(persistent_handle, pcrs, &prop));
				    quickscope_wrapper prop_deleter{[&] { free(prop); }};
				    TRY_MAIN(set_key_props(dataset, THIS_BACKEND, prop));
			    }

			    TRY_MAIN(change_key(dataset, wrap_key));

			    ok = true;
			    return 0;
		    });
	    },
	    [&]() {
		    if(allow_PCR_or_pass && !pcrs.count)
			    return __LINE__;
		    return 0;
	    });
}
