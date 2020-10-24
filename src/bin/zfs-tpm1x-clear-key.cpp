/* SPDX-License-Identifier: MIT */


#include <libzfs.h>

#include <stdio.h>

#include "../main.hpp"
#include "../tpm1x.hpp"
#include "../zfs.hpp"


#define THIS_BACKEND "TPM1.X"


int main(int argc, char ** argv) {
	return do_main(
	    argc, argv, "", "", [&](auto) {},
	    [&](auto dataset) {
		    REQUIRE_KEY_LOADED(dataset);

		    char * handle_s{};
		    TRY_MAIN(parse_key_props(dataset, THIS_BACKEND, handle_s));

		    tpm1x_handle handle{};  // Not like we use this, but for symmetry with the other -clear-keys
		    TRY_MAIN(tpm1x_parse_handle(zfs_get_name(dataset), handle_s, handle));


		    if(zfs_crypto_rewrap(dataset, TRY_PTR("get clear rewrap args", clear_rewrap_args()), B_FALSE))
			    return __LINE__;  // Error printed by libzfs


		    TRY_MAIN(clear_key_props(dataset));

		    return 0;
	    });
}
