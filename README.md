# tzpfms [![builds.sr.ht badge](//builds.sr.ht/~nabijaczleweli/tzpfms.svg)](https://builds.sr.ht/~nabijaczleweli/tzpfms) [![Licence](//img.shields.io/badge/license-MIT-blue.svg?style=flat)](LICENSE)
TPM-based encryption keys for ZFS datasets.

## [Manpages](//git.sr.ht/~nabijaczleweli/tzpfms/tree/man)

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

Both dracut (with/without Plymouth) (with/without hostonly) and initramfs-tools (with/without Plymouth) are supported for
[ZFS-on-root](https://nabijaczleweli.xyz/content/blogn_t/005-low-curse-zfs-on-root.html) set-ups.

### Building

You'll need `pkg-config`, `ronn`, `shellcheck`, `libzfslinux-dev`, `libtss2-dev`, `libtspi-dev`, and `make` should hopefully Just Work™ if you have a C++17-capable compiler.
The output binaries are trimmed of extraneous dependencies, so they're all just libc + libzfs and friends + the chosen TPM back-end, if any.

### Installation

Copy the `out/zfs-tpm*` binaries corresponding to the back-ends you want to `/sbin`,
continue as the [manual](//git.sr.ht/~nabijaczleweli/tzpfms/tree/man/zfs-tpm2-change-key.md) [page](//git.sr.ht/~nabijaczleweli/tzpfms/tree/man/zfs-tpm1x-change-key.md) instructs.

For initrd support, copy the content of either `out/dracut/` or `out/initramfs-tools/` over `/`;
these need `zfs-tpm-list` but will work with any combination of back-end `*-load-key` binaries.

<!-- #### From Debian repository

The following line in `/etc/apt/sources.list` or equivalent:
```apt
deb https://debian.nabijaczleweli.xyz sid main
```

With [my PGP key](//nabijaczleweli.xyz/pgp.txt) (the two URLs are interchangeable):
```sh
wget -O- https://debian.nabijaczleweli.xyz/nabijaczleweli.gpg.key | sudo apt-key add
# or
sudo wget -O/etc/apt/trusted.gpg.d/nabijaczleweli.asc //keybase.io/nabijaczleweli/pgp_keys.asc
```

Then the usual
```sh
sudo apt update
sudo apt install tzpfms
```
will work on amd64, x32, and i386.

See the [repository README](//debian.nabijaczleweli.xyz/README) for more information. -->

### Testing
#### TPM2

Build [`swtpm`](//github.com/stefanberger/swtpm), then prepare and run it:
```sh
swtpm_setup --tpmstate tpm2-state --tpm2 --createek --display --logfile /dev/stdout --overwrite
swtpm socket --server type=tcp,port=2321 --ctrl type=tcp,port=2322 --tpm2 --tpmstate dir=tpm2-state --flags not-need-init --log level=10
```

If your platform has a TPM, switch to `swtpm` by default:
```
ln -s /usr/lib/i386-linux-gnu/libtss2-tcti-{swtpm,default}.so
```
#### TPM1.x

Build [`swtpm`](//github.com/stefanberger/swtpm), then prepare and run it and
([hopefully](https://github.com/stefanberger/swtpm/issues/5#issuecomment-210607890)) [TrouSerS](//sourceforge.net/projects/trousers), as `root`/`tpm`:
```sh
swtpm_setup --tpmstate tpm1x-state --createek --display --logfile /dev/stdout --overwrite
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
[the list](//lists.sr.ht/~nabijaczleweli/tzpfms) ([~nabijaczleweli/tzpfms@lists.sr.ht](mailto:~nabijaczleweli/tzpfms)) or [me](mailto:nabijaczleweli@nabijaczleweli.xyz)
directly. I'm not picky, just please include the repo name in the subject prefix.

## Discussion

Please use the tracker, the list, or [Twitter](//twitter.com/nabijaczleweli/status/1315137083380559873).

## Special thanks

To all who support further development on Patreon, in particular:

  * ThePhD
  * Embark Studios
