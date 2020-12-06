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
	if [ "$(zpool list -H -o feature@encryption "$(echo "$fs" | awk -F/ '{print $1}')")" = "active" ]; then
		ENCRYPTIONROOT="$(get_fs_value "$fs" encryptionroot)"

		if ! [ "$ENCRYPTIONROOT" = "-" ]; then
			# Match this sexion to dracut/tzpfms-load-key.sh
			if command -v zfs-tpm2-load-key > /dev/null && ! [ "$(zfs-tpm-list -Hub TPM2 "$ENCRYPTIONROOT")" = "" ]; then
				with_promptable_tty zfs-tpm2-load-key "$ENCRYPTIONROOT"
				return
			fi

			if command -v zfs-tpm1x-load-key > /dev/null && ! [ "$(zfs-tpm-list -Hub TPM1.X "$ENCRYPTIONROOT")" = "" ]; then
				[ -z "$TZPFMS_TPM1X" ] && command -v tcsd > /dev/null && {
					ip l | awk -F '[[:space:]]*:[[:space:]]*' '{if($2 == "lo") exit $3 ~ /UP/}'
					lo_was_up="$?"
					if [ "$lo_was_up" = "0" ]; then
						ip l set up dev lo
						while ! ip a show dev lo | grep -qE '::1|127.0.0.1'; do sleep 0.1; done
					fi

					if [ "${quiet:-n}" = "y" ]; then
						tcsd -f > /tcsd.log 2>&1 &
					else
						tcsd -f &
					fi
					tcsd_port="$(awk -F '[[:space:]]*=[[:space:]]*' '!/^[[:space:]]*#/ && !/^$/ && $1 ~ /port$/ {gsub(/[[:space:]]/, "", $2); print $2}' /etc/tcsd.conf)"
					i=0; while [ "$i" -lt 100 ] && ! netstat -lt | grep -q "${tcsd_port:-30003}"; do sleep 0.1; i="$((i + 1))"; done
					[ "$i" = 100 ] && echo "Couldn't start tcsd!" >&2
				}

				with_promptable_tty zfs-tpm1x-load-key "$ENCRYPTIONROOT"; err="$?"

				[ -z "$TZPFMS_TPM1X" ] && command -v tcsd > /dev/null && {
					kill %+

					if [ "$lo_was_up" = "0" ]; then
						ip l set down dev lo
						# ::1 removed automatically
						ip a del 127.0.0.1/8 dev lo 2>/dev/null
					fi
				}

				return "$err"
			fi

			__tzpfms__decrypt_fs "${fs}"
			return
		fi
	fi

	return 0
}

# This sucks a lot of ass, since we don't know the questions or the amount thereof beforehand
# (0-2 (owner hierarchy/ownership + sealed object, both optional) best-case and 0-6 worst-case (both entered wrong twice)).
# Plymouth doesn't allow us to actually check what the splash status was, and ioctl(KDGETMODE) isn't reliable;
# ideally, we'd only clear the screen if we were making the switch, but not if the user was already switched to the log output.
# Instead, clear if there's a "quiet", leave alone otherwise, and always restore;
# cmdline option "plymouth.ignore-show-splash" can be used to disable splashes altogether, if desired.
#
# There's a similar but distinct version of this and the code above in dracut/tzpfms-load-key.sh
with_promptable_tty() {
	if command -v plymouth > /dev/null && plymouth --ping; then
		plymouth hide-splash
		[ "${quiet:-n}" = "y" ] && printf '\033c'

		"$@"; ret="$?"

		plymouth show-splash
	else
		# Mimic /scripts/zfs#decrypt_fs(): setting "printk" temporarily to "7" will allow prompt even if kernel option "quiet"
		printk="$(awk '{print $1}' /proc/sys/kernel/printk)"
		[ "$printk" = "7" ] || echo 7 > /proc/sys/kernel/printk

		"$@"; ret="$?"

		[ "$printk" = "7" ] || echo "$printk" > /proc/sys/kernel/printk
	fi
	return "$ret"
}
