# tzpfms [![builds.sr.ht badge](//builds.sr.ht/~nabijaczleweli/tzpfms.svg)](//builds.sr.ht/~nabijaczleweli/tzpfms) [![Licence](//img.shields.io/badge/license-MIT-blue.svg?style=flat)](LICENSE)
TPM-based encryption keys for ZFS datasets.

## [Manpages](//srhtcdn.githack.com/~nabijaczleweli/tzpfms/blob/man/zfs-tpm-list.8.html) ([PDF](//srhtcdn.githack.com/~nabijaczleweli/tzpfms/blob/man/tzpfms.pdf))

### Why?

```
T P M
 Z F S
```

Plus it's a pretty good annoyed sigh onomatopoeia.

### What?

Essentially BitLocker, but for ZFS –
a random raw key is generated and sealed to the TPM (both 2 and 1.x supported) with an additional optional password in front of it,
tying the dataset to the platform and an additional optional secret (or to the posession of the back-up).

Additionally, 1.x TPMs support PCR binding with and without passwords.
2 TPMs support PCR binding without a password and PCR binding *OR* a password – both may be set, and any can be used to unseal (exclusive by default to prevent foot-guns).

Both dracut (with/without Plymouth) (with/without hostonly) (only on systemd systems, I don't have a test-bed for the non-systemd path)
and initramfs-tools (with/without Plymouth) are supported for [ZFS-on-root](//nabijaczleweli.xyz/content/blogn_t/005-low-curse-zfs-on-root.html) set-ups.

### Building

You'll need `pkg-config`, `shellcheck`, `libzfslinux-dev` (0.8.x and 2.[01].x work), `libtss2-dev`, `libtspi-dev`, `libssl-dev`, and `make` should hopefully Just Work™ if you have a C++17-capable compiler.
The output binaries are trimmed of extraneous dependencies, so they're all just libc + libzfs and friends + the chosen TPM back-end, if any + libcrypto for TPM2 PCR handling.

`mandoc` is required for HTML manuals. Set `MANDOC=true` to forgo this.

The default `$TZPFMS_PASSPHRASE_HELPER` is the null string.
To set a different default, set `TZPFMS_PASSPHRASE_HELPER` and `TZPFMS_PASSPHRASE_HELPER_MAN` for `make` — `$`s need to be double-escaped and `'`s need to be full-`'` escaped (i.e. `'\''`).

As an example, for a sensible default value of `exec systemd-ask-password --id="tzpfms:$2" "$1:"` for OOB systemd integration, pass `TZPFMS_PASSPHRASE_HELPER='exec systemd-ask-password --id="tzpfms:$$2" "$$1:"'` and `TZPFMS_PASSPHRASE_HELPER_MAN='Ic exec Nm systemd-ask-password Fl -id Ns Li = Ns Qo Li tzpfms:\& Ns Ar $$2 Qc Qo Ar $$1 Ns Li ":\&" Qc'`.

### Installation

Copy the `out/zfs-tpm*` binaries corresponding to the back-ends you want to `/sbin`,
continue as the [manual](//git.sr.ht/~nabijaczleweli/tzpfms/tree/man/zfs-tpm2-change-key.md) [page](//git.sr.ht/~nabijaczleweli/tzpfms/tree/man/zfs-tpm1x-change-key.md) instructs.

For initrd support, copy the content of either `out/dracut/` or `out/initramfs-tools/` over `/`;
these need `zfs-tpm-list` but will work with any combination of back-end `*-load-key` binaries
(local TPM1.X initrds need to be updated when the system state changes (e.g. the TPM is taken ownership of)).

To integrate with [zfs-mount-generator(8)](//manpages.debian.org/bookworm/zfsutils-linux/zfs-mount-generator.8.html)
[copy](//twitter.com/nabijaczleweli/status/1472986504272261124) `out/systemd/` over `/`.

#### From Debian repository

The following line in `/etc/apt/sources.list` or equivalent:
```apt
deb https://debian.nabijaczleweli.xyz sid main
```

With [my PGP key](//nabijaczleweli.xyz/pgp.txt) (the two URLs are interchangeable):
```sh
sudo wget -O/etc/apt/trusted.gpg.d/nabijaczleweli.asc https://debian.nabijaczleweli.xyz/nabijaczleweli.gpg.key
sudo wget -O/etc/apt/trusted.gpg.d/nabijaczleweli.asc https://nabijaczleweli.xyz/pgp.txt
```

Then the usual
```sh
sudo apt update
sudo apt install tzpfms-tpm2 tzpfms-dracut
```
will work on amd64, x32, and i386.

See the [repository README](//debian.nabijaczleweli.xyz/README) for more information.

### Testing
#### TPM2

Build [`swtpm`](//github.com/stefanberger/swtpm), then prepare and run it:
```sh
swtpm_setup --tpmstate tpm2-state --tpm2 --createek --display --logfile /dev/tty --overwrite
swtpm socket --server type=tcp,port=2321 --ctrl type=tcp,port=2322 --tpm2 --tpmstate dir=tpm2-state --flags not-need-init --log level=10
```

If your platform has a TPM, switch to `swtpm` by default:
```
ln -s /usr/lib/i386-linux-gnu/libtss2-tcti-{swtpm,default}.so
```
#### TPM1.x

Build [`swtpm`](//github.com/stefanberger/swtpm), then prepare and run it and
([hopefully](//github.com/stefanberger/swtpm/issues/5#issuecomment-210607890)) [TrouSerS](//sourceforge.net/projects/trousers), as `root`/`tpm`:
```sh
swtpm_setup --tpmstate tpm1x-state --createek --display --logfile /dev/tty --overwrite
swtpm cuse -n tpm --tpmstate dir=tpm1x-state --seccomp action=none --log level=10,file=/dev/fd/4 4>&1
swtpm_ioctl -i /dev/tpm
TPM_DEVICE=/dev/tpm swtpm_bios
tcsd -f

swtpm_ioctl -s /dev/tpm  # to shut down, apparently
```

If your platform has a TPM, occupy it first by running `exec 100<>/dev/tpm0` or equivalent. `tcsd` looks at `/dev/tpm0` before `/dev/tpm`.

#### initrd

Running
```sh
swtpm socket --ctrl type=unixio,path=/tmp/swtpm --tpm2 --tpmstate dir=tpm2-state --flags not-need-init --log level=10
# or
swtpm socket --ctrl type=unixio,path=/tmp/swtpm --tpmstate dir=tpm1x-state --log level=10
```
instead, alongside passing
```
-chardev socket,id=chrtpm,path=/tmp/swtpm -tpmdev emulator,id=tpm0,chardev=chrtpm -device tpm-tis,tpmdev=tpm0
```
to QEMU will create a TPM device on the guest which Should® be fully funxional.

## Reporting bugs

There's [the tracker](//todo.sr.ht/~nabijaczleweli/tzpfms), but also see the list below.

## Contributing

Send a patch inline, as an attachment, or a git link and a ref to pull from to
[the list](//lists.sr.ht/~nabijaczleweli/tzpfms) ([~nabijaczleweli/tzpfms@lists.sr.ht](mailto:~nabijaczleweli/tzpfms@lists.sr.ht)) or [me](mailto:nabijaczleweli@nabijaczleweli.xyz)
directly. I'm not picky, just please include the repo name in the subject prefix.

## Discussion

Please use the tracker, the list, or [Twitter](//twitter.com/nabijaczleweli/status/1315137083380559873).

## Special thanks

To all who support further development on Patreon, in particular:

  * ThePhD
  * Embark Studios
  * Jasper Bekkers
