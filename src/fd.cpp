/* SPDX-License-Identifier: MIT */


#include "fd.hpp"

#include "main.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>


/// Matches libzfs
#define MIN_PASSPHRASE_LEN 8


int filled_fd(int & fd, const void * with, size_t with_len) {
	int pipes[2];
	TRY("create buffer pipe", pipe2(pipes, O_CLOEXEC));
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
	auto infd = TRY("open input file", open(path, O_RDONLY | O_CLOEXEC));
	quickscope_wrapper infd_deleter{[=] { close(infd); }};

	while(len)
		if(const auto rd = TRY("read input file", read(infd, data, len))) {
			len -= rd;
			data = static_cast<char *>(data) + rd;
		} else
			return fprintf(stderr, "Couldn't read %zu bytes from input file: too short\n", len), __LINE__;

	return 0;
}


int write_exact(const char * path, const void * data, size_t len, mode_t mode) {
	auto outfd = TRY("create output file", open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode));
	quickscope_wrapper outfd_deleter{[=] { close(outfd); }};

	while(len) {
		const auto rd = TRY("write to output file", write(outfd, data, len));
		len -= rd;
		data = static_cast<const char *>(data) + rd;
	}

	return 0;
}


#define TRY_HELPER(what, ...) TRY_GENERIC(what, , == -1, errno, -1, strerror, __VA_ARGS__)

/// TRY_MAIN rules, plus -1 for ENOENT
static int get_key_material_helper(const char * helper, const char * whom, bool again, bool newkey, uint8_t *& buf, size_t & len_out) {
#if __linux__ || __FreeBSD__
	auto outfd = TRY_HELPER("create helper output", memfd_create(whom, MFD_CLOEXEC));
#else
	int outfd;
	char fname[8 + 10 + 1 + 20 + 1];  // 4294967296, 18446744073709551616
	auto pid = getpid();
	for(uint64_t i = 0; i < UINT64_MAX; ++i) {
		snprintf(fname, sizeof(fname), "/tzpfms:%" PRIu32 ":%" PRIu64 "", static_cast<uint32_t>(pid), i);
		if((outfd = shm_open(fname, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0000)) != -1 || errno != EEXIST)
			break;
	}
	TRY_HELPER("create helper output", outfd);
	shm_unlink(fname);
#endif
	quickscope_wrapper outfd_deleter{[=] { close(outfd); }};

	switch(auto pid = TRY_HELPER("create child", fork())) {
		case 0:  // child
			dup2(outfd, 1);

			char * msg;
			if(asprintf(&msg, "%sassphrase for %s%s", newkey ? "New p" : "P", whom, again ? " (again)" : "") == -1)
				msg = const_cast<char *>(whom);
			execl("/bin/sh", "sh", "-c", helper, helper, msg, whom, newkey ? "new" : "", again ? "again" : "", nullptr);
			fprintf(stderr, "exec(/bin/sh): %s\n", strerror(errno));
			_exit(127);
			break;

		default:  // parent
			int err, ret;
			while((ret = waitpid(pid, &err, 0)) == -1 && errno == EINTR)
				;
			TRY("wait for helper", ret);

			if(WIFEXITED(err)) {
				switch(WEXITSTATUS(err)) {
					case 0:
						struct stat sb;
						fstat(outfd, &sb);
						if(auto out = mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, outfd, 0); out != MAP_FAILED) {
							quickscope_wrapper out_deleter{[=] { munmap(out, sb.st_size); }};
							buf     = static_cast<uint8_t *>(malloc(sb.st_size));  // TODO:if failed
							len_out = sb.st_size;
							memcpy(buf, out, sb.st_size);
							// Trim ending newline, if any
							if(buf[len_out - 1] == '\n')
								buf[--len_out] = '\0';
							return 0;
						} else
							;  // error

					case 127:  // enoent, error already written by shell or child
						return -1;

					default:
						fprintf(stderr, "Helper '%s' failed with %d.\n", helper, WEXITSTATUS(err));
						return __LINE__;
				}
			} else {
				fprintf(stderr, "Helper '%s' died to signal %d: %s.\n", helper, WTERMSIG(err), strsignal(WTERMSIG(err)));
				return __LINE__;
			}
	}
}


/// Adapted from src:zfs's lib/libzfs/libzfs_crypto.c#get_key_material_raw()
static int get_key_material_raw(const char * whom, bool again, bool newkey, uint8_t *& buf, size_t & len_out) {
	static int caught_interrupt;

	struct termios old_term;
	struct sigaction osigint, osigtstp;

	len_out = 0;

	auto from_tty = isatty(0);
	if(from_tty) {
		// Handle SIGINT and ignore SIGSTP.
		// This is necessary to restore the state of the terminal.
		struct sigaction act {};
		sigemptyset(&act.sa_mask);

		caught_interrupt = 0;
		act.sa_handler   = [](auto sig) {
      caught_interrupt = sig;
      fputs("^C\n", stderr);
		};
		sigaction(SIGINT, &act, &osigint);

		act.sa_handler = SIG_IGN;
		sigaction(SIGTSTP, &act, &osigtstp);

		// Prompt for the key
		printf("%s %spassphrase for %s: ", again ? "Re-enter" : "Enter", newkey ? "new " : "", whom);
		fflush(stdout);

		// Disable the terminal echo for key input
		tcgetattr(0, &old_term);

		auto new_term = old_term;
		new_term.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
		TRY("disable echo", tcsetattr(0, TCSAFLUSH, &new_term));
	}
	quickscope_wrapper stdin_restorer{[&] {
		if(from_tty) {
			// Reset the terminal
			tcsetattr(0, TCSAFLUSH, &old_term);
			sigaction(SIGINT, &osigint, nullptr);
			sigaction(SIGTSTP, &osigtstp, nullptr);

			// If we caught a signal, re-throw it now
			if(caught_interrupt != 0)
				raise(caught_interrupt);

			// Print the newline that was not echoed
			putchar('\n');
		}
	}};


	// Read the key material
	size_t buflen{};
	errno      = 0;
	auto bytes = getline((char **)&buf, &buflen, stdin);
	switch(bytes) {
		case -1:
			if(errno != 0)
				TRY("read in passphrase", bytes);
			else  // EOF
				bytes = 0;
			break;
		case 0:
			break;
		default:
			// Trim ending newline, if any
			if(buf[bytes - 1] == '\n')
				buf[--bytes] = '\0';
			break;
	}

	len_out = bytes;
	return 0;
}

static int get_key_material_dispatch(const char * whom, bool again, bool newkey, uint8_t *& buf, size_t & len_out) {
	static const char * helper{};
	if(!helper)
		helper = getenv("TZPFMS_PASSPHRASE_HELPER");
	if(helper && *helper) {
		if(auto err = get_key_material_helper(helper, whom, again, newkey, buf, len_out); err != -1)
			return err;
		else
			helper = "";
	}
	return get_key_material_raw(whom, again, newkey, buf, len_out);
}


int read_known_passphrase(const char * whom, uint8_t *& buf, size_t & len_out, size_t max_len) {
	TRY_MAIN(get_key_material_dispatch(whom, false, false, buf, len_out));
	if(len_out <= max_len)
		return 0;

	fprintf(stderr, "Passphrase too long (max %zu)\n", max_len);
	free(buf);
	buf     = nullptr;
	len_out = 0;
	return __LINE__;
}

int read_new_passphrase(const char * whom, uint8_t *& buf, size_t & len_out, size_t max_len) {
	uint8_t * first_passphrase{};
	size_t first_passphrase_len{};
	TRY_MAIN(get_key_material_dispatch(whom, false, true, first_passphrase, first_passphrase_len));
	quickscope_wrapper first_passphrase_deleter{[&] { free(first_passphrase); }};

	if(first_passphrase_len != 0 && first_passphrase_len < MIN_PASSPHRASE_LEN)
		return fprintf(stderr, "Passphrase too short (min %u)\n", MIN_PASSPHRASE_LEN), __LINE__;
	if(first_passphrase_len > max_len)
		return fprintf(stderr, "Passphrase too long (max %zu)\n", max_len), __LINE__;

	uint8_t * second_passphrase{};
	size_t second_passphrase_len{};
	TRY_MAIN(get_key_material_dispatch(whom, true, true, second_passphrase, second_passphrase_len));
	quickscope_wrapper second_passphrase_deleter{[&] { free(second_passphrase); }};

	if(second_passphrase_len != first_passphrase_len || memcmp(first_passphrase, second_passphrase, first_passphrase_len))
		return fprintf(stderr, "Provided keys do not match.\n"), __LINE__;

	if(second_passphrase_len) {
		buf               = second_passphrase;
		second_passphrase = nullptr;
	} else
		buf = nullptr;

	len_out = second_passphrase_len;
	return 0;
}
