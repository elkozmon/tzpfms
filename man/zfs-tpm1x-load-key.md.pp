zfs-tpm1x-load-key(8) -- load tzpfms TPM1.X-encrypted ZFS dataset key
=====================================================================

## SYNOPSIS

`zfs-tpm1x-load-key` [-n] <dataset>

## DESCRIPTION

zfs-tpm1x-load-key(8), after verifying that `dataset` was encrypted with tzpfms backend *TPM1.X* will unseal the key and load it into `dataset`.

The user is prompted for, first, the SRK passphrase, set when taking ownership, if it's not "well-known" (all zeroes),
then the additional passphrase set when creating the key, if it was provided.

See zfs-tpm1x-change-key(8) for a detailed description.

## OPTIONS

  * `-n`:
    Do a no-op/dry run, can be used even if the key is already loaded. Equivalent to **zfs(8) load-key**'s `-n` option.

#include "backend-tpm1x.h"

#include "common.h"

## SEE ALSO

&lt;<https://git.sr.ht/~nabijaczleweli/tzpfms>&gt;
