.\" SPDX-License-Identifier: MIT
.
.Dd
.Dt ZFS-TPM2-CLEAR-KEY 8
.Os
.
.Sh NAME
.Nm zfs-tpm2-clear-key
.Nd rewrap ZFS dataset key in passsword and clear tzpfms TPM2 metadata
.Sh SYNOPSIS
.Nm
.Ar dataset
.
.Sh DESCRIPTION
After verifying
.Ar dataset
was encrypted with
.Nm tzpfms
backend
.Sy TPM2 :
.Bl -enum -compact -offset 4n -width ""
.It
performs the equivalent of
.Nm zfs Cm change-key Fl o Li keylocation=prompt Fl o Li  keyformat=passphrase Ar dataset ,
.It
frees the sealed key previously used to encrypt
.Ar dataset ,
.It
removes the
.Li xyz.nabijaczleweli:tzpfms.\& Ns Brq Li backend , key
properties from
.Ar dataset .
.El
.Pp
See
.Xr zfs-tpm2-change-key 8
for a detailed description.
.
#include "backend-tpm2.h"
.
#include "common.h"
.
.Sh SEE ALSO
.Lk https:/\&/git.sr.ht/~nabijaczleweli/tzpfms
