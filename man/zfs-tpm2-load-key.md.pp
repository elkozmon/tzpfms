zfs-tpm2-load-key(8) -- load tzpfms TPM2-encrypted ZFS dataset key
==================================================================

## SYNOPSIS

`zfs-tpm2-load-key` [-n] <dataset>

## DESCRIPTION

zfs-tpm2-load-key(8), after verifying that `dataset` was encrypted with tzpfms backend *TPM2* will unseal the key and load it into `dataset`.

See zfs-tpm2-change-key(8) for a detailed description.

## OPTIONS

  * `-n`:
    Do a no-op/dry run, can be used even if the key is already loaded. Equivalent to **zfs(8) load-key**'s `-n` option.

#include "backend-tpm2.h"

#include "common.h"

## SEE ALSO

&lt;<https://git.sr.ht/~nabijaczleweli/tzpfms>&gt;
