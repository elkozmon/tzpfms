/* SPDX-License-Identifier: MIT */


#include <libzfs.h>

#include <stdio.h>

#include "../main.hpp"
#include "../tpm2.hpp"
#include "../zfs.hpp"


#define THIS_BACKEND "TPM2"


int main(int argc, char ** argv) {
	return do_main(
	    argc, argv, "", "", [&](auto) {},
	    [&](auto dataset) {
		    REQUIRE_KEY_LOADED(dataset);

		    char * persistent_handle_s{};
		    TRY_MAIN(parse_key_props(dataset, THIS_BACKEND, persistent_handle_s));

		    TPMI_DH_PERSISTENT persistent_handle{};
		    TRY_MAIN(tpm2_parse_handle(zfs_get_name(dataset), persistent_handle_s, persistent_handle));


		    if(zfs_crypto_rewrap(dataset, TRY_PTR("get clear rewrap args", clear_rewrap_args()), B_FALSE))
			    return __LINE__;  // Error printed by libzfs


		    TRY_MAIN(with_tpm2_session([&](auto tpm2_ctx, auto tpm2_session) {
			    TRY_MAIN(tpm2_free_persistent(tpm2_ctx, tpm2_session, persistent_handle));
			    return 0;
		    }));

		    TRY_MAIN(clear_key_props(dataset));

		    return 0;
	    });
}
