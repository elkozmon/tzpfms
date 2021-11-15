# SPDX-License-Identifier: MIT


#define WITH_PROMPTABLE_TTY(REDIREXIONS)
	# This sucks a lot of ass, since we don't know the questions or the amount thereof beforehand
	# (0-2 (owner hierarchy/ownership + sealed object, both optional) best-case and 0-6 worst-case (both entered wrong twice)).
	with_promptable_tty() {
		if plymouth --ping 2>/dev/null; then
			# shellcheck disable=SC2016
			TZPFMS_PASSPHRASE_HELPER='exec plymouth ask-for-password --prompt="$1: "' "$@" 2>/run/tzpfms-err; ret="$?"
			[ -s /run/tzpfms-err ] && plymouth display-message --text="$(cat /run/tzpfms-err)"
		elif [ -e /run/systemd/system ] && command -v systemd-ask-password > /dev/null; then  # --no-tty matches zfs and actually works
			# shellcheck disable=SC2016
			TZPFMS_PASSPHRASE_HELPER='exec systemd-ask-password --no-tty --id="tzpfms:$2" "$1: "' "$@" 2>/run/tzpfms-err; ret="$?"
		else
			# Mimic /scripts/zfs#decrypt_fs(): setting "printk" temporarily to "7" will allow prompt even if kernel option "quiet"
			read -r printk _ < /proc/sys/kernel/printk
			[ "$printk" = "7" ] || echo 7 > /proc/sys/kernel/printk

			TZPFMS_PASSPHRASE_HELPER="${TZPFMS_PASSPHRASE_HELPER:-}" "$@" REDIREXIONS; ret="$?"  # allow overriding in cmdline, but always set to raze default

			[ "$printk" = "7" ] || echo "$printk" > /proc/sys/kernel/printk
		fi
		[ -s /run/tzpfms-err ] && cat /run/tzpfms-err >&2
		[ -s /run/tzpfms-err ] && [ "$ret" -ne 0 ] && sed 's;^;'"$1"': ;' /run/tzpfms-err >> /dev/kmsg
		rm -f /run/tzpfms-err
		return "$ret"
	}
#endefine


#define POTENTIALLY_START_TCSD(LISTENING_TCP, REDIREXIONS)
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
			tcsd -f REDIREXIONS &
		fi
		tcsd_port="$(awk -F '[[:space:]]*=[[:space:]]*' '!/^[[:space:]]*#/ && !/^$/ && $1 ~ /port$/ {gsub(/[[:space:]]/, "", $2); print $2}' /etc/tcsd.conf)"
		i=0; while [ "$i" -lt 100 ] && ! LISTENING_TCP | grep -q "${tcsd_port:-30003}"; do sleep 0.1; i="$((i + 1))"; done
		[ "$i" = 100 ] && echo "Couldn't start tcsd!" >&2
	}
#endefine


#define POTENTIALLY_KILL_TCSD()
	[ -z "$TZPFMS_TPM1X" ] && command -v tcsd > /dev/null && {
		kill %+

		if [ "$lo_was_up" = "0" ]; then
			ip l set down dev lo
			# ::1 removed automatically
			ip a del 127.0.0.1/8 dev lo 2>/dev/null
		fi
	}
#endefine
