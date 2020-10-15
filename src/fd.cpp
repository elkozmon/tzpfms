/* SPDX-License-Identifier: MIT */


#include "fd.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


int filled_fd(int & fd, const void * with, size_t with_len) {
	int pipes[2];
	TRY("create buffer pipe", pipe(pipes));
	quickscope_wrapper pipes_w_deleter{[=] { close(pipes[1]); }};
	fd = pipes[0];

	auto ret = write(pipes[1], with, with_len);
	if(ret >= 0 && static_cast<size_t>(ret) < with_len) {
		ret   = -1;
		errno = ENODATA;
	}
	TRY("write to buffer pipe", ret);

	return 0;
}


int read_exact(const char * path, void * data, size_t len) {
	auto infd = TRY("open input file", open(path, O_RDONLY));
	quickscope_wrapper infd_deleter{[=] { close(infd); }};

	while(len)
		if(const auto rd = TRY("read input file", read(infd, data, len))) {
			len -= rd;
			data = static_cast<char *>(data) + rd;
		} else {
			fprintf(stderr, "Couldn't read %zu bytes from input file: too short\n", len);
			return __LINE__;
		}

	return 0;
}


int write_exact(const char * path, const void * data, size_t len, mode_t mode) {
	auto outfd = TRY("open output file", open(path, O_WRONLY | O_CREAT | O_EXCL, mode));
	quickscope_wrapper infd_deleter{[=] { close(outfd); }};

	while(len) {
		const auto rd = TRY("write to output file", write(outfd, data, len));
		len -= rd;
		data = static_cast<const char *>(data) + rd;
	}

	return 0;
}
