/* SPDX-License-Identifier: MIT */


#include <libzfs.h>
// #include <sys/zio_crypt.h>
#define WRAPPING_KEY_LEN 32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tss/platform.h>
#include <tss/tspi.h>
#include <tss/tss_structs.h>
#include <tss/tss_typedef.h>

#include <trousers/trousers.h>

#include "../fd.hpp"
#include "../main.hpp"
#include "../zfs.hpp"


#define THIS_BACKEND "TPM1.X"

// I just got this out of /dev/random
static const constexpr uint8_t sealing_secret[TPM_SHA1_160_HASH_LEN]{0xB9, 0xEE, 0x71, 0x5D, 0xBE, 0x4B, 0x24, 0x3F, 0xAA, 0x81,
                                                                     0xEA, 0x04, 0x30, 0x6E, 0x06, 0x37, 0x10, 0x38, 0x3E, 0x35};


int fromxchar(uint8_t & out, char c) {
	switch(c) {
		case '0':
			out |= 0x0;
			return 0;
		case '1':
			out |= 0x1;
			return 0;
		case '2':
			out |= 0x2;
			return 0;
		case '3':
			out |= 0x3;
			return 0;
		case '4':
			out |= 0x4;
			return 0;
		case '5':
			out |= 0x5;
			return 0;
		case '6':
			out |= 0x6;
			return 0;
		case '7':
			out |= 0x7;
			return 0;
		case '8':
			out |= 0x8;
			return 0;
		case '9':
			out |= 0x9;
			return 0;
		case 'A':
		case 'a':
			out |= 0xA;
			return 0;
		case 'B':
		case 'b':
			out |= 0xB;
			return 0;
		case 'C':
		case 'c':
			out |= 0xC;
			return 0;
		case 'D':
		case 'd':
			out |= 0xD;
			return 0;
		case 'E':
		case 'e':
			out |= 0xE;
			return 0;
		case 'F':
		case 'f':
			out |= 0xF;
			return 0;
		default:
			errno = EINVAL;
			return -1;
	}
}


int main(int argc, char ** argv) {
	auto noop = false;
	return do_main(
	    argc, argv, "n", "[-n]", [&](auto) { noop = true; },
	    [&](auto dataset) {
		    /// Vaguely based on tpmUnsealFile(3) from src:tpm-tools.

		    char * handle_s{};
		    TRY_MAIN(parse_key_props(dataset, THIS_BACKEND, handle_s));
		    fprintf(stderr, "handle=%u\n", strlen(handle_s));
		    fprintf(stderr, "handle_s=%s\n", handle_s);

		    auto midpoint = strchr(handle_s, ':');
		    if(!midpoint) {
			    fprintf(stderr, "Dataset %s's handle %s not valid.\n", zfs_get_name(dataset), handle_s);
			    return __LINE__;
		    }
		    *midpoint = '\0';
		    fprintf(stderr, "handle=%u\n", strlen(handle_s));
		    fprintf(stderr, "handle_s=%s\n", handle_s);

		    auto parent_key_wide_blob    = handle_s;
		    auto sealed_object_wide_blob = midpoint + 1;

		    size_t parent_key_blob_len    = strlen(parent_key_wide_blob) / 2;
		    fprintf(stderr, "parent_key_blob_len=%u\n", parent_key_blob_len);
		    // fprintf(stderr, "parent_key_blob=%s\n", parent_key_wide_blob);
		    size_t sealed_object_blob_len = strlen(sealed_object_wide_blob) / 2;
		    fprintf(stderr, "sealed_object_blob_len=%u\n", sealed_object_blob_len);

		    // Very likely on the order of 559+310; don't try our luck with alloca()
		    void * blobs = TRY_PTR("allocate blob buffer", calloc(parent_key_blob_len + sealed_object_blob_len, 1));
		    quickscope_wrapper blobs_deleter{[=] { free(blobs); }};

		    auto parent_key_blob    = static_cast<uint8_t *>(blobs);
		    auto sealed_object_blob = static_cast<uint8_t *>(blobs) + parent_key_blob_len;

		    {
			    auto cur = parent_key_wide_blob;
			    for(size_t i = 0; i < parent_key_blob_len; ++i) {
				    TRY("parse hex parent key blob", fromxchar(parent_key_blob[i], *cur++));
				    parent_key_blob[i] <<= 4;
				    TRY("parse hex parent key blob", fromxchar(parent_key_blob[i], *cur++));
			    }
		    }
		    {
			    auto cur = sealed_object_wide_blob;
			    for(size_t i = 0; i < sealed_object_blob_len; ++i) {
				    TRY("parse hex parent key blob", fromxchar(sealed_object_blob[i], *cur++));
				    sealed_object_blob[i] <<= 4;
				    TRY("parse hex parent key blob", fromxchar(sealed_object_blob[i], *cur++));
			    }
		    }


		    // All memory lives as long as this does
		    TSS_HCONTEXT ctx{};
		    fprintf(stderr, "Tspi_Context_Create() = %s\n", Trspi_Error_String(Tspi_Context_Create(&ctx)));
		    fprintf(stderr, "Tspi_Context_Connect() = %s\n", Trspi_Error_String(Tspi_Context_Connect(ctx, nullptr)));
		    quickscope_wrapper ctx_deleter{[&] {
			    fprintf(stderr, "Tspi_Context_FreeMemory() = %s\n", Trspi_Error_String(Tspi_Context_FreeMemory(ctx, nullptr)));
			    fprintf(stderr, "Tspi_Context_Close() = %s\n", Trspi_Error_String(Tspi_Context_Close(ctx)));
		    }};


		    TSS_HKEY srk{};
		    fprintf(stderr, "Tspi_Context_LoadKeyByUUID() = %s\n",
		            Trspi_Error_String(Tspi_Context_LoadKeyByUUID(ctx, TSS_PS_TYPE_SYSTEM, TSS_UUID_SRK, &srk)));  // if fails: need to take ownership

		    TSS_HPOLICY srk_policy{};
		    fprintf(stderr, "Tspi_GetPolicyObject() = %s\n", Trspi_Error_String(Tspi_GetPolicyObject(srk, TSS_POLICY_USAGE, &srk_policy)));
		    fprintf(stderr, "Tspi_Policy_AssignToObject(srk_policy) = %s\n", Trspi_Error_String(Tspi_Policy_AssignToObject(srk_policy, srk)));
		    quickscope_wrapper srk_policy_deleter{
		        [&] { fprintf(stderr, "Tspi_Policy_FlushSecret() = %s\n", Trspi_Error_String(Tspi_Policy_FlushSecret(srk_policy))); }};

		    uint8_t well_known[TPM_SHA1_160_HASH_LEN]{};
		    fprintf(stderr, "Tspi_Policy_SetSecret(well known) = %s\n",
		            Trspi_Error_String(Tspi_Policy_SetSecret(srk_policy, TSS_SECRET_MODE_SHA1, sizeof(well_known), well_known)));

		    TSS_HKEY parent_key{};
		    fprintf(stderr, "Tspi_Context_LoadKeyByBlob() = %s\n",
		            Trspi_Error_String(Tspi_Context_LoadKeyByBlob(ctx, srk, parent_key_blob_len, parent_key_blob, &parent_key)));

		    TSS_HKEY parent_key_policy{};
		    fprintf(stderr, "Tspi_Context_CreateObject() = %s\n",
		            Trspi_Error_String(Tspi_Context_CreateObject(ctx, TSS_OBJECT_TYPE_POLICY, TSS_POLICY_USAGE, &parent_key_policy)));
		    fprintf(stderr, "Tspi_Policy_AssignToObject(parent_key_policy) = %s\n", Trspi_Error_String(Tspi_Policy_AssignToObject(parent_key_policy, parent_key)));
		    quickscope_wrapper parent_key_policy_deleter{
		        [&] { fprintf(stderr, "Tspi_Policy_FlushSecret() = %s\n", Trspi_Error_String(Tspi_Policy_FlushSecret(parent_key_policy))); }};
		    fprintf(stderr, "Tspi_Policy_SetSecret(\"adenozynotrójfosforan\") = %s\n",
		            Trspi_Error_String(
		                Tspi_Policy_SetSecret(parent_key_policy, TSS_SECRET_MODE_PLAIN, strlen("adenozynotrójfosforan"), (BYTE *)"adenozynotrójfosforan")));


		    TSS_HKEY sealed_object{};
		    fprintf(stderr, "Tspi_Context_CreateObject() = %s\n",
		            Trspi_Error_String(Tspi_Context_CreateObject(ctx, TSS_OBJECT_TYPE_ENCDATA, TSS_ENCDATA_SEAL, &sealed_object)));
		    TSS_HPOLICY sealed_object_policy{};
		    fprintf(stderr, "Tspi_GetPolicyObject() = %s\n", Trspi_Error_String(Tspi_GetPolicyObject(srk, TSS_POLICY_USAGE, &sealed_object_policy)));

		    fprintf(stderr, "Tspi_Policy_AssignToObject(sealed_object) = %s\n",
		            Trspi_Error_String(Tspi_Policy_AssignToObject(sealed_object_policy, sealed_object)));
		    fprintf(stderr, "Tspi_Policy_SetSecret(sealing_secret) = %s\n",
		            Trspi_Error_String(Tspi_Policy_SetSecret(sealed_object_policy, TSS_SECRET_MODE_SHA1, sizeof(sealing_secret), (uint8_t *)sealing_secret)));

		    fprintf(stderr, "Tspi_SetAttribData() = %s\n",
		            Trspi_Error_String(
		                Tspi_SetAttribData(sealed_object, TSS_TSPATTRIB_ENCDATA_BLOB, TSS_TSPATTRIB_ENCDATABLOB_BLOB, sealed_object_blob_len, sealed_object_blob)));


		    uint8_t * wrap_key{};
		    uint32_t wrap_key_len{};
		    fprintf(stderr, "Tspi_Data_Unseal() = %s\n", Trspi_Error_String(Tspi_Data_Unseal(sealed_object, parent_key, &wrap_key_len, &wrap_key)));
		    fprintf(stderr, "%u\n", wrap_key_len);
		    for(auto i = 0u; i < wrap_key_len; ++i)
			    fprintf(stderr, "%02X", wrap_key[i]);
		    fprintf(stderr, "\n");


		    // TODO: find out what fucks off with "Authorisation failed" on non-well-known

		    TRY_MAIN(load_key(dataset, wrap_key, noop));
		    return 0;
	    });
}
