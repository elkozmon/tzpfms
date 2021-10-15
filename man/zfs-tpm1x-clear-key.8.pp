.Dd
.Dt ZFS-TPM1X-CLEAR-KEY 8
.Os
.
.Sh NAME
.Nm zfs-tpm1x-clear-key
.Nd rewrap ZFS dataset key in passsword and clear tzpfms TPM1.X metadata
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
.Sy TPM1.X :
.Bl -enum -compact -offset 4n -width ""
.It
performs the equivalent of
.Nm zfs Cm change-key Fl o Li keylocation=prompt Fl o Li keyformat=passphrase Ar dataset ,
.It
removes the
.Li xyz.nabijaczleweli:tzpfms.\& Ns Brq Li backend , key
properties from
.Ar dataset .
.El
.Pp
See
.Xr zfs-tpm1x-change-key 8
for a detailed description.
.
#include "backend-tpm1x.h"
.
#include "common.h"
.
.Sh SEE ALSO
.Lk https:/\&/git.sr.ht/~nabijaczleweli/tzpfms
