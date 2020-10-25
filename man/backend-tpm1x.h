## TPM1.X back-end configuration

### TPM selection

The tzpfms suite always connects to a local tcsd(8) process (at `localhost:30003`).

The TrouSerS tcsd(8) daemon will try `/dev/tpm0`, then `/udev/tpm0`, then `/dev/tpm`;
by occupying one of the earlier ones with, for example, shell redirection, a later one can be selected.

### See also

The TrouSerS project page at <https://sourceforge.net/projects/trousers>.

The TPM 1.2 main specification index at &lt;<https://trustedcomputinggroup.org/resource/tpm-main-specification>&gt;.
