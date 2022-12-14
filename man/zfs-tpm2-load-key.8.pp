.\" SPDX-License-Identifier: MIT
.
.Dd
.Dt ZFS-TPM2-LOAD-KEY 8
.Os
.
.Sh NAME
.Nm zfs-tpm2-load-key
.Nd load TPM2-encrypted ZFS dataset key
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
.Sy TPM2 ,
unseals the key and loads it into
.Ar dataset .
.Pp
The user is prompted for the additional passphrase, set when creating the key, if one was set.
.Pp
See
.Xr zfs-tpm2-change-key 8
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
#include "passphrase.h"
.
#include "backend-tpm1x.h"
.
#include "common.h"
