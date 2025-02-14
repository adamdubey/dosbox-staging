/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_STRING_UTILS_H
#define DOSBOX_STRING_UTILS_H

#include "dosbox.h"

#include <cassert>
#include <cstdarg>
#include <cstring>
#include <string>

template <size_t N>
int safe_sprintf(char (&dst)[N], const char *fmt, ...)
        GCC_ATTRIBUTE(format(printf, 2, 3));

template <size_t N>
int safe_sprintf(char (&dst)[N], const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const int ret = vsnprintf(dst, N, fmt, args);
	va_end(args);
	return ret;
}

/* Copy a string into C array
 *
 * This function copies string pointed by src to a fixed-size buffer dst.
 * At most N bytes from src are copied, where N is size of dst. If exactly
 * N bytes are copied, then terminating null byte is put into dst, thus
 * buffer overrun is prevented.
 *
 * Function returns pointer to buffer to be compatible with strcpy.
 *
 * This is a safer drop-in replacement for strcpy function (when used to fill
 * buffers, whose size is known at compilation time), however some caveats
 * still apply:
 *
 * - src cannot be null, otherwise the behaviour is undefined
 * - dst and src strings must not overlap, otherwise the behaviour is undefined
 * - src string must be null-terminated, otherwise the behaviour is undefined
 *
 * Usage:
 *
 *     char buffer[2];
 *     safe_strcpy(buffer, "abc");
 *     // buffer is filled with "a"
 */
template <size_t N>
char *safe_strcpy(char (&dst)[N], const char *src) noexcept
{
	assert(src != nullptr);
	assert(src < &dst[0] || src > &dst[N - 1]);
	const auto rcode = safe_sprintf(dst, "%s", src);
	return (rcode >= 0) ? &dst[0] : nullptr;
}

template <size_t N>
char *safe_strcat(char (&dst)[N], const char *src) noexcept
{
	strncat(dst, src, N - strnlen(dst, N) - 1);
	return &dst[0];
}

template <size_t N>
size_t safe_strlen(char (&str)[N]) noexcept
{
	static_assert(N != 0, "zero-length arrays are not supported");
	return strnlen(str, N - 1);
}

template <size_t N>
bool starts_with(const char (&pfx)[N], const char *str) noexcept
{
	return (strncmp(pfx, str, N - 1) == 0);
}

template <size_t N>
bool starts_with(const char (&pfx)[N], const std::string &str) noexcept
{
	return (strncmp(pfx, str.c_str(), N - 1) == 0);
}

bool ends_with(const std::string &str, const std::string &suffix) noexcept;

bool find_in_case_insensitive(const std::string &needle, const std::string &haystack);

// Safely terminate a C string at the given offset
//
// Convert code like: stuff[n] = 0;
//  - "is stuff an integer array?"
//  - "are we setting a counter back to 0?"
//  - "is stuff a valid array?"
//
// To: terminate_str_at(stuff, n);
// which is self-documenting about the type (stuff is a string),
// intent (terminating), and where it's being applied (n);
//
template <typename T, typename INDEX_T>
void terminate_str_at(T *str, INDEX_T i) noexcept
{
	// Check that we're only operating on bona-fide C strings
	static_assert(std::is_same_v<T, char> || std::is_same_v<T, wchar_t>,
	              "Can only reset a *char or *wchar_t with the string-terminator");

	// Check that we don't underflow with a negative index
	assert(std::is_unsigned_v<INDEX_T> || i >= 0);

	// Check that we don't dereferrence a null pointer
	assert(str != nullptr);
	str[i] = '\0';
}

// reset a C string with the string-terminator character
template <typename T>
void reset_str(T *str) noexcept
{
	terminate_str_at(str, 0);
}

#endif
