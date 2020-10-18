/* SPDX-License-Identifier: MIT */


#include <libzfs.h>

#include <stdio.h>

#include "../main.hpp"
#include "../tpm2.hpp"
#include "../zfs.hpp"


#define THIS_BACKEND "TPM2"


int main(int argc, char ** argv) {
	return do_main(
	    argc, argv, "", [&](auto) {},
	    [&](auto dataset) {
		    REQUIRE_KEY_LOADED(dataset);

		    TPMI_DH_PERSISTENT persistent_handle{};
		    TRY_MAIN(parse_key_props(dataset, THIS_BACKEND, persistent_handle));

		    if(zfs_crypto_rewrap(dataset, TRY_PTR("get clear rewrap args", clear_rewrap_args()), B_FALSE))
			    return __LINE__;  // Error printed by libzfs


		    TRY_MAIN(with_tpm2_session([&](auto tpm2_ctx, auto tpm2_session) {
			    TRY_MAIN(tpm2_free_persistent(tpm2_ctx, tpm2_session, persistent_handle));
			    return 0;
		    }));

		    if(clear_key_props(dataset)) {  // Sync with zfs-tpm2-change-key
			    fprintf(stderr, "You might need to run \"zfs inherit %s %s\" and \"zfs inherit %s %s\"!\n", PROPNAME_BACKEND, zfs_get_name(dataset), PROPNAME_KEY,
			            zfs_get_name(dataset));
			    return __LINE__;
		    }

		    return 0;
	    });
}
