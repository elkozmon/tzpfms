/* SPDX-License-Identifier: MIT */


#include "fd.hpp"
#include "main.hpp"
#include "zfs.hpp"

#include <libzfs.h>
// #include <sys/zio_crypt.h>
#define WRAPPING_KEY_LEN 32


template <class F>
static int with_stdin_at_buffer(const void * buf, size_t buf_len, F && func) {
	int key_fd;
	TRY_MAIN(filled_fd(key_fd, buf, buf_len));
	quickscope_wrapper key_fd_deleter{[=] { close(key_fd); }};

	return with_stdin_at(key_fd, func);
}


int change_key(zfs_handle_t * on, const uint8_t * wrap_key) {
	/// zfs_crypto_rewrap() with "prompt" reads from stdin, but not if it's a TTY;
	/// this user-proofs the set-up, and means we don't have to touch the filesysten:
	/// instead, get an FD, write the raw key data there, dup() it onto stdin,
	/// let libzfs read it, then restore stdin

	return with_stdin_at_buffer(wrap_key, WRAPPING_KEY_LEN, [&] {
		if(zfs_crypto_rewrap(on, TRY_PTR("get rewrap args", rewrap_args()), B_FALSE))
			return __LINE__;  // Error printed by libzfs
		else
			printf("Key for %s changed\n", zfs_get_name(on));

		return 0;
	});
}


int load_key(zfs_handle_t * for_d, const uint8_t * wrap_key, bool noop) {
	return with_stdin_at_buffer(wrap_key, WRAPPING_KEY_LEN, [&] {
		if(zfs_crypto_load_key(for_d, noop ? B_TRUE : B_FALSE, nullptr))
			return __LINE__;  // Error printed by libzfs
		else
			printf("Key for %s %s\n", zfs_get_name(for_d), noop ? "OK" : "loaded");

		return 0;
	});
}
