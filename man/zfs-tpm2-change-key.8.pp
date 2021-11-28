.\" SPDX-License-Identifier: MIT
.
.Dd
.Dt ZFS-TPM2-CHANGE-KEY 8
.Os
.
.Sh NAME
.Nm zfs-tpm2-change-key
.Nd change ZFS dataset key to one stored on the TPM
.Sh SYNOPSIS
.Nm
.Op Fl b Ar backup-file
.Oo
.Fl P Ar algorithm Ns Cm \&: Ns Ar PCR Ns Oo Ns Cm \&, Ns Ar PCR Oc Ns … Ns Oo Cm + Ns Ar algorithm Ns Cm \&: Ns Ar PCR Ns Oo Ns Cm \&, Ns Ar PCR Oc Ns … Oc Ns …
.Op Fl A
.Oc
.Ar dataset
.
.Sh DESCRIPTION
To normalise
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
be TPM-2.0-compatible.
.Pp
If
.Ar dataset
was previously encrypted with
.Nm tzpfms
and the
.Sy TPM2
back-end was used, the previous key will be freed from the TPM.
Otherwise, or in case of an error, data required for manual intervention will be printed to the standard error stream.
.Pp
Next, a new wrapping key is generated on the TPM, optionally backed up
.Pq see Sx OPTIONS ,
and sealed to a persistent object on the TPM under the owner hierarchy;
if there is a passphrase set on the owner hierarchy, the user is prompted for it;
the user is always prompted for an optional passphrase to protect the sealed object with.
.Pp
The following properties are set on
.Ar dataset :
.Bl -bullet -compact -offset 4n -width "@"
.It
.Li xyz.nabijaczleweli:tzpfms.backend Ns = Ns Sy TPM2
.It
.Li xyz.nabijaczleweli:tzpfms.key Ns = Ns Ar persistent-object-ID Ns Op Cm ;\& Ar algorithm Ns Cm \&: Ns Ar PCR Ns Oo Ns Cm \&, Ns Ar PCR Oc Ns … Ns Oo Cm + Ns Ar algorithm Ns Cm \&: Ns Ar PCR Ns Oo Ns Cm \&, Ns Ar PCR Oc Ns … Oc Ns …
.El
.Pp
.Li tzpfms.backend
identifies this dataset for work with
.Sy TPM2 Ns -back-ended
.Nm tzpfms
tools
.Pq namely Xr zfs-tpm2-change-key 8 , Xr zfs-tpm2-load-key 8 , and Xr zfs-tpm2-clear-key 8 .
.Pp
.Li tzpfms.key
is an integer representing the sealed object, optionally followed by a semicolon and PCR list as specified with
.Fl P ,
normalised to be
.Nm tpm-tools Ns -toolchain-compatible ;
if needed, it can be passed to
.Nm tpm2_unseal Fl c Ev ${tzpfms.key Ns Cm %% Ns Li ;* Ns Ev }\&
with
.Fl p Qq Li str:\& Ns Ev ${passphrase}
or
.Fl p Qq Li pcr:\& Ns Ev ${tzpfms.key Ns Cm # Ns Li *; Ns Ev }\& ,
as the case may be, or equivalent, for back-up
.Pq see Sx OPTIONS .
If you have a sealed key you can access with that or equivalent tool and set both of these properties, it will funxion seamlessly.
.Pp
Finally, the equivalent of
.Nm zfs Cm change-key Fl o Li keylocation=prompt Fl o Li keyformat=raw Ar dataset
is performed with the new key.
If an error occurred, best effort is made to clean up the persistent object and properties,
or to issue a note for manual intervention into the standard error stream.
.Pp
A final verification should be made by running
.Nm zfs-tpm2-load-key Fl n Ar dataset .
If that command succeeds, all is well,
but otherwise the dataset can be manually rolled back to a passphrase with
.Nm zfs-tpm2-clear-key Ar dataset
.Pq or, if that fails to work, Nm zfs Cm change-key Fl o Li keyformat=passphrase Ar dataset ,
and you are hereby asked to report a bug, please.
.Pp
.Nm zfs-tpm2-clear-key Ar dataset
can be used to free the TPM persistent object and go back to using a passphrase.
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
.It Fl P Ar algorithm Ns Cm \&: Ns Ar PCR Ns Oo Ns Cm \&, Ns Ar PCR Oc Ns … Ns Oo Cm + Ns Ar algorithm Ns Cm \&: Ns Ar PCR Ns Oo Ns Cm \&, Ns Ar PCR Oc Ns … Oc Ns …
Bind the key to space- or comma-separated
.Ar PCR Ns s
within their corresponding hashing
.Ar algorithm
\(em if they change, the wrapping key will not be able to be unsealed.
There are
.Sy 24
PCRs, numbered
.Sy 0 Ns .. Ns Sy 23 .
.Pp
.Ar algorithm
may be any of case-insensitive
.Qq Sy sha1 ,
.Qq Sy sha256 ,
.Qq Sy sha384 ,
.Qq Sy sha512 ,
.Qq Sy sm3_256 ,
.Qq Sy sm3-256 ,
.Qq Sy sha3_256 ,
.Qq Sy sha3-256 ,
.Qq Sy sha3_384 ,
.Qq Sy sha3-384 ,
.Qq Sy sha3_512 ,
or
.Qq Sy sha3-512 ,
and must be supported by the TPM.
.Pp
.
.It Fl A
With
.Fl P ,
also prompt for a passphrase.
This is skipped by default because the passphrase is
.Em OR Ns ed
with the PCR policy \(em the wrapping key can be unsealed
.Em either
passphraseless with the right PCRs
.Em or
with the passphrase, and this is usually not the intent.
.El
.
#include "passphrase.h"
.
#include "backend-tpm2.h"
.
#include "common.h"
.
.Sh SEE ALSO
.Xr tpm2_unseal 1
.Pp
.\" Match this to zfs-tpm1x-change-key.8:
PCR allocations:
.Lk https:/\&/wiki.archlinux.org/title/Trusted_Platform_Module#Accessing_PCR_registers
and
.Lk https:/\&/trustedcomputinggroup.org/wp-content/uploads/PC-ClientSpecific_Platform_Profile_for_TPM_2p0_Systems_v51.pdf ,
Section 2.3.4 "PCR Usage", Table 1.
