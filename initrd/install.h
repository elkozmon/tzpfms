# SPDX-License-Identifier: MIT


#define INSTALL_TPM1X(PREREQ, TARGET_DIR, INST_UDEV, INST_CFG, INST_STATE, INST_LIB)
	command -v tcsd > /dev/null && {
		PREREQ

		# tcsd exits if it doesn't have tss group and user
		grep tss /etc/group  >> "${TARGET_DIR:-MUST_EXIST}/etc/group"
		grep tss /etc/passwd >> "${TARGET_DIR:-MUST_EXIST}/etc/passwd"

		for f in /lib/udev/rules.d/*tpm*; do
			INST_UDEV "$f"
		done

		if [ -e /etc/tcsd.conf ]; then
			INST_CFG /etc/tcsd.conf
			chown tss:tss "${TARGET_DIR:-MUST_EXIST}/etc/tcsd.conf"
			system_ps_file="$(awk -F '[[:space:]]*=[[:space:]]*' '!/^[[:space:]]*#/ && !/^$/ && $1 ~ /system_ps_file$/ {gsub(/[[:space:]]*$/, "", $2); print $2}' /etc/tcsd.conf)"
			system_ps_file="${system_ps_file:-/var/lib/tpm/system.data}"
		fi

		# tcsd can't find SRK if state not present, refuses to start if it doesn't have parent to tss dir
		mkdir -p "${TARGET_DIR:-MUST_EXIST}/$(dirname "$(dirname "$system_ps_file")")"
		[ -f "$system_ps_file" ] && INST_STATE "$system_ps_file"
	}

	# localhost needs to resolve at the very least
	for f in /etc/hosts /etc/resolv.conf /etc/host.conf /etc/gai.conf; do
		[ -e "$f" ] && INST_CFG "$f"
	done

	if [ -e /etc/nsswitch.conf ]; then
		INST_CFG /etc/nsswitch.conf
		databases="$(awk '/^group|hosts/ {for(i = 2; i <= NF; ++i) print $i}' /etc/nsswitch.conf | sort | uniq)"
		for db in $databases; do
			for f in /lib/*/"libnss_$db"*; do
				INST_LIB "$f"
			done
		done
	fi
#endefine
