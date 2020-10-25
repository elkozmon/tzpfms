/* SPDX-License-Identifier: MIT */


#pragma once


#include "main.hpp"
#include "zfs.hpp"


template <class H, class F>
int do_clear_main(int argc, char ** argv, const char * this_backend, H && handlefn, F && freefn) {
	return do_main(
	    argc, argv, "", "", [&](auto) {},
	    [&](auto dataset) {
	    	REQUIRE_KEY_LOADED(dataset);

		    char * handle_s{};
		    TRY_MAIN(parse_key_props(dataset, this_backend, handle_s));

		    TRY_MAIN(handlefn(dataset, handle_s));


		    if(zfs_crypto_rewrap(dataset, TRY_PTR("get clear rewrap args", clear_rewrap_args()), B_FALSE))
			    return __LINE__;  // Error printed by libzfs


		    TRY_MAIN(freefn());

		    TRY_MAIN(clear_key_props(dataset));

		    return 0;
	    });
}
