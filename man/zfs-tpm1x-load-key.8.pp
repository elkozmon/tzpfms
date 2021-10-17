.\" SPDX-License-Identifier: MIT
.
.Dd
.Dt ZFS-TPM1X-LOAD-KEY 8
.Os
.
.Sh NAME
.Nm zfs-tpm1x-load-key
.Nd load tzpfms TPM1.X-encrypted ZFS dataset key
.Sh SYNOPSIS
.Nm
.Op Fl n
.Ar dataset
.
.Sh DESCRIPTION
After verifying
.Ar dataset
was encrypted with
.Nm tzpfms
backend
.Sy TPM1.X
will unseal the key and load it into
.Ar dataset .
.Pp
The user is prompted for, first, the SRK passphrase, set when taking ownership, if it's not "well-known" (all zeroes),
then the additional passphrase set when creating the key, if it was provided.
.Pp
See
.Xr zfs-tpm1x-change-key 8
for a detailed description.
.
.Sh OPTIONS
.Bl -tag -compact -width "-n"
.It Fl n
Do a no-op/dry run, can be used even if the key is already loaded.
Equivalent to
.Nm zfs Cm load-key Ns 's
.Fl n
option.
.El
.
#include "backend-tpm1x.h"
.
#include "common.h"
.
.Sh SEE ALSO
.Lk https:/\&/git.sr.ht/~nabijaczleweli/tzpfms
