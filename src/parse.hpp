/* SPDX-License-Identifier: MIT */


#pragma once


#include <charconv>
#include <limits>
#include <stdio.h>
#include <errno.h>
#include <string.h>


template <class T>
bool parse_uint(const char * val, T & out) {
	if(val[0] == '\0')
		return errno = EINVAL, false;
	if(val[0] == '-')
		return errno = ERANGE, false;

	char * end{};
	errno    = 0;
	auto res = strtoull(val, &end, 0);
	out      = res;
	if(errno)
		return false;
	if(res > std::numeric_limits<T>::max())
		return errno = ERANGE, false;
	if(*end != '\0')
		return errno = EINVAL, false;

	return true;
}
