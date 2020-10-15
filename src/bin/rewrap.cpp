/* SPDX-License-Identifier: MIT */


#include <libzfs.h>
// #include <sys/zio_crypt.h>
#define WRAPPING_KEY_LEN 32

#include <stdio.h>

#include "../fd.hpp"
#include "../main.hpp"
#include "../zfs.hpp"


int main(int argc, char ** argv) {
	const char * backup{};
	return do_main(
	    argc, argv, "b:", [&](auto) { backup = optarg; },
	    [&](auto dataset) {
		    if(zfs_prop_get_int(dataset, ZFS_PROP_KEYSTATUS) == ZFS_KEYSTATUS_UNAVAILABLE) {
			    fprintf(stderr, "Key change error: Key must be loaded.\n");  // mimic libzfs error output
			    return __LINE__;
		    }


		    uint8_t wrap_key[WRAPPING_KEY_LEN];
		    TRY_MAIN(read_exact("/dev/random", wrap_key, sizeof(wrap_key)));
		    if(backup)
			    TRY_MAIN(write_exact(backup, wrap_key, sizeof(wrap_key), 0400));


		    auto wrap_key_s = static_cast<char *>(TRY_PTR("wrap_key_s", alloca(WRAPPING_KEY_LEN * 2 + 1)));
		    {
			    auto cur = wrap_key_s;
			    for(auto kb : wrap_key) {
				    *cur++ = "0123456789ABCDEF"[(kb >> 4) & 0x0F];
				    *cur++ = "0123456789ABCDEF"[(kb >> 0) & 0x0F];
			    }
			    *cur = '\0';
		    }
		    TRY_MAIN(zfs_prop_set(dataset, "xyz.nabijaczleweli:tzpfms.key", wrap_key_s));


		    /// zfs_crypto_rewrap() with "prompt" reads from stdin, but not if it's a TTY;
		    /// this user-proofs the set-up, and means we don't have to touch the filesysten:
		    /// instead, get an FD, write the raw key data there, dup() it onto stdin,
		    /// let libzfs read it, then restore stdin

		    int key_fd;
		    TRY_MAIN(filled_fd(key_fd, wrap_key, WRAPPING_KEY_LEN));
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
