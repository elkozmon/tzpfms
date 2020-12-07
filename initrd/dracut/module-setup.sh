#!/bin/sh
# SPDX-License-Identifier: MIT


#include "../install.h"


_get_backend() {
	rootfs="$(awk '$2 == "/" && $3 == "zfs" {print $1; exit 1}' /etc/mtab)"
	[ -z "$rootfs" ] && return 1

	eroot="$(zfs get encryptionroot -Ho value "$rootfs")"
	[ -z "$eroot" ] || [ "$eroot" = "-" ] && return 1


	backend="$(zfs-tpm-list -H "$eroot" | awk -F'\t' '{print $2}')"
	[ -n "$backend" ]
	return
}

_install_tpm2() {
	inst_binary zfs-tpm2-load-key
  # shellcheck disable=SC2046
	inst_library $(find /usr/lib -name 'libtss2-tcti*.so*')  # TODO: there's got to be a better way™!
}

_install_tpm1x() {
	inst_binary zfs-tpm1x-load-key
	INSTALL_TPM1X{inst_binary tcsd; inst_binary ip; inst_binary ss, initdir, inst_simple, inst_simple, inst_simple, inst_library}
	command -v tpm_resetdalock > /dev/null && inst_binary tpm_resetdalock
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
	inst_binary zfs-tpm-list

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
