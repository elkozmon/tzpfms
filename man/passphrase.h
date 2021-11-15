.\" SPDX-License-Identifier: MIT
.
.Sh ENVIRONMENT VARIABLES
.Bl -tag -compact -width "TZPFMS"
.It Ev TZPFMS_PASSPHRASE_HELPER
If set and nonempty, will be run as
.Dl Pa /bin/ Ns Nm sh Fl c Li \&"$TZPFMS_PASSPHRASE_HELPER" \&"$TZPFMS_PASSPHRASE_HELPER" Qo Ar prepared prompt Qc Qo Ar target Qc Qo Oo Li new Oc Qc Qo Oo Li again Oc Qc
to provide a passphrase, instead of reading from the standard input.
.Pp
The standard output stream of the helper is tied to an anonymous file and used in its entirety as the passphrase, except for a trailing new-line, if any.
The second argument contains either the dataset name or the element of the TPM hierarchy.
The third argument is
.Li new
if this is for a new passphrase, and the fourth is
.Li again
if it's the second prompt for that passphrase.
The first argument already contains all of this information, as a pre-formatted noun phrase.
.Pp
If the helper doesn't exist
.Pq the shell exits with Sy 127 ,
a diagnostic is issued and the normal prompt is used as fall-back.
If it fails for any other reason, the prompting is aborted.
.Pp
An example value would be:
.No ' Ns Nm systemd-ask-password Fl -id Ns Li = Ns Qo Li tzpfms:\& Ns Ar $2 Qc Qo Ar $1 Ns Li ": " Qc Ns ' .
.El
