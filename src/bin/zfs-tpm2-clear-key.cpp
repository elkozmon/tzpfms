/* SPDX-License-Identifier: MIT */


#include "../main_clear.hpp"
#include "../tpm2.hpp"


#define THIS_BACKEND "TPM2"


int main(int argc, char ** argv) {
	TPMI_DH_PERSISTENT persistent_handle{};
	return do_clear_main(
	    argc, argv, THIS_BACKEND,
	    [&](auto dataset, auto persistent_handle_s) { return tpm2_parse_handle(zfs_get_name(dataset), persistent_handle_s, persistent_handle); },
	    [&] { return with_tpm2_session([&](auto tpm2_ctx, auto tpm2_session) { return tpm2_free_persistent(tpm2_ctx, tpm2_session, persistent_handle); }); });
}
