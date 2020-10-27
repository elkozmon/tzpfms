/* SPDX-License-Identifier: MIT */


#include "fd.hpp"

#include "main.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
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
	auto outfd = TRY("create output file", open(path, O_WRONLY | O_CREAT | O_EXCL, mode));
	quickscope_wrapper infd_deleter{[=] { close(outfd); }};

	while(len) {
		const auto rd = TRY("write to output file", write(outfd, data, len));
		len -= rd;
		data = static_cast<const char *>(data) + rd;
	}

	return 0;
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
		act.sa_handler   = [](auto sig) { caught_interrupt = sig; };
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
				kill(getpid(), caught_interrupt);

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
			if(buf[bytes - 1] == '\n') {
				buf[bytes - 1] = '\0';
				--bytes;
			}
			break;
	}

	len_out = bytes;
	return 0;
}

int read_known_passphrase(const char * whom, uint8_t *& buf, size_t & len_out, size_t max_len) {
	TRY_MAIN(get_key_material_raw(whom, false, false, buf, len_out));
	if(len_out <= max_len)
		return 0;

	fprintf(stderr, "Passphrase too long: (max %zu)\n", max_len);
	free(buf);
	buf     = nullptr;
	len_out = 0;
	return __LINE__;
}

int read_new_passphrase(const char * whom, uint8_t *& buf, size_t & len_out, size_t max_len) {
	uint8_t * first_passphrase{};
	size_t first_passphrase_len{};
	TRY_MAIN(get_key_material_raw(whom, false, true, first_passphrase, first_passphrase_len));
	quickscope_wrapper first_passphrase_deleter{[&] { free(first_passphrase); }};

	if(first_passphrase_len > max_len) {
		fprintf(stderr, "Passphrase too long: (max %zu)\n", max_len);
		return __LINE__;
	}

	uint8_t * second_passphrase{};
	size_t second_passphrase_len{};
	TRY_MAIN(get_key_material_raw(whom, true, true, second_passphrase, second_passphrase_len));
	quickscope_wrapper second_passphrase_deleter{[&] { free(second_passphrase); }};

	if(second_passphrase_len != first_passphrase_len || memcmp(first_passphrase, second_passphrase, first_passphrase_len)) {
		fprintf(stderr, "Provided keys do not match.\n");
		return __LINE__;
	}

	if(second_passphrase_len) {
		buf               = second_passphrase;
		second_passphrase = nullptr;
	} else
		buf = nullptr;

	len_out = second_passphrase_len;
	return 0;
}
