zfs-tpm2-clear-key(8) -- rewrap ZFS dataset key in passsword and clear tzpfms TPM2 metadata
===========================================================================================

## SYNOPSIS

`zfs-tpm2-clear-key` <dataset>

## DESCRIPTION

zfs-tpm2-clear-key(8), after verifying that `dataset` was encrypted with tzpfms backend *TPM2* will:

  1. perform the equivalent of **zfs(8) change-key -o keylocation=prompt -o keyformat=passphrase dataset**,
  2. free the sealed key previously used to encrypt `dataset`,
  3. remove the `xyz.nabijaczleweli:tzpfms.{backend,key}` properties from `dataset`.

See zfs-tpm2-change-key(8) for a detailed description.

#include "backend-tpm2.h"

#include "common.h"

## SEE ALSO

&lt;<https://git.sr.ht/~nabijaczleweli/tzpfms>&gt;
