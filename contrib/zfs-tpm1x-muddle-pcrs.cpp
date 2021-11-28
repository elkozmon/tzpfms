/* SPDX-License-Identifier: MIT */


#include <stdio.h>
#include <sys/random.h>

#include "../main.hpp"
#include "../tpm1x.hpp"


#define THIS_BACKEND "TPM1.X"


int main(int argc, char ** argv) {
	uint32_t * pcrs{};
	size_t pcrs_len{};
	bool just_read{};
	return do_main(
	    argc, argv, "RP:", "[-R] [-P PCR[,PCR]...]",
	    [&](auto o) {
		    switch(o) {
			    case 'R':
				    return just_read = true, 0;
			    case 'P':
				    return tpm1x_parse_pcrs(optarg, pcrs, pcrs_len);
			    default:
				    __builtin_unreachable();
		    }
	    },
	    [&](auto) {
		    return with_tpm1x_session([&](auto ctx, auto, auto) {
			    TSS_HTPM tpm_h{};
			    TRY_TPM1X("extract TPM from context", Tspi_Context_GetTpmObject(ctx, &tpm_h));


			    for(size_t i = 0; i < pcrs_len; i++) {
				    char buf[512];
				    snprintf(buf, sizeof(buf), "muddle PCR %" PRIu32 "", pcrs[i]);

				    BYTE * val{};
				    uint32_t val_len{};

				    if(just_read)
					    TRY_TPM1X(buf, Tspi_TPM_PcrRead(tpm_h, pcrs[i], &val_len, &val));
				    else {
					    BYTE data[TPM_SHA1_160_HASH_LEN];
					    getrandom(data, sizeof(data), 0);
					    TRY_TPM1X(buf, Tspi_TPM_PcrExtend(tpm_h, pcrs[i], sizeof(data), data, nullptr, &val_len, &val));
				    }


				    printf("PCR%u: ", pcrs[i]);
				    for(auto i = 0u; i < val_len; ++i)
					    printf("%02hhX", ((uint8_t *)val)[i]);
				    printf("\n");
			    }
			    return 0;
		    });
	    });
}
