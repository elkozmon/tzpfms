#!/bin/sh
# SPDX-License-Identifier: MIT

DSET="$1"
exec 2>>/dev/kmsg

zfs-tpm-list -H "$DSET" | while read -r _ backend keystatus coherent; do
	[ "$keystatus" = 'available' ] && exit

	[ "$coherent" = 'yes' ] || {
		printf "%s\n" "${0##*/}[$$]: $DSET: incoherent tzpfms back-end $backend." "You might need to restore from back-up!" >&2
		exit 1
	}

	case "$backend" in
		TPM1.X) unlock='zfs-tpm1x-load-key'; deps='trousers.service' ;;
		TPM2)   unlock='zfs-tpm2-load-key';  deps=                   ;;
		*)      unlock=;                     deps=                   ;;
	esac

	command -v "$unlock" >/dev/null || {
		printf "%s\n" "${0##*/}[$$]: $DSET: unknown tzpfms back-end $backend." >&2
		exit # fall through, maybe there's another handler
	}

	# shellcheck disable=2086
	[ -n "$deps" ] && systemctl start $deps

	# shellcheck disable=2016
	[ -z "$TZPFMS_PASSPHRASE_HELPER" ] && export TZPFMS_PASSPHRASE_HELPER='exec systemd-ask-password --id="tzpfms:$2" "$1:"'
	exec "$unlock" "$DSET"
done

# Dataset doesn't exist, fall through
