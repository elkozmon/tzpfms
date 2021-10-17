#!/bin/sh
# SPDX-License-Identifier: MIT


#include "../mount.h"


# Only run on systemd systems, mimicking zfs-dracut's zfs-load-key.sh; TODO: "see mount-zfs.sh for non-systemd systems", confer README
[ -d /run/systemd ] || exit 0


. "/lib/dracut-lib.sh"


# If root is not "ZFS=" or "zfs:", or rootfstype is not "zfs" then we aren't supposed to handle it
root="${root:=$(getarg root=)}"
rootfstype="${rootfstype:=$(getarg rootfstype=)}"
[ "${root##zfs:}" = "$root" ] && [ "${root##ZFS=}" = "$root" ] && [ "$rootfstype" != "zfs" ] && exit 0

TZPFMS_TPM1X="$(getarg TZPFMS_TPM1X=)"
[ -z "$TZPFMS_TPM1X" ] || export TZPFMS_TPM1X

getarg 0 quiet && quiet=y


# There is a race between the zpool import and the pre-mount hooks, so we wait for a pool to be imported
while [ "$(zpool list -H)" = "" ]; do
    sleep 0.1s
    systemctl is-failed --quiet zfs-import-cache.service zfs-import-scan.service && exit 1
done


if [ "$root" = "zfs:AUTO" ] ; then
    BOOTFS="$(zpool list -H -o bootfs | awk '!/^-$/ {print; exit}')"
else
    BOOTFS="${root##zfs:}"
    BOOTFS="${BOOTFS##ZFS=}"
fi


WITH_PROMPTABLE_TTY{< /dev/console > /dev/console 2>&1}


# If pool encryption is active and the zfs command understands '-o encryption'
if [ "$(zpool list -H -o feature@encryption "${BOOTFS%%/*}")" = "active" ]; then
    ENCRYPTIONROOT="$(zfs get -H -o value encryptionroot "$BOOTFS")"

    if ! [ "${ENCRYPTIONROOT}" = "-" ]; then
        # Match this sexion to i-t/zfs-patch.sh
        if command -v zfs-tpm2-load-key > /dev/null && [ -n "$(zfs-tpm-list -Hub TPM2 "$ENCRYPTIONROOT")" ]; then
            with_promptable_tty zfs-tpm2-load-key "$ENCRYPTIONROOT"
            exit
        fi

        if command -v zfs-tpm1x-load-key > /dev/null && [ -n "$(zfs-tpm-list -Hub TPM1.X "$ENCRYPTIONROOT")" ]; then
            POTENTIALLY_START_TCSD{ss -ltO, > /dev/console 2>&1}
            with_promptable_tty zfs-tpm1x-load-key "$ENCRYPTIONROOT"; err="$?"
            POTENTIALLY_KILL_TCSD{}
            exit "$err"
        fi

        # Fall through to zfs-dracut's zfs-load-key.sh
    fi
fi
