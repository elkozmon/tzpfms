# tzpfms [![builds.sr.ht badge](//builds.sr.ht/~nabijaczleweli/tzpfms.svg)](https://builds.sr.ht/~nabijaczleweli/tzpfms) [![Licence](//img.shields.io/badge/license-MIT-blue.svg?style=flat)](LICENSE)
TPM-based encryption keys for ZFS datasets.

## [Manpage](//git.sr.ht/~nabijaczleweli/tzpfms-man#NAME)

### Why?

```
T P M
 Z F S
```

Plus it's a pretty good annoyed sigh onomatopoeia.

### Building

You'll need `pkg-config`, `patchelf` (or to set `$PATCHELF=true`), and `libzfslinux-dev`<!-- , to initialise the submodules -->, and `make` should hopefully Just Work™ if you have a C++17-capable compiler.

### Installation

Copy `out/tzpfms` to `/sbin` and write a `/etc/tzpfms/{description,cmdline}`, as seen in the [manpage](//git.sr.ht/~nabijaczleweli/tzpfms/tree/trunk/man/tzpfms.md),

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

## Reporting bugs

<!-- There's [the tracker](//todo.sr.ht/~nabijaczleweli/tzpfms), but also see the list below. -->

## Contributing

<!-- Send a patch inline, as an attachment, or a git link and a ref to pull from to
[the list](//lists.sr.ht/~nabijaczleweli/tzpfms) ([~nabijaczleweli/tzpfms@lists.sr.ht](mailto:~nabijaczleweli/tzpfms)) or [me](mailto:nabijaczleweli@nabijaczleweli.xyz)
directly. I'm not picky, just please include the repo name in the subject prefix. -->

## Discussion

Please use the tracker, the list, or [Twitter](//twitter.com/nabijaczleweli/status/1315137083380559873).

## Special thanks

To all who support further development on Patreon, in particular:

  * ThePhD
  * Embark Studios
