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


template <class G, class M, class V = int (*)()>
static int do_bare_main(
    int argc, char ** argv, const char * getoptions, const char * usage, const char * dataset_usage, G && getoptfn, M && main,
    V && validate = []() { return 0; }) {
	const auto libz = TRY_PTR("initialise libzfs", libzfs_init());
	quickscope_wrapper libz_deleter{[=] { libzfs_fini(libz); }};

	libzfs_print_on_error(libz, B_TRUE);

#if __GLIBC__
	setenv("POSIXLY_CORRECT", "1", true);
#endif
	auto gopts = reinterpret_cast<char *>(alloca(strlen(getoptions) + 2 + 1));
	gopts[0] = 'h', gopts[1] = 'V';
	strcpy(gopts + 2, getoptions);
	for(int opt; (opt = getopt(argc, argv, gopts)) != -1;)
		switch(opt) {
			case '?':
			case 'h':
				fprintf(opt == 'h' ? stdout : stderr, "Usage: %s [-hV] %s%s%s\n", argv[0], usage, strlen(usage) ? " " : "", dataset_usage);
				return opt == 'h' ? 0 : __LINE__;
			case 'V':
				puts("tzpfms version " TZPFMS_VERSION);
				return 0;
			default:
				if constexpr(std::is_same_v<std::invoke_result_t<G, decltype(opt)>, void>)
					getoptfn(opt);
				else if constexpr(std::is_arithmetic_v<std::invoke_result_t<G, decltype(opt)>>)
					TRY_MAIN(getoptfn(opt));
				else {
					if(auto err = getoptfn(opt))
						return fprintf(stderr, "Usage: %s [-hV] %s%s%s\n", argv[0], usage, strlen(usage) ? " " : "", dataset_usage), err;
				}
		}

	if(auto err = validate())
		return fprintf(stderr, "Usage: %s [-hV] %s%s%s\n", argv[0], usage, strlen(usage) ? " " : "", dataset_usage), err;
	return main(libz);
}

template <class G, class M, class V = int (*)()>
static int do_main(
    int argc, char ** argv, const char * getoptions, const char * usage, G && getoptfn, M && main, V && validate = []() { return 0; }) {
	return do_bare_main(
	    argc, argv, getoptions, usage, "dataset", getoptfn,
	    [&](auto libz) {
		    if(optind >= argc)
			    return fprintf(stderr,
			                   "No dataset to act on?\n"
			                   "Usage: %s [-hV] %s%sdataset\n",
			                   argv[0], usage, strlen(usage) ? " " : ""),
			           __LINE__;
		    auto dataset = TRY_PTR(nullptr, zfs_open(libz, argv[optind], ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME));
		    quickscope_wrapper dataset_deleter{[&] { zfs_close(dataset); }};

		    {
			    char encryption_root[MAXNAMELEN];
			    boolean_t dataset_is_root;
			    TRY("get encryption root", zfs_crypto_get_encryption_root(dataset, &dataset_is_root, encryption_root));

			    if(!dataset_is_root && !strlen(encryption_root))
				    return fprintf(stderr, "Dataset %s not encrypted?\n", zfs_get_name(dataset)), __LINE__;
			    else if(!dataset_is_root) {
				    fprintf(stderr, "Using dataset %s's encryption root %s instead.\n", zfs_get_name(dataset), encryption_root);
				    zfs_close(dataset);
				    dataset = TRY_PTR(nullptr, zfs_open(libz, encryption_root, ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME));
			    }
		    }


		    return main(dataset);
	    },
	    validate);
}
