#!/bin/sh
# SPDX-License-Identifier: MIT


# -h
# 0
# "$hostonly" set?
check() {
	command -v zfs-tpm-list > /dev/null || return 1

	# TODO: handle hostonly, proper only-include-if-all-found
	#command -v zfs-tpm2-load-key > /dev/null
	#command -v zfs-tpm1x-load-key > /dev/null

	return 0
}


depends() {
	echo zfs
	return 0
}


installkernel() {
	instmods '=drivers/char/tpm'
}


install() {
	dracut_install zfs-tpm-list
	dracut_install zfs-tpm2-load-key
	dracut_install zfs-tpm1x-load-key
	dracut_install tcsd

	inst_hook pre-mount 89 "${moddir:-}/tzpfms-load-key.sh"  # zfs installs with 90, we *must* run beforehand
	return 0

	inst_simple "${moddir:-}/zfs-lib.sh" "/lib/dracut-zfs-lib.sh"
	if [ -e @sysconfdir@/zfs/zpool.cache ]; then
		inst @sysconfdir@/zfs/zpool.cache
		type mark_hostonly >/dev/null 2>&1 && mark_hostonly @sysconfdir@/zfs/zpool.cache
	fi

	if dracut_module_included "systemd"; then
		mkdir -p "${initdir:-}/${systemdsystemunitdir:-}/zfs-import.target.wants"
		for _item in scan cache ; do
			dracut_install @systemdunitdir@/zfs-import-$_item.service
			if ! [ -L "${initdir}/${systemdsystemunitdir:-}/zfs-import.target.wants"/zfs-import-$_item.service ]; then
				ln -s ../zfs-import-$_item.service "${initdir}/${systemdsystemunitdir:-}/zfs-import.target.wants"/zfs-import-$_item.service
				type mark_hostonly >/dev/null 2>&1 && mark_hostonly @systemdunitdir@/zfs-import-$_item.service
			fi
		done
		inst "${moddir}"/zfs-env-bootfs.service "${systemdsystemunitdir:-}"/zfs-env-bootfs.service
		ln -s ../zfs-env-bootfs.service "${initdir}/${systemdsystemunitdir:-}/zfs-import.target.wants"/zfs-env-bootfs.service
		type mark_hostonly >/dev/null 2>&1 && mark_hostonly @systemdunitdir@/zfs-env-bootfs.service
		dracut_install systemd-ask-password
		dracut_install systemd-tty-ask-password-agent
		mkdir -p "${initdir}/${systemdsystemunitdir:-}/initrd.target.wants"
		dracut_install @systemdunitdir@/zfs-import.target
		if ! [ -L "${initdir}/${systemdsystemunitdir:-}/initrd.target.wants"/zfs-import.target ]; then
			ln -s ../zfs-import.target "${initdir}/${systemdsystemunitdir:-}/initrd.target.wants"/zfs-import.target
			type mark_hostonly >/dev/null 2>&1 && mark_hostonly @systemdunitdir@/zfs-import.target
		fi
	fi
}
