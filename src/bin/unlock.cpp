/* SPDX-License-Identifier: MIT */


#include <libzfs.h>

#include <stdio.h>

#include "../fd.hpp"
#include "../main.hpp"
#include "../zfs.hpp"


static int hex_nibble(char c) {
	switch(c) {
		case '0':
			return 0x0;
		case '1':
			return 0x1;
		case '2':
			return 0x2;
		case '3':
			return 0x3;
		case '4':
			return 0x4;
		case '5':
			return 0x5;
		case '6':
			return 0x6;
		case '7':
			return 0x7;
		case '8':
			return 0x8;
		case '9':
			return 0x9;
		case 'A':
		case 'a':
			return 0xA;
		case 'B':
		case 'b':
			return 0xB;
		case 'C':
		case 'c':
			return 0xC;
		case 'D':
		case 'd':
			return 0xD;
		case 'E':
		case 'e':
			return 0xE;
		case 'F':
		case 'f':
			return 0xF;
		default:
			fprintf(stderr, "Character %c (0x%02X) not hex?\n", c, static_cast<int>(c));
			return 0;
	}
}


int main(int argc, char ** argv) {
	auto noop = B_FALSE;
	return do_main(
	    argc, argv, "n", [&](auto) { noop = B_TRUE; },
	    [&](auto dataset) {
		    char * stored_key_s{};
		    TRY_MAIN(lookup_userprop(zfs_get_user_props(dataset), "xyz.nabijaczleweli:tzpfms.key", stored_key_s));
		    errno = EEXIST;
		    TRY_PTR("find encryption key prop", stored_key_s);

		    auto stored_key_len = strlen(stored_key_s) / 2;
		    auto stored_key     = static_cast<uint8_t *>(TRY_PTR("stored_key", alloca(stored_key_len)));
		    {
			    auto cur = stored_key_s;
			    for(auto kcur = stored_key; kcur < stored_key + stored_key_len; ++kcur) {
				    *kcur      = (hex_nibble(cur[0]) << 4) | hex_nibble(cur[1]);
				    cur += 2;
			    }
		    }

		    int key_fd;
		    TRY_MAIN(filled_fd(key_fd, (void *)stored_key, stored_key_len));
		    quickscope_wrapper key_fd_deleter{[=] { close(key_fd); }};


		    TRY_MAIN(with_stdin_at(key_fd, [&] {
			    if(zfs_crypto_load_key(dataset, noop, nullptr))
				    return __LINE__;  // Error printed by libzfs
			    else
				    printf("Key for %s loaded\n", zfs_get_name(dataset));

			    return 0;
		    }));

		    return 0;
	    });
}
