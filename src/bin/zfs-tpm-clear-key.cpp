/* SPDX-License-Identifier: MIT */


#include <libzfs.h>

#include <stdio.h>

#include "../main.hpp"
#include "../zfs.hpp"


int main(int argc, char ** argv) {
	return do_main(
	    argc, argv, "", [&](auto) {},
	    [&](auto dataset) {
		    REQUIRE_KEY_LOADED(dataset);

		    if(zfs_crypto_rewrap(dataset, TRY_PTR("get clear rewrap args", clear_rewrap_args()), B_FALSE))
			    return __LINE__;  // Error printed by libzfs

		    if(clear_key_props(dataset)) {
			    fprintf(stderr, "You might need to run \"zfs inherit %s %s\" and \"zfs inherit %s %s\"!\n", PROPNAME_BACKEND, zfs_get_name(dataset), PROPNAME_KEY,
			            zfs_get_name(dataset));
			    return __LINE__;
		    }

		    return 0;
	    });
}
