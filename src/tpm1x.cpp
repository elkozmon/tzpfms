/* SPDX-License-Identifier: MIT */


#include "tpm1x.hpp"

#include "main.hpp"

#include <stdlib.h>


/// Used as secret for the sealed object itself
// I just got this out of /dev/random
static const constexpr uint8_t sealing_secret[TPM_SHA1_160_HASH_LEN]{0xB9, 0xEE, 0x71, 0x5D, 0xBE, 0x4B, 0x24, 0x3F, 0xAA, 0x81,
                                                                     0xEA, 0x04, 0x30, 0x6E, 0x06, 0x37, 0x10, 0x38, 0x3E, 0x35};


static int fromxchar(uint8_t & out, char c);


tpm1x_handle::~tpm1x_handle() {
	free(this->parent_key_blob);
}

int tpm1x_parse_handle(const char * dataset_name, char * handle_s, tpm1x_handle & handle) {
	auto midpoint = strchr(handle_s, ':');
	if(!midpoint)
		return fprintf(stderr, "Dataset %s's handle %s not valid.\n", dataset_name, handle_s), __LINE__;
	*midpoint = '\0';

	auto parent_key_wide_blob    = handle_s;
	auto sealed_object_wide_blob = midpoint + 1;

	handle.parent_key_blob_len    = strlen(parent_key_wide_blob) / 2;
	handle.sealed_object_blob_len = strlen(sealed_object_wide_blob) / 2;


	handle.parent_key_blob    = static_cast<uint8_t *>(TRY_PTR("allocate blob buffer", calloc(handle.parent_key_blob_len + handle.sealed_object_blob_len, 1)));
	handle.sealed_object_blob = handle.parent_key_blob + handle.parent_key_blob_len;

	{
		auto cur = parent_key_wide_blob;
		for(size_t i = 0; i < handle.parent_key_blob_len; ++i) {
			TRY("parse hex parent key blob", fromxchar(handle.parent_key_blob[i], *cur++));
			handle.parent_key_blob[i] <<= 4;
			TRY("parse hex parent key blob", fromxchar(handle.parent_key_blob[i], *cur++));
		}
	}
	{
		auto cur = sealed_object_wide_blob;
		for(size_t i = 0; i < handle.sealed_object_blob_len; ++i) {
			TRY("parse hex sealed object blob", fromxchar(handle.sealed_object_blob[i], *cur++));
			handle.sealed_object_blob[i] <<= 4;
			TRY("parse hex sealed object blob", fromxchar(handle.sealed_object_blob[i], *cur++));
		}
	}

	return 0;
}


int tpm1x_prep_sealed_object(TSS_HCONTEXT ctx, TSS_HOBJECT & sealed_object, TSS_HPOLICY & sealed_object_policy) {
	bool ok = false;

	TRY_TPM1X("create sealed object", Tspi_Context_CreateObject(ctx, TSS_OBJECT_TYPE_ENCDATA, TSS_ENCDATA_SEAL, &sealed_object));
	quickscope_wrapper sealed_object_deleter{[&] {
		if(!ok)
			Tspi_Context_CloseObject(ctx, sealed_object);
	}};
	TRY_TPM1X("create sealed object policy", Tspi_Context_CreateObject(ctx, TSS_OBJECT_TYPE_POLICY, TSS_POLICY_USAGE, &sealed_object_policy));
	quickscope_wrapper sealed_object_policy_deleter{[&] {
		if(!ok)
			Tspi_Context_CloseObject(ctx, sealed_object_policy);
	}};

	TRY_TPM1X("assign policy to sealed object", Tspi_Policy_AssignToObject(sealed_object_policy, sealed_object));
	TRY_TPM1X("set secret on sealed object policy",
	          Tspi_Policy_SetSecret(sealed_object_policy, TSS_SECRET_MODE_SHA1, sizeof(sealing_secret), (uint8_t *)sealing_secret));

	ok = true;
	return 0;
}


/// This feels suboptimal somehow, and yet
static int fromxchar(uint8_t & out, char c) {
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
