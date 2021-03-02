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

#include <math.h>

#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "jrt-libc-includes.h"
#include "lit-char-helpers.h"
#include "lit-magic-strings.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmahelpers Helpers for operations with ECMA data types
 * @{
 */

#if CONFIG_ECMA_NUMBER_TYPE == CONFIG_ECMA_NUMBER_FLOAT64

/**
 * \addtogroup ecmahelpersbigintegers Helpers for operations intermediate 128-bit integers
 * @{
 */

/**
 * 128-bit integer type
 */
typedef struct
{
  uint64_t lo; /**< low 64 bits */
  uint64_t hi; /**< high 64 bits */
} ecma_uint128_t;

/**
 * Round high part of 128-bit integer to uint64_t
 */
#define ECMA_UINT128_ROUND_HIGH_TO_UINT64(name) \
  (name.hi + (name.lo >> 63u))

/**
 * Check if 128-bit integer is zero
 */
#define ECMA_UINT128_IS_ZERO(name) \
  (name.hi == 0 && name.lo == 0)

/**
 * Left shift 128-bit integer by max 63 bits
 */
#define ECMA_UINT128_LEFT_SHIFT_MAX63(name, shift) \
{ \
  name.hi = (name.hi << (shift)) | (name.lo >> (64 - (shift))); \
  name.lo <<= (shift); \
}

/**
 * Right shift 128-bit integer by max 63 bits
 */
#define ECMA_UINT128_RIGHT_SHIFT_MAX63(name, shift) \
{ \
  name.lo = (name.lo >> (shift)) | (name.hi << (64 - (shift))); \
  name.hi >>= (shift); \
}

/**
 * Add 128-bit integer
 */
#define ECMA_UINT128_ADD(name_add_to, name_to_add) \
{ \
  name_add_to.hi += name_to_add.hi; \
  name_add_to.lo += name_to_add.lo; \
  if (name_add_to.lo < name_to_add.lo) \
  { \
    name_add_to.hi++; \
  } \
}

/**
 * Multiply 128-bit integer by 10
 */
#define ECMA_UINT128_MUL10(name) \
{ \
  ECMA_UINT128_LEFT_SHIFT_MAX63 (name, 1u); \
  \
  ecma_uint128_t name ## _tmp = name; \
  \
  ECMA_UINT128_LEFT_SHIFT_MAX63 (name ## _tmp, 2u); \
  \
  ECMA_UINT128_ADD (name, name ## _tmp); \
}

/**
 * Divide 128-bit integer by 10
 *
 * N = N3 *2^96  + N2 *2^64  + N1 *2^32  + N0 *2^0     // 128-bit dividend
 * T = T3 *2^-32 + T2 *2^-64 + T1 *2^-96 + T0 *2^-128  // 128-bit divisor reciprocal, 1/10 * 2^-128
 *
 * N * T    = N3*T3 *2^64 + N2*T3 *2^32 + N1*T3 *2^0 + N0*T3 *2^-32
 *          +               N3*T2 *2^32 + N2*T2 *2^0 + N1*T2 *2^-32 + N0*T2 *2^-64
 *          +                             N3*T1 *2^0 + N2*T1 *2^-32 + N1*T1 *2^-64 + N0*T1 *2^-96
 *          +                                          N3*T0 *2^-32 + N2*T0 *2^-64 + N1*T0 *2^-96 + N0*T0 *2^-128
 *
 *  Q3=carry  Q2=^+carry    Q1=^+carry    Q0=^+carry   fraction=^...
 *
 * Q = Q3 *2^96  + Q2 *2^64  + Q1 *2^32  + Q0 *2^0     // 128-bit quotient
 */
#define ECMA_UINT128_DIV10(name) \
{ \
  /* estimation of reciprocal of 10, 128 bits right of the binary point (T1 == T2) */ \
  const uint64_t tenth_l = 0x9999999aul; \
  const uint64_t tenth_m = 0x99999999ul; \
  const uint64_t tenth_h = 0x19999999ul; \
  \
  uint64_t l0 = ((uint32_t) name.lo) * tenth_l; \
  uint64_t l1 = (name.lo >> 32u) * tenth_l; \
  uint64_t l2 = ((uint32_t) name.hi) * tenth_l; \
  uint64_t l3 = (name.hi >> 32u) * tenth_l; \
  uint64_t m0 = ((uint32_t) name.lo) * tenth_m; \
  uint64_t m1 = (name.lo >> 32u) * tenth_m; \
  uint64_t m2 = ((uint32_t) name.hi) * tenth_m; \
  uint64_t m3 = (name.hi >> 32u) * tenth_m; \
  uint64_t h0 = ((uint32_t) name.lo) * tenth_h; \
  uint64_t h1 = (name.lo >> 32u) * tenth_h; \
  uint64_t h2 = ((uint32_t) name.hi) * tenth_h; \
  uint64_t h3 = (name.hi >> 32u) * tenth_h; \
  \
  uint64_t q0 = l0 >> 32u; \
  q0 += (uint32_t) l1; \
  q0 += (uint32_t) m0; \
  \
  q0 >>= 32u; \
  q0 += l1 >> 32u; \
  q0 += m0 >> 32u; \
  q0 += (uint32_t) l2; \
  q0 += (uint32_t) m1; \
  q0 += (uint32_t) m0; \
  \
  q0 >>= 32u; \
  q0 += l2 >> 32u; \
  q0 += m1 >> 32u; \
  q0 += m0 >> 32u; \
  q0 += (uint32_t) l3; \
  q0 += (uint32_t) m2; \
  q0 += (uint32_t) m1; \
  q0 += (uint32_t) h0; \
  \
  q0 >>=32u; \
  q0 += l3 >> 32u; \
  q0 += m2 >> 32u; \
  q0 += m1 >> 32u; \
  q0 += h0 >> 32u; \
  q0 += (uint32_t) m3; \
  q0 += (uint32_t) m2; \
  q0 += (uint32_t) h1; \
  \
  uint64_t q1 = q0 >> 32u; \
  q1 += m3 >> 32u; \
  q1 += m2 >> 32u; \
  q1 += h1 >> 32u; \
  q1 += (uint32_t) m3; \
  q1 += (uint32_t) h2; \
  \
  uint64_t q32 = q1 >> 32u; \
  q32 += m3 >> 32u; \
  q32 += h2 >> 32u; \
  q32 += h3; \
  \
  name.lo = (q1 << 32u) | ((uint32_t) q0); \
  name.hi = q32; \
}

#if defined (__GNUC__) || defined (__clang__)

/**
 * Count leading zeros in the topmost 64 bits of a 128-bit integer.
 */
#define ECMA_UINT128_CLZ_MAX63(name) \
  __builtin_clzll (name.hi)

/**
 * Count leading zeros in the topmost 4 bits of a 128-bit integer.
 */
#define ECMA_UINT128_CLZ_MAX4(name) \
  __builtin_clzll (name.hi)

#else /* !__GNUC__ && !__clang__ */

/**
 * Count leading zeros in a 64-bit integer. The behaviour is undefined for 0.
 *
 * @return number of leading zeros.
 */
static inline int JERRY_ATTR_ALWAYS_INLINE
ecma_uint64_clz (uint64_t n) /**< integer to count leading zeros in */
{
  JERRY_ASSERT (n != 0);

  int cnt = 0;
  uint64_t one = 0x8000000000000000ull;
  while ((n & one) == 0)
  {
    cnt++;
    one >>= 1;
  }
  return cnt;
} /* ecma_uint64_clz */

/**
 * Number of leading zeros in 4-bit integers.
 */
static const uint8_t ecma_uint4_clz[] = { 4, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 };

/**
 * Count leading zeros in the topmost 64 bits of a 128-bit integer.
 */
#define ECMA_UINT128_CLZ_MAX63(name) \
  ecma_uint64_clz (name.hi)

/**
 * Count leading zeros in the topmost 4 bits of a 128-bit integer.
 */
#define ECMA_UINT128_CLZ_MAX4(name) \
  ecma_uint4_clz[name.hi >> 60]

#endif /* __GNUC__ || __clang__ */

/**
 * @}
 */

/**
 * Number.MAX_VALUE exponent part when using 64 bit float representation.
 */
#define NUMBER_MAX_DECIMAL_EXPONENT 308
/**
 * Number.MIN_VALUE exponent part when using 64 bit float representation.
 */
#define NUMBER_MIN_DECIMAL_EXPONENT -324

#elif CONFIG_ECMA_NUMBER_TYPE == CONFIG_ECMA_NUMBER_FLOAT32

/**
 * Number.MAX_VALUE exponent part when using 32 bit float representation.
 */
#define NUMBER_MAX_DECIMAL_EXPONENT 38
/**
 * Number.MIN_VALUE exponent part when using 32 bit float representation.
 */
#define NUMBER_MIN_DECIMAL_EXPONENT -45

#endif /* CONFIG_ECMA_NUMBER_TYPE == CONFIG_ECMA_NUMBER_FLOAT64 */

/**
 * Value of epsilon
 */
#define EPSILON 0.0000001

/**
 * ECMA-defined conversion of string to Number.
 *
 * See also:
 *          ECMA-262 v5, 9.3.1
 *
 * @return ecma-number
 */
ecma_number_t
ecma_utf8_string_to_number (const lit_utf8_byte_t *str_p, /**< utf-8 string */
                            lit_utf8_size_t str_size) /**< string size */
{
  /* TODO: Check license issues */

  if (str_size == 0)
  {
    return ECMA_NUMBER_ZERO;
  }

  const lit_utf8_byte_t *str_curr_p = str_p;
  const lit_utf8_byte_t *str_end_p = str_p + str_size;
  ecma_char_t code_unit;

  while (str_curr_p < str_end_p)
  {
    code_unit = lit_utf8_peek_next (str_curr_p);
    if (lit_char_is_white_space (code_unit) || lit_char_is_line_terminator (code_unit))
    {
      lit_utf8_incr (&str_curr_p);
    }
    else
    {
      break;
    }
  }

  const lit_utf8_byte_t *begin_p = str_curr_p;
  str_curr_p = (lit_utf8_byte_t *) str_end_p;

  while (str_curr_p > str_p)
  {
    code_unit = lit_utf8_peek_prev (str_curr_p);
    if (lit_char_is_white_space (code_unit) || lit_char_is_line_terminator (code_unit))
    {
      lit_utf8_decr (&str_curr_p);
    }
    else
    {
      break;
    }
  }

  const lit_utf8_byte_t *end_p = str_curr_p - 1;

  if (begin_p > end_p)
  {
    return ECMA_NUMBER_ZERO;
  }

  if ((end_p >= begin_p + 2)
      && begin_p[0] == LIT_CHAR_0
      && (begin_p[1] == LIT_CHAR_LOWERCASE_X
          || begin_p[1] == LIT_CHAR_UPPERCASE_X))
  {
    /* Hex literal handling */
    begin_p += 2;

    ecma_number_t num = ECMA_NUMBER_ZERO;

    for (const lit_utf8_byte_t * iter_p = begin_p;
         iter_p <= end_p;
         iter_p++)
    {
      int32_t digit_value;

      if (*iter_p >= LIT_CHAR_0
          && *iter_p <= LIT_CHAR_9)
      {
        digit_value = (*iter_p - LIT_CHAR_0);
      }
      else if (*iter_p >= LIT_CHAR_LOWERCASE_A
               && *iter_p <= LIT_CHAR_LOWERCASE_F)
      {
        digit_value = 10 + (*iter_p - LIT_CHAR_LOWERCASE_A);
      }
      else if (*iter_p >= LIT_CHAR_UPPERCASE_A
               && *iter_p <= LIT_CHAR_UPPERCASE_F)
      {
        digit_value = 10 + (*iter_p - LIT_CHAR_UPPERCASE_A);
      }
      else
      {
        return ecma_number_make_nan ();
      }

      num = num * 16 + (ecma_number_t) digit_value;
    }

    return num;
  }

  bool sign = false; /* positive */

  if (*begin_p == LIT_CHAR_PLUS)
  {
    begin_p++;
  }
  else if (*begin_p == LIT_CHAR_MINUS)
  {
    sign = true; /* negative */

    begin_p++;
  }

  if (begin_p > end_p)
  {
    return ecma_number_make_nan ();
  }

  /* Checking if significant part of parse string is equal to "Infinity" */
  const lit_utf8_byte_t *infinity_zt_str_p = lit_get_magic_string_utf8 (LIT_MAGIC_STRING_INFINITY_UL);

  JERRY_ASSERT (strlen ((const char *) infinity_zt_str_p) == 8);

  if ((end_p - begin_p) == (8 - 1) && memcmp (infinity_zt_str_p, begin_p, 8) == 0)
  {
    return ecma_number_make_infinity (sign);
  }

  uint64_t fraction_uint64 = 0;
  uint32_t digits = 0;
  int32_t e = 0;
  bool digit_seen = false;

  /* Parsing digits before dot (or before end of digits part if there is no dot in number) */
  while (begin_p <= end_p)
  {
    int32_t digit_value;

    if (*begin_p >= LIT_CHAR_0
        && *begin_p <= LIT_CHAR_9)
    {
      digit_seen = true;
      digit_value = (*begin_p - LIT_CHAR_0);
    }
    else
    {
      break;
    }

    if (digits != 0 || digit_value != 0)
    {
      if (digits < ECMA_NUMBER_MAX_DIGITS)
      {
        fraction_uint64 = fraction_uint64 * 10 + (uint32_t) digit_value;
        digits++;
      }
      else
      {
        e++;
      }
    }

    begin_p++;
  }

  if (begin_p <= end_p
      && *begin_p == LIT_CHAR_DOT)
  {
    begin_p++;

    if (!digit_seen && begin_p > end_p)
    {
      return ecma_number_make_nan ();
    }

    /* Parsing number's part that is placed after dot */
    while (begin_p <= end_p)
    {
      int32_t digit_value;

      if (*begin_p >= LIT_CHAR_0
          && *begin_p <= LIT_CHAR_9)
      {
        digit_seen = true;
        digit_value = (*begin_p - LIT_CHAR_0);
      }
      else
      {
        break;
      }

      if (digits < ECMA_NUMBER_MAX_DIGITS)
      {
        if (digits != 0 || digit_value != 0)
        {
          fraction_uint64 = fraction_uint64 * 10 + (uint32_t) digit_value;
          digits++;
        }

        e--;
      }

      begin_p++;
    }
  }

  /* Parsing exponent literal */
  int32_t e_in_lit = 0;
  bool e_in_lit_sign = false;

  if (begin_p <= end_p
      && (*begin_p == LIT_CHAR_LOWERCASE_E
          || *begin_p == LIT_CHAR_UPPERCASE_E))
  {
    begin_p++;

    if (!digit_seen || begin_p > end_p)
    {
      return ecma_number_make_nan ();
    }

    if (*begin_p == LIT_CHAR_PLUS)
    {
      begin_p++;
    }
    else if (*begin_p == LIT_CHAR_MINUS)
    {
      e_in_lit_sign = true;
      begin_p++;
    }

    if (begin_p > end_p)
    {
      return ecma_number_make_nan ();
    }

    while (begin_p <= end_p)
    {
      int32_t digit_value;

      if (*begin_p >= LIT_CHAR_0
          && *begin_p <= LIT_CHAR_9)
      {
        digit_value = (*begin_p - LIT_CHAR_0);
      }
      else
      {
        return ecma_number_make_nan ();
      }

      e_in_lit = e_in_lit * 10 + digit_value;
      int32_t e_check = e + (int32_t) digits - 1  + (e_in_lit_sign ? -e_in_lit : e_in_lit);

      if (e_check > NUMBER_MAX_DECIMAL_EXPONENT)
      {
        return ecma_number_make_infinity (sign);
      }
      else if (e_check < NUMBER_MIN_DECIMAL_EXPONENT)
      {
        return sign ? -ECMA_NUMBER_ZERO : ECMA_NUMBER_ZERO;
      }

      begin_p++;
    }
  }

  /* Adding value of exponent literal to exponent value */
  if (e_in_lit_sign)
  {
    e -= e_in_lit;
  }
  else
  {
    e += e_in_lit;
  }

  bool e_sign;

  if (e < 0)
  {
    e_sign = true;
    e = -e;
  }
  else
  {
    e_sign = false;
  }

  if (begin_p <= end_p)
  {
    return ecma_number_make_nan ();
  }

  JERRY_ASSERT (begin_p == end_p + 1);

  if (fraction_uint64 == 0)
  {
    return sign ? -ECMA_NUMBER_ZERO : ECMA_NUMBER_ZERO;
  }

#if CONFIG_ECMA_NUMBER_TYPE == CONFIG_ECMA_NUMBER_FLOAT64
  /*
   * 128-bit mantissa storage
   *
   * Normalized: |4 bits zero|124-bit mantissa with highest bit set to 1|
   */
  ecma_uint128_t fraction_uint128 = { 0, fraction_uint64 };

  /* Normalizing mantissa */
  int shift = 4 - ECMA_UINT128_CLZ_MAX63 (fraction_uint128);
  if (shift < 0)
  {
    ECMA_UINT128_LEFT_SHIFT_MAX63 (fraction_uint128, -shift);
  }
  else
  {
    ECMA_UINT128_RIGHT_SHIFT_MAX63 (fraction_uint128, shift);
  }
  int32_t binary_exponent = 1 + shift;

  if (!e_sign)
  {
    /* positive or zero decimal exponent */
    JERRY_ASSERT (e >= 0);

    while (e > 0)
    {
      JERRY_ASSERT (ECMA_UINT128_CLZ_MAX63 (fraction_uint128) == 4);

      ECMA_UINT128_MUL10 (fraction_uint128);

      e--;

      /* Normalizing mantissa */
      shift = 4 - ECMA_UINT128_CLZ_MAX4 (fraction_uint128);
      JERRY_ASSERT (shift >= 0);
      ECMA_UINT128_RIGHT_SHIFT_MAX63 (fraction_uint128, shift);
      binary_exponent += shift;
    }
  }
  else
  {
    /* negative decimal exponent */
    JERRY_ASSERT (e != 0);

    while (e > 0)
    {
      /* Denormalizing mantissa, moving highest 1 to bit 127 */
      shift = ECMA_UINT128_CLZ_MAX4 (fraction_uint128);
      JERRY_ASSERT (shift <= 4);
      ECMA_UINT128_LEFT_SHIFT_MAX63 (fraction_uint128, shift);
      binary_exponent -= shift;

      JERRY_ASSERT (!ECMA_UINT128_IS_ZERO (fraction_uint128));

      ECMA_UINT128_DIV10 (fraction_uint128);

      e--;
    }

    /* Normalizing mantissa */
    shift = 4 - ECMA_UINT128_CLZ_MAX4 (fraction_uint128);
    JERRY_ASSERT (shift >= 0);
    ECMA_UINT128_RIGHT_SHIFT_MAX63 (fraction_uint128, shift);
    binary_exponent += shift;

    JERRY_ASSERT (ECMA_UINT128_CLZ_MAX63 (fraction_uint128) == 4);
  }

  JERRY_ASSERT (!ECMA_UINT128_IS_ZERO (fraction_uint128));
  JERRY_ASSERT (ECMA_UINT128_CLZ_MAX63 (fraction_uint128) == 4);

  /*
   * Preparing mantissa for conversion to 52-bit representation, converting it to:
   *
   * |11 zero bits|1|116 mantissa bits|
   */
  ECMA_UINT128_RIGHT_SHIFT_MAX63 (fraction_uint128, 7u);
  binary_exponent += 7;

  JERRY_ASSERT (ECMA_UINT128_CLZ_MAX63 (fraction_uint128) == 11);

  fraction_uint64 = ECMA_UINT128_ROUND_HIGH_TO_UINT64 (fraction_uint128);

  return ecma_number_make_from_sign_mantissa_and_exponent (sign, fraction_uint64, binary_exponent);
#elif CONFIG_ECMA_NUMBER_TYPE == CONFIG_ECMA_NUMBER_FLOAT32
  /* Less precise conversion */
  ecma_number_t num = (ecma_number_t) (uint32_t) fraction_uint64;

  ecma_number_t m = e_sign ? (ecma_number_t) 0.1 : (ecma_number_t) 10.0;

  while (e)
  {
    if (e % 2)
    {
      num *= m;
    }

    m *= m;
    e /= 2;
  }

  return num;
#endif /* CONFIG_ECMA_NUMBER_TYPE == CONFIG_ECMA_NUMBER_FLOAT64 */
} /* ecma_utf8_string_to_number */

/**
 * ECMA-defined conversion of UInt32 to String (zero-terminated).
 *
 * See also:
 *          ECMA-262 v5, 9.8.1
 *
 * @return number of bytes copied to buffer
 */
lit_utf8_size_t
ecma_uint32_to_utf8_string (uint32_t value, /**< value to convert */
                            lit_utf8_byte_t *out_buffer_p, /**< buffer for string */
                            lit_utf8_size_t buffer_size) /**< size of buffer */
{
  lit_utf8_byte_t *buf_p = out_buffer_p + buffer_size;

  do
  {
    JERRY_ASSERT (buf_p >= out_buffer_p);

    buf_p--;
    *buf_p = (lit_utf8_byte_t) ((value % 10) + LIT_CHAR_0);
    value /= 10;
  }
  while (value != 0);

  JERRY_ASSERT (buf_p >= out_buffer_p);

  lit_utf8_size_t bytes_copied = (lit_utf8_size_t) (out_buffer_p + buffer_size - buf_p);

  if (JERRY_LIKELY (buf_p != out_buffer_p))
  {
    memmove (out_buffer_p, buf_p, bytes_copied);
  }

  return bytes_copied;
} /* ecma_uint32_to_utf8_string */

/**
 * ECMA-defined conversion of Number value to UInt32 value
 *
 * See also:
 *          ECMA-262 v5, 9.6
 *
 * @return 32-bit unsigned integer - result of conversion.
 */
uint32_t
ecma_number_to_uint32 (ecma_number_t num) /**< ecma-number */
{
  if (ecma_number_is_nan (num)
      || ecma_number_is_zero (num)
      || ecma_number_is_infinity (num))
  {
    return 0;
  }

  const bool sign = ecma_number_is_negative (num);
  const ecma_number_t abs_num = sign ? -num : num;

  /* 2 ^ 32 */
  const uint64_t uint64_2_pow_32 = (1ull << 32);

  const ecma_number_t num_2_pow_32 = (float) uint64_2_pow_32;

  ecma_number_t num_in_uint32_range;

  if (abs_num >= num_2_pow_32)
  {
    num_in_uint32_range = ecma_number_calc_remainder (abs_num,
                                                      num_2_pow_32);
  }
  else
  {
    num_in_uint32_range = abs_num;
  }

  /* Check that the floating point value can be represented with uint32_t. */
  JERRY_ASSERT (num_in_uint32_range < uint64_2_pow_32);
  uint32_t uint32_num = (uint32_t) num_in_uint32_range;

  const uint32_t ret = sign ? -uint32_num : uint32_num;

#ifndef JERRY_NDEBUG
  if (sign
      && uint32_num != 0)
  {
    JERRY_ASSERT (ret == uint64_2_pow_32 - uint32_num);
  }
  else
  {
    JERRY_ASSERT (ret == uint32_num);
  }
#endif /* !JERRY_NDEBUG */

  return ret;
} /* ecma_number_to_uint32 */

/**
 * ECMA-defined conversion of Number value to Int32 value
 *
 * See also:
 *          ECMA-262 v5, 9.5
 *
 * @return 32-bit signed integer - result of conversion.
 */
int32_t
ecma_number_to_int32 (ecma_number_t num) /**< ecma-number */
{
  uint32_t uint32_num = ecma_number_to_uint32 (num);

  /* 2 ^ 32 */
  const int64_t int64_2_pow_32 = (1ll << 32);

  /* 2 ^ 31 */
  const uint32_t uint32_2_pow_31 = (1ull << 31);

  int32_t ret;

  if (uint32_num >= uint32_2_pow_31)
  {
    ret = (int32_t) (uint32_num - int64_2_pow_32);
  }
  else
  {
    ret = (int32_t) uint32_num;
  }

#ifndef JERRY_NDEBUG
  int64_t int64_num = uint32_num;

  JERRY_ASSERT (int64_num >= 0);

  if (int64_num >= uint32_2_pow_31)
  {
    JERRY_ASSERT (ret == int64_num - int64_2_pow_32);
  }
  else
  {
    JERRY_ASSERT (ret == int64_num);
  }
#endif /* !JERRY_NDEBUG */

  return ret;
} /* ecma_number_to_int32 */

/**
  * Perform conversion of ecma-number to decimal representation with decimal exponent.
  *
  * Note:
  *      The calculated values correspond to s, n, k parameters in ECMA-262 v5, 9.8.1, item 5:
  *         - parameter out_digits_p corresponds to s, the digits of the number;
  *         - parameter out_decimal_exp_p corresponds to n, the decimal exponent;
  *         - return value corresponds to k, the number of digits.
  *
  * @return the number of digits
  */
lit_utf8_size_t
ecma_number_to_decimal (ecma_number_t num, /**< ecma-number */
                        lit_utf8_byte_t *out_digits_p, /**< [out] buffer to fill with digits */
                        int32_t *out_decimal_exp_p) /**< [out] decimal exponent */
{
  JERRY_ASSERT (!ecma_number_is_nan (num));
  JERRY_ASSERT (!ecma_number_is_zero (num));
  JERRY_ASSERT (!ecma_number_is_infinity (num));
  JERRY_ASSERT (!ecma_number_is_negative (num));

  return ecma_errol0_dtoa ((double) num, out_digits_p, out_decimal_exp_p);
} /* ecma_number_to_decimal */

/**
 * Calculate the number of digits from the given double value whithout franction part
 *
 * @return number of digits
 */
inline static int32_t JERRY_ATTR_ALWAYS_INLINE
ecma_number_of_digits (double val) /**< ecma number */
{
  JERRY_ASSERT (fabs (fmod (val, 1.0)) < EPSILON);
  int32_t exponent = 0;

  while (val >= 1.0)
  {
    val /= 10.0;
    exponent++;
  }

  return exponent;
} /* ecma_number_of_digits */

/**
 * Convert double value to ASCII
 */
inline static void JERRY_ATTR_ALWAYS_INLINE
ecma_double_to_ascii (double val, /**< ecma number */
                      lit_utf8_byte_t *buffer_p, /**< buffer to generate digits into */
                      int32_t num_of_digits, /**< number of digits */
                      int32_t *exp_p) /**< [out] exponent */
{
  int32_t char_cnt = 0;

  double divider = 10.0;
  double prev_residual;
  double mod_res = fmod (val, divider);

  buffer_p[num_of_digits - 1 - char_cnt++] = (lit_utf8_byte_t) ((int) mod_res + '0');
  divider *= 10.0;
  prev_residual = mod_res;

  while (char_cnt < num_of_digits)
  {
    mod_res = fmod (val, divider);
    double residual = mod_res - prev_residual;
    buffer_p[num_of_digits - 1 - char_cnt++] = (lit_utf8_byte_t) ((int) (residual / (divider / 10.0)) + '0');

    divider *= 10.0;
    prev_residual = mod_res;
  }

  *exp_p = char_cnt;
} /* ecma_double_to_ascii */

/**
 * Double to binary floating-point number conversion
 *
 * @return number of generated digits
 */
static inline lit_utf8_size_t JERRY_ATTR_ALWAYS_INLINE
ecma_double_to_binary_floating_point (double val, /**< ecma number */
                                      lit_utf8_byte_t *buffer_p, /**< buffer to generate digits into */
                                      int32_t *exp_p) /**< [out] exponent */
{
  int32_t char_cnt = 0;
  double integer_part, fraction_part;

  fraction_part = fmod (val, 1.0);
  integer_part = floor (val);
  int32_t num_of_digits = ecma_number_of_digits (integer_part);

  if (fabs (integer_part) < EPSILON)
  {
    buffer_p[0] = '0';
    char_cnt++;
  }
  else if (num_of_digits <= 16) /* Ensure that integer_part is not rounded */
  {
    while (integer_part > 0.0)
    {
      buffer_p[num_of_digits - 1 - char_cnt++] = (lit_utf8_byte_t) ((int) fmod (integer_part, 10.0) + '0');
      integer_part = floor (integer_part / 10.0);
    }
  }
  else if (num_of_digits <= 21)
  {
    ecma_double_to_ascii (integer_part, buffer_p, num_of_digits, &char_cnt);
  }
  else
  {
    /* According to ECMA-262 v5, 15.7.4.5, step 7: if x >= 10^21, then execution will continue with
     * ToString(x) so in this case no further conversions are required. Number 21 in the else if condition
     * above must be kept in sync with the number 21 in ecma_builtin_number_prototype_object_to_fixed
     * method, step 7. */
    *exp_p = num_of_digits;
    return 0;
  }

  *exp_p = char_cnt;

  while (fraction_part > 0 && char_cnt < ECMA_MAX_CHARS_IN_STRINGIFIED_NUMBER - 1)
  {
    fraction_part *= 10;
    double tmp = fraction_part;
    fraction_part = fmod (fraction_part, 1.0);
    integer_part = floor (tmp);
    buffer_p[char_cnt++] = (lit_utf8_byte_t) ('0' + (int) integer_part);
  }

  buffer_p[char_cnt] = '\0';

  return (lit_utf8_size_t) (char_cnt - *exp_p);
} /* ecma_double_to_binary_floating_point */

/**
 * Perform conversion of ecma-number to equivalent binary floating-point number representation with decimal exponent.
 *
 * Note:
 *      The calculated values correspond to s, n, k parameters in ECMA-262 v5, 9.8.1, item 5:
 *         - parameter out_digits_p corresponds to s, the digits of the number;
 *         - parameter out_decimal_exp_p corresponds to n, the decimal exponent;
 *         - return value corresponds to k, the number of digits.
 *
 * @return the number of digits
 */
lit_utf8_size_t
ecma_number_to_binary_floating_point_number (ecma_number_t num, /**< ecma-number */
                                             lit_utf8_byte_t *out_digits_p, /**< [out] buffer to fill with digits */
                                             int32_t *out_decimal_exp_p) /**< [out] decimal exponent */
{
  JERRY_ASSERT (!ecma_number_is_nan (num));
  JERRY_ASSERT (!ecma_number_is_zero (num));
  JERRY_ASSERT (!ecma_number_is_infinity (num));
  JERRY_ASSERT (!ecma_number_is_negative (num));

  return ecma_double_to_binary_floating_point ((double) num, out_digits_p, out_decimal_exp_p);
} /* ecma_number_to_binary_floating_point_number */

/**
 * Convert ecma-number to zero-terminated string
 *
 * See also:
 *          ECMA-262 v5, 9.8.1
 *
 *
 * @return size of utf-8 string
 */
lit_utf8_size_t
ecma_number_to_utf8_string (ecma_number_t num, /**< ecma-number */
                            lit_utf8_byte_t *buffer_p, /**< buffer for utf-8 string */
                            lit_utf8_size_t buffer_size) /**< size of buffer */
{
  lit_utf8_byte_t *dst_p;

  if (ecma_number_is_nan (num))
  {
    /* 1. */
    dst_p = lit_copy_magic_string_to_buffer (LIT_MAGIC_STRING_NAN, buffer_p, buffer_size);
    return (lit_utf8_size_t) (dst_p - buffer_p);
  }

  if (ecma_number_is_zero (num))
  {
    /* 2. */
    *buffer_p = LIT_CHAR_0;
    JERRY_ASSERT (1 <= buffer_size);
    return 1;
  }

  dst_p = buffer_p;

  if (ecma_number_is_negative (num))
  {
    /* 3. */
    *dst_p++ = LIT_CHAR_MINUS;
    num = -num;
  }

  if (ecma_number_is_infinity (num))
  {
    /* 4. */
    dst_p = lit_copy_magic_string_to_buffer (LIT_MAGIC_STRING_INFINITY_UL, dst_p,
                                             (lit_utf8_size_t) (buffer_p + buffer_size - dst_p));
    JERRY_ASSERT (dst_p <= buffer_p + buffer_size);
    return (lit_utf8_size_t) (dst_p - buffer_p);
  }

  JERRY_ASSERT (ecma_number_get_next (ecma_number_get_prev (num)) == num);

  /* 5. */
  uint32_t num_uint32 = ecma_number_to_uint32 (num);

  if (((ecma_number_t) num_uint32) == num)
  {
    dst_p += ecma_uint32_to_utf8_string (num_uint32, dst_p, (lit_utf8_size_t) (buffer_p + buffer_size - dst_p));
    JERRY_ASSERT (dst_p <= buffer_p + buffer_size);
    return (lit_utf8_size_t) (dst_p - buffer_p);
  }

  /* decimal exponent */
  int32_t n;
  /* number of digits in mantissa */
  int32_t k;

  k = (int32_t) ecma_number_to_decimal (num, dst_p, &n);

  if (k <= n && n <= 21)
  {
    /* 6. */
    dst_p += k;

    memset (dst_p, LIT_CHAR_0, (size_t) (n - k));
    dst_p += n - k;

    JERRY_ASSERT (dst_p <= buffer_p + buffer_size);
    return (lit_utf8_size_t) (dst_p - buffer_p);
  }

  if (0 < n && n <= 21)
  {
    /* 7. */
    memmove (dst_p + n + 1, dst_p + n, (size_t) (k - n));
    *(dst_p + n) = LIT_CHAR_DOT;
    dst_p += k + 1;

    JERRY_ASSERT (dst_p <= buffer_p + buffer_size);
    return (lit_utf8_size_t) (dst_p - buffer_p);
  }

  if (-6 < n && n <= 0)
  {
    /* 8. */
    memmove (dst_p + 2 - n, dst_p, (size_t) k);
    memset (dst_p + 2, LIT_CHAR_0, (size_t) -n);
    *dst_p = LIT_CHAR_0;
    *(dst_p + 1) = LIT_CHAR_DOT;
    dst_p += k - n + 2;

    JERRY_ASSERT (dst_p <= buffer_p + buffer_size);
    return (lit_utf8_size_t) (dst_p - buffer_p);
  }

  if (k == 1)
  {
    /* 9. */
    dst_p++;
  }
  else
  {
    /* 10. */
    memmove (dst_p + 2, dst_p + 1, (size_t) (k - 1));
    *(dst_p + 1) = LIT_CHAR_DOT;
    dst_p += k + 1;
  }

  /* 9., 10. */
  *dst_p++ = LIT_CHAR_LOWERCASE_E;
  *dst_p++ = (n >= 1) ? LIT_CHAR_PLUS : LIT_CHAR_MINUS;
  uint32_t t = (uint32_t) (n >= 1 ? (n - 1) : -(n - 1));

  dst_p += ecma_uint32_to_utf8_string (t, dst_p, (lit_utf8_size_t) (buffer_p + buffer_size - dst_p));

  JERRY_ASSERT (dst_p <= buffer_p + buffer_size);

  return (lit_utf8_size_t) (dst_p - buffer_p);
} /* ecma_number_to_utf8_string */

/**
 * @}
 * @}
 */
