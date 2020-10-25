zfs-tpm1x-clear-key(8) -- rewrap ZFS dataset key in passsword and clear tzpfms TPM1.X metadata
==============================================================================================

## SYNOPSIS

`zfs-tpm1x-clear-key` <dataset>

## DESCRIPTION

zfs-tpm1x-clear-key(8), after verifying that `dataset` was encrypted with tzpfms backend *TPM1.X* will:

  1. perform the equivalent of **zfs(8) change-key -o keylocation=prompt -o keyformat=passphrase dataset**,
  2. remove the `xyz.nabijaczleweli:tzpfms.{backend,key}` properties from `dataset`.

See zfs-tpm1x-change-key(8) for a detailed description.

#include "backend-tpm1x.h"

#include "common.h"

## SEE ALSO

&lt;<https://git.sr.ht/~nabijaczleweli/tzpfms>&gt;
