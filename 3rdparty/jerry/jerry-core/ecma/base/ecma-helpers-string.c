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

#include "ecma-alloc.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "jrt.h"
#include "jrt-libc-includes.h"
#include "lit-char-helpers.h"
#include "lit-magic-strings.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmahelpers Helpers for operations with ECMA data types
 * @{
 */

JERRY_STATIC_ASSERT (ECMA_STRING_CONTAINER_MASK + 1 == ECMA_STRING_REF_ONE,
                     ecma_string_ref_counter_should_start_after_the_container_field);

JERRY_STATIC_ASSERT (ECMA_STRING_CONTAINER_MASK >= ECMA_STRING_CONTAINER__MAX,
                     ecma_string_container_types_must_be_lower_than_the_container_mask);

JERRY_STATIC_ASSERT ((ECMA_STRING_MAX_REF | ECMA_STRING_CONTAINER_MASK) == UINT16_MAX,
                     ecma_string_ref_and_container_fields_should_fill_the_16_bit_field);

JERRY_STATIC_ASSERT (ECMA_STRING_NOT_ARRAY_INDEX == UINT32_MAX,
                     ecma_string_not_array_index_must_be_equal_to_uint32_max);

JERRY_STATIC_ASSERT ((ECMA_TYPE_DIRECT_STRING & 0x1) != 0,
                     ecma_type_direct_string_must_be_odd_number);

JERRY_STATIC_ASSERT (LIT_MAGIC_STRING__COUNT <= ECMA_DIRECT_STRING_MAX_IMM,
                     all_magic_strings_must_be_encoded_as_direct_string);

JERRY_STATIC_ASSERT ((int) ECMA_DIRECT_STRING_UINT == (int) ECMA_STRING_CONTAINER_UINT32_IN_DESC
                     && (int) ECMA_DIRECT_STRING_MAGIC_EX == (int) ECMA_STRING_CONTAINER_MAGIC_STRING_EX,
                     ecma_direct_and_container_types_must_match);

JERRY_STATIC_ASSERT (ECMA_PROPERTY_NAME_TYPE_SHIFT > ECMA_VALUE_SHIFT,
                     ecma_property_name_type_shift_must_be_greater_than_ecma_value_shift);


/**
 * Convert a string to an unsigned 32 bit value if possible
 *
 * @return true if the conversion is successful
 *         false otherwise
 */
static bool
ecma_string_to_array_index (const lit_utf8_byte_t *string_p, /**< utf-8 string */
                            lit_utf8_size_t string_size, /**< string size */
                            uint32_t *result_p) /**< [out] converted value */
{
  JERRY_ASSERT (string_size > 0 && *string_p >= LIT_CHAR_0 && *string_p <= LIT_CHAR_9);

  if (*string_p == LIT_CHAR_0)
  {
    *result_p = 0;
    return (string_size == 1);
  }

  if (string_size > ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32)
  {
    return false;
  }

  uint32_t index = 0;
  const lit_utf8_byte_t *string_end_p = string_p + string_size;

  if (string_size == ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32)
  {
    string_end_p--;
  }

  do
  {
    if (*string_p > LIT_CHAR_9 || *string_p < LIT_CHAR_0)
    {
      return false;
    }

    index = (index * 10) + (uint32_t) (*string_p++ - LIT_CHAR_0);
  }
  while (string_p < string_end_p);

  if (string_size < ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32)
  {
    *result_p = index;
    return true;
  }

  /* Overflow must be checked as well when size is
   * equal to ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32. */
  if (*string_p > LIT_CHAR_9
      || *string_p < LIT_CHAR_0
      || index > (UINT32_MAX / 10)
      || (index == (UINT32_MAX / 10) && *string_p > LIT_CHAR_5))
  {
    return false;
  }

  *result_p = (index * 10) + (uint32_t) (*string_p - LIT_CHAR_0);
  return true;
} /* ecma_string_to_array_index */

/**
 * Returns the characters and size of a string.
 *
 * Note:
 *   UINT type is not supported
 *
 * @return byte array start - if the byte array of a string is available
 *         NULL - otherwise
 */
static const lit_utf8_byte_t *
ecma_string_get_chars_fast (const ecma_string_t *string_p, /**< ecma-string */
                            lit_utf8_size_t *size_p) /**< [out] size of the ecma string */
{
  if (ECMA_IS_DIRECT_STRING (string_p))
  {
    if (ECMA_GET_DIRECT_STRING_TYPE (string_p) == ECMA_DIRECT_STRING_MAGIC)
    {
      lit_magic_string_id_t id = (lit_magic_string_id_t) ECMA_GET_DIRECT_STRING_VALUE (string_p);

      *size_p = lit_get_magic_string_size (id);
      return lit_get_magic_string_utf8 (id);
    }

    JERRY_ASSERT (ECMA_GET_DIRECT_STRING_TYPE (string_p) == ECMA_DIRECT_STRING_MAGIC_EX);

    lit_magic_string_ex_id_t id = (lit_magic_string_ex_id_t) ECMA_GET_DIRECT_STRING_VALUE (string_p);

    *size_p = lit_get_magic_string_ex_size (id);
    return lit_get_magic_string_ex_utf8 (id);
  }

  JERRY_ASSERT (string_p->refs_and_container >= ECMA_STRING_REF_ONE);

  switch (ECMA_STRING_GET_CONTAINER (string_p))
  {
    case ECMA_STRING_CONTAINER_HEAP_UTF8_STRING:
    {
      *size_p = string_p->u.utf8_string.size;
      return (const lit_utf8_byte_t *) (string_p + 1);
    }
    case ECMA_STRING_CONTAINER_HEAP_LONG_UTF8_STRING:
    {
      *size_p = string_p->u.long_utf8_string_size;
      ecma_long_string_t *long_string_p = (ecma_long_string_t *) string_p;
      return (const lit_utf8_byte_t *) (long_string_p + 1);
    }
    default:
    {
      JERRY_ASSERT (ECMA_STRING_GET_CONTAINER (string_p) == ECMA_STRING_CONTAINER_MAGIC_STRING_EX);

      *size_p = lit_get_magic_string_ex_size (string_p->u.magic_string_ex_id);
      return lit_get_magic_string_ex_utf8 (string_p->u.magic_string_ex_id);
    }
  }
} /* ecma_string_get_chars_fast */

/**
 * Allocate new ecma-string and fill it with reference to ECMA magic string
 *
 * @return pointer to ecma-string descriptor
 */
static ecma_string_t *
ecma_new_ecma_string_from_magic_string_ex_id (lit_magic_string_ex_id_t id) /**< identifier of externl magic string */
{
  JERRY_ASSERT (id < lit_get_magic_string_ex_count ());

  if (JERRY_LIKELY (id <= ECMA_DIRECT_STRING_MAX_IMM))
  {
    return (ecma_string_t *) ECMA_CREATE_DIRECT_STRING (ECMA_DIRECT_STRING_MAGIC_EX, (uintptr_t) id);
  }

  ecma_string_t *string_desc_p = ecma_alloc_string ();

  string_desc_p->refs_and_container = ECMA_STRING_CONTAINER_MAGIC_STRING_EX | ECMA_STRING_REF_ONE;
  string_desc_p->hash = (lit_string_hash_t) (LIT_MAGIC_STRING__COUNT + id);
  string_desc_p->u.magic_string_ex_id = id;

  return string_desc_p;
} /* ecma_new_ecma_string_from_magic_string_ex_id */

#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
/**
 * Allocate new ecma-string and fill it with reference to the symbol descriptor
 *
 * @return pointer to ecma-string descriptor
 */
ecma_string_t *
ecma_new_symbol_from_descriptor_string (ecma_value_t string_desc) /**< ecma-string */
{
  JERRY_ASSERT (!ecma_is_value_symbol (string_desc));

  ecma_string_t *symbol_p = ecma_alloc_string ();
  symbol_p->refs_and_container = ECMA_STRING_REF_ONE | ECMA_STRING_CONTAINER_SYMBOL;
  symbol_p->u.symbol_descriptor = string_desc;
  symbol_p->hash = (uint16_t) (((uintptr_t) symbol_p) >> ECMA_SYMBOL_HASH_SHIFT);
  JERRY_ASSERT ((symbol_p->hash & ECMA_GLOBAL_SYMBOL_FLAG) == 0);

  return symbol_p;
} /* ecma_new_symbol_from_descriptor_string */

/**
 * Check whether an ecma-string contains an ecma-symbol
 *
 * @return true - if the ecma-string contains an ecma-symbol
 *         false - otherwise
 */
bool
ecma_prop_name_is_symbol (ecma_string_t *string_p) /**< ecma-string */
{
  JERRY_ASSERT (string_p != NULL);

  return (!ECMA_IS_DIRECT_STRING (string_p)
          && ECMA_STRING_GET_CONTAINER (string_p) == ECMA_STRING_CONTAINER_SYMBOL);
} /* ecma_prop_name_is_symbol */
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */

/**
 * Allocate new ecma-string and fill it with characters from the utf8 string
 *
 * @return pointer to ecma-string descriptor
 */
ecma_string_t *
ecma_new_ecma_string_from_utf8 (const lit_utf8_byte_t *string_p, /**< utf-8 string */
                                lit_utf8_size_t string_size) /**< string size */
{
  JERRY_ASSERT (string_p != NULL || string_size == 0);
  JERRY_ASSERT (lit_is_valid_cesu8_string (string_p, string_size));

  lit_magic_string_id_t magic_string_id = lit_is_utf8_string_magic (string_p, string_size);

  if (magic_string_id != LIT_MAGIC_STRING__COUNT)
  {
    return ecma_get_magic_string (magic_string_id);
  }

  JERRY_ASSERT (string_size > 0);

  if (*string_p >= LIT_CHAR_0 && *string_p <= LIT_CHAR_9)
  {
    uint32_t array_index;

    if (ecma_string_to_array_index (string_p, string_size, &array_index))
    {
      return ecma_new_ecma_string_from_uint32 (array_index);
    }
  }

  if (lit_get_magic_string_ex_count () > 0)
  {
    lit_magic_string_ex_id_t magic_string_ex_id = lit_is_ex_utf8_string_magic (string_p, string_size);

    if (magic_string_ex_id < lit_get_magic_string_ex_count ())
    {
      return ecma_new_ecma_string_from_magic_string_ex_id (magic_string_ex_id);
    }
  }

  ecma_string_t *string_desc_p;
  lit_utf8_byte_t *data_p;

  if (JERRY_LIKELY (string_size <= UINT16_MAX))
  {
    string_desc_p = ecma_alloc_string_buffer (sizeof (ecma_string_t) + string_size);

    string_desc_p->refs_and_container = ECMA_STRING_CONTAINER_HEAP_UTF8_STRING | ECMA_STRING_REF_ONE;
    string_desc_p->u.common_uint32_field = 0;
    string_desc_p->u.utf8_string.size = (uint16_t) string_size;
    string_desc_p->u.utf8_string.length = (uint16_t) lit_utf8_string_length (string_p, string_size);

    data_p = (lit_utf8_byte_t *) (string_desc_p + 1);
  }
  else
  {
    string_desc_p = ecma_alloc_string_buffer (sizeof (ecma_long_string_t) + string_size);

    string_desc_p->refs_and_container = ECMA_STRING_CONTAINER_HEAP_LONG_UTF8_STRING | ECMA_STRING_REF_ONE;
    string_desc_p->u.common_uint32_field = 0;
    string_desc_p->u.long_utf8_string_size = string_size;

    ecma_long_string_t *long_string_desc_p = (ecma_long_string_t *) string_desc_p;
    long_string_desc_p->long_utf8_string_length = lit_utf8_string_length (string_p, string_size);

    data_p = (lit_utf8_byte_t *) (long_string_desc_p + 1);
  }

  string_desc_p->hash = lit_utf8_string_calc_hash (string_p, string_size);
  memcpy (data_p, string_p, string_size);
  return string_desc_p;
} /* ecma_new_ecma_string_from_utf8 */

/**
 * Allocate a new ecma-string and initialize it from the utf8 string argument.
 * All 4-bytes long unicode sequences are converted into two 3-bytes long sequences.
 *
 * @return pointer to ecma-string descriptor
 */
ecma_string_t *
ecma_new_ecma_string_from_utf8_converted_to_cesu8 (const lit_utf8_byte_t *string_p, /**< utf-8 string */
                                                   lit_utf8_size_t string_size) /**< utf-8 string size */
{
  JERRY_ASSERT (string_p != NULL || string_size == 0);

  ecma_string_t *string_desc_p = NULL;

  ecma_length_t converted_string_length = 0;
  lit_utf8_size_t converted_string_size = 0;
  lit_utf8_size_t pos = 0;

  /* Calculate the required length and size information of the converted cesu-8 encoded string */
  while (pos < string_size)
  {
    if ((string_p[pos] & LIT_UTF8_1_BYTE_MASK) == LIT_UTF8_1_BYTE_MARKER)
    {
      pos++;
    }
    else if ((string_p[pos] & LIT_UTF8_2_BYTE_MASK) == LIT_UTF8_2_BYTE_MARKER)
    {
      pos += 2;
    }
    else if ((string_p[pos] & LIT_UTF8_3_BYTE_MASK) == LIT_UTF8_3_BYTE_MARKER)
    {
      pos += 3;
    }
    else
    {
      JERRY_ASSERT ((string_p[pos] & LIT_UTF8_4_BYTE_MASK) == LIT_UTF8_4_BYTE_MARKER);
      pos += 4;
      converted_string_size += 2;
      converted_string_length++;
    }

    converted_string_length++;
  }

  JERRY_ASSERT (pos == string_size);

  if (converted_string_size == 0)
  {
    return ecma_new_ecma_string_from_utf8 (string_p, string_size);
  }
  else
  {
    converted_string_size += string_size;

    JERRY_ASSERT (lit_is_valid_utf8_string (string_p, string_size));

    lit_utf8_byte_t *data_p;

    if (JERRY_LIKELY (converted_string_size <= UINT16_MAX))
    {
      string_desc_p = ecma_alloc_string_buffer (sizeof (ecma_string_t) + converted_string_size);

      string_desc_p->refs_and_container = ECMA_STRING_CONTAINER_HEAP_UTF8_STRING | ECMA_STRING_REF_ONE;
      string_desc_p->u.common_uint32_field = 0;
      string_desc_p->u.utf8_string.size = (uint16_t) converted_string_size;
      string_desc_p->u.utf8_string.length = (uint16_t) converted_string_length;

      data_p = (lit_utf8_byte_t *) (string_desc_p + 1);
    }
    else
    {
      string_desc_p = ecma_alloc_string_buffer (sizeof (ecma_long_string_t) + converted_string_size);

      string_desc_p->refs_and_container = ECMA_STRING_CONTAINER_HEAP_LONG_UTF8_STRING | ECMA_STRING_REF_ONE;
      string_desc_p->u.common_uint32_field = 0;
      string_desc_p->u.long_utf8_string_size = converted_string_size;

      ecma_long_string_t *long_string_desc_p = (ecma_long_string_t *) string_desc_p;
      long_string_desc_p->long_utf8_string_length = converted_string_length;

      data_p = (lit_utf8_byte_t *) (long_string_desc_p + 1);
    }

    const lit_utf8_byte_t *const begin_data_p = data_p;
    pos = 0;

    while (pos < string_size)
    {
      if ((string_p[pos] & LIT_UTF8_4_BYTE_MASK) == LIT_UTF8_4_BYTE_MARKER)
      {
        /* Processing 4 byte unicode sequence. Always converted to two 3 byte long sequence. */
        uint32_t character = ((((uint32_t) string_p[pos++]) & 0x7) << 18);
        character |= ((((uint32_t) string_p[pos++]) & LIT_UTF8_LAST_6_BITS_MASK) << 12);
        character |= ((((uint32_t) string_p[pos++]) & LIT_UTF8_LAST_6_BITS_MASK) << 6);
        character |= (((uint32_t) string_p[pos++]) & LIT_UTF8_LAST_6_BITS_MASK);

        JERRY_ASSERT (character >= 0x10000);
        character -= 0x10000;

        data_p += lit_char_to_utf8_bytes (data_p, (ecma_char_t) (0xd800 | (character >> 10)));
        data_p += lit_char_to_utf8_bytes (data_p, (ecma_char_t) (0xdc00 | (character & LIT_UTF16_LAST_10_BITS_MASK)));
      }
      else
      {
        *data_p++ = string_p[pos++];
      }
    }

    JERRY_ASSERT (pos == string_size);

    string_desc_p->hash = lit_utf8_string_calc_hash (begin_data_p, converted_string_size);
  }

  return string_desc_p;
} /* ecma_new_ecma_string_from_utf8_converted_to_cesu8 */

/**
 * Allocate new ecma-string and fill it with cesu-8 character which represents specified code unit
 *
 * @return pointer to ecma-string descriptor
 */
ecma_string_t *
ecma_new_ecma_string_from_code_unit (ecma_char_t code_unit) /**< code unit */
{
  lit_utf8_byte_t lit_utf8_bytes[LIT_UTF8_MAX_BYTES_IN_CODE_UNIT];
  lit_utf8_size_t bytes_size = lit_code_unit_to_utf8 (code_unit, lit_utf8_bytes);

  return ecma_new_ecma_string_from_utf8 (lit_utf8_bytes, bytes_size);
} /* ecma_new_ecma_string_from_code_unit */

/**
 * Allocate new ecma-string and fill it with ecma-number
 *
 * @return pointer to ecma-string descriptor
 */
ecma_string_t *
ecma_new_ecma_string_from_uint32 (uint32_t uint32_number) /**< uint32 value of the string */
{
  if (JERRY_LIKELY (uint32_number <= ECMA_DIRECT_STRING_MAX_IMM))
  {
    return (ecma_string_t *) ECMA_CREATE_DIRECT_STRING (ECMA_DIRECT_STRING_UINT, (uintptr_t) uint32_number);
  }

  ecma_string_t *string_p = ecma_alloc_string ();

  string_p->refs_and_container = ECMA_STRING_CONTAINER_UINT32_IN_DESC | ECMA_STRING_REF_ONE;
  string_p->hash = (lit_string_hash_t) uint32_number;
  string_p->u.uint32_number = uint32_number;

  return string_p;
} /* ecma_new_ecma_string_from_uint32 */

/**
 * Returns the constant assigned to the uint32 number.
 *
 * Note:
 *   Calling ecma_deref_ecma_string on the returned pointer is optional.
 *
 * @return pointer to ecma-string descriptor
 */
ecma_string_t *
ecma_get_ecma_string_from_uint32 (uint32_t uint32_number) /**< input number */
{
  JERRY_ASSERT (uint32_number <= ECMA_DIRECT_STRING_MAX_IMM);

  return (ecma_string_t *) ECMA_CREATE_DIRECT_STRING (ECMA_DIRECT_STRING_UINT, (uintptr_t) uint32_number);
} /* ecma_get_ecma_string_from_uint32 */

/**
 * Allocate new ecma-string and fill it with ecma-number
 *
 * @return pointer to ecma-string descriptor
 */
ecma_string_t *
ecma_new_ecma_string_from_number (ecma_number_t num) /**< ecma-number */
{
  uint32_t uint32_num = ecma_number_to_uint32 (num);
  if (num == ((ecma_number_t) uint32_num))
  {
    return ecma_new_ecma_string_from_uint32 (uint32_num);
  }

  if (ecma_number_is_nan (num))
  {
    return ecma_get_magic_string (LIT_MAGIC_STRING_NAN);
  }

  if (ecma_number_is_infinity (num))
  {
    lit_magic_string_id_t id = (ecma_number_is_negative (num) ? LIT_MAGIC_STRING_NEGATIVE_INFINITY_UL
                                                              : LIT_MAGIC_STRING_INFINITY_UL);
    return ecma_get_magic_string (id);
  }

  lit_utf8_byte_t str_buf[ECMA_MAX_CHARS_IN_STRINGIFIED_NUMBER];
  lit_utf8_size_t str_size = ecma_number_to_utf8_string (num, str_buf, sizeof (str_buf));

  JERRY_ASSERT (str_size > 0);
#ifndef JERRY_NDEBUG
  JERRY_ASSERT (lit_is_utf8_string_magic (str_buf, str_size) == LIT_MAGIC_STRING__COUNT
                && lit_is_ex_utf8_string_magic (str_buf, str_size) == lit_get_magic_string_ex_count ());
#endif /* !JERRY_NDEBUG */

  ecma_string_t *string_desc_p = ecma_alloc_string_buffer (sizeof (ecma_string_t) + str_size);

  string_desc_p->refs_and_container = ECMA_STRING_CONTAINER_HEAP_UTF8_STRING | ECMA_STRING_REF_ONE;
  string_desc_p->hash = lit_utf8_string_calc_hash (str_buf, str_size);
  string_desc_p->u.common_uint32_field = 0;
  string_desc_p->u.utf8_string.size = (uint16_t) str_size;
  string_desc_p->u.utf8_string.length = (uint16_t) str_size;

  lit_utf8_byte_t *data_p = (lit_utf8_byte_t *) (string_desc_p + 1);
  memcpy (data_p, str_buf, str_size);
  return string_desc_p;
} /* ecma_new_ecma_string_from_number */

/**
 * Returns the constant assigned to the magic string id.
 *
 * Note:
 *   Calling ecma_deref_ecma_string on the returned pointer is optional.
 *
 * @return pointer to ecma-string descriptor
 */
inline ecma_string_t * JERRY_ATTR_ALWAYS_INLINE
ecma_get_magic_string (lit_magic_string_id_t id) /**< identifier of magic string */
{
  JERRY_ASSERT (id < LIT_MAGIC_STRING__COUNT);
  return (ecma_string_t *) ECMA_CREATE_DIRECT_STRING (ECMA_DIRECT_STRING_MAGIC, (uintptr_t) id);
} /* ecma_get_magic_string */

/**
 * Append a cesu8 string after an ecma-string
 *
 * Note:
 *   The string1_p argument is freed. If it needs to be preserved,
 *   call ecma_ref_ecma_string with string1_p before the call.
 *
 * @return concatenation of an ecma-string and a cesu8 string
 */
ecma_string_t *
ecma_append_chars_to_string (ecma_string_t *string1_p, /**< base ecma-string */
                             const lit_utf8_byte_t *cesu8_string2_p, /**< characters to be appended */
                             lit_utf8_size_t cesu8_string2_size, /**< byte size of cesu8_string2_p */
                             lit_utf8_size_t cesu8_string2_length) /**< character length of cesu8_string2_p */
{
  JERRY_ASSERT (string1_p != NULL && cesu8_string2_size > 0 && cesu8_string2_length > 0);

  if (JERRY_UNLIKELY (ecma_string_is_empty (string1_p)))
  {
    return ecma_new_ecma_string_from_utf8 (cesu8_string2_p, cesu8_string2_size);
  }

  const lit_utf8_byte_t *cesu8_string1_p;
  lit_utf8_size_t cesu8_string1_size;
  lit_utf8_size_t cesu8_string1_length;

  lit_utf8_byte_t uint32_to_string_buffer[ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32];

  bool string1_is_uint32 = false;
  bool string1_rehash_needed = false;

  if (ECMA_IS_DIRECT_STRING (string1_p))
  {
    string1_rehash_needed = true;

    switch (ECMA_GET_DIRECT_STRING_TYPE (string1_p))
    {
      case ECMA_DIRECT_STRING_MAGIC:
      {
        lit_magic_string_id_t id = (lit_magic_string_id_t) ECMA_GET_DIRECT_STRING_VALUE (string1_p);
        cesu8_string1_p = lit_get_magic_string_utf8 (id);
        cesu8_string1_size = lit_get_magic_string_size (id);
        cesu8_string1_length = cesu8_string1_size;
        break;
      }
      case ECMA_DIRECT_STRING_UINT:
      {
        cesu8_string1_size = ecma_uint32_to_utf8_string ((uint32_t) ECMA_GET_DIRECT_STRING_VALUE (string1_p),
                                                         uint32_to_string_buffer,
                                                         ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32);
        cesu8_string1_p = uint32_to_string_buffer;
        cesu8_string1_length = cesu8_string1_size;
        string1_is_uint32 = true;
        break;
      }
      default:
      {
        JERRY_ASSERT (ECMA_GET_DIRECT_STRING_TYPE (string1_p) == ECMA_DIRECT_STRING_MAGIC_EX);

        lit_magic_string_ex_id_t id = (lit_magic_string_ex_id_t) ECMA_GET_DIRECT_STRING_VALUE (string1_p);
        cesu8_string1_p = lit_get_magic_string_ex_utf8 (id);
        cesu8_string1_size = lit_get_magic_string_ex_size (id);
        cesu8_string1_length = lit_utf8_string_length (cesu8_string1_p, cesu8_string1_size);
        break;
      }
    }
  }
  else
  {
    JERRY_ASSERT (string1_p->refs_and_container >= ECMA_STRING_REF_ONE);

    switch (ECMA_STRING_GET_CONTAINER (string1_p))
    {
      case ECMA_STRING_CONTAINER_HEAP_UTF8_STRING:
      {
        cesu8_string1_p = (lit_utf8_byte_t *) (string1_p + 1);
        cesu8_string1_size = string1_p->u.utf8_string.size;
        cesu8_string1_length = string1_p->u.utf8_string.length;
        break;
      }
      case ECMA_STRING_CONTAINER_HEAP_LONG_UTF8_STRING:
      {
        ecma_long_string_t *long_string_desc_p = (ecma_long_string_t *) string1_p;

        cesu8_string1_p = (lit_utf8_byte_t *) (long_string_desc_p + 1);
        cesu8_string1_size = string1_p->u.long_utf8_string_size;
        cesu8_string1_length = long_string_desc_p->long_utf8_string_length;
        break;
      }
      case ECMA_STRING_CONTAINER_UINT32_IN_DESC:
      {
        cesu8_string1_size = ecma_uint32_to_utf8_string (string1_p->u.uint32_number,
                                                         uint32_to_string_buffer,
                                                         ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32);
        cesu8_string1_p = uint32_to_string_buffer;
        cesu8_string1_length = cesu8_string1_size;
        string1_is_uint32 = true;
        string1_rehash_needed = true;
        break;
      }
      default:
      {
        JERRY_ASSERT (ECMA_STRING_GET_CONTAINER (string1_p) == ECMA_STRING_CONTAINER_MAGIC_STRING_EX);

        cesu8_string1_p = lit_get_magic_string_ex_utf8 (string1_p->u.magic_string_ex_id);
        cesu8_string1_size = lit_get_magic_string_ex_size (string1_p->u.magic_string_ex_id);
        cesu8_string1_length = lit_utf8_string_length (cesu8_string1_p, cesu8_string1_size);
        string1_rehash_needed = true;
        break;
      }
    }
  }

  JERRY_ASSERT (cesu8_string1_length > 0);
  JERRY_ASSERT (cesu8_string1_length <= cesu8_string1_size);

  lit_utf8_size_t new_size = cesu8_string1_size + cesu8_string2_size;

  /* Poor man's carry flag check: it is impossible to allocate this large string. */
  if (new_size < (cesu8_string1_size | cesu8_string2_size))
  {
    jerry_fatal (ERR_OUT_OF_MEMORY);
  }

  lit_magic_string_id_t magic_string_id;
  magic_string_id = lit_is_utf8_string_pair_magic (cesu8_string1_p,
                                                   cesu8_string1_size,
                                                   cesu8_string2_p,
                                                   cesu8_string2_size);

  if (magic_string_id != LIT_MAGIC_STRING__COUNT)
  {
    ecma_deref_ecma_string (string1_p);
    return ecma_get_magic_string (magic_string_id);
  }

  if (string1_is_uint32 && new_size <= ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32)
  {
    memcpy (uint32_to_string_buffer + cesu8_string1_size,
            cesu8_string2_p,
            cesu8_string2_size);

    uint32_t array_index;

    if (ecma_string_to_array_index (uint32_to_string_buffer, new_size, &array_index))
    {
      ecma_deref_ecma_string (string1_p);
      return ecma_new_ecma_string_from_uint32 (array_index);
    }
  }

  if (lit_get_magic_string_ex_count () > 0)
  {
    lit_magic_string_ex_id_t magic_string_ex_id;
    magic_string_ex_id = lit_is_ex_utf8_string_pair_magic (cesu8_string1_p,
                                                           cesu8_string1_size,
                                                           cesu8_string2_p,
                                                           cesu8_string2_size);

    if (magic_string_ex_id < lit_get_magic_string_ex_count ())
    {
      ecma_deref_ecma_string (string1_p);
      return ecma_new_ecma_string_from_magic_string_ex_id (magic_string_ex_id);
    }
  }

  ecma_string_t *string_desc_p;
  lit_utf8_byte_t *data_p;

  if (JERRY_LIKELY (new_size <= UINT16_MAX))
  {
    string_desc_p = ecma_alloc_string_buffer (sizeof (ecma_string_t) + new_size);

    string_desc_p->refs_and_container = ECMA_STRING_CONTAINER_HEAP_UTF8_STRING | ECMA_STRING_REF_ONE;
    string_desc_p->u.common_uint32_field = 0;
    string_desc_p->u.utf8_string.size = (uint16_t) new_size;
    string_desc_p->u.utf8_string.length = (uint16_t) (cesu8_string1_length + cesu8_string2_length);

    data_p = (lit_utf8_byte_t *) (string_desc_p + 1);
  }
  else
  {
    string_desc_p = ecma_alloc_string_buffer (sizeof (ecma_long_string_t) + new_size);

    string_desc_p->refs_and_container = ECMA_STRING_CONTAINER_HEAP_LONG_UTF8_STRING | ECMA_STRING_REF_ONE;
    string_desc_p->u.common_uint32_field = 0;
    string_desc_p->u.long_utf8_string_size = new_size;

    ecma_long_string_t *long_string_desc_p = (ecma_long_string_t *) string_desc_p;
    long_string_desc_p->long_utf8_string_length = cesu8_string1_length + cesu8_string2_length;

    data_p = (lit_utf8_byte_t *) (long_string_desc_p + 1);
  }

  lit_string_hash_t hash_start;

  if (string1_rehash_needed)
  {
    hash_start = lit_utf8_string_calc_hash (cesu8_string1_p, cesu8_string1_size);
  }
  else
  {
    JERRY_ASSERT (!ECMA_IS_DIRECT_STRING (string1_p));
    hash_start = string1_p->hash;
  }

  string_desc_p->hash = lit_utf8_string_hash_combine (hash_start, cesu8_string2_p, cesu8_string2_size);

  memcpy (data_p, cesu8_string1_p, cesu8_string1_size);
  memcpy (data_p + cesu8_string1_size, cesu8_string2_p, cesu8_string2_size);

  ecma_deref_ecma_string (string1_p);
  return string_desc_p;
} /* ecma_append_chars_to_string */

/**
 * Concatenate ecma-strings
 *
 * Note:
 *   The string1_p argument is freed. If it needs to be preserved,
 *   call ecma_ref_ecma_string with string1_p before the call.
 *
 * @return concatenation of two ecma-strings
 */
ecma_string_t *
ecma_concat_ecma_strings (ecma_string_t *string1_p, /**< first ecma-string */
                          ecma_string_t *string2_p) /**< second ecma-string */
{
  JERRY_ASSERT (string1_p != NULL && string2_p != NULL);

  if (JERRY_UNLIKELY (ecma_string_is_empty (string1_p)))
  {
    ecma_ref_ecma_string (string2_p);
    return string2_p;
  }
  else if (JERRY_UNLIKELY (ecma_string_is_empty (string2_p)))
  {
    return string1_p;
  }

  const lit_utf8_byte_t *cesu8_string2_p;
  lit_utf8_size_t cesu8_string2_size;
  lit_utf8_size_t cesu8_string2_length;

  lit_utf8_byte_t uint32_to_string_buffer[ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32];

  if (ECMA_IS_DIRECT_STRING (string2_p))
  {
    switch (ECMA_GET_DIRECT_STRING_TYPE (string2_p))
    {
      case ECMA_DIRECT_STRING_MAGIC:
      {
        lit_magic_string_id_t id = (lit_magic_string_id_t) ECMA_GET_DIRECT_STRING_VALUE (string2_p);
        cesu8_string2_p = lit_get_magic_string_utf8 (id);
        cesu8_string2_size = lit_get_magic_string_size (id);
        cesu8_string2_length = cesu8_string2_size;
        break;
      }
      case ECMA_DIRECT_STRING_UINT:
      {
        cesu8_string2_size = ecma_uint32_to_utf8_string ((uint32_t) ECMA_GET_DIRECT_STRING_VALUE (string2_p),
                                                         uint32_to_string_buffer,
                                                         ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32);
        cesu8_string2_p = uint32_to_string_buffer;
        cesu8_string2_length = cesu8_string2_size;
        break;
      }
      default:
      {
        JERRY_ASSERT (ECMA_GET_DIRECT_STRING_TYPE (string2_p) == ECMA_DIRECT_STRING_MAGIC_EX);

        lit_magic_string_ex_id_t id = (lit_magic_string_ex_id_t) ECMA_GET_DIRECT_STRING_VALUE (string2_p);
        cesu8_string2_p = lit_get_magic_string_ex_utf8 (id);
        cesu8_string2_size = lit_get_magic_string_ex_size (id);
        cesu8_string2_length = lit_utf8_string_length (cesu8_string2_p, cesu8_string2_size);
        break;
      }
    }
  }
  else
  {
    JERRY_ASSERT (string2_p->refs_and_container >= ECMA_STRING_REF_ONE);

    switch (ECMA_STRING_GET_CONTAINER (string2_p))
    {
      case ECMA_STRING_CONTAINER_HEAP_UTF8_STRING:
      {
        cesu8_string2_p = (lit_utf8_byte_t *) (string2_p + 1);
        cesu8_string2_size = string2_p->u.utf8_string.size;
        cesu8_string2_length = string2_p->u.utf8_string.length;
        break;
      }
      case ECMA_STRING_CONTAINER_HEAP_LONG_UTF8_STRING:
      {
        ecma_long_string_t *long_string_desc_p = (ecma_long_string_t *) string2_p;

        cesu8_string2_p = (lit_utf8_byte_t *) (long_string_desc_p + 1);
        cesu8_string2_size = string2_p->u.long_utf8_string_size;
        cesu8_string2_length = long_string_desc_p->long_utf8_string_length;
        break;
      }
      case ECMA_STRING_CONTAINER_UINT32_IN_DESC:
      {
        cesu8_string2_size = ecma_uint32_to_utf8_string (string2_p->u.uint32_number,
                                                         uint32_to_string_buffer,
                                                         ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32);
        cesu8_string2_p = uint32_to_string_buffer;
        cesu8_string2_length = cesu8_string2_size;
        break;
      }
      default:
      {
        JERRY_ASSERT (ECMA_STRING_GET_CONTAINER (string2_p) == ECMA_STRING_CONTAINER_MAGIC_STRING_EX);

        cesu8_string2_p = lit_get_magic_string_ex_utf8 (string2_p->u.magic_string_ex_id);
        cesu8_string2_size = lit_get_magic_string_ex_size (string2_p->u.magic_string_ex_id);
        cesu8_string2_length = lit_utf8_string_length (cesu8_string2_p, cesu8_string2_size);
        break;
      }
    }
  }

  return ecma_append_chars_to_string (string1_p, cesu8_string2_p, cesu8_string2_size, cesu8_string2_length);
} /* ecma_concat_ecma_strings */

/**
 * Append a magic string after an ecma-string
 *
 * Note:
 *   The string1_p argument is freed. If it needs to be preserved,
 *   call ecma_ref_ecma_string with string1_p before the call.
 *
 * @return concatenation of an ecma-string and a magic string
 */
ecma_string_t *
ecma_append_magic_string_to_string (ecma_string_t *string1_p, /**< string descriptor */
                                    lit_magic_string_id_t string2_id) /**< magic string ID */
{
  if (JERRY_UNLIKELY (ecma_string_is_empty (string1_p)))
  {
    return ecma_get_magic_string (string2_id);
  }

  const lit_utf8_byte_t *cesu8_string2_p = lit_get_magic_string_utf8 (string2_id);
  lit_utf8_size_t cesu8_string2_size = lit_get_magic_string_size (string2_id);

  return ecma_append_chars_to_string (string1_p, cesu8_string2_p, cesu8_string2_size, cesu8_string2_size);
} /* ecma_append_magic_string_to_string */

/**
 * Increase reference counter of ecma-string.
 */
void
ecma_ref_ecma_string (ecma_string_t *string_p) /**< string descriptor */
{
  JERRY_ASSERT (string_p != NULL);

  if (ECMA_IS_DIRECT_STRING (string_p))
  {
    return;
  }

  JERRY_ASSERT (string_p->refs_and_container >= ECMA_STRING_REF_ONE);

  if (JERRY_LIKELY (string_p->refs_and_container < ECMA_STRING_MAX_REF))
  {
    /* Increase reference counter. */
    string_p->refs_and_container = (uint16_t) (string_p->refs_and_container + ECMA_STRING_REF_ONE);
  }
  else
  {
    jerry_fatal (ERR_REF_COUNT_LIMIT);
  }
} /* ecma_ref_ecma_string */

/**
 * Decrease reference counter and deallocate ecma-string
 * if the counter becomes zero.
 */
void
ecma_deref_ecma_string (ecma_string_t *string_p) /**< ecma-string */
{
  JERRY_ASSERT (string_p != NULL);

  if (ECMA_IS_DIRECT_STRING (string_p))
  {
    return;
  }

  JERRY_ASSERT (string_p->refs_and_container >= ECMA_STRING_REF_ONE);

  /* Decrease reference counter. */
  string_p->refs_and_container = (uint16_t) (string_p->refs_and_container - ECMA_STRING_REF_ONE);

  if (string_p->refs_and_container >= ECMA_STRING_REF_ONE)
  {
    return;
  }

  switch (ECMA_STRING_GET_CONTAINER (string_p))
  {
    case ECMA_STRING_CONTAINER_HEAP_UTF8_STRING:
    {
#ifndef JERRY_NDEBUG
      const lit_utf8_byte_t *chars_p = (const lit_utf8_byte_t *) (string_p + 1);

      if (*chars_p >= LIT_CHAR_0 && *chars_p <= LIT_CHAR_9)
      {
        uint32_t array_index;

        JERRY_ASSERT (!ecma_string_to_array_index (chars_p,
                                                   string_p->u.utf8_string.size,
                                                   &array_index));
      }
#endif /* !JERRY_NDEBUG */

      ecma_dealloc_string_buffer (string_p, string_p->u.utf8_string.size + sizeof (ecma_string_t));
      return;
    }
    case ECMA_STRING_CONTAINER_HEAP_LONG_UTF8_STRING:
    {
      JERRY_ASSERT (string_p->u.long_utf8_string_size > UINT16_MAX);

      ecma_dealloc_string_buffer (string_p, string_p->u.long_utf8_string_size + sizeof (ecma_long_string_t));
      return;
    }
    case ECMA_STRING_LITERAL_NUMBER:
    {
      ecma_free_value (string_p->u.lit_number);
      break;
    }
#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
    case ECMA_STRING_CONTAINER_SYMBOL:
    {
      ecma_free_value (string_p->u.symbol_descriptor);
      break;
    }
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */
    default:
    {
      JERRY_ASSERT (ECMA_STRING_GET_CONTAINER (string_p) == ECMA_STRING_CONTAINER_UINT32_IN_DESC
                    || ECMA_STRING_GET_CONTAINER (string_p) == ECMA_STRING_CONTAINER_MAGIC_STRING_EX);

      /* only the string descriptor itself should be freed */
      break;
    }
  }

  ecma_dealloc_string (string_p);
} /* ecma_deref_ecma_string */

/**
 * Convert ecma-string to number
 *
 * @return converted ecma-number
 */
ecma_number_t
ecma_string_to_number (const ecma_string_t *string_p) /**< ecma-string */
{
  JERRY_ASSERT (string_p != NULL);

  if (ECMA_IS_DIRECT_STRING (string_p))
  {
    if (ECMA_IS_DIRECT_STRING_WITH_TYPE (string_p, ECMA_DIRECT_STRING_UINT))
    {
      return (ecma_number_t) ECMA_GET_DIRECT_STRING_VALUE (string_p);
    }
  }
  else if (ECMA_STRING_GET_CONTAINER (string_p) == ECMA_STRING_CONTAINER_UINT32_IN_DESC)
  {
    return ((ecma_number_t) string_p->u.uint32_number);
  }

  lit_utf8_size_t size;
  const lit_utf8_byte_t *chars_p = ecma_string_get_chars_fast (string_p, &size);

  JERRY_ASSERT (chars_p != NULL);

  if (size == 0)
  {
    return ECMA_NUMBER_ZERO;
  }

  return ecma_utf8_string_to_number (chars_p, size);
} /* ecma_string_to_number */

/**
 * Check if string is array index.
 *
 * @return ECMA_STRING_NOT_ARRAY_INDEX if string is not array index
 *         the array index otherwise
 */
inline uint32_t JERRY_ATTR_ALWAYS_INLINE
ecma_string_get_array_index (const ecma_string_t *str_p) /**< ecma-string */
{
  if (ECMA_IS_DIRECT_STRING (str_p))
  {
    if (ECMA_IS_DIRECT_STRING_WITH_TYPE (str_p, ECMA_DIRECT_STRING_UINT))
    {
      /* Value cannot be equal to the maximum value of a 32 bit unsigned number. */
      return (uint32_t) ECMA_GET_DIRECT_STRING_VALUE (str_p);
    }

    return ECMA_STRING_NOT_ARRAY_INDEX;
  }

  if (ECMA_STRING_GET_CONTAINER (str_p) == ECMA_STRING_CONTAINER_UINT32_IN_DESC)
  {
    /* When the uint32_number is equal to the maximum value of 32 bit unsigned integer number,
     * it is also an invalid array index. The comparison to ECMA_STRING_NOT_ARRAY_INDEX will
     * be true in this case. */
    return str_p->u.uint32_number;
  }

  return ECMA_STRING_NOT_ARRAY_INDEX;
} /* ecma_string_get_array_index */

/**
 * Convert ecma-string's contents to a cesu-8 string and put it to the buffer.
 * It is the caller's responsibility to make sure that the string fits in the buffer.
 *
 * @return number of bytes, actually copied to the buffer.
 */
lit_utf8_size_t JERRY_ATTR_WARN_UNUSED_RESULT
ecma_string_copy_to_cesu8_buffer (const ecma_string_t *string_p, /**< ecma-string descriptor */
                                  lit_utf8_byte_t *buffer_p, /**< destination buffer pointer
                                                              * (can be NULL if buffer_size == 0) */
                                  lit_utf8_size_t buffer_size) /**< size of buffer */
{
  JERRY_ASSERT (string_p != NULL);
  JERRY_ASSERT (buffer_p != NULL || buffer_size == 0);
  JERRY_ASSERT (ecma_string_get_size (string_p) <= buffer_size);

  lit_utf8_size_t size;

  if (ECMA_IS_DIRECT_STRING (string_p))
  {
    if (ECMA_IS_DIRECT_STRING_WITH_TYPE (string_p, ECMA_DIRECT_STRING_UINT))
    {
      uint32_t uint32_number = (uint32_t) ECMA_GET_DIRECT_STRING_VALUE (string_p);
      size = ecma_uint32_to_utf8_string (uint32_number, buffer_p, buffer_size);
      JERRY_ASSERT (size <= buffer_size);
      return size;
    }
  }
  else
  {
    JERRY_ASSERT (string_p->refs_and_container >= ECMA_STRING_REF_ONE);

    if (ECMA_STRING_GET_CONTAINER (string_p) == ECMA_STRING_CONTAINER_UINT32_IN_DESC)
    {
      uint32_t uint32_number = string_p->u.uint32_number;
      size = ecma_uint32_to_utf8_string (uint32_number, buffer_p, buffer_size);
      JERRY_ASSERT (size <= buffer_size);
      return size;
    }
  }

  const lit_utf8_byte_t *chars_p = ecma_string_get_chars_fast (string_p, &size);

  JERRY_ASSERT (chars_p != NULL);
  JERRY_ASSERT (size <= buffer_size);

  memcpy (buffer_p, chars_p, size);
  return size;
} /* ecma_string_copy_to_cesu8_buffer */

/**
 * Convert ecma-string's contents to an utf-8 string and put it to the buffer.
 * It is the caller's responsibility to make sure that the string fits in the buffer.
 *
 * @return number of bytes, actually copied to the buffer.
 */
lit_utf8_size_t JERRY_ATTR_WARN_UNUSED_RESULT
ecma_string_copy_to_utf8_buffer (const ecma_string_t *string_p, /**< ecma-string descriptor */
                                 lit_utf8_byte_t *buffer_p, /**< destination buffer pointer
                                                             * (can be NULL if buffer_size == 0) */
                                 lit_utf8_size_t buffer_size) /**< size of buffer */
{
  JERRY_ASSERT (string_p != NULL);
  JERRY_ASSERT (buffer_p != NULL || buffer_size == 0);
  JERRY_ASSERT (ecma_string_get_utf8_size (string_p) <= buffer_size);

  lit_utf8_size_t size;

  if (ECMA_IS_DIRECT_STRING (string_p))
  {
    if (ECMA_IS_DIRECT_STRING_WITH_TYPE (string_p, ECMA_DIRECT_STRING_UINT))
    {
      uint32_t uint32_number = (uint32_t) ECMA_GET_DIRECT_STRING_VALUE (string_p);
      size = ecma_uint32_to_utf8_string (uint32_number, buffer_p, buffer_size);
      JERRY_ASSERT (size <= buffer_size);
      return size;
    }
  }
  else
  {
    JERRY_ASSERT (string_p->refs_and_container >= ECMA_STRING_REF_ONE);

    if (ECMA_STRING_GET_CONTAINER (string_p) == ECMA_STRING_CONTAINER_UINT32_IN_DESC)
    {
      uint32_t uint32_number = string_p->u.uint32_number;
      size = ecma_uint32_to_utf8_string (uint32_number, buffer_p, buffer_size);
      JERRY_ASSERT (size <= buffer_size);
      return size;
    }
  }

  uint8_t flags = ECMA_STRING_FLAG_IS_ASCII;
  const lit_utf8_byte_t *chars_p = ecma_string_get_chars (string_p, &size, &flags);

  JERRY_ASSERT (chars_p != NULL);

  if (flags & ECMA_STRING_FLAG_IS_ASCII)
  {
    JERRY_ASSERT (size <= buffer_size);
    memcpy (buffer_p, chars_p, size);
    return size;
  }

  size = lit_convert_cesu8_string_to_utf8_string (chars_p,
                                                  size,
                                                  buffer_p,
                                                  buffer_size);

  if (flags & ECMA_STRING_FLAG_MUST_BE_FREED)
  {
    jmem_heap_free_block ((void *) chars_p, size);
  }

  JERRY_ASSERT (size <= buffer_size);
  return size;
} /* ecma_string_copy_to_utf8_buffer */

/**
 * Convert ecma-string's contents to a cesu-8 string, extract the parts of the converted string between the specified
 * start position and the end position (or the end of the string, whichever comes first), and copy these characters
 * into the buffer.
 *
 * @return number of bytes, actually copied to the buffer.
 */
lit_utf8_size_t
ecma_substring_copy_to_cesu8_buffer (const ecma_string_t *string_desc_p, /**< ecma-string descriptor */
                                     ecma_length_t start_pos, /**< position of the first character */
                                     ecma_length_t end_pos, /**< position of the last character */
                                     lit_utf8_byte_t *buffer_p, /**< destination buffer pointer
                                                                 * (can be NULL if buffer_size == 0) */
                                     lit_utf8_size_t buffer_size) /**< size of buffer */
{
  JERRY_ASSERT (string_desc_p != NULL);
  JERRY_ASSERT (buffer_p != NULL || buffer_size == 0);

  ecma_length_t string_length = ecma_string_get_length (string_desc_p);
  lit_utf8_size_t size = 0;

  if (start_pos >= string_length || start_pos >= end_pos)
  {
    return 0;
  }

  if (end_pos > string_length)
  {
    end_pos = string_length;
  }

  ECMA_STRING_TO_UTF8_STRING (string_desc_p, utf8_str_p, utf8_str_size);

  const lit_utf8_byte_t *start_p = utf8_str_p;

  if (string_length == utf8_str_size)
  {
    start_p += start_pos;
    size = end_pos - start_pos;

    if (size > buffer_size)
    {
      size = buffer_size;
    }

    memcpy (buffer_p, start_p, size);
  }
  else
  {
    end_pos -= start_pos;
    while (start_pos--)
    {
      start_p += lit_get_unicode_char_size_by_utf8_first_byte (*start_p);
    }

    const lit_utf8_byte_t *end_p = start_p;

    while (end_pos--)
    {
      lit_utf8_size_t code_unit_size = lit_get_unicode_char_size_by_utf8_first_byte (*end_p);

      if ((size + code_unit_size) > buffer_size)
      {
        break;
      }

      end_p += code_unit_size;
      size += code_unit_size;
    }

    memcpy (buffer_p, start_p, size);
  }

  ECMA_FINALIZE_UTF8_STRING (utf8_str_p, utf8_str_size);

  JERRY_ASSERT (size <= buffer_size);
  return size;
} /* ecma_substring_copy_to_cesu8_buffer */

/**
 * Convert ecma-string's contents to an utf-8 string, extract the parts of the converted string between the specified
 * start position and the end position (or the end of the string, whichever comes first), and copy these characters
 * into the buffer.
 *
 * @return number of bytes, actually copied to the buffer.
 */
lit_utf8_size_t
ecma_substring_copy_to_utf8_buffer (const ecma_string_t *string_desc_p, /**< ecma-string descriptor */
                                    ecma_length_t start_pos, /**< position of the first character */
                                    ecma_length_t end_pos, /**< position of the last character */
                                    lit_utf8_byte_t *buffer_p, /**< destination buffer pointer
                                                                * (can be NULL if buffer_size == 0) */
                                    lit_utf8_size_t buffer_size) /**< size of buffer */
{
  JERRY_ASSERT (string_desc_p != NULL);
  JERRY_ASSERT (ECMA_IS_DIRECT_STRING (string_desc_p) || string_desc_p->refs_and_container >= ECMA_STRING_REF_ONE);
  JERRY_ASSERT (buffer_p != NULL || buffer_size == 0);

  lit_utf8_size_t size = 0;

  ecma_length_t utf8_str_length = ecma_string_get_utf8_length (string_desc_p);

  if (start_pos >= utf8_str_length || start_pos >= end_pos)
  {
    return 0;
  }

  if (end_pos > utf8_str_length)
  {
    end_pos = utf8_str_length;
  }

  ECMA_STRING_TO_UTF8_STRING (string_desc_p, cesu8_str_p, cesu8_str_size);
  ecma_length_t cesu8_str_length = ecma_string_get_length (string_desc_p);

  if (cesu8_str_length == cesu8_str_size)
  {
    cesu8_str_p += start_pos;
    size = end_pos - start_pos;

    if (size > buffer_size)
    {
      size = buffer_size;
    }

    memcpy (buffer_p, cesu8_str_p, size);
  }
  else
  {
    const lit_utf8_byte_t *cesu8_end_pos = cesu8_str_p + cesu8_str_size;
    end_pos -= start_pos;

    while (start_pos--)
    {
      ecma_char_t ch;
      lit_utf8_size_t code_unit_size = lit_read_code_unit_from_utf8 (cesu8_str_p, &ch);

      cesu8_str_p += code_unit_size;
      if ((cesu8_str_p != cesu8_end_pos) && lit_is_code_point_utf16_high_surrogate (ch))
      {
        ecma_char_t next_ch;
        lit_utf8_size_t next_ch_size = lit_read_code_unit_from_utf8 (cesu8_str_p, &next_ch);
        if (lit_is_code_point_utf16_low_surrogate (next_ch))
        {
          JERRY_ASSERT (code_unit_size == next_ch_size);
          cesu8_str_p += code_unit_size;
        }
      }
    }

    const lit_utf8_byte_t *cesu8_pos = cesu8_str_p;

    lit_utf8_byte_t *utf8_pos = buffer_p;
    lit_utf8_byte_t *utf8_end_pos = buffer_p + buffer_size;

    while (end_pos--)
    {
      ecma_char_t ch;
      lit_utf8_size_t code_unit_size = lit_read_code_unit_from_utf8 (cesu8_pos, &ch);

      if ((size + code_unit_size) > buffer_size)
      {
        break;
      }

      if (((cesu8_pos + code_unit_size) != cesu8_end_pos) && lit_is_code_point_utf16_high_surrogate (ch))
      {
        ecma_char_t next_ch;
        lit_utf8_size_t next_ch_size = lit_read_code_unit_from_utf8 (cesu8_pos + code_unit_size, &next_ch);

        if (lit_is_code_point_utf16_low_surrogate (next_ch))
        {
          JERRY_ASSERT (code_unit_size == next_ch_size);

          if ((size + code_unit_size + 1) > buffer_size)
          {
            break;
          }

          cesu8_pos += next_ch_size;

          lit_code_point_t code_point = lit_convert_surrogate_pair_to_code_point (ch, next_ch);
          lit_code_point_to_utf8 (code_point, utf8_pos);
          size += (code_unit_size + 1);
        }
        else
        {
          memcpy (utf8_pos, cesu8_pos, code_unit_size);
          size += code_unit_size;
        }
      }
      else
      {
        memcpy (utf8_pos, cesu8_pos, code_unit_size);
        size += code_unit_size;
      }

      utf8_pos = buffer_p + size;
      cesu8_pos += code_unit_size;
    }

    JERRY_ASSERT (utf8_pos <= utf8_end_pos);
  }

  ECMA_FINALIZE_UTF8_STRING (cesu8_str_p, cesu8_str_size);
  JERRY_ASSERT (size <= buffer_size);

  return size;
} /* ecma_substring_copy_to_utf8_buffer */

/**
 * Convert ecma-string's contents to a cesu-8 string and put it to the buffer.
 * It is the caller's responsibility to make sure that the string fits in the buffer.
 * Check if the size of the string is equal with the size of the buffer.
 */
inline void JERRY_ATTR_ALWAYS_INLINE
ecma_string_to_utf8_bytes (const ecma_string_t *string_desc_p, /**< ecma-string descriptor */
                           lit_utf8_byte_t *buffer_p, /**< destination buffer pointer
                                                       * (can be NULL if buffer_size == 0) */
                           lit_utf8_size_t buffer_size) /**< size of buffer */
{
  const lit_utf8_size_t size = ecma_string_copy_to_cesu8_buffer (string_desc_p, buffer_p, buffer_size);
  JERRY_ASSERT (size == buffer_size);
} /* ecma_string_to_utf8_bytes */

/**
 * Get size of the uint32 number stored locally in the string's descriptor
 *
 * Note: the represented number size and length are equal
 *
 * @return size in bytes
 */
static inline ecma_length_t JERRY_ATTR_ALWAYS_INLINE
ecma_string_get_uint32_size (const uint32_t uint32_number) /**< number in the string-descriptor */
{
  uint32_t prev_number = 1;
  uint32_t next_number = 100;
  ecma_length_t size = 1;

  const uint32_t max_size = 9;

  while (size < max_size && uint32_number >= next_number)
  {
    prev_number = next_number;
    next_number *= 100;
    size += 2;
  }

  if (uint32_number >= prev_number * 10)
  {
    size++;
  }

  return size;
} /* ecma_string_get_uint32_size */

/**
 * Checks whether the given string is a sequence of ascii characters.
 */
#define ECMA_STRING_IS_ASCII(char_p, size) ((size) == lit_utf8_string_length ((char_p), (size)))

/**
 * Returns with the cesu8 character array of a string.
 *
 * Note:
 *   - This function returns with a newly allocated buffer for uint32 strings,
 *     which must be freed.
 *   - The ASCII check only happens if the flags parameter gets
 *     'ECMA_STRING_FLAG_IS_ASCII' as an input.
 *
 * @return start of cesu8 characters
 */
const lit_utf8_byte_t *
ecma_string_get_chars (const ecma_string_t *string_p, /**< ecma-string */
                       lit_utf8_size_t *size_p, /**< [out] size of the ecma string */
                       uint8_t *flags_p) /**< [in,out] flags: ECMA_STRING_FLAG_EMPTY,
                                                              ECMA_STRING_FLAG_IS_ASCII,
                                                              ECMA_STRING_FLAG_MUST_BE_FREED */
{
  ecma_length_t length;
  lit_utf8_size_t size;
  const lit_utf8_byte_t *result_p;

  if (ECMA_IS_DIRECT_STRING (string_p))
  {
    switch (ECMA_GET_DIRECT_STRING_TYPE (string_p))
    {
      case ECMA_DIRECT_STRING_MAGIC:
      {
        lit_magic_string_id_t id = (lit_magic_string_id_t) ECMA_GET_DIRECT_STRING_VALUE (string_p);

        size = lit_get_magic_string_size (id);
        length = size;

        result_p = lit_get_magic_string_utf8 (id);

        /* All magic strings must be ascii strings. */
        JERRY_ASSERT (ECMA_STRING_IS_ASCII (result_p, size));
        break;
      }
      case ECMA_DIRECT_STRING_UINT:
      {
        uint32_t uint32_number = (uint32_t) ECMA_GET_DIRECT_STRING_VALUE (string_p);
        size = (lit_utf8_size_t) ecma_string_get_uint32_size (uint32_number);

        result_p = (const lit_utf8_byte_t *) jmem_heap_alloc_block (size);
        length = ecma_uint32_to_utf8_string (uint32_number, (lit_utf8_byte_t *) result_p, size);
        JERRY_ASSERT (length == size);
        *flags_p |= ECMA_STRING_FLAG_MUST_BE_FREED;
        break;
      }
      default:
      {
        JERRY_ASSERT (ECMA_GET_DIRECT_STRING_TYPE (string_p) == ECMA_DIRECT_STRING_MAGIC_EX);

        lit_magic_string_ex_id_t id = (lit_magic_string_ex_id_t) ECMA_GET_DIRECT_STRING_VALUE (string_p);

        size = lit_get_magic_string_ex_size (id);
        result_p = lit_get_magic_string_ex_utf8 (id);
        length = 0;

        if (JERRY_UNLIKELY (*flags_p & ECMA_STRING_FLAG_IS_ASCII))
        {
          length = lit_utf8_string_length (result_p, size);
        }
        break;
      }
    }
  }
  else
  {
    JERRY_ASSERT (string_p->refs_and_container >= ECMA_STRING_REF_ONE);

    switch (ECMA_STRING_GET_CONTAINER (string_p))
    {
      case ECMA_STRING_CONTAINER_HEAP_UTF8_STRING:
      {
        size = string_p->u.utf8_string.size;
        length = string_p->u.utf8_string.length;
        result_p = (const lit_utf8_byte_t *) (string_p + 1);
        break;
      }
      case ECMA_STRING_CONTAINER_HEAP_LONG_UTF8_STRING:
      {
        size = string_p->u.long_utf8_string_size;
        ecma_long_string_t *long_string_p = (ecma_long_string_t *) string_p;
        length = long_string_p->long_utf8_string_length;
        result_p = (const lit_utf8_byte_t *) (long_string_p + 1);
        break;
      }
      case ECMA_STRING_CONTAINER_UINT32_IN_DESC:
      {
        size = (lit_utf8_size_t) ecma_string_get_uint32_size (string_p->u.uint32_number);

        result_p = (const lit_utf8_byte_t *) jmem_heap_alloc_block (size);
        length = ecma_uint32_to_utf8_string (string_p->u.uint32_number, (lit_utf8_byte_t *) result_p, size);
        JERRY_ASSERT (length == size);
        *flags_p |= ECMA_STRING_FLAG_MUST_BE_FREED;
        break;

      }
      default:
      {
        JERRY_ASSERT (ECMA_STRING_GET_CONTAINER (string_p) == ECMA_STRING_CONTAINER_MAGIC_STRING_EX);

        size = lit_get_magic_string_ex_size (string_p->u.magic_string_ex_id);
        length = 0;

        if (JERRY_UNLIKELY (*flags_p & ECMA_STRING_FLAG_IS_ASCII))
        {
          length = lit_utf8_string_length (lit_get_magic_string_ex_utf8 (string_p->u.magic_string_ex_id), size);
        }

        result_p = lit_get_magic_string_ex_utf8 (string_p->u.magic_string_ex_id);
        break;
      }
    }
  }

  *size_p = size;
  if ((*flags_p & ECMA_STRING_FLAG_IS_ASCII)
      && length != size)
  {
    *flags_p = (uint8_t) (*flags_p & ~ECMA_STRING_FLAG_IS_ASCII);
  }

  return result_p;
} /* ecma_string_get_chars */

/**
 * Checks whether the string equals to the magic string id.
 *
 * @return true - if the string equals to the magic string id
 *         false - otherwise
 */
inline bool JERRY_ATTR_ALWAYS_INLINE
ecma_compare_ecma_string_to_magic_id (const ecma_string_t *string_p, /**< property name */
                                      lit_magic_string_id_t id) /**< magic string id */
{
  return (string_p == ecma_get_magic_string (id));
} /* ecma_compare_ecma_string_to_magic_id */

/**
 * Checks whether ecma string is empty or not
 *
 * @return true - if the string is an empty string
 *         false - otherwise
 */
inline bool JERRY_ATTR_ALWAYS_INLINE
ecma_string_is_empty (const ecma_string_t *string_p) /**< ecma-string */
{
  return ecma_compare_ecma_string_to_magic_id (string_p, LIT_MAGIC_STRING__EMPTY);
} /* ecma_string_is_empty */

/**
 * Checks whether the string equals to "length".
 *
 * @return true - if the string equals to "length"
 *         false - otherwise
 */
inline bool JERRY_ATTR_ALWAYS_INLINE
ecma_string_is_length (const ecma_string_t *string_p) /**< property name */
{
  return ecma_compare_ecma_string_to_magic_id (string_p, LIT_MAGIC_STRING_LENGTH);
} /* ecma_string_is_length */

/**
 * Converts a property name into a string
 *
 * @return pointer to the converted ecma string
 */
static inline ecma_string_t * JERRY_ATTR_ALWAYS_INLINE
ecma_property_to_string (ecma_property_t property, /**< property name type */
                         jmem_cpointer_t prop_name_cp) /**< property name compressed pointer */
{
  uintptr_t property_string = ((uintptr_t) (property)) & (0x3 << ECMA_PROPERTY_NAME_TYPE_SHIFT);
  property_string = (property_string >> ECMA_STRING_TYPE_CONVERSION_SHIFT) | ECMA_TYPE_DIRECT_STRING;
  return (ecma_string_t *) (property_string | (((uintptr_t) prop_name_cp) << ECMA_DIRECT_STRING_SHIFT));
} /* ecma_property_to_string */

/**
 * Converts a string into a property name
 *
 * @return the compressed pointer part of the name
 */
inline jmem_cpointer_t JERRY_ATTR_ALWAYS_INLINE
ecma_string_to_property_name (ecma_string_t *prop_name_p, /**< property name */
                              ecma_property_t *name_type_p) /**< [out] property name type */
{
  if (ECMA_IS_DIRECT_STRING (prop_name_p))
  {
    *name_type_p = (ecma_property_t) ECMA_DIRECT_STRING_TYPE_TO_PROP_NAME_TYPE (prop_name_p);
    return (jmem_cpointer_t) ECMA_GET_DIRECT_STRING_VALUE (prop_name_p);
  }

  *name_type_p = ECMA_DIRECT_STRING_PTR << ECMA_PROPERTY_NAME_TYPE_SHIFT;

  ecma_ref_ecma_string (prop_name_p);

  jmem_cpointer_t prop_name_cp;
  ECMA_SET_NON_NULL_POINTER (prop_name_cp, prop_name_p);
  return prop_name_cp;
} /* ecma_string_to_property_name */

/**
 * Converts a property name into a string
 *
 * @return the string pointer
 *         string must be released with ecma_deref_ecma_string
 */
ecma_string_t *
ecma_string_from_property_name (ecma_property_t property, /**< property name type */
                                jmem_cpointer_t prop_name_cp) /**< property name compressed pointer */
{
  if (ECMA_PROPERTY_GET_NAME_TYPE (property) != ECMA_DIRECT_STRING_PTR)
  {
    return ecma_property_to_string (property, prop_name_cp);
  }

  ecma_string_t *prop_name_p = ECMA_GET_NON_NULL_POINTER (ecma_string_t, prop_name_cp);
  ecma_ref_ecma_string (prop_name_p);
  return prop_name_p;
} /* ecma_string_from_property_name */

/**
 * Get hash code of property name
 *
 * @return hash code of property name
 */
inline lit_string_hash_t JERRY_ATTR_ALWAYS_INLINE
ecma_string_get_property_name_hash (ecma_property_t property, /**< property name type */
                                    jmem_cpointer_t prop_name_cp) /**< property name compressed pointer */
{
  switch (ECMA_PROPERTY_GET_NAME_TYPE (property))
  {
    case ECMA_DIRECT_STRING_PTR:
    {
      ecma_string_t *prop_name_p = ECMA_GET_NON_NULL_POINTER (ecma_string_t, prop_name_cp);
      return prop_name_p->hash;
    }
    case ECMA_DIRECT_STRING_MAGIC_EX:
    {
      return (lit_string_hash_t) (LIT_MAGIC_STRING__COUNT + prop_name_cp);
    }
    default:
    {
      return (lit_string_hash_t) prop_name_cp;
    }
  }
} /* ecma_string_get_property_name_hash */

/**
 * Check if property name is array index.
 *
 * @return ECMA_STRING_NOT_ARRAY_INDEX if string is not array index
 *         the array index otherwise
 */
uint32_t
ecma_string_get_property_index (ecma_property_t property, /**< property name type */
                                jmem_cpointer_t prop_name_cp) /**< property name compressed pointer */
{
  switch (ECMA_PROPERTY_GET_NAME_TYPE (property))
  {
    case ECMA_DIRECT_STRING_UINT:
    {
      return (uint32_t) prop_name_cp;
    }
    case ECMA_DIRECT_STRING_PTR:
    {
      ecma_string_t *prop_name_p = ECMA_GET_NON_NULL_POINTER (ecma_string_t, prop_name_cp);
      return ecma_string_get_array_index (prop_name_p);
    }
    default:
    {
      return ECMA_STRING_NOT_ARRAY_INDEX;
    }
  }
} /* ecma_string_get_property_index */

/**
 * Compare a property name to a string
 *
 * @return true if they are equals
 *         false otherwise
 */
inline bool JERRY_ATTR_ALWAYS_INLINE
ecma_string_compare_to_property_name (ecma_property_t property, /**< property name type */
                                      jmem_cpointer_t prop_name_cp, /**< property name compressed pointer */
                                      const ecma_string_t *string_p) /**< other string */
{
  if (ECMA_PROPERTY_GET_NAME_TYPE (property) != ECMA_DIRECT_STRING_PTR)
  {
    return ecma_property_to_string (property, prop_name_cp) == string_p;
  }

  if (ECMA_IS_DIRECT_STRING (string_p))
  {
    return false;
  }

  ecma_string_t *prop_name_p = ECMA_GET_NON_NULL_POINTER (ecma_string_t, prop_name_cp);
  return ecma_compare_ecma_non_direct_strings (prop_name_p, string_p);
} /* ecma_string_compare_to_property_name */

/**
 * Long path part of ecma-string to ecma-string comparison routine
 *
 * See also:
 *          ecma_compare_ecma_strings
 *
 * @return true - if strings are equal;
 *         false - otherwise
 */
static bool JERRY_ATTR_NOINLINE
ecma_compare_ecma_strings_longpath (const ecma_string_t *string1_p, /**< ecma-string */
                                    const ecma_string_t *string2_p) /**< ecma-string */
{
  JERRY_ASSERT (ECMA_STRING_GET_CONTAINER (string1_p) == ECMA_STRING_GET_CONTAINER (string2_p));

  const lit_utf8_byte_t *utf8_string1_p, *utf8_string2_p;
  lit_utf8_size_t utf8_string1_size, utf8_string2_size;

  if (ECMA_STRING_GET_CONTAINER (string1_p) == ECMA_STRING_CONTAINER_HEAP_UTF8_STRING)
  {
    utf8_string1_p = (lit_utf8_byte_t *) (string1_p + 1);
    utf8_string1_size = string1_p->u.utf8_string.size;
    utf8_string2_p = (lit_utf8_byte_t *) (string2_p + 1);
    utf8_string2_size = string2_p->u.utf8_string.size;
  }
  else
  {
    JERRY_ASSERT (ECMA_STRING_GET_CONTAINER (string1_p) == ECMA_STRING_CONTAINER_HEAP_LONG_UTF8_STRING);

    utf8_string1_p = (lit_utf8_byte_t *) (((ecma_long_string_t *) string1_p) + 1);
    utf8_string1_size = string1_p->u.long_utf8_string_size;
    utf8_string2_p = (lit_utf8_byte_t *) (((ecma_long_string_t *) string2_p) + 1);
    utf8_string2_size = string2_p->u.long_utf8_string_size;
  }

  if (utf8_string1_size != utf8_string2_size)
  {
    return false;
  }

  return !memcmp ((char *) utf8_string1_p, (char *) utf8_string2_p, utf8_string1_size);
} /* ecma_compare_ecma_strings_longpath */

/**
 * Compare two ecma-strings
 *
 * @return true - if strings are equal;
 *         false - otherwise
 */
inline bool JERRY_ATTR_ALWAYS_INLINE
ecma_compare_ecma_strings (const ecma_string_t *string1_p, /**< ecma-string */
                           const ecma_string_t *string2_p) /**< ecma-string */
{
  JERRY_ASSERT (string1_p != NULL && string2_p != NULL);

  /* Fast paths first. */
  if (string1_p == string2_p)
  {
    return true;
  }

  /* Either string is direct, return with false. */
  if (ECMA_IS_DIRECT_STRING (((uintptr_t) string1_p) | ((uintptr_t) string2_p)))
  {
    return false;
  }

  if (string1_p->hash != string2_p->hash)
  {
    return false;
  }

  ecma_string_container_t string1_container = ECMA_STRING_GET_CONTAINER (string1_p);

  if (string1_container != ECMA_STRING_GET_CONTAINER (string2_p))
  {
    return false;
  }

#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
  if (string1_container == ECMA_STRING_CONTAINER_SYMBOL)
  {
    return false;
  }
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */

  if (string1_container >= ECMA_STRING_CONTAINER_UINT32_IN_DESC)
  {
    return string1_p->u.common_uint32_field == string2_p->u.common_uint32_field;
  }

  return ecma_compare_ecma_strings_longpath (string1_p, string2_p);
} /* ecma_compare_ecma_strings */

/**
 * Compare two non-direct ecma-strings
 *
 * @return true - if strings are equal;
 *         false - otherwise
 */
inline bool JERRY_ATTR_ALWAYS_INLINE
ecma_compare_ecma_non_direct_strings (const ecma_string_t *string1_p, /**< ecma-string */
                                      const ecma_string_t *string2_p) /**< ecma-string */
{
  JERRY_ASSERT (string1_p != NULL && string2_p != NULL);
  JERRY_ASSERT (!ECMA_IS_DIRECT_STRING (string1_p) && !ECMA_IS_DIRECT_STRING (string2_p));

  /* Fast paths first. */
  if (string1_p == string2_p)
  {
    return true;
  }

  if (string1_p->hash != string2_p->hash)
  {
    return false;
  }

  ecma_string_container_t string1_container = ECMA_STRING_GET_CONTAINER (string1_p);

  if (string1_container != ECMA_STRING_GET_CONTAINER (string2_p))
  {
    return false;
  }

  if (string1_container >= ECMA_STRING_CONTAINER_UINT32_IN_DESC)
  {
    return string1_p->u.common_uint32_field == string2_p->u.common_uint32_field;
  }

  return ecma_compare_ecma_strings_longpath (string1_p, string2_p);
} /* ecma_compare_ecma_non_direct_strings */

/**
 * Relational compare of ecma-strings.
 *
 * First string is less than second string if:
 *  - strings are not equal;
 *  - first string is prefix of second or is lexicographically less than second.
 *
 * @return true - if first string is less than second string,
 *         false - otherwise
 */
bool
ecma_compare_ecma_strings_relational (const ecma_string_t *string1_p, /**< ecma-string */
                                      const ecma_string_t *string2_p) /**< ecma-string */
{
  if (ecma_compare_ecma_strings (string1_p,
                                 string2_p))
  {
    return false;
  }

  const lit_utf8_byte_t *utf8_string1_p, *utf8_string2_p;
  lit_utf8_size_t utf8_string1_size, utf8_string2_size;

  lit_utf8_byte_t uint32_to_string_buffer1[ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32];
  lit_utf8_byte_t uint32_to_string_buffer2[ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32];

  if (ECMA_IS_DIRECT_STRING (string1_p))
  {
    if (ECMA_GET_DIRECT_STRING_TYPE (string1_p) != ECMA_DIRECT_STRING_UINT)
    {
      utf8_string1_p = ecma_string_get_chars_fast (string1_p, &utf8_string1_size);
    }
    else
    {
      utf8_string1_size = ecma_uint32_to_utf8_string ((uint32_t) ECMA_GET_DIRECT_STRING_VALUE (string1_p),
                                                      uint32_to_string_buffer1,
                                                      ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32);
      utf8_string1_p = uint32_to_string_buffer1;
    }
  }
  else
  {
    JERRY_ASSERT (string1_p->refs_and_container >= ECMA_STRING_REF_ONE);

    if (ECMA_STRING_GET_CONTAINER (string1_p) != ECMA_STRING_CONTAINER_UINT32_IN_DESC)
    {
      utf8_string1_p = ecma_string_get_chars_fast (string1_p, &utf8_string1_size);
    }
    else
    {
      utf8_string1_size = ecma_uint32_to_utf8_string (string1_p->u.uint32_number,
                                                      uint32_to_string_buffer1,
                                                      ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32);
      utf8_string1_p = uint32_to_string_buffer1;
    }
  }

  if (ECMA_IS_DIRECT_STRING (string2_p))
  {
    if (ECMA_GET_DIRECT_STRING_TYPE (string2_p) != ECMA_DIRECT_STRING_UINT)
    {
      utf8_string2_p = ecma_string_get_chars_fast (string2_p, &utf8_string2_size);
    }
    else
    {
      utf8_string2_size = ecma_uint32_to_utf8_string ((uint32_t) ECMA_GET_DIRECT_STRING_VALUE (string2_p),
                                                      uint32_to_string_buffer2,
                                                      ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32);
      utf8_string2_p = uint32_to_string_buffer2;
    }
  }
  else
  {
    JERRY_ASSERT (string2_p->refs_and_container >= ECMA_STRING_REF_ONE);

    if (ECMA_STRING_GET_CONTAINER (string2_p) != ECMA_STRING_CONTAINER_UINT32_IN_DESC)
    {
      utf8_string2_p = ecma_string_get_chars_fast (string2_p, &utf8_string2_size);
    }
    else
    {
      utf8_string2_size = ecma_uint32_to_utf8_string (string2_p->u.uint32_number,
                                                      uint32_to_string_buffer2,
                                                      ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32);
      utf8_string2_p = uint32_to_string_buffer2;
    }
  }

  return lit_compare_utf8_strings_relational (utf8_string1_p,
                                              utf8_string1_size,
                                              utf8_string2_p,
                                              utf8_string2_size);
} /* ecma_compare_ecma_strings_relational */

/**
 * Special value to represent that no size is available.
 */
#define ECMA_STRING_NO_ASCII_SIZE 0xffff

/**
 * Return the size of uint32 and magic strings.
 * The length of these strings are equal to thier size.
 *
 * @return number of characters in the string
 */
static ecma_length_t
ecma_string_get_ascii_size (const ecma_string_t *string_p) /**< ecma-string */
{
  if (ECMA_IS_DIRECT_STRING (string_p))
  {
    switch (ECMA_GET_DIRECT_STRING_TYPE (string_p))
    {
      case ECMA_DIRECT_STRING_MAGIC:
      {
        lit_magic_string_id_t id = (lit_magic_string_id_t) ECMA_GET_DIRECT_STRING_VALUE (string_p);

        JERRY_ASSERT (ECMA_STRING_IS_ASCII (lit_get_magic_string_utf8 (id),
                                            lit_get_magic_string_size (id)));
        return lit_get_magic_string_size (id);
      }
      case ECMA_DIRECT_STRING_UINT:
      {
        uint32_t uint32_number = (uint32_t) ECMA_GET_DIRECT_STRING_VALUE (string_p);
        return ecma_string_get_uint32_size (uint32_number);
      }
      default:
      {
        JERRY_ASSERT (ECMA_GET_DIRECT_STRING_TYPE (string_p) == ECMA_DIRECT_STRING_MAGIC_EX);

        return ECMA_STRING_NO_ASCII_SIZE;
      }
    }
  }

  JERRY_ASSERT (string_p->refs_and_container >= ECMA_STRING_REF_ONE);

  if (ECMA_STRING_GET_CONTAINER (string_p) == ECMA_STRING_CONTAINER_UINT32_IN_DESC)
  {
    return ecma_string_get_uint32_size (string_p->u.uint32_number);
  }

  return ECMA_STRING_NO_ASCII_SIZE;
} /* ecma_string_get_ascii_size */

/**
 * Get length of ecma-string
 *
 * @return number of characters in the string
 */
ecma_length_t
ecma_string_get_length (const ecma_string_t *string_p) /**< ecma-string */
{
  ecma_length_t length = ecma_string_get_ascii_size (string_p);

  if (length != ECMA_STRING_NO_ASCII_SIZE)
  {
    return length;
  }

  if (ECMA_IS_DIRECT_STRING (string_p))
  {
    JERRY_ASSERT (ECMA_GET_DIRECT_STRING_TYPE (string_p) == ECMA_DIRECT_STRING_MAGIC_EX);

    lit_magic_string_ex_id_t id = (lit_magic_string_ex_id_t) ECMA_GET_DIRECT_STRING_VALUE (string_p);
    return lit_utf8_string_length (lit_get_magic_string_ex_utf8 (id),
                                   lit_get_magic_string_ex_size (id));
  }

  switch (ECMA_STRING_GET_CONTAINER (string_p))
  {
    case ECMA_STRING_CONTAINER_HEAP_UTF8_STRING:
    {
      return (ecma_length_t) (string_p->u.utf8_string.length);
    }
    case ECMA_STRING_CONTAINER_HEAP_LONG_UTF8_STRING:
    {
      return (ecma_length_t) (((ecma_long_string_t *) string_p)->long_utf8_string_length);
    }
    default:
    {
      JERRY_ASSERT (ECMA_STRING_GET_CONTAINER (string_p) == ECMA_STRING_CONTAINER_MAGIC_STRING_EX);

      return lit_utf8_string_length (lit_get_magic_string_ex_utf8 (string_p->u.magic_string_ex_id),
                                     lit_get_magic_string_ex_size (string_p->u.magic_string_ex_id));
    }
  }
} /* ecma_string_get_length */

/**
 * Get length of UTF-8 encoded string length from ecma-string
 *
 * @return number of characters in the UTF-8 encoded string
 */
ecma_length_t
ecma_string_get_utf8_length (const ecma_string_t *string_p) /**< ecma-string */
{
  ecma_length_t length = ecma_string_get_ascii_size (string_p);

  if (length != ECMA_STRING_NO_ASCII_SIZE)
  {
    return length;
  }

  if (ECMA_IS_DIRECT_STRING (string_p))
  {
    JERRY_ASSERT (ECMA_GET_DIRECT_STRING_TYPE (string_p) == ECMA_DIRECT_STRING_MAGIC_EX);

    lit_magic_string_ex_id_t id = (lit_magic_string_ex_id_t) ECMA_GET_DIRECT_STRING_VALUE (string_p);
    return lit_get_utf8_length_of_cesu8_string (lit_get_magic_string_ex_utf8 (id),
                                                lit_get_magic_string_ex_size (id));
  }

  switch (ECMA_STRING_GET_CONTAINER (string_p))
  {
    case ECMA_STRING_CONTAINER_HEAP_UTF8_STRING:
    {
      if (string_p->u.utf8_string.size == (lit_utf8_size_t) string_p->u.utf8_string.length)
      {
        return (ecma_length_t) (string_p->u.utf8_string.length);
      }

      return lit_get_utf8_length_of_cesu8_string ((const lit_utf8_byte_t *) (string_p + 1),
                                                  (lit_utf8_size_t) string_p->u.utf8_string.size);
    }
    case ECMA_STRING_CONTAINER_HEAP_LONG_UTF8_STRING:
    {
      ecma_long_string_t *long_string_p = (ecma_long_string_t *) string_p;
      if (string_p->u.long_utf8_string_size == (lit_utf8_size_t) long_string_p->long_utf8_string_length)
      {
        return (ecma_length_t) (long_string_p->long_utf8_string_length);
      }

      return lit_get_utf8_length_of_cesu8_string ((const lit_utf8_byte_t *) (long_string_p + 1),
                                                  (lit_utf8_size_t) string_p->u.long_utf8_string_size);
    }
    default:
    {
      JERRY_ASSERT (ECMA_STRING_GET_CONTAINER (string_p) == ECMA_STRING_CONTAINER_MAGIC_STRING_EX);

      return lit_get_utf8_length_of_cesu8_string (lit_get_magic_string_ex_utf8 (string_p->u.magic_string_ex_id),
                                                  lit_get_magic_string_ex_size (string_p->u.magic_string_ex_id));
    }
  }
} /* ecma_string_get_utf8_length */

/**
 * Get size of ecma-string
 *
 * @return number of bytes in the buffer needed to represent the string
 */
lit_utf8_size_t
ecma_string_get_size (const ecma_string_t *string_p) /**< ecma-string */
{
  ecma_length_t length = ecma_string_get_ascii_size (string_p);

  if (length != ECMA_STRING_NO_ASCII_SIZE)
  {
    return length;
  }

  if (ECMA_IS_DIRECT_STRING (string_p))
  {
    JERRY_ASSERT (ECMA_GET_DIRECT_STRING_TYPE (string_p) == ECMA_DIRECT_STRING_MAGIC_EX);

    lit_magic_string_ex_id_t id = (lit_magic_string_ex_id_t) ECMA_GET_DIRECT_STRING_VALUE (string_p);
    return lit_get_magic_string_ex_size (id);
  }

  switch (ECMA_STRING_GET_CONTAINER (string_p))
  {
    case ECMA_STRING_CONTAINER_HEAP_UTF8_STRING:
    {
      return (lit_utf8_size_t) string_p->u.utf8_string.size;
    }
    case ECMA_STRING_CONTAINER_HEAP_LONG_UTF8_STRING:
    {
      return (lit_utf8_size_t) string_p->u.long_utf8_string_size;
    }
    default:
    {
      JERRY_ASSERT (ECMA_STRING_GET_CONTAINER (string_p) == ECMA_STRING_CONTAINER_MAGIC_STRING_EX);

      return lit_get_magic_string_ex_size (string_p->u.magic_string_ex_id);
    }
  }
} /* ecma_string_get_size */

/**
 * Get the UTF-8 encoded string size from ecma-string
 *
 * @return number of bytes in the buffer needed to represent an UTF-8 encoded string
 */
lit_utf8_size_t
ecma_string_get_utf8_size (const ecma_string_t *string_p) /**< ecma-string */
{
  ecma_length_t length = ecma_string_get_ascii_size (string_p);

  if (length != ECMA_STRING_NO_ASCII_SIZE)
  {
    return length;
  }

  if (ECMA_IS_DIRECT_STRING (string_p))
  {
    JERRY_ASSERT (ECMA_GET_DIRECT_STRING_TYPE (string_p) == ECMA_DIRECT_STRING_MAGIC_EX);

    lit_magic_string_ex_id_t id = (lit_magic_string_ex_id_t) ECMA_GET_DIRECT_STRING_VALUE (string_p);
    return lit_get_utf8_size_of_cesu8_string (lit_get_magic_string_ex_utf8 (id),
                                              lit_get_magic_string_ex_size (id));
  }

  switch (ECMA_STRING_GET_CONTAINER (string_p))
  {
    case ECMA_STRING_CONTAINER_HEAP_UTF8_STRING:
    {
      if (string_p->u.utf8_string.size == (lit_utf8_size_t) string_p->u.utf8_string.length)
      {
        return (lit_utf8_size_t) string_p->u.utf8_string.size;
      }

      return lit_get_utf8_size_of_cesu8_string ((const lit_utf8_byte_t *) (string_p + 1),
                                                (lit_utf8_size_t) string_p->u.utf8_string.size);
    }
    case ECMA_STRING_CONTAINER_HEAP_LONG_UTF8_STRING:
    {
      ecma_long_string_t *long_string_p = (ecma_long_string_t *) string_p;
      if (string_p->u.long_utf8_string_size == (lit_utf8_size_t) long_string_p->long_utf8_string_length)
      {
        return (lit_utf8_size_t) string_p->u.long_utf8_string_size;
      }

      return lit_get_utf8_size_of_cesu8_string ((const lit_utf8_byte_t *) (string_p + 1),
                                                (lit_utf8_size_t) string_p->u.long_utf8_string_size);
    }
    default:
    {
      JERRY_ASSERT (ECMA_STRING_GET_CONTAINER (string_p) == ECMA_STRING_CONTAINER_MAGIC_STRING_EX);

      return lit_get_utf8_size_of_cesu8_string (lit_get_magic_string_ex_utf8 (string_p->u.magic_string_ex_id),
                                                lit_get_magic_string_ex_size (string_p->u.magic_string_ex_id));
    }
  }
} /* ecma_string_get_utf8_size */

/**
 * Get character from specified position in the ecma-string.
 *
 * @return character value
 */
ecma_char_t
ecma_string_get_char_at_pos (const ecma_string_t *string_p, /**< ecma-string */
                             ecma_length_t index) /**< index of character */
{
  JERRY_ASSERT (index < ecma_string_get_length (string_p));

  lit_utf8_size_t buffer_size;
  uint8_t flags = ECMA_STRING_FLAG_IS_ASCII;
  const lit_utf8_byte_t *chars_p = ecma_string_get_chars (string_p, &buffer_size, &flags);

  ecma_char_t ch;
  if (flags & ECMA_STRING_FLAG_IS_ASCII)
  {
    ch = chars_p[index];
  }
  else
  {
    ch = lit_utf8_string_code_unit_at (chars_p, buffer_size, index);
  }

  if (flags & ECMA_STRING_FLAG_MUST_BE_FREED)
  {
    jmem_heap_free_block ((void *) chars_p, buffer_size);
  }

  return ch;
} /* ecma_string_get_char_at_pos */

/**
 * Check if passed string equals to one of magic strings
 * and if equal magic string was found, return it's id in 'out_id_p' argument.
 *
 * @return id - if magic string equal to passed string was found,
 *         LIT_MAGIC_STRING__COUNT - otherwise.
 */
lit_magic_string_id_t
ecma_get_string_magic (const ecma_string_t *string_p) /**< ecma-string */
{
  if (ECMA_IS_DIRECT_STRING_WITH_TYPE (string_p, ECMA_DIRECT_STRING_MAGIC))
  {
    return (lit_magic_string_id_t) ECMA_GET_DIRECT_STRING_VALUE (string_p);
  }

  return LIT_MAGIC_STRING__COUNT;
} /* ecma_get_string_magic */

/**
 * Try to calculate hash of the ecma-string
 *
 * @return calculated hash
 */
inline lit_string_hash_t JERRY_ATTR_ALWAYS_INLINE
ecma_string_hash (const ecma_string_t *string_p) /**< ecma-string to calculate hash for */
{
  if (!ECMA_IS_DIRECT_STRING (string_p))
  {
    return (string_p->hash);
  }

  lit_string_hash_t hash = (lit_string_hash_t) ECMA_GET_DIRECT_STRING_VALUE (string_p);

  if (ECMA_GET_DIRECT_STRING_TYPE (string_p) == ECMA_DIRECT_STRING_MAGIC_EX)
  {
    hash = (lit_string_hash_t) (hash + LIT_MAGIC_STRING__COUNT);
  }

  return hash;
} /* ecma_string_hash */

/**
 * Create a substring from an ecma string
 *
 * @return a newly consturcted ecma string with its value initialized to a copy of a substring of the first argument
 */
ecma_string_t *
ecma_string_substr (const ecma_string_t *string_p, /**< pointer to an ecma string */
                    ecma_length_t start_pos, /**< start position, should be less or equal than string length */
                    ecma_length_t end_pos) /**< end position, should be less or equal than string length */
{
  const ecma_length_t string_length = ecma_string_get_length (string_p);
  JERRY_ASSERT (start_pos <= string_length);
  JERRY_ASSERT (end_pos <= string_length);

  if (start_pos >= end_pos)
  {
    return ecma_get_magic_string (LIT_MAGIC_STRING__EMPTY);
  }

  ecma_string_t *ecma_string_p = NULL;
  end_pos -= start_pos;

  ECMA_STRING_TO_UTF8_STRING (string_p, start_p, buffer_size);

  if (string_length == buffer_size)
  {
    ecma_string_p = ecma_new_ecma_string_from_utf8 (start_p + start_pos,
                                                    (lit_utf8_size_t) end_pos);
  }
  else
  {
    while (start_pos--)
    {
      start_p += lit_get_unicode_char_size_by_utf8_first_byte (*start_p);
    }

    const lit_utf8_byte_t *end_p = start_p;
    while (end_pos--)
    {
      end_p += lit_get_unicode_char_size_by_utf8_first_byte (*end_p);
    }

    ecma_string_p = ecma_new_ecma_string_from_utf8 (start_p, (lit_utf8_size_t) (end_p - start_p));
  }

  ECMA_FINALIZE_UTF8_STRING (start_p, buffer_size);

  return ecma_string_p;
} /* ecma_string_substr */

/**
 * Trim leading and trailing whitespace characters from string.
 *
 * @return trimmed ecma string
 */
ecma_string_t *
ecma_string_trim (const ecma_string_t *string_p) /**< pointer to an ecma string */
{
  ecma_string_t *ret_string_p;

  ECMA_STRING_TO_UTF8_STRING (string_p, utf8_str_p, utf8_str_size);

  if (utf8_str_size > 0)
  {
    ecma_char_t ch;
    lit_utf8_size_t read_size;
    const lit_utf8_byte_t *nonws_start_p = utf8_str_p + utf8_str_size;
    const lit_utf8_byte_t *current_p = utf8_str_p;

    /* Trim front. */
    while (current_p < nonws_start_p)
    {
      read_size = lit_read_code_unit_from_utf8 (current_p, &ch);

      if (!lit_char_is_white_space (ch)
          && !lit_char_is_line_terminator (ch))
      {
        nonws_start_p = current_p;
        break;
      }

      current_p += read_size;
    }

    current_p = utf8_str_p + utf8_str_size;

    /* Trim back. */
    while (current_p > utf8_str_p)
    {
      read_size = lit_read_prev_code_unit_from_utf8 (current_p, &ch);

      if (!lit_char_is_white_space (ch)
          && !lit_char_is_line_terminator (ch))
      {
        break;
      }

      current_p -= read_size;
    }

    /* Construct new string. */
    if (current_p > nonws_start_p)
    {
      ret_string_p = ecma_new_ecma_string_from_utf8 (nonws_start_p,
                                                     (lit_utf8_size_t) (current_p - nonws_start_p));
    }
    else
    {
      ret_string_p = ecma_get_magic_string (LIT_MAGIC_STRING__EMPTY);
    }
  }
  else
  {
    ret_string_p = ecma_get_magic_string (LIT_MAGIC_STRING__EMPTY);
  }

  ECMA_FINALIZE_UTF8_STRING (utf8_str_p, utf8_str_size);

  return ret_string_p;
} /* ecma_string_trim */

/**
 * @}
 * @}
 */
