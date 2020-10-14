/* SPDX-License-Identifier: MIT */


#pragma once


#include <errno.h>
#include <stdio.h>
#include <string.h>


#define TRY_GENERIC(what, cond_pre, cond_post, err_src, err_ret, ...)                             \
	({                                                                                              \
		auto _try_ret = (__VA_ARGS__);                                                                \
		if(cond_pre _try_ret cond_post) {                                                             \
			if constexpr(what != nullptr)                                                               \
				fprintf(stderr, "Couldn't %s: %s\n", static_cast<const char *>(what), strerror(err_src)); \
			return err_ret;                                                                             \
		}                                                                                             \
		_try_ret;                                                                                     \
	})
#define TRY(what, ...) TRY_GENERIC(what, , == -1, errno, __LINE__, __VA_ARGS__)


template <class F>
struct quickscope_wrapper {
	F func;

	~quickscope_wrapper() { func(); }
};

template <class F>
quickscope_wrapper(F)->quickscope_wrapper<F>;
