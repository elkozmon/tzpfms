zfs-tpm1x-change-key(8) -- change ZFS dataset key to one stored on the TPM
==========================================================================

## SYNOPSIS

`zfs-tpm1x-change-key` [-b file] <dataset>

## DESCRIPTION

To normalise `dataset`, zfs-tpm1x-change-key(8) will open its encryption root in its stead.
zfs-tpm1x-change-key(8) will *never* create or destroy encryption roots; use **zfs(8) change-key** for that.

First, a connection is made to the TPM, which *must* be TPM-1.X-compatible.

If `dataset` was previously encrypted with tzpfms and the *TPM1.X* back-end was used, the metadata will be silently cleared.
Otherwise, or in case of an error, data required for manual intervention will be printed to the standard error stream.

Next, a new wrapping key is be generated on the TPM, optionally backed up (see [OPTIONS][]),
and sealed on the TPM;
the user is prompted for an optional passphrase to protect the key with,
and for the SRK passphrase, set when taking ownership, if it is not "well-known" (all zeroes).

The following properties are set on `dataset`:

  * `xyz.nabijaczleweli:tzpfms.backend`=`TPM1.X`
  * `xyz.nabijaczleweli:tzpfms.key`=*(parent key blob)*`:`*(sealed object blob)*

`tzpfms.backend` identifies this dataset for work with *TPM1.X*-back-ended tzpfms tools
(namely zfs-tpm1x-change-key(8), zfs-tpm1x-load-key(8), and zfs-tpm1x-clear-key(8)).

`tzpfms.key` is a colon-separated pair of hexadecimal-string (i.e. "4F7730" for "Ow0") blobs;
the first one represents the RSA key protecting the blob,
and it is protected with either the password, if provided, or the SHA1 constant *CE4CF677875B5EB8993591D5A9AF1ED24A3A8736*;
the second represents the sealed object containing the wrapping key,
and is protected with the SHA1 constant *B9EE715DBE4B243FAA81EA04306E063710383E35*.
There exists no other user-land tool for decrypting this; perhaps there should be.
#comment (TODO: make an LD_PRELOADable for extracting the key maybe)

Finally, the equivalent of **zfs(8) change-key -o keylocation=prompt -o keyformat=raw dataset** is performed with the new key.
If an error occurred, best effort is made to clean up the properties,
or to issue a note for manual intervention into the standard error stream.

A final verification should be made by running **zfs-tpm1x-load-key(8) -n dataset**.
If that command succeeds, all is well,
but otherwise the dataset can be manually rolled back to a password with **zfs-tpm1x-clear-key(8) dataset** (or, if that fails to work, **zfs(8) change-key -o keyformat=passphrase dataset**), and you are hereby asked to report a bug, please.

**zfs-tpm1x-clear-key(8) dataset** can be used to clear the properties and go back to using a password.

## OPTIONS

  * `-b` *file*:
    Save a back-up of the key to *file*, which must not exist beforehand.
    This back-up **must** be stored securely, off-site.
    In case of a catastrophic event, the key can be loaded by running **zfs(8) load-key dataset < backup-file**.

#include "backend-tpm1x.h"

#include "common.h"

## SEE ALSO

&lt;<https://git.sr.ht/~nabijaczleweli/tzpfms>&gt;
