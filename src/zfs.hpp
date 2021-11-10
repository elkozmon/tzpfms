/* SPDX-License-Identifier: MIT */


#pragma once


#include <libzfs.h>
#include <sys/nvpair.h>

#include "main.hpp"


#define TRY_NVL(what, ...) TRY_GENERIC(what, , , _try_ret, _try_ret, strerror, __VA_ARGS__)


#define PROPNAME_BACKEND "xyz.nabijaczleweli:tzpfms.backend"
#define PROPNAME_KEY "xyz.nabijaczleweli:tzpfms.key"

#define MAXDEPTH_UNSET (SIZE_MAX - 1)


/// Mimic libzfs error output
#define REQUIRE_KEY_LOADED(dataset)                                                  \
	do {                                                                               \
		if(zfs_prop_get_int(dataset, ZFS_PROP_KEYSTATUS) == ZFS_KEYSTATUS_UNAVAILABLE) { \
			return fprintf(stderr, "Key change error: Key must be loaded.\n"), __LINE__;   \
		}                                                                                \
	} while(0)


/// Static nvlist with {keyformat=raw, keylocation=prompt}
extern nvlist_t * rewrap_args();
/// Static nvlist with {keyformat=passphrase, keylocation=prompt}
extern nvlist_t * clear_rewrap_args();

/// Extract user property name from ZFS property list from to out.
///
/// Returns success but does not touch out on not found.
extern int lookup_userprop(zfs_handle_t * from, const char * name, char *& out);

/// Set required decoding props on the dataset
extern int set_key_props(zfs_handle_t * on, const char * backend, const char * handle);

/// Remove decoding props from the dataset
extern int clear_key_props(zfs_handle_t * from);

/// Read in decoding props from the dataset
extern int parse_key_props(zfs_handle_t * in, const char * our_backend, char *& handle);


/// Rewrap key on on to wrap_key.
///
/// wrap_key must be WRAPPING_KEY_LEN long.
extern int change_key(zfs_handle_t * on, const uint8_t * wrap_key);

/// (Try to) load key wrap_key for for_d.
///
/// wrap_key must be WRAPPING_KEY_LEN long.
extern int load_key(zfs_handle_t * for_d, const uint8_t * wrap_key, bool noop);

/// Check back-end integrity; if the previous backend matches this_backend, run func(); otherwise warn.
template <class F>
int verify_backend(zfs_handle_t * on, const char * this_backend, F && func) {
	char *previous_backend{}, *previous_handle{};
	TRY_MAIN(lookup_userprop(on, PROPNAME_BACKEND, previous_backend));
	TRY_MAIN(lookup_userprop(on, PROPNAME_KEY, previous_handle));

	if(!!previous_backend ^ !!previous_handle)
		fprintf(stderr, "Inconsistent tzpfms metadata for %s: back-end is %s, but handle is %s?\n", zfs_get_name(on), previous_backend, previous_handle);
	else if(previous_backend && previous_handle) {
		if(strcmp(previous_backend, this_backend))
			fprintf(stderr, "Dataset %s was encrypted with tzpfms back-end %s before, but we are %s. You will have to free handle %s for back-end %s manually!\n",
			        zfs_get_name(on), previous_backend, this_backend, previous_handle, previous_backend);
		else
			func(previous_handle);
	}

	return 0;
}


template <class F>
struct for_all_datasets_iterator_data {
	F & func;
	zfs_iter_f iterator;
	size_t depth;
};

/// Iterate over datasets like zfs(8) list
template <class F>
int for_all_datasets(libzfs_handle_t * libz, char ** datasets, size_t maxdepth, F && func) {
	auto iterator = [](zfs_handle_t * dataset, void * dat_p) {
		auto dat = reinterpret_cast<for_all_datasets_iterator_data<F> *>(dat_p);
		TRY_MAIN(dat->func(dataset));

		if(dat->depth) {
			for_all_datasets_iterator_data<F> ndat{dat->func, dat->iterator, dat->depth - 1};
			return zfs_iter_filesystems(dataset, dat->iterator, &ndat);
		} else
			return 0;
	};

	if(!*datasets) {
		for_all_datasets_iterator_data<F> dat{func, iterator, (maxdepth == MAXDEPTH_UNSET) ? SIZE_MAX : maxdepth};
		switch(auto err = zfs_iter_root(libz, dat.iterator, &dat)) {
			case -1:  // zfs_iter_filesystems() only bubbles errors from callback, but zfs_iter_root() might produce new ones and return -1
				TRY("iterate root datasets", err);
				__builtin_unreachable();
			case 0:
				return 0;
			default:
				return err;
		}
	} else {
		for_all_datasets_iterator_data<F> dat{func, iterator, (maxdepth == MAXDEPTH_UNSET) ? 0 : maxdepth};
		for(; *datasets; ++datasets) {
			auto dataset = zfs_open(libz, *datasets, ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME);
			if(!dataset)
				continue;  // error printed by libzfs; mirror zfs(8) list behaviour here and continue despite any errors

			TRY_MAIN(dat.iterator(dataset, &dat));
		}
		return 0;
	}
}
