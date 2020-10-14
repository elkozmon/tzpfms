 /* SPDX-License-Identifier: MIT */


#include "fd.hpp"


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
