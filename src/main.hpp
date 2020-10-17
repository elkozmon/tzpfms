/* SPDX-License-Identifier: MIT */


#pragma once


#include "common.hpp"
#include <libzfs.h>
#include <stdlib.h>
#include <unistd.h>


#define TRY_PTR(what, ...) TRY_GENERIC(what, !, , errno, __LINE__, strerror, __VA_ARGS__)
#define TRY_MAIN(...)                 \
	do {                                \
		if(auto _try_ret = (__VA_ARGS__)) \
			return _try_ret;                \
	} while(0)


template <class G, class M>
int do_main(int argc, char ** argv, const char * getoptions, G && getoptfn, M && main) {
	const auto libz = TRY_PTR("initialise libzfs", libzfs_init());
	quickscope_wrapper libz_deleter{[=] { libzfs_fini(libz); }};

	libzfs_print_on_error(libz, B_TRUE);

#if __GLIBC__
	setenv("POSIXLY_CORRECT", "1", true);
#endif
	for(int opt; (opt = getopt(argc, argv, getoptions)) != -1;)
		if(opt == '?')
			return __LINE__;
		else
			getoptfn(opt);

	if(optind >= argc) {
		fprintf(stderr, "No dataset to act on?\n");
		return __LINE__;
	}
	auto dataset = TRY_PTR(nullptr, zfs_open(libz, argv[optind], ZFS_TYPE_FILESYSTEM));
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
			dataset = TRY_PTR(nullptr, zfs_open(libz, encryption_root, ZFS_TYPE_FILESYSTEM));
		}
	}


	return main(dataset);
}
