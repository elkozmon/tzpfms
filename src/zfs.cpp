/* SPDX-License-Identifier: MIT */


#include "zfs.hpp"
#include "common.hpp"

#include <libzfs.h>


// Funxion statics pull in libc++'s __cxa_guard_acquire()
static nvlist_t * rrargs{};
static quickscope_wrapper ret_deleter{[] { nvlist_free(rrargs); }};
nvlist_t * rewrap_args() {
	if(!rrargs)
		if(auto err =
		       [&] {
			       TRY_NVL("allocate rewrap nvlist", nvlist_alloc(&rrargs, NV_UNIQUE_NAME, 0));
			       TRY_NVL("add keyformat to rewrap nvlist",
			               nvlist_add_string(rrargs, zfs_prop_to_name(ZFS_PROP_KEYFORMAT), "raw"));  // Why can't this be uint64 and ZFS_KEYFORMAT_RAW?
			       TRY_NVL("add keylocation to rewrap nvlist", nvlist_add_string(rrargs, zfs_prop_to_name(ZFS_PROP_KEYLOCATION), "prompt"));
			       return 0;
		       }();
		   err && rrargs) {
			nvlist_free(rrargs);
			rrargs   = nullptr;
			errno = err;
		}

	return rrargs;
}


#define TRY_LOOKUP(what, ...)             \
	({                                      \
		const auto _try_retl = (__VA_ARGS__); \
		if(_try_retl == ENOENT)               \
			return 0;                           \
		TRY_NVL(what, _try_retl);             \
	})

int lookup_userprop(nvlist_t * from, const char * name, char *& out) {
	// xyz.nabijaczleweli:tzpfms.key:
	//   value: '76B0286BEB3FAF57536C47D9A2BAD38157FD522A75A59E72867BBFD6AF167395'
	//   source: 'owo/enc'

	nvlist_t * vs{};
	TRY_LOOKUP("look up user property", nvlist_lookup_nvlist(from, name, &vs));
	TRY_LOOKUP("look up user property value", nvlist_lookup_string(vs, "value", &out));
	return 0;
}
