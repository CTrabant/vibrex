/*********************************************************************************
 * A limited regular expression engine, boldly going where no regex has gone
 * before (maybe).
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or distribute
 * this software, either in source code form or as a compiled binary, for any
 * purpose, commercial or non-commercial, and by any means.
 *
 * For more information, please refer to <https://creativecommons.org/publicdomain/zero/1.0/>
 *
 * SPDX-License-Identifier: CC0-1.0
 *********************************************************************************/
#ifndef VIBREX_H
#define VIBREX_H

#include <stddef.h>
#include <stdbool.h>

/* Opaque type for compiled regex pattern */
typedef struct vibrex_pattern vibrex_t;

/********************************************************************************
 * @brief Compiles a regular expression pattern
 *
 * @param pattern The null-terminated regular expression string.
 * @param error_message If not NULL, will be set to a pointer to a
 * description of the error on failure.
 *
 * @return A pointer to a compiled vibex_t object on success,
 * or NULL if the pattern has a syntax error or on memory allocation failure.
 *********************************************************************************/
extern vibrex_t* vibrex_compile(const char* pattern, const char **error_message);

/********************************************************************************
 * @brief Match a compiled pattern against a string
 *
 * @param compiled_pattern The compiled regex pattern
 * @param text The text to match against
 *
 * @return true if match found, false otherwise
 *********************************************************************************/
extern bool vibrex_match(const vibrex_t* compiled_pattern, const char* text);

/********************************************************************************
 * @brief Free a compiled pattern
 *
 * @param compiled_pattern The pattern to free
 *********************************************************************************/
extern void vibrex_free(vibrex_t* compiled_pattern);

#endif /* VIBREX_H */
