#!/bin/sh
# SPDX-License-Identifier: MIT


# Only run on systemd systems, mimicking zfs-dracut's zfs-load-key.sh, TODO: "see mount-zfs.sh for non-systemd systems"
[ -d /run/systemd ] || exit 0


. "/lib/dracut-lib.sh"


# If root is not "ZFS=" or "zfs:", or rootfstype is not "zfs" then we aren't supposed to handle it
root="${root:=$(getarg root=)}"
rootfstype="${rootfstype:=$(getarg rootfstype=)}"
[ "${root##zfs:}" = "$root" ] && [ "${root##ZFS=}" = "$root" ] && [ "$rootfstype" != "zfs" ] && exit 0


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


# This sucks a lot of ass, since we don't know the questions or the amount thereof beforehand
# (0-2 (owner hierarchy/ownership + sealed object, both optional) best-case and 0-6 worst-case (both entered wrong twice)).
# Plymouth doesn't allow us to actually check what the splash status was, and ioctl(KDGETMODE) isn't reliable;
# ideally, we'd only clear the screen if we were making the switch, but not if the user was already switched to the log output.
# Instead, clear if there's a "quiet", leave alone otherwise, and always restore;
# cmdline option "plymouth.ignore-show-splash" can be used to disable splashes altogether, if desired.
with_promptable_tty() {
    echo "$@" > /dev/console
    if command -v plymouth > /dev/null && plymouth --ping; then
        #plymouth hide-splash
        plymouth deactivate
        grep -q 'quiet' /proc/cmdline && printf '\033c'

        "$@" < /dev/console > /dev/console 2>&1; ret="$?"

        # TODO: some combination of all this does an absolute fucky wucky
        plymouth reactivate????
        #plymouth show-splash
    else
        # Mimic /scripts/zfs#decrypt_fs(): setting "printk" temporarily to "7" will allow prompt even if kernel option "quiet"
        printk="$(awk '{print $1}' /proc/sys/kernel/printk)"
        [ "$printk" = "7" ] || echo 7 > /proc/sys/kernel/printk

        "$@" < /dev/console > /dev/console 2>&1; ret="$?"

        [ "$printk" = "7" ] || echo 7 > /proc/sys/kernel/printk
    fi
    return "$ret"
}


# If pool encryption is active and the zfs command understands '-o encryption'
if [ "$(zpool list -H -o feature@encryption "$(echo "$BOOTFS" | awk -F/ '{print $1}')")" = "active" ]; then
    ENCRYPTIONROOT="$(zfs get -H -o value encryptionroot "$BOOTFS")"

    if ! [ "${ENCRYPTIONROOT}" = "-" ]; then
        # Match this sexion to i-t/zfs-patch.sh
        if command -v zfs-tpm2-load-key > /dev/null && ! [ "$(zfs-tpm-list -Hub TPM2 "$ENCRYPTIONROOT")" = "" ]; then
            with_promptable_tty zfs-tpm2-load-key "$ENCRYPTIONROOT"
            exit
        fi

        if command -v zfs-tpm1x-load-key > /dev/null && ! [ "$(zfs-tpm-list -Hub TPM1.X "$ENCRYPTIONROOT")" = "" ]; then
            [ -z "$TZPFMS_TPM1X" ] && command -v tcsd > /dev/null && tcsd -f &
            with_promptable_tty zfs-tpm1x-load-key "$ENCRYPTIONROOT"; err="$?"
            [ -z "$TZPFMS_TPM1X" ] && command -v tcsd > /dev/null && kill %+
            exit "$err"
        fi

        # Fall through to zfs-dracut's zfs-load-key.sh
        with_promptable_tty zfs load-key "$ENCRYPTIONROOT"
        exit
    fi
fi
