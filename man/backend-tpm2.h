## TPM2 back-end configuration

### Environment variables

  * `TSS2_LOG`=:
    Any of: *NONE*, *ERROR*, *WARNING*, *INFO*, *DEBUG*, *TRACE*. Default: *WARNING*.

### TPM selection

The library `libtss2-tcti-default.so` can be linked to any of the `libtss2-tcti-*.so` libraries to select the default,
otherwise `/dev/tpmrm0`, then `/dev/tpm0`, then `localhost:2321` will be tried, in order (see ESYS_CONTEXT(3)).

### See also

The tpm2-tss git repository at <https://github.com/tpm2-software/tpm2-tss> and the documentation at <https://tpm2-tss.readthedocs.io>.

The TPM 2.0 specifications, mainly at &lt;<https://trustedcomputinggroup.org/wp-content/uploads/TPM-Rev-2.0-Part-1-Architecture-01.38.pdf>&gt; and related pages.
