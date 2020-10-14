/* SPDX-License-Identifier: MIT */


#pragma once


#include <libnvpair.h>
// #include <libzfs.h>
// #include <sys/fs/zfs.h>
//// #include <sys/zio_crypt.h>
// #define WRAPPING_KEY_LEN 32


/// Static nvlist with {keyformat=raw, keylocation=prompt}
extern nvlist_t * rewrap_args();
