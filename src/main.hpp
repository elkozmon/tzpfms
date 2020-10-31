/* SPDX-License-Identifier: MIT */


#pragma once


#include "common.hpp"
#include <libzfs.h>
#include <stdlib.h>
#include <type_traits>
#include <unistd.h>


#define TRY_PTR(what, ...) TRY_GENERIC(what, !, , errno, __LINE__, strerror, __VA_ARGS__)
#define TRY_MAIN(...)                 \
	do {                                \
		if(auto _try_ret = (__VA_ARGS__)) \
			return _try_ret;                \
	} while(0)


template <class G, class M>
int do_bare_main(int argc, char ** argv, const char * getoptions, const char * usage, G && getoptfn, M && main) {
	const auto libz = TRY_PTR("initialise libzfs", libzfs_init());
	quickscope_wrapper libz_deleter{[=] { libzfs_fini(libz); }};

	libzfs_print_on_error(libz, B_TRUE);

#if __GLIBC__
	setenv("POSIXLY_CORRECT", "1", true);
#endif
	auto gopts = reinterpret_cast<char *>(TRY_PTR("allocate options string", alloca(strlen(getoptions) + 2 + 1)));
	snprintf(gopts, strlen(getoptions) + 2 + 1, "%shV", getoptions);
	for(int opt; (opt = getopt(argc, argv, gopts)) != -1;)
		switch(opt) {
			case '?':
			case 'h':
				fprintf(opt == 'h' ? stdout : stderr, "Usage: %s [-hV] %s%s<dataset>\n", argv[0], usage, strlen(usage) ? " " : "");
				return opt == 'h' ? 0 : __LINE__;
			case 'V':
				printf("tzpfms version %s\n", TZPFMS_VERSION);
				return 0;
			default:
				if constexpr(std::is_same_v<std::invoke_result_t<G, decltype(opt)>, void>)
					getoptfn(opt);
				else {
					if(auto err = getoptfn(opt)) {
						fprintf(stderr, "Usage: %s [-hV] %s%s<dataset>\n", argv[0], usage, strlen(usage) ? " " : "");
						return err;
					}
				}
		}

	return main(libz);
}

template <class G, class M>
int do_main(int argc, char ** argv, const char * getoptions, const char * usage, G && getoptfn, M && main) {
	return do_bare_main(argc, argv, getoptions, usage, getoptfn, [&](auto libz) {
		if(optind >= argc) {
			fprintf(stderr,
			        "No dataset to act on?\n"
			        "Usage: %s [-hV] %s%s<dataset>\n",
			        argv[0], usage, strlen(usage) ? " " : "");
			return __LINE__;
		}
		auto dataset = TRY_PTR(nullptr, zfs_open(libz, argv[optind], ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME));
		quickscope_wrapper dataset_deleter{[&] { zfs_close(dataset); }};

		{
			char encryption_root[MAXNAMELEN];
			boolean_t dataset_is_root;
			TRY("get encryption root", zfs_crypto_get_encryption_root(dataset, &dataset_is_root, encryption_root));

			if(!dataset_is_root && !strlen(encryption_root)) {
				fprintf(stderr, "Dataset %s not encrypted?\n", zfs_get_name(dataset));
				return __LINE__;
			} else if(!dataset_is_root) {
				printf("Using dataset %s's encryption root %s instead.\n", zfs_get_name(dataset), encryption_root);
				// TODO: disallow maybe? or require force option?
				zfs_close(dataset);
				dataset = TRY_PTR(nullptr, zfs_open(libz, encryption_root, ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME));
			}
		}


		return main(dataset);
	});
}
