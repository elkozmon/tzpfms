/* SPDX-License-Identifier: MIT */


#include "../main_clear.hpp"
#include "../tpm1x.hpp"


#define THIS_BACKEND "TPM1.X"


int main(int argc, char ** argv) {
	tpm1x_handle handle{};  // Not like we use this, but for symmetry with the other -clear-keys
	return do_clear_main(
	    argc, argv, THIS_BACKEND, [&](auto dataset, auto handle_s) { return parse_key_props(dataset, THIS_BACKEND, handle_s); }, [&] { return 0; });
}
