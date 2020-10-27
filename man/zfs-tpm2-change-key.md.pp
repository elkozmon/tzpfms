zfs-tpm2-change-key(8) -- change ZFS dataset key to one stored on the TPM
=========================================================================

## SYNOPSIS

`zfs-tpm2-change-key` [-b file] <dataset>

## DESCRIPTION

To normalise `dataset`, zfs-tpm2-change-key(8) will open its encryption root in its stead.
zfs-tpm2-change-key(8) will *never* create or destroy encryption roots; use **zfs(8) change-key** for that.

First, a connection is made to the TPM, which *must* be TPM-2.0-compatible.

If `dataset` was previously encrypted with tzpfms and the *TPM2* back-end was used, the previous key will be freed from the TPM.
Otherwise, or in case of an error, data required for manual intervention will be printed to the standard error stream.

Next, a new wrapping key is be generated on the TPM, optionally backed up (see [OPTIONS][]),
and sealed to a persistent object on the TPM under the owner hierarchy;
the user is also prompted for an optional passphrase to protect the object with.

The following properties are set on `dataset`:

  * `xyz.nabijaczleweli:tzpfms.backend`=`TPM2`
  * `xyz.nabijaczleweli:tzpfms.key`=*(ID of persistent object)*

`tzpfms.backend` identifies this dataset for work with *TPM2*-back-ended tzpfms tools
(namely zfs-tpm2-change-key(8), zfs-tpm2-load-key(8), and zfs-tpm2-clear-key(8)).

`tzpfms.key` is an integer representing the sealed object;
if needed, it can be passed to **tpm2_unseal(1) -c ${tzpfms.key} [-p ${password}]** or equivalent for back-up (see [OPTIONS][]).
If you have a sealed key you can access with that or equivalent tool and set both of these properties, it will funxion seamlessly.

Finally, the equivalent of **zfs(8) change-key -o keylocation=prompt -o keyformat=raw dataset** is performed with the new key.
If an error occurred, best effort is made to clean up the persistent object and properties,
or to issue a note for manual intervention into the standard error stream.

A final verification should be made by running **zfs-tpm2-load-key(8) -n dataset**.
If that command succeeds, all is well,
but otherwise the dataset can be manually rolled back to a password with **zfs-tpm2-clear-key(8) dataset** (or, if that fails to work, **zfs(8) change-key -o keyformat=passphrase dataset**), and you are hereby asked to report a bug, please.

**zfs-tpm2-clear-key(8) dataset** can be used to free the TPM persistent object and go back to using a password.

## OPTIONS

  * `-b` *file*:
    Save a back-up of the key to *file*, which must not exist beforehand.
    This back-up **must** be stored securely, off-site.
    In case of a catastrophic event, the key can be loaded by running **zfs(8) load-key dataset < backup-file**.

#include "backend-tpm2.h"

#include "common.h"

## SEE ALSO

&lt;<https://git.sr.ht/~nabijaczleweli/tzpfms>&gt;
