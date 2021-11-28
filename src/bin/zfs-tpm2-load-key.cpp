/* SPDX-License-Identifier: MIT */


#include <libzfs.h>
// #include <sys/zio_crypt.h>
#define WRAPPING_KEY_LEN 32

#include <stdio.h>

#include "../fd.hpp"
#include "../main.hpp"
#include "../tpm2.hpp"
#include "../zfs.hpp"


#define THIS_BACKEND "TPM2"


int main(int argc, char ** argv) {
	auto noop = false;
	return do_main(
	    argc, argv, "n", "[-n]", [&](auto) { noop = true; },
	    [&](auto dataset) {
		    char * handle_s{};
		    TRY_MAIN(parse_key_props(dataset, THIS_BACKEND, handle_s));

		    TPMI_DH_PERSISTENT handle{};
		    TPML_PCR_SELECTION pcrs{};
		    TRY_MAIN(tpm2_parse_prop(zfs_get_name(dataset), handle_s, handle, &pcrs));


		    uint8_t wrap_key[WRAPPING_KEY_LEN];
		    TRY_MAIN(with_tpm2_session([&](auto tpm2_ctx, auto tpm2_session) {
			    TRY_MAIN(tpm2_unseal(zfs_get_name(dataset), tpm2_ctx, tpm2_session, handle, pcrs, wrap_key, sizeof(wrap_key)));
			    return 0;
		    }));


		    TRY_MAIN(load_key(dataset, wrap_key, noop));
		    return 0;
	    });
}
