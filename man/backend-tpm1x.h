.Sh TPM1.X back-end configuration
.Ss TPM selection
The
.Nm tzpfms
suite connects to a local
.Xr tcsd 8
process
.Pq at Pa localhost:30003
by default.
Use the environment variable
.Ev TZPFMS_TPM1X
to specify a remote TCS hostname.
.Pp
The TrouSerS
.Xr tcsd 8
daemon will try
.Pa /dev/tpm0 ,
then
.Pa /udev/tpm0 ,
then
.Pa /dev/tpm ;
by occupying one of the earlier ones with, for example, shell redirection, a later one can be selected.
.
.Ss See also
The TrouSerS project page at
.Lk https:/\&/sourceforge.net/projects/trousers .
.Pp
The TPM 1.2 main specification index at
.Lk https:/\&/trustedcomputinggroup.org/resource/tpm-main-specification .
