#!/bin/sh
# SPDX-License-Identifier: MIT

# Included into /scripts/zfs in the initrd, replacing the original decrypt_fs(), now available as __tzpfms__decrypt_fs()
decrypt_fs() {
	fs="$1"

	# Bail early if we don't have even the common binaries
	if ! command -v zfs-tpm-list > /dev/null; then
		__tzpfms__decrypt_fs "${fs}"
		return
	fi

	# First three lines borrowed from /scripts/zfs#decrypt_fs()
	# If pool encryption is active and the zfs command understands '-o encryption'
	if [ "$(zpool list -H -o feature@encryption "$(echo "${fs}" | awk -F/ '{print $1}')")" = 'active' ]; then
		ENCRYPTIONROOT="$(get_fs_value "${fs}" encryptionroot)"

		if ! [ "$ENCRYPTIONROOT" = "-" ]; then
			if command -v zfs-tpm2-load-key > /dev/null && ! [ "$(zfs-tpm-list -Hub TPM2 "$ENCRYPTIONROOT")" = "" ]; then
				with_promptable_tty zfs-tpm2-load-key "$ENCRYPTIONROOT"
				return
			fi

			if command -v zfs-tpm1x-load-key > /dev/null && ! [ "$(zfs-tpm-list -Hub TPM1.X "$ENCRYPTIONROOT")" = "" ]; then
				with_promptable_tty zfs-tpm1x-load-key "$ENCRYPTIONROOT"
				return
			fi

			__tzpfms__decrypt_fs "${fs}"
			return
		fi
	fi

	return 0
}

# Mimic /scripts/zfs#decrypt_fs(): setting "printk" temporarily to "7" will allow prompt even if kernel option "quiet"
# TODO?: /scripts/zfs#decrypt_fs() checks for plymouth and systemd,
# but we don't know how many passphrases we're gonna read (anywhere between 0 and 2 best-base or 0 and 6 worst-case);
# can we "disable" plymouth somehow?
with_promptable_tty() {
	printk="$(awk '{print $1}' /proc/sys/kernel/printk)"
	echo 7 > /proc/sys/kernel/printk

	"$@"
	ret="$?"

	echo "$printk" > /proc/sys/kernel/printk

	return "$ret"
}
