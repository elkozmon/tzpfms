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
Next, a new wrapping key is be generated on the TPM, optionally backed up
.Pq see Sx OPTIONS ,
and sealed to a persistent object on the TPM under the owner hierarchy;
if there is a passphrase set on the owner hierarchy, the user is prompted for it;
the user is always prompted for an optional passphrase to protect the sealed object with.
.Pp
The following properties are set on
.Ar dataset :
.Bl -bullet -compact -offset 4n -width ""
.\"" TODO: width?
.It
.Li xyz.nabijaczleweli:tzpfms.backend Ns = Ns Sy TPM2
.It
.Li xyz.nabijaczleweli:tzpfms.key Ns = Ns Ar ID of persistent object
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
is an integer representing the sealed object;
if needed, it can be passed to
.Nm tpm2_unseal Fl c Ev ${tzpfms.key} Op Fl p Ev ${password}
or equivalent for back-up
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
but otherwise the dataset can be manually rolled back to a password with
.Nm zfs-tpm2-clear-key Ar dataset
.Pq or, if that fails to work, Nm zfs Cm change-key Fl o Li keyformat=passphrase Ar dataset ,
and you are hereby asked to report a bug, please.
.Pp
.Nm zfs-tpm2-clear-key Ar dataset
can be used to free the TPM persistent object and go back to using a password.
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
.El
.
#include "backend-tpm2.h"
.
#include "common.h"
.
.Sh SEE ALSO
.Xr tpm2_unseal 1
.Pp
.Lk https:/\&/git.sr.ht/~nabijaczleweli/tzpfms
