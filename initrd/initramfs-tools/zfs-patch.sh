#!/bin/sh
# SPDX-License-Identifier: MIT


#include "../mount.h"


# Included into /scripts/zfs in the initrd, replacing the original decrypt_fs(), now available as __tzpfms__decrypt_fs()
decrypt_fs() {
	fs="$1"

	# Bail early if we don't have even the common binaries
	if ! command -v zfs-tpm-list > /dev/null; then
		__tzpfms__decrypt_fs "$fs"
		return
	fi

	# First three lines borrowed from /scripts/zfs#decrypt_fs()
	# If pool encryption is active and the zfs command understands '-o encryption'
	if [ "$(zpool list -H -o feature@encryption "$(echo "$fs" | awk -F/ '{print $1}')")" = "active" ]; then
		ENCRYPTIONROOT="$(get_fs_value "$fs" encryptionroot)"

		if ! [ "$ENCRYPTIONROOT" = "-" ]; then
			# Match this sexion to dracut/tzpfms-load-key.sh
			if command -v zfs-tpm2-load-key > /dev/null && ! [ "$(zfs-tpm-list -Hub TPM2 "$ENCRYPTIONROOT")" = "" ]; then
				with_promptable_tty zfs-tpm2-load-key "$ENCRYPTIONROOT"
				return
			fi

			if command -v zfs-tpm1x-load-key > /dev/null && ! [ "$(zfs-tpm-list -Hub TPM1.X "$ENCRYPTIONROOT")" = "" ]; then
				POTENTIALLY_START_TCSD{}
				with_promptable_tty zfs-tpm1x-load-key "$ENCRYPTIONROOT"; err="$?"
				POTENTIALLY_KILL_TCSD{}
				return "$err"
			fi

			__tzpfms__decrypt_fs "${fs}"
			return
		fi
	fi

	return 0
}


WITH_PROMPTABLE_TTY{ }
