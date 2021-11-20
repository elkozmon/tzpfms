.\" SPDX-License-Identifier: MIT
.
.Sh ENVIRONMENT VARIABLES
.Bl -tag -compact -width "TZPFMS"
.It Ev TZPFMS_PASSPHRASE_HELPER
If set and nonempty, will be run via
.Pa /bin/ Ns Nm sh Fl c
to provide a passphrase, instead of reading from the standard input stream.
.Pp
The standard output stream of the helper is tied to an anonymous file and used in its entirety as the passphrase, except for a trailing new-line, if any.
The arguments are:
.Bl -tag -compact -offset "@@" -width "@@"
.It Li $1
Pre-formatted noun phrase with all the information below, like
.Qq Passphrase for tarta-zoot
or
.Qq New passphrase for tarta-zoot (again)
.It Li $2
Either the dataset name or the element of the TPM hierarchy
.It Li $3
.Qq new
if this is for a new passphrase
.It Li $4
.Qq again
if it's the second prompt for that passphrase
.El
.Pp
If the helper doesn't exist
.Pq the shell exits with Sy 127 ,
a diagnostic is issued and the normal prompt is used as fall-back.
If it fails for any other reason, the prompting is aborted.
.El
