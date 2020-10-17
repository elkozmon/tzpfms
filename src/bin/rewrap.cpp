/* SPDX-License-Identifier: MIT */


#include <libzfs.h>
// #include <sys/zio_crypt.h>
#define WRAPPING_KEY_LEN 32

#include <stdio.h>

#include "../fd.hpp"
#include "../main.hpp"
#include "../parse.hpp"
#include "../tpm2.hpp"
#include "../zfs.hpp"

// ./src/swtpm/swtpm socket --server port=2321 --ctrl type=tcp,port=2322 --tpm2


#define THIS_BACKEND "TPM2"


template <class T>
struct slice_iter {
	T * data;
	size_t len;

	T & operator*() { return *this->data; }
	bool operator!=(const slice_iter & other) { return this->data != other.data || this->len != other.len; }
	slice_iter operator++() {
		++this->data;
		--this->len;
		return *this;
	}
};

slice_iter<uint8_t> begin(TPM2B_DIGEST & dg) {
	return {&dg.buffer[0], dg.size};
}
slice_iter<uint8_t> end(TPM2B_DIGEST & dg) {
	return {dg.buffer + dg.size, 0};
}


int main(int argc, char ** argv) {
	const char * backup{};
	return do_main(
	    argc, argv, "b:", [&](auto) { backup = optarg; },
	    [&](auto dataset) {
		    if(zfs_prop_get_int(dataset, ZFS_PROP_KEYSTATUS) == ZFS_KEYSTATUS_UNAVAILABLE) {
			    fprintf(stderr, "Key change error: Key must be loaded.\n");  // mimic libzfs error output
			    return __LINE__;
		    }


		    // https://software.intel.com/content/www/us/en/develop/articles/code-sample-protecting-secret-data-and-keys-using-intel-platform-trust-technology.html
		    // tssstartup
		    // tpm2_createprimary -Q --hierarchy=o --key-context=prim.ctx
		    // cat /tmp/sk |  tpm2_create --hash-algorithm=sha256 --public=seal.pub --private=seal.priv --sealing-input=- --parent-context=prim.ctx
		    // tpm2_flushcontext -t
		    // tpm2_load -Q --parent-context=prim.ctx --public=seal.pub --private=seal.priv --name=seal.name --key-context=seal.ctx
		    // tpm2_evictcontrol --hierarchy=o --object-context=seal.ctx
		    //   persistent-handle: 0x81000001
		    //
		    // tpm2_unseal -Q --object-context=0x81000000

		    TRY_MAIN(with_tpm2_session([&](auto tpm2_ctx, auto tpm2_session) {
			    char *previous_backend{}, *previous_handle_s{};
			    TRY_MAIN(lookup_userprop(zfs_get_user_props(dataset), PROPNAME_BACKEND, previous_backend));
			    TRY_MAIN(lookup_userprop(zfs_get_user_props(dataset), PROPNAME_KEY, previous_handle_s));
			    if(!!previous_backend ^ !!previous_handle_s)
				    fprintf(stderr, "Inconsistent tzpfms metadata for %s: back-end is %s, but handle is %s?\n", zfs_get_name(dataset), previous_backend,
				            previous_handle_s);
			    else if(previous_backend && previous_handle_s) {
				    if(strcmp(previous_backend, THIS_BACKEND))
					    fprintf(stderr,
					            "Dataset %s was encrypted with tzpfms back-end %s before, but we are %s. You will have to free handle %s for back-end %s manually!\n",
					            zfs_get_name(dataset), previous_backend, THIS_BACKEND, previous_handle_s, previous_backend);
				    else {
					    TPMI_DH_PERSISTENT previous_handle{};
					    if(parse_int(previous_handle_s, previous_handle))
						    fprintf(stderr, "Couldn't parse previous persistent handle for dataset %s. You might need to run \"tpm2_evictcontrol -c %s\" or equivalent!\n",
						            zfs_get_name(dataset), previous_handle_s);
					    else {
						    if(tpm2_free_persistent(tpm2_ctx, tpm2_session, previous_handle))
							    fprintf(stderr,
							            "Couldn't free previous persistent handle for dataset %s. You might need to run \"tpm2_evictcontrol -c 0x%X\" or equivalent!\n",
							            zfs_get_name(dataset), previous_handle);
					    }
				    }
			    }

			    uint8_t wrap_key[WRAPPING_KEY_LEN];
			    TPMI_DH_PERSISTENT persistent_handle{};

			    TRY_MAIN(tpm2_generate_rand(tpm2_ctx, wrap_key, sizeof(wrap_key)));
			    if(backup)
				    TRY_MAIN(write_exact(backup, wrap_key, sizeof(wrap_key), 0400));

			    TRY_MAIN(tpm2_seal(tpm2_ctx, tpm2_session, persistent_handle, tpm2_creation_metadata(zfs_get_name(dataset)), wrap_key, sizeof(wrap_key)));
			    bool ok = false;  // Try to free the persistent handle if we're unsuccessful in actually using it later on
			    quickscope_wrapper persistent_clearer{[&] {
				    if(!ok && tpm2_free_persistent(tpm2_ctx, tpm2_session, persistent_handle))
					    fprintf(stderr, "Couldn't free persistent handle. You might need to run \"tpm2_evictcontrol -c 0x%X\" or equivalent!\n", persistent_handle);
			    }};

			    TRY_MAIN(set_key_props(dataset, THIS_BACKEND, persistent_handle));

			    /// zfs_crypto_rewrap() with "prompt" reads from stdin, but not if it's a TTY;
			    /// this user-proofs the set-up, and means we don't have to touch the filesysten:
			    /// instead, get an FD, write the raw key data there, dup() it onto stdin,
			    /// let libzfs read it, then restore stdin

			    int key_fd;
			    TRY_MAIN(filled_fd(key_fd, wrap_key, WRAPPING_KEY_LEN));
			    quickscope_wrapper key_fd_deleter{[=] { close(key_fd); }};


			    TRY_MAIN(with_stdin_at(key_fd, [&] {
				    if(zfs_crypto_rewrap(dataset, TRY_PTR("get rewrap args", rewrap_args()), B_FALSE))
					    return __LINE__;  // Error printed by libzfs
				    else
					    printf("Key for %s changed\n", zfs_get_name(dataset));

				    return 0;
			    }));

			    ok = true;
			    return 0;
		    }));

		    return 0;
	    });
}
