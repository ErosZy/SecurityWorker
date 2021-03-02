/* Copyright JS Foundation and other contributors, http://js.foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef JRT_H
#define JRT_H

#include <stdio.h>
#include <string.h>

#include "jerryscript-port.h"
#include "jrt-types.h"

/*
 * Normally compilers store const(ant)s in ROM. Thus saving RAM.
 * But if your compiler does not support it then the directive below can force it.
 *
 * For the moment it is mainly meant for the following targets:
 *      - ESP8266
 */
#ifndef JERRY_CONST_DATA
# define JERRY_CONST_DATA
#endif /* JERRY_CONST_DATA */

/*
 * Constants
 */
#define JERRY_BITSINBYTE 8

/*
 * Make sure unused parameters, variables, or expressions trigger no compiler warning.
 */
#define JERRY_UNUSED(x) ((void) (x))

#define JERRY_UNUSED_1(_1)                 JERRY_UNUSED (_1)
#define JERRY_UNUSED_2(_1, _2)             JERRY_UNUSED (_1), JERRY_UNUSED_1 (_2)
#define JERRY_UNUSED_3(_1, _2, _3)         JERRY_UNUSED (_1), JERRY_UNUSED_2 (_2, _3)
#define JERRY_UNUSED_4(_1, _2, _3, _4)     JERRY_UNUSED (_1), JERRY_UNUSED_3 (_2, _3, _4)
#define JERRY_UNUSED_5(_1, _2, _3, _4, _5) JERRY_UNUSED (_1), JERRY_UNUSED_4 (_2, _3, _4, _5)

#define JERRY_VA_ARGS_NUM_IMPL(_1, _2, _3, _4, _5, N, ...) N
#define JERRY_VA_ARGS_NUM(...) JERRY_VA_ARGS_NUM_IMPL (__VA_ARGS__, 5, 4, 3, 2, 1, 0)

#define JERRY_UNUSED_ALL_IMPL_(nargs) JERRY_UNUSED_ ## nargs
#define JERRY_UNUSED_ALL_IMPL(nargs) JERRY_UNUSED_ALL_IMPL_ (nargs)
#define JERRY_UNUSED_ALL(...) JERRY_UNUSED_ALL_IMPL (JERRY_VA_ARGS_NUM (__VA_ARGS__)) (__VA_ARGS__)

/*
 * Asserts
 *
 * Warning:
 *         Don't use JERRY_STATIC_ASSERT in headers, because
 *         __LINE__ may be the same for asserts in a header
 *         and in an implementation file.
 */
#define JERRY_STATIC_ASSERT_GLUE_(a, b, c) a ## b ## _ ## c
#define JERRY_STATIC_ASSERT_GLUE(a, b, c) JERRY_STATIC_ASSERT_GLUE_ (a, b, c)
#define JERRY_STATIC_ASSERT(x, msg) \
  enum { JERRY_STATIC_ASSERT_GLUE (static_assertion_failed_, __LINE__, msg) = 1 / (!!(x)) }

#ifndef JERRY_NDEBUG
void JERRY_ATTR_NORETURN
jerry_assert_fail (const char *assertion, const char *file, const char *function, const uint32_t line);
void JERRY_ATTR_NORETURN
jerry_unreachable (const char *file, const char *function, const uint32_t line);

#define JERRY_ASSERT(x) \
  do \
  { \
    if (JERRY_UNLIKELY (!(x))) \
    { \
      jerry_assert_fail (#x, __FILE__, __func__, __LINE__); \
    } \
  } while (0)

#define JERRY_UNREACHABLE() \
  do \
  { \
    jerry_unreachable (__FILE__, __func__, __LINE__); \
  } while (0)
#else /* JERRY_NDEBUG */
#define JERRY_ASSERT(x) \
  do \
  { \
    if (false) \
    { \
      JERRY_UNUSED (x); \
    } \
  } while (0)

#ifdef __GNUC__
#define JERRY_UNREACHABLE() __builtin_unreachable ()
#endif /* __GNUC__ */

#ifdef _MSC_VER
#define JERRY_UNREACHABLE()  _assume (0)
#endif /* _MSC_VER */

#ifndef JERRY_UNREACHABLE
#define JERRY_UNREACHABLE()
#endif /* !JERRY_UNREACHABLE */

#endif /* !JERRY_NDEBUG */

/**
 * Exit on fatal error
 */
void JERRY_ATTR_NORETURN jerry_fatal (jerry_fatal_code_t code);

/*
 * Logging
 */
#ifdef JERRY_ENABLE_LOGGING
#define JERRY_ERROR_MSG(...) jerry_port_log (JERRY_LOG_LEVEL_ERROR, __VA_ARGS__)
#define JERRY_WARNING_MSG(...) jerry_port_log (JERRY_LOG_LEVEL_WARNING, __VA_ARGS__)
#define JERRY_DEBUG_MSG(...) jerry_port_log (JERRY_LOG_LEVEL_DEBUG, __VA_ARGS__)
#define JERRY_TRACE_MSG(...) jerry_port_log (JERRY_LOG_LEVEL_TRACE, __VA_ARGS__)
#else /* !JERRY_ENABLE_LOGGING */
#define JERRY_ERROR_MSG(...) do { if (false) { JERRY_UNUSED_ALL (__VA_ARGS__); } } while (0)
#define JERRY_WARNING_MSG(...) do { if (false) { JERRY_UNUSED_ALL (__VA_ARGS__); } } while (0)
#define JERRY_DEBUG_MSG(...) do { if (false) { JERRY_UNUSED_ALL (__VA_ARGS__); } } while (0)
#define JERRY_TRACE_MSG(...) do { if (false) { JERRY_UNUSED_ALL (__VA_ARGS__); } } while (0)
#endif /* JERRY_ENABLE_LOGGING */

/**
 * Size of struct member
 */
#define JERRY_SIZE_OF_STRUCT_MEMBER(struct_name, member_name) sizeof (((struct_name *) NULL)->member_name)

/**
 * Aligns @a value to @a alignment. @a must be the power of 2.
 *
 * Returns minimum positive value, that divides @a alignment and is more than or equal to @a value
 */
#define JERRY_ALIGNUP(value, alignment) (((value) + ((alignment) - 1)) & ~((alignment) - 1))

/*
 * min, max
 */
#define JERRY_MIN(v1, v2) (((v1) < (v2)) ? (v1) : (v2))
#define JERRY_MAX(v1, v2) (((v1) < (v2)) ? (v2) : (v1))

/**
 * Calculate the index of the first non-zero bit of a 32 bit integer value
 */
#define JERRY__LOG2_1(n) (((n) >= 2) ? 1 : 0)
#define JERRY__LOG2_2(n) (((n) >= 1 << 2) ? (2 + JERRY__LOG2_1 ((n) >> 2)) : JERRY__LOG2_1 (n))
#define JERRY__LOG2_4(n) (((n) >= 1 << 4) ? (4 + JERRY__LOG2_2 ((n) >> 4)) : JERRY__LOG2_2 (n))
#define JERRY__LOG2_8(n) (((n) >= 1 << 8) ? (8 + JERRY__LOG2_4 ((n) >> 8)) : JERRY__LOG2_4 (n))
#define JERRY_LOG2(n) (((n) >= 1 << 16) ? (16 + JERRY__LOG2_8 ((n) >> 16)) : JERRY__LOG2_8 (n))

#endif /* !JRT_H */
