.\" SPDX-License-Identifier: MIT
.
.Dd
.Dt ZFS-TPM-LIST 8
.Os
.
.Sh NAME
.Nm zfs-tpm-list
.Nd print dataset tzpfms metadata
.Sh SYNOPSIS
.Nm
.Op Fl H
.Op Fl r Ns \&| Ns Fl d Ar depth
.Op Fl a Ns \&| Ns Fl b Ar back-end
.Op Fl u Ns \&| Ns Fl l
.Oo Ar filesystem Ns \&| Ns Ar volume Oc Ns …
.
.Sh DESCRIPTION
Lists the following properties on encryption roots:
.Bl -tag -compact -offset Ds -width "keystatus"
.It Li name
.It Li back-end
the
.Nm tzpfms
back-end
.Pq e.g. Sy TPM2 No for Xr zfs-tpm2-change-key 8 or Sy TPM1.X No for Xr zfs-tpm1x-change-key 8 ,
or
.Qq Sy -
if none is configured
.It Li keystatus
.Sy available
or
.Sy unavailable
.It Li coherent
.Sy yes
if either both
.Li xyz.nabijaczleweli:tzpfms.backend
and
.Li xyz.nabijaczleweli:tzpfms.key
are present or missing,
.Sy no
otherwise
.El
.Pp
Incoherent datasets require immediate operator attention, with either the appropriate
.Nm zfs-tpm*-clear-key
program or
.Nm zfs Cm change-key
and
.Nm zfs Cm inherit
\(em if the key becomes unloaded, they will require restoration from back-up.
However, this should never occur, unless something went horribly wrong with the dataset properties.
.Pp
If no datasets are specified, all matching encryption roots are listed \(em by default, those managed by
.Nm tzpfms .
.
.Sh OPTIONS
.Bl -tag -compact -width "-b back-end"
.It Fl H
Scripting mode \(em remove headers and separate fields by a single tab instead of columnating them with spaces.
.Pp
.It Fl r
Recurse into all descendants of specified datasets.
.It Fl d Ar depth
Recurse at most
.Ar depth
datasets deep.
Default:
.Sy 0 .
.Pp
.It Fl a
List all encryption roots, even ones not managed by
.Nm tzpfms .
.It Fl b Ar back-end
List only encryption roots with the specified
.Nm tzpfms
.Ar back-end .
.Pp
.It Fl l
List only encryption roots whose keys are available.
.It Fl y
List only encryption roots whose keys are unavailable.
.El
.
.Sh EXAMPLES
.Bd -literal -compact
.Li $ Nm
NAME             BACK-END  KEYSTATUS    COHERENT
tarta-zoot       TPM1.X    available    yes
tarta-zoot/home  TPM2      unavailable  yes

.Li $ Nm Fl ad0
NAME     BACK-END  KEYSTATUS  COHERENT
filling  -         available  yes

.Li $ Nm Fl b Sy TPM2
NAME             BACK-END  KEYSTATUS    COHERENT
tarta-zoot/home  TPM2      unavailable  yes

.Li $ Nm Fl ra Ar tarta-zoot
NAME             BACK-END  KEYSTATUS    COHERENT
tarta-zoot       TPM1.X    available    yes
tarta-zoot/home  TPM2      unavailable  yes
tarta-zoot/bkp   -         available    yes
tarta-zoot/vm    -         available    yes

.Li $ Nm Fl al
NAME             BACK-END  KEYSTATUS  COHERENT
filling          -         available  yes
tarta-zoot       TPM1.X    available  yes
tarta-zoot/bkp   -         available  yes
tarta-zoot/vm    -         available  yes
.Ed
.
#include "common.h"
