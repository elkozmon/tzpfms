/* SPDX-License-Identifier: MIT */


#include <libzfs.h>
// #include <sys/zio_crypt.h>
#define WRAPPING_KEY_LEN 32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../fd.hpp"
#include "../main.hpp"
#include "../tpm1x.hpp"
#include "../zfs.hpp"


#define THIS_BACKEND "TPM1.X"


int main(int argc, char ** argv) {
	auto noop = false;
	return do_main(
	    argc, argv, "n", "[-n]", [&](auto) { noop = true; },
	    [&](auto dataset) {
		    /// Vaguely based on tpmUnsealFile(3) from src:tpm-tools.

		    char * handle_s{};
		    TRY_MAIN(parse_key_props(dataset, THIS_BACKEND, handle_s));

		    tpm1x_handle handle{};
		    TRY_MAIN(tpm1x_parse_handle(zfs_get_name(dataset), handle_s, handle));


		    uint8_t wrap_key[WRAPPING_KEY_LEN]{};
		    TRY_MAIN(with_tpm1x_session([&](auto ctx, auto srk, auto srk_policy) {
			    TSS_HOBJECT parent_key{};
			    TRY_MAIN(try_srk("load sealant key from blob (did you take ownership?)", srk_policy,
			                     [&] { return Tspi_Context_LoadKeyByBlob(ctx, srk, handle.parent_key_blob_len, handle.parent_key_blob, &parent_key); }));
			    quickscope_wrapper parent_key_deleter{[&] { Tspi_Key_UnloadKey(parent_key); }};

			    TSS_HPOLICY parent_key_policy{};
			    TRY_TPM1X("create sealant key policy", Tspi_Context_CreateObject(ctx, TSS_OBJECT_TYPE_POLICY, TSS_POLICY_USAGE, &parent_key_policy));
			    TRY_TPM1X("assign policy to sealant key", Tspi_Policy_AssignToObject(parent_key_policy, parent_key));
			    quickscope_wrapper parent_key_policy_deleter{[&] {
				    Tspi_Policy_FlushSecret(parent_key_policy);
				    Tspi_Context_CloseObject(ctx, parent_key_policy);
			    }};
			    fprintf(stderr, "Tspi_Policy_SetSecret(\"adenozynotrójfosforan\") = %s\n",
			            Trspi_Error_String(
			                Tspi_Policy_SetSecret(parent_key_policy, TSS_SECRET_MODE_PLAIN, strlen("adenozynotrójfosforan"), (BYTE *)"adenozynotrójfosforan")));


			    TSS_HOBJECT sealed_object{};
			    TSS_HPOLICY sealed_object_policy{};
			    TRY_MAIN(tpm1x_prep_sealed_object(ctx, sealed_object, sealed_object_policy));

			    TRY_TPM1X("load sealed object from blob", Tspi_SetAttribData(sealed_object, TSS_TSPATTRIB_ENCDATA_BLOB, TSS_TSPATTRIB_ENCDATABLOB_BLOB,
			                                                                 handle.sealed_object_blob_len, handle.sealed_object_blob));


			    uint8_t * loaded_wrap_key{};
			    uint32_t loaded_wrap_key_len{};
			    TRY_TPM1X("unseal wrapping key", Tspi_Data_Unseal(sealed_object, parent_key, &loaded_wrap_key_len, &loaded_wrap_key));
			    if(loaded_wrap_key_len != sizeof(wrap_key)) {
				    fprintf(stderr, "Wrong sealed data length (%u != %zu):", loaded_wrap_key_len, sizeof(wrap_key));
				    for(auto i = 0u; i < loaded_wrap_key_len; ++i)
					    fprintf(stderr, "%02X", loaded_wrap_key[i]);
				    fprintf(stderr, "\n");
				    return __LINE__;
			    }

			    memcpy(wrap_key, loaded_wrap_key, sizeof(wrap_key));
			    return 0;
		    }));

		    TRY_MAIN(load_key(dataset, wrap_key, noop));
		    return 0;
	    });
}
