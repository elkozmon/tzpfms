/* SPDX-License-Identifier: MIT */


#pragma once


#include <charconv>
#include <stdio.h>
#include <string.h>


template <class T>
int parse_int(const char * what, T & out) {
	int base = 10;
	if(!strncmp(what, "0x", 2) || !strncmp(what, "0X", 2)) {
		base = 16;
		what += 2;
	}

	if(std::from_chars(what, what + strlen(what), out, base).ptr == what)
		return __LINE__;
	else
		return 0;
}
