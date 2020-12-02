#!/bin/sh
# SPDX-License-Identifier: MIT


_get_backend() {
	rootfs="$(awk '$2 == "/" && $3 == "zfs" {print $1; exit 1}' /etc/mtab)"
	[ -z "$rootfs" ] && return 1

	eroot="$(zfs get encryptionroot -Ho value "$rootfs")"
	[ -z "$eroot" ] || [ "$eroot" = "-" ] && return 1


	backend="$(zfs-tpm-list -H "$eroot" | awk -F'\t' '{print $2}')"
	[ -z "$backend" ] && return 1
}

_install_tpm2() {
  # shellcheck disable=SC2046
	dracut_install zfs-tpm2-load-key $(find /usr/lib -name 'libtss2-tcti*.so*')  # TODO: there's got to be a better way™!
}

_install_tpm1x() {
	dracut_install zfs-tpm1x-load-key
	command -v tcsd > /dev/null && dracut_install tcsd
}


check() {
	command -v zfs-tpm-list > /dev/null || return 1

  # shellcheck disable=SC2154
	if [ -n "$hostonly" ]; then
		_get_backend || return

		[ "$backend" = "TPM2"   ] && command -v zfs-tpm2-load-key  > /dev/null && return 0
		[ "$backend" = "TPM1.X" ] && command -v zfs-tpm1x-load-key > /dev/null && return 0

		return 1
	fi

	return 0
}


depends() {
	echo zfs
}


installkernel() {
	instmods '=drivers/char/tpm'
}


install() {
	dracut_install zfs-tpm-list

	if [ -n "${hostonly}" ]; then
		_get_backend

		[ "$backend" = "TPM2"   ] && _install_tpm2
		[ "$backend" = "TPM1.X" ] && _install_tpm1x
	else
		command -v zfs-tpm2-load-key  > /dev/null && _install_tpm2
		command -v zfs-tpm1x-load-key > /dev/null && _install_tpm1x
	fi

	inst_hook pre-mount 89 "${moddir:-}/tzpfms-load-key.sh"  # zfs installs with 90, we *must* run beforehand
}
