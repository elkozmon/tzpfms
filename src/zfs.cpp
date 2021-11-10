/* SPDX-License-Identifier: MIT */


#include "zfs.hpp"
#include "common.hpp"
#include "main.hpp"

#include <libzfs.h>

#include <string.h>


// Funxion statics pull in libc++'s __cxa_guard_acquire()
static nvlist_t * rrargs{};
static quickscope_wrapper rrargs_deleter{[] { nvlist_free(rrargs); }};
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
			rrargs = nullptr;
			errno  = err;
		}

	return rrargs;
}


static nvlist_t * crrargs{};
static quickscope_wrapper crrargs_deleter{[] { nvlist_free(crrargs); }};
nvlist_t * clear_rewrap_args() {
	if(!crrargs)
		if(auto err =
		       [&] {
			       TRY_NVL("allocate rewrap nvlist", nvlist_alloc(&crrargs, NV_UNIQUE_NAME, 0));
			       TRY_NVL("add keyformat to rewrap nvlist", nvlist_add_string(crrargs, zfs_prop_to_name(ZFS_PROP_KEYFORMAT), "passphrase"));
			       TRY_NVL("add keylocation to rewrap nvlist", nvlist_add_string(crrargs, zfs_prop_to_name(ZFS_PROP_KEYLOCATION), "prompt"));
			       return 0;
		       }();
		   err && crrargs) {
			nvlist_free(crrargs);
			crrargs = nullptr;
			errno   = err;
		}

	return crrargs;
}


#define TRY_LOOKUP(what, ...)             \
	({                                      \
		const auto _try_retl = (__VA_ARGS__); \
		if(_try_retl == ENOENT)               \
			return 0;                           \
		TRY_NVL(what, _try_retl);             \
	})

int lookup_userprop(zfs_handle_t * in, const char * name, char *& out) {
	// xyz.nabijaczleweli:tzpfms.key:
	//   value: '76B0286BEB3FAF57536C47D9A2BAD38157FD522A75A59E72867BBFD6AF167395'
	//   source: 'owo/enc'

	nvlist_t * vs{};
	TRY_LOOKUP("look up user property", nvlist_lookup_nvlist(zfs_get_user_props(in), name, &vs));

	char * source{};
	TRY_LOOKUP("look up user property source", nvlist_lookup_string(vs, "source", &source));
	if(!source || strcmp(source, zfs_get_name(in)))
		return 0;

	TRY_LOOKUP("look up user property value", nvlist_lookup_string(vs, "value", &out));
	return 0;
}


int set_key_props(zfs_handle_t * on, const char * backend, const char * handle) {
	nvlist_t * props{};
	quickscope_wrapper props_deleter{[&] { nvlist_free(props); }};

	TRY_NVL("allocate key nvlist", nvlist_alloc(&props, NV_UNIQUE_NAME, 0));
	TRY_NVL("add back-end to key nvlist", nvlist_add_string(props, PROPNAME_BACKEND, backend));
	TRY_NVL("add handle to key nvlist", nvlist_add_string(props, PROPNAME_KEY, handle));

	TRY("set tzpfms.{backend,key}", zfs_prop_set_list(on, props));

	return 0;
}


int clear_key_props(zfs_handle_t * from) {
	bool ok = false;
	quickscope_wrapper props_deleter{[&] {
		if(!ok)
			fprintf(stderr, "You might need to run \"zfs inherit %s %s\" and \"zfs inherit %s %s\" to fully clear metadata!\n", PROPNAME_BACKEND, zfs_get_name(from),
			        PROPNAME_KEY, zfs_get_name(from));
	}};

	TRY("delete tzpfms.backend", zfs_prop_inherit(from, PROPNAME_BACKEND, B_FALSE));
	TRY("delete tzpfms.key", zfs_prop_inherit(from, PROPNAME_KEY, B_FALSE));
	return ok = true, 0;
}


int parse_key_props(zfs_handle_t * in, const char * our_backend, char *& handle) {
	char * backend{};
	TRY_MAIN(lookup_userprop(in, PROPNAME_BACKEND, backend));

	if(!backend)
		return fprintf(stderr, "Dataset %s not encrypted with tzpfms!\n", zfs_get_name(in)), __LINE__;
	if(strcmp(backend, our_backend))
		return fprintf(stderr, "Dataset %s encrypted with tzpfms back-end %s, but we are %s.\n", zfs_get_name(in), backend, our_backend), __LINE__;

	TRY_MAIN(lookup_userprop(in, PROPNAME_KEY, handle));
	if(!handle)
		return fprintf(stderr, "Dataset %s missing key data.\n", zfs_get_name(in)), __LINE__;

	return 0;
}
