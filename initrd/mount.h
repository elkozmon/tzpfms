# SPDX-License-Identifier: MIT


#define WITH_PROMPTABLE_TTY(REDIREXIONS)
	# This sucks a lot of ass, since we don't know the questions or the amount thereof beforehand
	# (0-2 (owner hierarchy/ownership + sealed object, both optional) best-case and 0-6 worst-case (both entered wrong twice)).
	# Plymouth doesn't allow us to actually check what the splash status was, and ioctl(KDGETMODE) isn't reliable;
	# ideally, we'd only clear the screen if we were making the switch, but not if the user was already switched to the log output.
	# Instead, clear if there's a "quiet", leave alone otherwise, and always restore;
	# cmdline option "plymouth.ignore-show-splash" can be used to disable splashes altogether, if desired.
	with_promptable_tty() {
		if plymouth --ping 2>/dev/null; then
			plymouth hide-splash
			# shellcheck disable=SC2217
			[ "${quiet:-n}" = "y" ] && printf '\033c' REDIREXIONS

			"$@" REDIREXIONS; ret="$?"

			plymouth show-splash
		else
			# Mimic /scripts/zfs#decrypt_fs(): setting "printk" temporarily to "7" will allow prompt even if kernel option "quiet"
			read -r printk _ < /proc/sys/kernel/printk
			[ "$printk" = "7" ] || echo 7 > /proc/sys/kernel/printk

			"$@" REDIREXIONS; ret="$?"

			[ "$printk" = "7" ] || echo "$printk" > /proc/sys/kernel/printk
		fi
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
