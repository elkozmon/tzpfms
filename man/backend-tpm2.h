.\" SPDX-License-Identifier: MIT
.
.Sh TPM2 back-end configuration
.Ss Environment variables
.Bl -tag -compact -width "TSS2_LOG"
.It Ev TSS2_LOG
Any of:
.Sy NONE , ERROR , WARNING , INFO , DEBUG , TRACE .
Default:
.Sy WARNING .
.El
.
.Ss TPM selection
The library
.Nm libtss2-tcti-default.so
can be linked to any of the
.Pa libtss2-tcti-*.so
libraries to select the default, otherwise
.Pa /dev/tpmrm0 ,
then
.Pa /dev/tpm0 ,
then
.Pa localhost:2321
will be tried, in order
.Pq see Xr ESYS_CONTEXT 3 .
.
.Ss See also
The tpm2-tss git repository at
.Lk https:/\&/github.com/tpm2-software/tpm2-tss
and the documentation at
.Lk https:/\&/tpm2-tss.readthedocs.io .
.Pp
The TPM 2.0 specifications, mainly at
.Lk https:/\&/trustedcomputinggroup.org/resource/tpm-library-specification/ ,
.Lk https:/\&/trustedcomputinggroup.org/wp-content/uploads/TPM-Rev-2.0-Part-1-Architecture-01.38.pdf ,
and related pages.
