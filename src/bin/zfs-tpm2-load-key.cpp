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
	auto noop = B_FALSE;
	return do_main(
	    argc, argv, "n", [&](auto) { noop = B_TRUE; },
	    [&](auto dataset) {
		    TPMI_DH_PERSISTENT handle{};
		    TRY_MAIN(parse_key_props(dataset, THIS_BACKEND, handle));


		    uint8_t wrap_key[WRAPPING_KEY_LEN];
		    TRY_MAIN(with_tpm2_session([&](auto tpm2_ctx, auto tpm2_session) {
			    TRY_MAIN(tpm2_unseal(tpm2_ctx, tpm2_session, handle, wrap_key, sizeof(wrap_key)));
			    return 0;
		    }));


		    int key_fd;
		    TRY_MAIN(filled_fd(key_fd, (void *)wrap_key, sizeof(wrap_key)));
		    quickscope_wrapper key_fd_deleter{[=] { close(key_fd); }};


		    TRY_MAIN(with_stdin_at(key_fd, [&] {
			    if(zfs_crypto_load_key(dataset, noop, nullptr))
				    return __LINE__;  // Error printed by libzfs
			    else
				    printf("Key for %s %s\n", zfs_get_name(dataset), noop ? "OK" : "loaded");

			    return 0;
		    }));

		    return 0;
	    });
}
