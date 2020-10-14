/* SPDX-License-Identifier: MIT */


#include <libzfs.h>
// #include <sys/zio_crypt.h>
#define WRAPPING_KEY_LEN 32

#include <stdio.h>

#include "../fd.hpp"
#include "../main.hpp"


static const constexpr uint8_t our_test_key[WRAPPING_KEY_LEN] = {
    0xe2, 0xac, 0xf7, 0x89, 0x32, 0x37, 0xcb, 0x94, 0x67, 0xeb, 0x2b, 0xe9, 0xa3, 0x48, 0x83, 0x72,
    0xd5, 0x4c, 0xc5, 0x1c, 0x99, 0x65, 0xb0, 0x8d, 0x05, 0xa6, 0xd5, 0xff, 0x7a, 0xf7, 0xeb, 0xfc,
};


int main(int argc, char ** argv) {
	auto noop = B_FALSE;
	return do_main(
	    argc, argv, "n", [&](char) { noop = B_TRUE; },
	    [&](auto dataset) {
		    int key_fd;
		    TRY_MAIN(filled_fd(key_fd, (void *)our_test_key, WRAPPING_KEY_LEN));
		    quickscope_wrapper key_fd_deleter{[=] { close(key_fd); }};


		    TRY_MAIN(with_stdin_at(key_fd, [&] {
			    if(zfs_crypto_load_key(dataset, noop, nullptr))
				    return __LINE__;  // Error printed by libzfs
			    else
				    printf("Key for %s loaded\n", zfs_get_name(dataset));

			    return 0;
		    }));

		    return 0;
	    });
}
