/* SPDX-License-Identifier: MIT */


#include <libzfs.h>
// #include <sys/zio_crypt.h>
#define WRAPPING_KEY_LEN 32

#include <stdio.h>

#include "../fd.hpp"
#include "../main.hpp"
#include "../zfs.hpp"


static const constexpr uint8_t our_test_key[WRAPPING_KEY_LEN] = {
    0xe2, 0xac, 0xf7, 0x89, 0x32, 0x37, 0xcb, 0x94, 0x67, 0xeb, 0x2b, 0xe9, 0xa3, 0x48, 0x83, 0x72,
    0xd5, 0x4c, 0xc5, 0x1c, 0x99, 0x65, 0xb0, 0x8d, 0x05, 0xa6, 0xd5, 0xff, 0x7a, 0xf7, 0xeb, 0xfc,
};


int main(int argc, char ** argv) {
	return do_main(
	    argc, argv, "", [](auto) {},
	    [](auto dataset) {
		    /// zfs_crypto_rewrap() with "prompt" reads from stdin, but not if it's a TTY;
		    /// this user-proofs the set-up, and means we don't have to touch the filesysten:
		    /// instead, get an FD, write the raw key data there, dup() it onto stdin,
		    /// let libzfs read it, then restore stdin

		    int key_fd;
		    TRY_MAIN(filled_fd(key_fd, (void *)our_test_key, WRAPPING_KEY_LEN));
		    quickscope_wrapper key_fd_deleter{[=] { close(key_fd); }};


		    TRY_MAIN(with_stdin_at(key_fd, [&] {
			    if(zfs_crypto_rewrap(dataset, TRY_PTR("get rewrap args", rewrap_args()), B_FALSE))
				    return __LINE__;  // Error printed by libzfs
			    else
				    printf("Key for %s changed\n", zfs_get_name(dataset));

			    return 0;
		    }));

		    return 0;
	    });
}
