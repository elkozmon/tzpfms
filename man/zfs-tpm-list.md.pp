zfs-tpm-list(8) -- print dataset tzpfms metadata
================================================

## SYNOPSIS

`zfs-tpm-list` [-H] [-r\|-d *depth*] [-a\|-b *back-end*] [-u\|-l] [*filesystem*\|*volume*]…

## DESCRIPTION

zfs-tpm-list(8) lists the following properties on encryption roots:

  * `name`,
  * `back-end`: the tzpfms back-end (e.g. "TPM2" for zfs-tpm2-change-key(8) or "TPM1.X" for zfs-tpm1x-change-key(8)),
                or "-" if none is configured,
  * `keystatus`: "available" or "unavailable",
  * `coherent`: "yes" if either both `xyz.nabijaczleweli:tzpfms.backend` and `xyz.nabijaczleweli:tzpfms.key` are present or missing, "no" otherwise.

Incoherent datasets require immediate operator attention, with either the appropriate zfs-tpm\*-clear-key program or zfs(8) change-key and zfs(8) inherit —
if the key becomes unloaded, they will require restoration from back-up.
However, they should never occur, unless something went terribly wrong with the dataset properties.

If no datasets are specified, lists all matching encryption roots.
The default filter is to list all roots managed by tzpfms.
The `-a` and `-b` [OPTIONS]() can be used to either list all roots or only ones backed by a particular end, respectively.

## OPTIONS

  * `-H`:
    Used for scripting mode. Do not print headers and separate fields by a single tab instead of arbitrary white space.

  * `-r`:
    Recurse into all descendant datasets. Default if no datasets listed on the command-line.
  * `-d` *depth*:
    Recurse at most *depth* datasets deep. Defaults to zero if datasets were listed on the command-line.

  * `-a`:
    List all encryption roots, even ones not managed by tzpfms.
  * `-b` *back-end*:
    List only encryption roots with tzpfms back-end *back-end*.

  * `-l`:
    List only encryption roots whose keys are available.
  * `-u`:
    List only encryption roots whose keys are unavailable.

## EXAMPLES

    $ zfs-tpm-list
    NAME      BACK-END  KEYSTATUS    COHERENT
    owo/venc  TPM2      unavailable  yes
    owo/enc   TPM1.X    available    yes

    $ zfs-tpm-list -ad0
    NAME  BACK-END  KEYSTATUS  COHERENT
    awa   -         available  yes

    $ zfs-tpm-list -b TPM2
    NAME      BACK-END  KEYSTATUS    COHERENT
    owo/venc  TPM2      unavailable  yes

    $ zfs-tpm-list -ra owo
    NAME      BACK-END  KEYSTATUS    COHERENT
    owo/venc  TPM2      unavailable  yes
    owo/vtnc  -         available    yes
    owo/v nc  -         available    yes
    owo/enc   TPM1.X    available    yes

    $ zfs-tpm-list -al
    NAME      BACK-END  KEYSTATUS  COHERENT
    awa       -         available  yes
    owo/vtnc  -         available  yes
    owo/v nc  -         available  yes
    owo/enc   TPM1.X    available  yes


#include "common.h"

## SEE ALSO

&lt;<https://git.sr.ht/~nabijaczleweli/tzpfms>&gt;
