/* SPDX-License-Identifier: MIT */


#include "zfs.hpp"
#include "common.hpp"

#include <libzfs.h>


#define TRY_NVL(what, ...) TRY_GENERIC(what, , , _try_ret, _try_ret, __VA_ARGS__)


nvlist_t * rewrap_args() {
	static nvlist_t * ret{};
	static quickscope_wrapper ret_deleter{[&] { nvlist_free(ret); }};

	if(!ret)
		if(auto err =
		       [&] {
			       TRY_NVL("allocate rewrap nvlist", nvlist_alloc(&ret, NV_UNIQUE_NAME, 0));
			       TRY_NVL("add keyformat to rewrap nvlist",
			               nvlist_add_string(ret, zfs_prop_to_name(ZFS_PROP_KEYFORMAT), "raw"));  // Why can't this be uint64 and ZFS_KEYFORMAT_RAW?
			       TRY_NVL("add keylocation to rewrap nvlist", nvlist_add_string(ret, zfs_prop_to_name(ZFS_PROP_KEYLOCATION), "prompt"));
			       return 0;
		       }();
		   err && ret) {
			nvlist_free(ret);
			ret   = nullptr;
			errno = err;
		}

	return ret;
}
