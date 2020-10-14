#include <libnvpair.h>
#include <libzfs.h>
#include <sys/fs/zfs.h>
// #include <sys/zio_crypt.h>
#define WRAPPING_KEY_LEN 32

#include <string.h>

#include <sys/types.h>
#include <unistd.h>

// clang++ -Wall -Wextra -pedantic -Wno-gnu-{statement-expression,include-next} -fno-exceptions -O3 -std=c++17 getprop.cpp -ogetprop $(pkg-config --cflags --libs libzfs{,_core}) -lrt


template <class F>
struct quickscope_wrapper {
	F func;

	~quickscope_wrapper() { func(); }
};

template <class F>
quickscope_wrapper(F)->quickscope_wrapper<F>;


static const constexpr uint8_t our_test_key[WRAPPING_KEY_LEN] = {
    0xe2, 0xac, 0xf7, 0x89, 0x32, 0x37, 0xcb, 0x94, 0x67, 0xeb, 0x2b, 0xe9, 0xa3, 0x48, 0x83, 0x72,
    0xd5, 0x4c, 0xc5, 0x1c, 0x99, 0x65, 0xb0, 0x8d, 0x05, 0xa6, 0xd5, 0xff, 0x7a, 0xf7, 0xeb, 0xfc,
};


#define RETERR (__COUNTER__ + 1)
#define TRY_GENERIC(what, cond_pre, cond_post, err_src, ...)                                      \
	({                                                                                              \
		auto _try_ret = (__VA_ARGS__);                                                                \
		if(cond_pre _try_ret cond_post) {                                                             \
			if constexpr(what != nullptr)                                                               \
				fprintf(stderr, "Couldn't %s: %s\n", static_cast<const char *>(what), strerror(err_src)); \
			return RETERR;                                                                              \
		}                                                                                             \
		_try_ret;                                                                                     \
	})
#define TRY(what, ...) TRY_GENERIC(what, , == -1, errno, __VA_ARGS__)
#define TRY_PTR(what, ...) TRY_GENERIC(what, !, , errno, __VA_ARGS__)
#define TRY_NVL(what, ...) TRY_GENERIC(what, , , _try_ret, __VA_ARGS__)
#define TRY_MAIN(...)                 \
	do {                                \
		if(auto _try_ret = (__VA_ARGS__)) \
			return _try_ret;                \
	} while(0)


template <class F>
static int with_stdin_at(int fd, F && what) {
	auto stdin_saved = TRY("dup() stdin", dup(0));
	quickscope_wrapper stdin_saved_deleter{[=] { close(stdin_saved); }};

	TRY("dup2() onto stdin", dup2(fd, 0));

	if(int ret = what()) {
		dup2(stdin_saved, 0);
		return ret;
	}

	TRY("dup2() stdin back onto stdin", dup2(stdin_saved, 0));
	return 0;
}

/// with_len may not exceed pipe capacity (64k by default)
static int filled_fd(int & fd, const void * with, size_t with_len) {
	int pipes[2];
	TRY("create buffer pipe", pipe(pipes));
	quickscope_wrapper pipes_w_deleter{[=] { close(pipes[1]); }};
	fd = pipes[0];

	auto ret = write(pipes[1], with, with_len);
	if(ret >= 0 && ret < WRAPPING_KEY_LEN) {
		ret   = -1;
		errno = ENODATA;
	}
	TRY("write to buffer pipe", ret);

	return 0;
}


int main(int, char ** argv) {
	const auto libz = TRY_PTR("initialise libzfs", libzfs_init());
	quickscope_wrapper libz_deleter{[=] { libzfs_fini(libz); }};

	libzfs_print_on_error(libz, B_TRUE);

	auto dataset = TRY_PTR(nullptr, zfs_open(libz, argv[1], ZFS_TYPE_FILESYSTEM));
	quickscope_wrapper dataset_deleter{[&] { zfs_close(dataset); }};

	{
		char encryption_root[MAXNAMELEN];
		boolean_t dataset_is_root;
		TRY("get encryption root", zfs_crypto_get_encryption_root(dataset, &dataset_is_root, encryption_root));

		if(!dataset_is_root && !strlen(encryption_root)) {
			fprintf(stderr, "Dataset %s not encrypted?\n", zfs_get_name(dataset));
			return RETERR;
		} else if(!dataset_is_root) {
			printf("Using dataset %s's encryption root %s instead.\n", zfs_get_name(dataset), encryption_root);
			// TODO: disallow maybe? or require force option?
			zfs_close(dataset);
			dataset = TRY_PTR(nullptr, zfs_open(libz, encryption_root, ZFS_TYPE_FILESYSTEM));
		}
	}


	/// zfs_crypto_rewrap() with "prompt" reads from stdin, but not if it's a TTY;
	/// this user-proofs the set-up, and means we don't have to touch the filesysten:
	/// instead, get an FD, write the raw key data there, dup() it onto stdin,
	/// let libzfs read it, then restore stdin

	int key_fd;
	TRY_MAIN(filled_fd(key_fd, (void *)our_test_key, WRAPPING_KEY_LEN));
	quickscope_wrapper key_fd_deleter{[=] { close(key_fd); }};

	TRY_MAIN(with_stdin_at(key_fd, [&] {
		nvlist_t * rewrap_args;
		TRY_NVL("allocate rewrap nvlist", nvlist_alloc(&rewrap_args, NV_UNIQUE_NAME, 0));
		quickscope_wrapper rewrap_args_deleter{[=] { nvlist_free(rewrap_args); }};
		TRY_NVL("add keyformat to rewrap nvlist",
		        nvlist_add_string(rewrap_args, zfs_prop_to_name(ZFS_PROP_KEYFORMAT), "raw"));  // Why can't this be uint64 and ZFS_KEYFORMAT_RAW?
		TRY_NVL("add keylocation to rewrap nvlist", nvlist_add_string(rewrap_args, zfs_prop_to_name(ZFS_PROP_KEYLOCATION), "prompt"));
		if(zfs_crypto_rewrap(dataset, rewrap_args, B_FALSE))
			return RETERR;  // Error printed by libzfs
		else
			printf("Key for %s changed\n", zfs_get_name(dataset));

		return 0;
	}));

	const auto props = zfs_get_all_props(dataset);
	dump_nvlist(props, 2);
}
