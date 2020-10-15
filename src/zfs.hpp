/* SPDX-License-Identifier: MIT */


#pragma once


#include <sys/nvpair.h>
// #include <libzfs.h>
// #include <sys/fs/zfs.h>
//// #include <sys/zio_crypt.h>
// #define WRAPPING_KEY_LEN 32


#define TRY_NVL(what, ...) TRY_GENERIC(what, , , _try_ret, _try_ret, __VA_ARGS__)


/// Static nvlist with {keyformat=raw, keylocation=prompt}
extern nvlist_t * rewrap_args();

/// Extract user property name from ZFS property list from to out.
///
/// Returns success but does not touch out on not found.
extern int lookup_userprop(nvlist_t * from, const char * name, char*&out);
