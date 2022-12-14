/* SPDX-License-Identifier: MIT */


#include "../main.hpp"
#include "../parse.hpp"
#include "../zfs.hpp"

#include <algorithm>


#define TZPFMS_BACKEND_MAX_LEN 16


enum class key_loadedness : char {
	none     = -1,
	unloaded = 0,
	loaded   = 1,
};

/// zfs(8) uses struct zprop_get_cbdata_t, which is powerful, but inscrutable; we have a fixed format, which makes this easier
struct output_line {
	static const char * const key_available_display[2];
	static const char * const coherent_display[2];


	char name[ZFS_MAX_DATASET_NAME_LEN + 1];
	char backend[TZPFMS_BACKEND_MAX_LEN + 1];
	bool key_available : 1;
	bool coherent : 1;

	bool included(bool print_nontzpfms, const char * backend_restrixion, key_loadedness key_loadedness_restrixion) const {
		return (print_nontzpfms || !this->coherent || this->backend[0] != '\0') && (!backend_restrixion || !strcmp(backend_restrixion, this->backend)) &&
		       (key_loadedness_restrixion == key_loadedness::none || key_loadedness_restrixion == static_cast<key_loadedness>(this->key_available));
	}

	const char * backend_display() const { return (this->backend[0] != '\0') ? this->backend : "-"; }
};

const char * const output_line::key_available_display[2]{"unavailable", key_available_display[0] + 2};
const char * const output_line::coherent_display[2]{"no", "yes"};


int main(int argc, char ** argv) {
	bool human                      = true;
	bool print_nontzpfms            = false;
	size_t maxdepth                 = MAXDEPTH_UNSET;
	const char * backend_restrixion = nullptr;
	auto key_loadedness_restrixion  = key_loadedness::none;
	return do_bare_main(
	    argc, argv, "Hrd:ab:ul", "[-H] [-r|-d max] [-a|-b back-end] [-u|-l]", "[filesystem|volume]…",
	    [&](auto arg) {
		    switch(arg) {
			    case 'H':
				    human = false;
				    break;
			    case 'r':
				    maxdepth = SIZE_MAX;
				    break;
			    case 'd':
				    if(!parse_uint(optarg, maxdepth))
					    return fprintf(stderr, "-d %s: %s\n", optarg, strerror(errno)), __LINE__;
				    break;
			    case 'a':
				    print_nontzpfms = true;
				    break;
			    case 'b':
				    backend_restrixion = optarg;
				    break;
			    case 'u':
				    key_loadedness_restrixion = key_loadedness::unloaded;
				    break;
			    case 'l':
				    key_loadedness_restrixion = key_loadedness::loaded;
				    break;
		    }
		    return 0;
	    },
	    [&](auto libz) {
		    output_line * lines{};
		    size_t lines_len{};
		    quickscope_wrapper lines_deleter{[&] { free(lines); }};


		    TRY_MAIN(for_all_datasets(libz, argv + optind, maxdepth, [&](auto dataset) {
			    boolean_t dataset_is_root;
			    TRY("get encryption root", zfs_crypto_get_encryption_root(dataset, &dataset_is_root, nullptr));
			    if(!dataset_is_root)
				    return 0;

			    char *backend{}, *handle{};
			    TRY_MAIN(lookup_userprop(dataset, PROPNAME_BACKEND, backend));
			    TRY_MAIN(lookup_userprop(dataset, PROPNAME_KEY, handle));

			    ++lines_len;
			    lines = TRY_PTR("allocate line buffer", reinterpret_cast<output_line *>(realloc(lines, sizeof(output_line) * lines_len)));

			    auto & cur_line = lines[lines_len - 1];
			    strncpy(cur_line.name, zfs_get_name(dataset), ZFS_MAX_DATASET_NAME_LEN);
			    strncpy(cur_line.backend, (backend && strlen(backend) <= TZPFMS_BACKEND_MAX_LEN) ? backend : "\0", TZPFMS_BACKEND_MAX_LEN);
			    // Tristate available/unavailable/none, but it's gonna be either available or unavailable on envryption roots, so
			    cur_line.key_available = zfs_prop_get_int(dataset, ZFS_PROP_KEYSTATUS) == ZFS_KEYSTATUS_AVAILABLE;
			    cur_line.coherent      = !!backend == !!handle;

			    return 0;
		    }));

		    size_t max_name_len          = 0;
		    size_t max_backend_len       = 0;
		    size_t max_key_available_len = 0;
		    size_t max_coherent_len      = 0;
		    auto separator               = "\t";
		    if(human) {
			    max_name_len          = strlen("NAME");
			    max_backend_len       = strlen("BACK-END");
			    max_key_available_len = strlen("KEYSTATUS");
			    max_coherent_len      = strlen("COHERENT");
			    separator             = "  ";

			    for(auto cur = lines; cur != lines + lines_len; ++cur)
				    if(cur->included(print_nontzpfms, backend_restrixion, key_loadedness_restrixion)) {
					    max_name_len          = std::max(max_name_len, strlen(cur->name));
					    max_backend_len       = std::max(max_backend_len, strlen(cur->backend_display()));
					    max_key_available_len = std::max(max_key_available_len, strlen(output_line::key_available_display[cur->key_available]));
				    }
		    }

		    auto println = [&](auto name, auto backend, auto key_available, auto coherent) {
			    printf("%-*s%s%-*s%s%-*s%s%-*s\n",                                         //
			           static_cast<int>(max_name_len), name, separator,                    //
			           static_cast<int>(max_backend_len), backend, separator,              //
			           static_cast<int>(max_key_available_len), key_available, separator,  //
			           static_cast<int>(max_coherent_len), coherent);
		    };
		    if(human)
			    println("NAME", "BACK-END", "KEYSTATUS", "COHERENT");
		    for(auto cur = lines; cur != lines + lines_len; ++cur)
			    if(cur->included(print_nontzpfms, backend_restrixion, key_loadedness_restrixion))
				    println(cur->name, cur->backend_display(), output_line::key_available_display[cur->key_available], output_line::coherent_display[cur->coherent]);

		    return 0;
	    });
}
