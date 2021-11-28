.\" SPDX-License-Identifier: MIT
.
.Dd
.Dt ZFS-TPM1X-CHANGE-KEY 8
.Os
.
.Sh NAME
.Nm zfs-tpm1x-change-key
.Nd change ZFS dataset key to one stored on the TPM
.Sh SYNOPSIS
.Nm
.Op Fl b Ar backup-file
.Op Fl P Ar PCR Ns Oo Ns Cm \&, Ns Ar PCR Oc Ns …
.Ar dataset
.
.Sh DESCRIPTION
To normalise the
.Ar dataset ,
.Nm
will open its encryption root in its stead.
.Nm
will
.Em never
create or destroy encryption roots; use
.Xr zfs-change-key 8
for that.
.Pp
First, a connection is made to the TPM, which
.Em must
be TPM-1.X-compatible.
.Pp
If
.Ar dataset
was previously encrypted with
.Nm tzpfms
and the
.Sy TPM1.X
back-end was used, the metadata will be silently cleared.
Otherwise, or in case of an error, data required for manual intervention will be printed to the standard error stream.
.Pp
Next, a new wrapping key is generated on the TPM, optionally backed up
.Pq see Sx OPTIONS ,
and sealed on the TPM;
the user is prompted for an optional passphrase to protect the key with,
and for the SRK passphrase, set when taking ownership, if not "well-known" (all zeroes).
.Pp
The following properties are set on
.Ar dataset :
.Bl -bullet -compact -offset 4n -width "@"
.It
.Li xyz.nabijaczleweli:tzpfms.backend Ns = Ns Sy TPM1.X
.It
.Li xyz.nabijaczleweli:tzpfms.key Ns = Ns Ar parent-key-blob Ns Cm \&: Ns Ar sealed-object-blob
.El
.Pp
.Li tzpfms.backend
identifies this dataset for work with
.Sy TPM1.X Ns -back-ended
.Nm tzpfms
tools
.Pq namely Xr zfs-tpm1x-change-key 8 , Xr zfs-tpm1x-load-key 8 , and Xr zfs-tpm1x-clear-key 8 .
.Pp
.Li tzpfms.key
is a colon-separated pair of hexadecimal-string (i.e. "4F7730" for "Ow0") blobs;
the first one represents the RSA key protecting the blob,
and it is protected with either the passphrase, if provided, or the SHA1 constant
.Li CE4CF677875B5EB8993591D5A9AF1ED24A3A8736 ;
the second represents the sealed object containing the wrapping key,
and is protected with the SHA1 constant
.Li B9EE715DBE4B243FAA81EA04306E063710383E35 .
There exists no other user-land tool for decrypting this; perhaps there should be.
.\"" TODO: make an LD_PRELOADable for extracting the key maybe?
.Pp
Finally, the equivalent of
.Nm zfs Cm change-key Fl o Li keylocation=prompt Fl o Li keyformat=raw Ar dataset
is performed with the new key.
If an error occurred, best effort is made to clean up the properties,
or to issue a note for manual intervention into the standard error stream.
.Pp
A final verification should be made by running
.Nm zfs-tpm1x-load-key Fl n Ar dataset .
If that command succeeds, all is well,
but otherwise the dataset can be manually rolled back to a passphrase with
.Nm zfs-tpm1x-clear-key Ar dataset
.Pq or, if that fails to work, Nm zfs Cm change-key Fl o Li keyformat=passphrase Ar dataset ,
and you are hereby asked to report a bug, please.
.Pp
.Nm zfs-tpm1x-clear-key Ar dataset
can be used to clear the properties and go back to using a passphrase.
.
.Sh OPTIONS
.Bl -tag -compact -width "-b backup-file"
.It Fl b Ar backup-file
Save a back-up of the key to
.Ar backup-file ,
which must not exist beforehand.
This back-up
.Em must
be stored securely, off-site.
In case of a catastrophic event, the key can be loaded by running
.Dl Nm zfs Cm load-key Ar dataset Li < Ar backup-file
.Pp
.
.It Fl P Ar PCR Ns Oo Ns Cm \&, Ns Ar PCR Oc Ns …
Bind the key to space- or comma-separated
.Ar PCR Ns s
\(em if they change, the wrapping key will not be able to be unsealed.
The minimum number of PCRs for a PC TPM is
.Sy 24 Pq numbered Sy 0 Ns .. Ns Sy 23 .
For most, this is also the maximum.
.El
.
#include "passphrase.h"
.
#include "backend-tpm1x.h"
.
#include "common.h"
.
.Sh SEE ALSO
.\" Match this to zfs-tpm2-change-key.8:
PCR allocations:
.Lk https:/\&/wiki.archlinux.org/title/Trusted_Platform_Module#Accessing_PCR_registers
and
.Lk https:/\&/trustedcomputinggroup.org/wp-content/uploads/PC-ClientSpecific_Platform_Profile_for_TPM_2p0_Systems_v51.pdf ,
Section 2.3.4 "PCR Usage", Table 1.
