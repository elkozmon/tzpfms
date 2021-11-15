/* SPDX-License-Identifier: MIT */


#pragma once


#include "common.hpp"
#include <unistd.h>
#include <stdint.h>


template <class F>
int with_stdin_at(int fd, F && what) {
	auto stdin_saved = TRY("dup() stdin", dup(0));
	quickscope_wrapper stdin_saved_deleter{[=] { close(stdin_saved); }};

	TRY("dup2() onto stdin", dup2(fd, 0));

	if(int ret = what()) {
		dup2(stdin_saved, 0);
		return ret;
	}

	TRY("dup2() stdin back onto stdin", dup2(stdin_saved, 0));
	return 0;
}

/// with_len may not exceed pipe capacity (64k by default)
extern int filled_fd(int & fd, const void * with, size_t with_len);

/// Read exactly len bytes from path into data, or error
extern int read_exact(const char * path, void * data, size_t len);

/// Write exactly len bytes from data into path, or error
extern int write_exact(const char * path, const void * data, size_t len, mode_t mode);

/// Prompt for passphrase for whom the user knows, up to max_len bytes
extern int read_known_passphrase(const char * whom, uint8_t *& buf, size_t & len_out, size_t max_len = SIZE_MAX);

/// Prompt twice for passphrase for whom the user is setting
extern int read_new_passphrase(const char * whom, uint8_t *& buf, size_t & len_out, size_t max_len = SIZE_MAX);
