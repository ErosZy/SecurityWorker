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
#include "jrt-bit-fields.h"
#include "vm-defines.h"

#include "ecma-function-object.h"

JERRY_STATIC_ASSERT (ECMA_TYPE___MAX <= ECMA_VALUE_TYPE_MASK,
                     ecma_types_must_be_less_than_mask);

JERRY_STATIC_ASSERT ((ECMA_VALUE_TYPE_MASK + 1) == (1 << ECMA_VALUE_SHIFT),
                     ecma_value_part_must_start_after_flags);

JERRY_STATIC_ASSERT (ECMA_VALUE_SHIFT <= JMEM_ALIGNMENT_LOG,
                     ecma_value_shift_must_be_less_than_or_equal_than_mem_alignment_log);

JERRY_STATIC_ASSERT (sizeof (jmem_cpointer_t) <= sizeof (ecma_value_t),
                     size_of_jmem_cpointer_t_must_be_less_or_equal_to_the_size_of_ecma_value_t);

#ifdef ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY

JERRY_STATIC_ASSERT (sizeof (uintptr_t) <= sizeof (ecma_value_t),
                     uintptr_t_must_fit_in_ecma_value_t);

#else /* !ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY */

JERRY_STATIC_ASSERT (sizeof (uintptr_t) > sizeof (ecma_value_t),
                     uintptr_t_must_not_fit_in_ecma_value_t);

#endif /* ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY */

JERRY_STATIC_ASSERT ((ECMA_VALUE_FALSE | (1 << ECMA_DIRECT_SHIFT)) == ECMA_VALUE_TRUE
                     && ECMA_VALUE_FALSE != ECMA_VALUE_TRUE,
                     only_the_lowest_bit_must_be_different_for_simple_value_true_and_false);

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmahelpers Helpers for operations with ECMA data types
 * @{
 */

/**
 * Get type field of ecma value
 *
 * @return type field
 */
static inline ecma_type_t JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_get_value_type_field (ecma_value_t value) /**< ecma value */
{
  return value & ECMA_VALUE_TYPE_MASK;
} /* ecma_get_value_type_field */

/**
 * Convert a pointer into an ecma value.
 *
 * @return ecma value
 */
static inline ecma_value_t JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_pointer_to_ecma_value (const void *ptr) /**< pointer */
{
#ifdef ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY

  JERRY_ASSERT (ptr != NULL);
  uintptr_t uint_ptr = (uintptr_t) ptr;
  JERRY_ASSERT ((uint_ptr & ECMA_VALUE_TYPE_MASK) == 0);
  return (ecma_value_t) uint_ptr;

#else /* !ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY */

  jmem_cpointer_t ptr_cp;
  ECMA_SET_NON_NULL_POINTER (ptr_cp, ptr);
  return ((ecma_value_t) ptr_cp) << ECMA_VALUE_SHIFT;

#endif /* ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY */
} /* ecma_pointer_to_ecma_value */

/**
 * Get a pointer from an ecma value
 *
 * @return pointer
 */
static inline void * JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_get_pointer_from_ecma_value (ecma_value_t value) /**< value */
{
#ifdef ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY
  void *ptr = (void *) (uintptr_t) ((value) & ~ECMA_VALUE_TYPE_MASK);
  JERRY_ASSERT (ptr != NULL);
  return ptr;
#else /* !ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY */
  return ECMA_GET_NON_NULL_POINTER (void, value >> ECMA_VALUE_SHIFT);
#endif /* ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY */
} /* ecma_get_pointer_from_ecma_value */

/**
 * Check if the value is direct ecma-value.
 *
 * @return true - if the value is a direct value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_direct (ecma_value_t value) /**< ecma value */
{
  return (ecma_get_value_type_field (value) == ECMA_TYPE_DIRECT);
} /* ecma_is_value_direct */

/**
 * Check if the value is simple ecma-value.
 *
 * @return true - if the value is a simple value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_simple (ecma_value_t value) /**< ecma value */
{
  return (value & ECMA_DIRECT_TYPE_MASK) == ECMA_DIRECT_TYPE_SIMPLE_VALUE;
} /* ecma_is_value_simple */

/**
 * Check whether the value is a given simple value.
 *
 * @return true - if the value is equal to the given simple value,
 *         false - otherwise
 */
static inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_equal_to_simple_value (ecma_value_t value, /**< ecma value */
                                     ecma_value_t simple_value) /**< simple value */
{
  return value == simple_value;
} /* ecma_is_value_equal_to_simple_value */

/**
 * Check if the value is empty.
 *
 * @return true - if the value contains implementation-defined empty simple value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_empty (ecma_value_t value) /**< ecma value */
{
  return ecma_is_value_equal_to_simple_value (value, ECMA_VALUE_EMPTY);
} /* ecma_is_value_empty */

/**
 * Check if the value is undefined.
 *
 * @return true - if the value contains ecma-undefined simple value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_undefined (ecma_value_t value) /**< ecma value */
{
  return ecma_is_value_equal_to_simple_value (value, ECMA_VALUE_UNDEFINED);
} /* ecma_is_value_undefined */

/**
 * Check if the value is null.
 *
 * @return true - if the value contains ecma-null simple value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_null (ecma_value_t value) /**< ecma value */
{
  return ecma_is_value_equal_to_simple_value (value, ECMA_VALUE_NULL);
} /* ecma_is_value_null */

/**
 * Check if the value is boolean.
 *
 * @return true - if the value contains ecma-true or ecma-false simple values,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_boolean (ecma_value_t value) /**< ecma value */
{
  return ecma_is_value_true (value | (1 << ECMA_DIRECT_SHIFT));
} /* ecma_is_value_boolean */

/**
 * Check if the value is true.
 *
 * @return true - if the value contains ecma-true simple value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_true (ecma_value_t value) /**< ecma value */
{
  return ecma_is_value_equal_to_simple_value (value, ECMA_VALUE_TRUE);
} /* ecma_is_value_true */

/**
 * Check if the value is false.
 *
 * @return true - if the value contains ecma-false simple value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_false (ecma_value_t value) /**< ecma value */
{
  return ecma_is_value_equal_to_simple_value (value, ECMA_VALUE_FALSE);
} /* ecma_is_value_false */

/**
 * Check if the value is not found.
 *
 * @return true - if the value contains ecma-not-found simple value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_found (ecma_value_t value) /**< ecma value */
{
  return value != ECMA_VALUE_NOT_FOUND;
} /* ecma_is_value_found */

/**
 * Check if the value is array hole.
 *
 * @return true - if the value contains ecma-array-hole simple value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_array_hole (ecma_value_t value) /**< ecma value */
{
  return ecma_is_value_equal_to_simple_value (value, ECMA_VALUE_ARRAY_HOLE);
} /* ecma_is_value_array_hole */

/**
 * Check if the value is integer ecma-number.
 *
 * @return true - if the value contains an integer ecma-number value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_integer_number (ecma_value_t value) /**< ecma value */
{
  return (value & ECMA_DIRECT_TYPE_MASK) == ECMA_DIRECT_TYPE_INTEGER_VALUE;
} /* ecma_is_value_integer_number */

/**
 * Check if both values are integer ecma-numbers.
 *
 * @return true - if both values contain integer ecma-number values,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_are_values_integer_numbers (ecma_value_t first_value, /**< first ecma value */
                                 ecma_value_t second_value) /**< second ecma value */
{
  JERRY_STATIC_ASSERT (ECMA_DIRECT_TYPE_INTEGER_VALUE == 0,
                       ecma_direct_type_integer_value_must_be_zero);

  return ((first_value | second_value) & ECMA_DIRECT_TYPE_MASK) == ECMA_DIRECT_TYPE_INTEGER_VALUE;
} /* ecma_are_values_integer_numbers */

/**
 * Check if the value is floating-point ecma-number.
 *
 * @return true - if the value contains a floating-point ecma-number value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_float_number (ecma_value_t value) /**< ecma value */
{
  return (ecma_get_value_type_field (value) == ECMA_TYPE_FLOAT);
} /* ecma_is_value_float_number */

/**
 * Check if the value is ecma-number.
 *
 * @return true - if the value contains ecma-number value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_number (ecma_value_t value) /**< ecma value */
{
  return (ecma_is_value_integer_number (value)
          || ecma_is_value_float_number (value));
} /* ecma_is_value_number */

JERRY_STATIC_ASSERT ((ECMA_TYPE_STRING | 0x4) == ECMA_TYPE_DIRECT_STRING,
                     ecma_type_string_and_direct_string_must_have_one_bit_difference);

/**
 * Check if the value is ecma-string.
 *
 * @return true - if the value contains ecma-string value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_string (ecma_value_t value) /**< ecma value */
{
  return ((value & (ECMA_VALUE_TYPE_MASK - 0x4)) == ECMA_TYPE_STRING);
} /* ecma_is_value_string */

#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
/**
 * Check if the value is symbol.
 *
 * @return true - if the value contains symbol value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_symbol (ecma_value_t value) /**< ecma value */
{
  return (ecma_get_value_type_field (value) == ECMA_TYPE_SYMBOL);
} /* ecma_is_value_symbol */
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */

/**
 * Check if the value can be property name.
 *
 * @return true - if the value can be property name value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_prop_name (ecma_value_t value) /**< ecma value */
{
#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
  return ecma_is_value_string (value) || ecma_is_value_symbol (value);
#else /* CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */
  return ecma_is_value_string (value);
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */
} /* ecma_is_value_prop_name */

/**
 * Check if the value is direct ecma-string.
 *
 * @return true - if the value contains direct ecma-string value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_direct_string (ecma_value_t value) /**< ecma value */
{
  return (ecma_get_value_type_field (value) == ECMA_TYPE_DIRECT_STRING);
} /* ecma_is_value_direct_string */

/**
 * Check if the value is non-direct ecma-string.
 *
 * @return true - if the value contains non-direct ecma-string value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_non_direct_string (ecma_value_t value) /**< ecma value */
{
  return (ecma_get_value_type_field (value) == ECMA_TYPE_STRING);
} /* ecma_is_value_non_direct_string */

/**
 * Check if the value is object.
 *
 * @return true - if the value contains object value,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_object (ecma_value_t value) /**< ecma value */
{
  return (ecma_get_value_type_field (value) == ECMA_TYPE_OBJECT);
} /* ecma_is_value_object */

/**
 * Check if the value is error reference.
 *
 * @return true - if the value contains an error reference,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_error_reference (ecma_value_t value) /**< ecma value */
{
  return (ecma_get_value_type_field (value) == ECMA_TYPE_ERROR);
} /* ecma_is_value_error_reference */

/**
 * Check if the value is an aligned pointer.
 *
 * @return true - if the value contains an aligned pointer,
 *         false - otherwise
 */
inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_value_pointer (ecma_value_t value) /**< ecma value */
{
  return (ecma_get_value_type_field (value) == ECMA_TYPE_POINTER);
} /* ecma_is_value_pointer */

/**
 * Debug assertion that specified value's type is one of ECMA-defined
 * script-visible types, i.e.: undefined, null, boolean, number, string, object.
 */
void
ecma_check_value_type_is_spec_defined (ecma_value_t value) /**< ecma value */
{
  JERRY_ASSERT (ecma_is_value_undefined (value)
                || ecma_is_value_null (value)
                || ecma_is_value_boolean (value)
                || ecma_is_value_number (value)
                || ecma_is_value_string (value)
                || ECMA_ASSERT_VALUE_IS_SYMBOL (value)
                || ecma_is_value_object (value));
} /* ecma_check_value_type_is_spec_defined */

/**
 * Creates an ecma value from the given raw boolean.
 *
 * @return boolean ecma_value
 */
inline ecma_value_t JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_make_boolean_value (bool boolean_value) /**< raw bool value from which the ecma value will be created */
{
  return boolean_value ? ECMA_VALUE_TRUE : ECMA_VALUE_FALSE;
} /* ecma_make_boolean_value */

/**
 * Encode an integer number into an ecma-value without allocating memory
 *
 * Note:
 *   The value must fit into the range of allowed ecma integer values
 *
 * @return ecma-value
 */
inline ecma_value_t JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_make_integer_value (ecma_integer_value_t integer_value) /**< integer number to be encoded */
{
  JERRY_ASSERT (ECMA_IS_INTEGER_NUMBER (integer_value));

  return (((ecma_value_t) integer_value) << ECMA_DIRECT_SHIFT) | ECMA_DIRECT_TYPE_INTEGER_VALUE;
} /* ecma_make_integer_value */

/**
 * Allocate and initialize a new float number without checks.
 *
 * @return ecma-value
 */
static ecma_value_t
ecma_create_float_number (ecma_number_t ecma_number) /**< value of the float number */
{
  ecma_number_t *ecma_num_p = ecma_alloc_number ();

  *ecma_num_p = ecma_number;

  return ecma_pointer_to_ecma_value (ecma_num_p) | ECMA_TYPE_FLOAT;
} /* ecma_create_float_number */

/**
 * Create a new NaN value.
 *
 * @return ecma-value
 */
inline ecma_value_t JERRY_ATTR_ALWAYS_INLINE
ecma_make_nan_value (void)
{
  return ecma_create_float_number (ecma_number_make_nan ());
} /* ecma_make_nan_value */

/**
 * Checks whether the passed number is +0.0
 *
 * @return true, if it is +0.0, false otherwise
 */
static inline bool JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_is_number_equal_to_positive_zero (ecma_number_t ecma_number) /**< number */
{
#if CONFIG_ECMA_NUMBER_TYPE == CONFIG_ECMA_NUMBER_FLOAT32
  union
  {
    uint32_t u32_value;
    ecma_number_t float_value;
  } u;

  u.float_value = ecma_number;

  return u.u32_value == 0;
#else /* CONFIG_ECMA_NUMBER_TYPE != CONFIG_ECMA_NUMBER_FLOAT32 */
  union
  {
    uint64_t u64_value;
    ecma_number_t float_value;
  } u;

  u.float_value = ecma_number;

  return u.u64_value == 0;
#endif /* CONFIG_ECMA_NUMBER_TYPE == CONFIG_ECMA_NUMBER_FLOAT32 */
} /* ecma_is_number_equal_to_positive_zero */

/**
 * Encode a number into an ecma-value
 *
 * @return ecma-value
 */
ecma_value_t
ecma_make_number_value (ecma_number_t ecma_number) /**< number to be encoded */
{
  ecma_integer_value_t integer_value = (ecma_integer_value_t) ecma_number;

  if ((ecma_number_t) integer_value == ecma_number
      && ((integer_value == 0) ? ecma_is_number_equal_to_positive_zero (ecma_number)
                               : ECMA_IS_INTEGER_NUMBER (integer_value)))
  {
    return ecma_make_integer_value (integer_value);
  }

  return ecma_create_float_number (ecma_number);
} /* ecma_make_number_value */

/**
 * Encode an int32 number into an ecma-value
 *
 * @return ecma-value
 */
ecma_value_t
ecma_make_int32_value (int32_t int32_number) /**< int32 number to be encoded */
{
  if (ECMA_IS_INTEGER_NUMBER (int32_number))
  {
    return ecma_make_integer_value ((ecma_integer_value_t) int32_number);
  }

  return ecma_create_float_number ((ecma_number_t) int32_number);
} /* ecma_make_int32_value */

/**
 * Encode an unsigned int32 number into an ecma-value
 *
 * @return ecma-value
 */
ecma_value_t
ecma_make_uint32_value (uint32_t uint32_number) /**< uint32 number to be encoded */
{
  if (uint32_number <= ECMA_INTEGER_NUMBER_MAX)
  {
    return ecma_make_integer_value ((ecma_integer_value_t) uint32_number);
  }

  return ecma_create_float_number ((ecma_number_t) uint32_number);
} /* ecma_make_uint32_value */

/**
 * String value constructor
 *
 * @return ecma-value representation of the string argument
 */
inline ecma_value_t JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_make_string_value (const ecma_string_t *ecma_string_p) /**< string to reference in value */
{
  JERRY_ASSERT (ecma_string_p != NULL);
#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
  JERRY_ASSERT (!ecma_prop_name_is_symbol ((ecma_string_t *) ecma_string_p));
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */

  if ((((uintptr_t) ecma_string_p) & ECMA_VALUE_TYPE_MASK) != 0)
  {
    return (ecma_value_t) (uintptr_t) ecma_string_p;
  }

  return ecma_pointer_to_ecma_value (ecma_string_p) | ECMA_TYPE_STRING;
} /* ecma_make_string_value */

#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
/**
 * Symbol value constructor
 *
 * @return ecma-value representation of the string argument
 */
inline ecma_value_t JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_make_symbol_value (const ecma_string_t *ecma_symbol_p) /**< symbol to reference in value */
{
  JERRY_ASSERT (ecma_symbol_p != NULL);
  JERRY_ASSERT (ecma_prop_name_is_symbol ((ecma_string_t *) ecma_symbol_p));

  return ecma_pointer_to_ecma_value (ecma_symbol_p) | ECMA_TYPE_SYMBOL;
} /* ecma_make_symbol_value */
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */

/**
 * Property-name value constructor
 *
 * @return ecma-value representation of a property name argument
 */
inline ecma_value_t JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_make_prop_name_value (const ecma_string_t *ecma_prop_name_p) /**< property name to reference in value */
{
  JERRY_ASSERT (ecma_prop_name_p != NULL);

#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
  if (ecma_prop_name_is_symbol ((ecma_string_t *) ecma_prop_name_p))
  {
    return ecma_make_symbol_value (ecma_prop_name_p);
  }
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */

  return ecma_make_string_value (ecma_prop_name_p);
} /* ecma_make_prop_name_value */

/**
 * String value constructor
 *
 * @return ecma-value representation of the string argument
 */
inline ecma_value_t JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_make_magic_string_value (lit_magic_string_id_t id) /**< magic string id */
{
  return (ecma_value_t) ECMA_CREATE_DIRECT_STRING (ECMA_DIRECT_STRING_MAGIC, (uintptr_t) id);
} /* ecma_make_magic_string_value */

/**
 * Object value constructor
 *
 * @return ecma-value representation of the object argument
 */
inline ecma_value_t JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_make_object_value (const ecma_object_t *object_p) /**< object to reference in value */
{
  JERRY_ASSERT (object_p != NULL);

  return ecma_pointer_to_ecma_value (object_p) | ECMA_TYPE_OBJECT;
} /* ecma_make_object_value */

/**
 * Error reference constructor
 *
 * @return ecma-value representation of the Error reference
 */
inline ecma_value_t JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_make_error_reference_value (const ecma_error_reference_t *error_ref_p) /**< error reference */
{
  JERRY_ASSERT (error_ref_p != NULL);

  return ecma_pointer_to_ecma_value (error_ref_p) | ECMA_TYPE_ERROR;
} /* ecma_make_error_reference_value */

/**
 * Create an ecma value from an aligned pointer
 *
 * @return ecma-value representation of the aligned pointer
 */
inline ecma_value_t JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_make_pointer_value (const void *any_p) /**< any aligned pointer */
{
#ifdef ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY

  uintptr_t uint_ptr = (uintptr_t) any_p;
  JERRY_ASSERT ((uint_ptr & ECMA_VALUE_TYPE_MASK) == 0);
  return ((ecma_value_t) uint_ptr) | ECMA_TYPE_POINTER;

#else /* !ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY */

  jmem_cpointer_t ptr_cp;
  ECMA_SET_POINTER (ptr_cp, any_p);
  return (((ecma_value_t) ptr_cp) << ECMA_VALUE_SHIFT) | ECMA_TYPE_POINTER;

#endif /* ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY */
} /* ecma_make_pointer_value */

/**
 * Get integer value from an integer ecma value
 *
 * @return integer value
 */
inline ecma_integer_value_t JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_get_integer_from_value (ecma_value_t value) /**< ecma value */
{
  JERRY_ASSERT (ecma_is_value_integer_number (value));

  return ((ecma_integer_value_t) value) >> ECMA_DIRECT_SHIFT;
} /* ecma_get_integer_from_value */

/**
 * Get floating point value from an ecma value
 *
 * @return floating point value
 */
inline ecma_number_t JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_get_float_from_value (ecma_value_t value) /**< ecma value */
{
  JERRY_ASSERT (ecma_get_value_type_field (value) == ECMA_TYPE_FLOAT);

  return *(ecma_number_t *) ecma_get_pointer_from_ecma_value (value);
} /* ecma_get_float_from_value */

/**
 * Get floating point value from an ecma value
 *
 * @return floating point value
 */
ecma_number_t JERRY_ATTR_PURE
ecma_get_number_from_value (ecma_value_t value) /**< ecma value */
{
  if (ecma_is_value_integer_number (value))
  {
    return (ecma_number_t) ecma_get_integer_from_value (value);
  }

  return ecma_get_float_from_value (value);
} /* ecma_get_number_from_value */

/**
 * Get pointer to ecma-string from ecma value
 *
 * @return the string pointer
 */
inline ecma_string_t *JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_get_string_from_value (ecma_value_t value) /**< ecma value */
{
  JERRY_ASSERT (ecma_is_value_string (value));

  if ((value & ECMA_VALUE_TYPE_MASK) == ECMA_TYPE_DIRECT_STRING)
  {
    return (ecma_string_t *) (uintptr_t) value;
  }

  return (ecma_string_t *) ecma_get_pointer_from_ecma_value (value);
} /* ecma_get_string_from_value */

#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
/**
 * Get pointer to ecma-string from ecma value
 *
 * @return the string pointer
 */
inline ecma_string_t *JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_get_symbol_from_value (ecma_value_t value) /**< ecma value */
{
  JERRY_ASSERT (ecma_is_value_symbol (value));

  return (ecma_string_t *) ecma_get_pointer_from_ecma_value (value);
} /* ecma_get_symbol_from_value */
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */

/**
 * Get pointer to a property name from ecma value
 *
 * @return the string pointer
 */
inline ecma_string_t *JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_get_prop_name_from_value (ecma_value_t value) /**< ecma value */
{
  JERRY_ASSERT (ecma_is_value_prop_name (value));

  if ((value & ECMA_VALUE_TYPE_MASK) == ECMA_TYPE_DIRECT_STRING)
  {
    return (ecma_string_t *) (uintptr_t) value;
  }

  return (ecma_string_t *) ecma_get_pointer_from_ecma_value (value);
} /* ecma_get_prop_name_from_value */

/**
 * Get pointer to ecma-object from ecma value
 *
 * @return the pointer
 */
inline ecma_object_t *JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_get_object_from_value (ecma_value_t value) /**< ecma value */
{
  JERRY_ASSERT (ecma_is_value_object (value));

  return (ecma_object_t *) ecma_get_pointer_from_ecma_value (value);
} /* ecma_get_object_from_value */

/**
 * Get pointer to error reference from ecma value
 *
 * @return the pointer
 */
inline ecma_error_reference_t *JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_get_error_reference_from_value (ecma_value_t value) /**< ecma value */
{
  JERRY_ASSERT (ecma_get_value_type_field (value) == ECMA_TYPE_ERROR);

  return (ecma_error_reference_t *) ecma_get_pointer_from_ecma_value (value);
} /* ecma_get_error_reference_from_value */

/**
 * Get an aligned pointer from an ecma value
 *
 * @return pointer value
 */
inline void * JERRY_ATTR_PURE JERRY_ATTR_ALWAYS_INLINE
ecma_get_pointer_from_value (ecma_value_t value) /**< ecma value */
{
  JERRY_ASSERT (ecma_get_value_type_field (value) == ECMA_TYPE_POINTER);

#ifdef ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY
  return (void *) (uintptr_t) ((value) & ~ECMA_VALUE_TYPE_MASK);
#else /* !ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY */
  return ECMA_GET_POINTER (void, value >> ECMA_VALUE_SHIFT);
#endif /* ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY */
} /* ecma_get_pointer_from_value */

/**
 * Invert a boolean value
 *
 * @return ecma value
 */
inline ecma_value_t JERRY_ATTR_CONST JERRY_ATTR_ALWAYS_INLINE
ecma_invert_boolean_value (ecma_value_t value) /**< ecma value */
{
  JERRY_ASSERT (ecma_is_value_boolean (value));

  return (value ^ (1 << ECMA_DIRECT_SHIFT));
} /* ecma_invert_boolean_value */

/**
 * Copy ecma value.
 *
 * @return copy of the given value
 */
ecma_value_t
ecma_copy_value (ecma_value_t value)  /**< value description */
{
  switch (ecma_get_value_type_field (value))
  {
    case ECMA_TYPE_FLOAT:
    {
      ecma_number_t *num_p = (ecma_number_t *) ecma_get_pointer_from_ecma_value (value);

      return ecma_create_float_number (*num_p);
    }
    case ECMA_TYPE_STRING:
    {
      ecma_ref_ecma_string (ecma_get_string_from_value (value));
      return value;
    }
#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
    case ECMA_TYPE_SYMBOL:
    {
      ecma_ref_ecma_string (ecma_get_symbol_from_value (value));
      return value;
    }
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */
    case ECMA_TYPE_OBJECT:
    {
      ecma_ref_object (ecma_get_object_from_value (value));
      return value;
    }
    default:
    {
      JERRY_ASSERT (ecma_get_value_type_field (value) == ECMA_TYPE_DIRECT
                    || ecma_get_value_type_field (value) == ECMA_TYPE_DIRECT_STRING);

      return value;
    }
  }
} /* ecma_copy_value */

/**
 * Copy ecma value.
 *
 * Note:
 *   this function is similar to ecma_copy_value, but it is
 *   faster for direct values since no function call is performed.
 *   It also increases the binary size so it is recommended for
 *   critical code paths only.
 *
 * @return copy of the given value
 */
inline ecma_value_t JERRY_ATTR_ALWAYS_INLINE
ecma_fast_copy_value (ecma_value_t value)  /**< value description */
{
  return (ecma_get_value_type_field (value) == ECMA_TYPE_DIRECT) ? value : ecma_copy_value (value);
} /* ecma_fast_copy_value */

/**
 * Copy the ecma value if not an object
 *
 * @return copy of the given value
 */
ecma_value_t
ecma_copy_value_if_not_object (ecma_value_t value) /**< value description */
{
  if (ecma_get_value_type_field (value) != ECMA_TYPE_OBJECT)
  {
    return ecma_copy_value (value);
  }

  return value;
} /* ecma_copy_value_if_not_object */

/**
 * Assign a new value to an ecma-value
 *
 * Note:
 *      value previously stored in the property is freed
 */
void
ecma_value_assign_value (ecma_value_t *value_p, /**< [in, out] ecma value */
                         ecma_value_t ecma_value) /**< value to assign */
{
  JERRY_STATIC_ASSERT (ECMA_TYPE_DIRECT == 0,
                       ecma_type_direct_must_be_zero_for_the_next_check);

  if (*value_p == ecma_value)
  {
    return;
  }

  if (ecma_get_value_type_field (ecma_value || *value_p) == ECMA_TYPE_DIRECT)
  {
    *value_p = ecma_value;
  }
  else if (ecma_is_value_float_number (ecma_value)
           && ecma_is_value_float_number (*value_p))
  {
    const ecma_number_t *num_src_p = (ecma_number_t *) ecma_get_pointer_from_ecma_value (ecma_value);
    ecma_number_t *num_dst_p = (ecma_number_t *) ecma_get_pointer_from_ecma_value (*value_p);

    *num_dst_p = *num_src_p;
  }
  else
  {
    ecma_free_value_if_not_object (*value_p);
    *value_p = ecma_copy_value_if_not_object (ecma_value);
  }
} /* ecma_value_assign_value */

/**
 * Update the value of a float number to a new value
 *
 * Note:
 *   The original value is destroyed.
 *
 * @return updated ecma value
 */
ecma_value_t
ecma_update_float_number (ecma_value_t float_value, /**< original float value */
                          ecma_number_t new_number) /**< updated number value */
{
  JERRY_ASSERT (ecma_is_value_float_number (float_value));

  ecma_integer_value_t integer_number = (ecma_integer_value_t) new_number;
  ecma_number_t *number_p = (ecma_number_t *) ecma_get_pointer_from_ecma_value (float_value);

  if ((ecma_number_t) integer_number == new_number
      && ((integer_number == 0) ? ecma_is_number_equal_to_positive_zero (new_number)
                                : ECMA_IS_INTEGER_NUMBER (integer_number)))
  {
    ecma_dealloc_number (number_p);
    return ecma_make_integer_value (integer_number);
  }

  *number_p = new_number;
  return float_value;
} /* ecma_update_float_number */

/**
 * Assign a float number to an ecma-value
 *
 * Note:
 *      value previously stored in the property is freed
 */
static void
ecma_value_assign_float_number (ecma_value_t *value_p, /**< [in, out] ecma value */
                                ecma_number_t ecma_number) /**< number to assign */
{
  if (ecma_is_value_float_number (*value_p))
  {
    ecma_number_t *num_dst_p = (ecma_number_t *) ecma_get_pointer_from_ecma_value (*value_p);

    *num_dst_p = ecma_number;
    return;
  }

  if (ecma_get_value_type_field (*value_p) != ECMA_TYPE_DIRECT
      && ecma_get_value_type_field (*value_p) != ECMA_TYPE_OBJECT)
  {
    ecma_free_value (*value_p);
  }

  *value_p = ecma_create_float_number (ecma_number);
} /* ecma_value_assign_float_number */

/**
 * Assign a number to an ecma-value
 *
 * Note:
 *      value previously stored in the property is freed
 */
void
ecma_value_assign_number (ecma_value_t *value_p, /**< [in, out] ecma value */
                          ecma_number_t ecma_number) /**< number to assign */
{
  ecma_integer_value_t integer_value = (ecma_integer_value_t) ecma_number;

  if ((ecma_number_t) integer_value == ecma_number
      && ((integer_value == 0) ? ecma_is_number_equal_to_positive_zero (ecma_number)
                               : ECMA_IS_INTEGER_NUMBER (integer_value)))
  {
    if (ecma_get_value_type_field (*value_p) != ECMA_TYPE_DIRECT
        && ecma_get_value_type_field (*value_p) != ECMA_TYPE_OBJECT)
    {
      ecma_free_value (*value_p);
    }
    *value_p = ecma_make_integer_value (integer_value);
    return;
  }

  ecma_value_assign_float_number (value_p, ecma_number);
} /* ecma_value_assign_number */

/**
 * Free the ecma value
 */
void
ecma_free_value (ecma_value_t value) /**< value description */
{
  switch (ecma_get_value_type_field (value))
  {
    case ECMA_TYPE_FLOAT:
    {
      ecma_number_t *number_p = (ecma_number_t *) ecma_get_pointer_from_ecma_value (value);
      ecma_dealloc_number (number_p);
      break;
    }

    case ECMA_TYPE_STRING:
    {
      ecma_string_t *string_p = ecma_get_string_from_value (value);
      ecma_deref_ecma_string (string_p);
      break;
    }
#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
    case ECMA_TYPE_SYMBOL:
    {
      ecma_deref_ecma_string (ecma_get_symbol_from_value (value));
      break;
    }
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */
    case ECMA_TYPE_OBJECT:
    {
      ecma_deref_object (ecma_get_object_from_value (value));
      break;
    }

    default:
    {
      JERRY_ASSERT (ecma_get_value_type_field (value) == ECMA_TYPE_DIRECT
                    || ecma_get_value_type_field (value) == ECMA_TYPE_DIRECT_STRING);

      /* no memory is allocated */
      break;
    }
  }
} /* ecma_free_value */

/**
 * Free the ecma value
 *
 * Note:
 *   this function is similar to ecma_free_value, but it is
 *   faster for direct values since no function call is performed.
 *   It also increases the binary size so it is recommended for
 *   critical code paths only.
 */
inline void JERRY_ATTR_ALWAYS_INLINE
ecma_fast_free_value (ecma_value_t value) /**< value description */
{
  if (ecma_get_value_type_field (value) != ECMA_TYPE_DIRECT)
  {
    ecma_free_value (value);
  }
} /* ecma_fast_free_value */

/**
 * Free the ecma value if not an object
 */
void
ecma_free_value_if_not_object (ecma_value_t value) /**< value description */
{
  if (ecma_get_value_type_field (value) != ECMA_TYPE_OBJECT)
  {
    ecma_free_value (value);
  }
} /* ecma_free_value_if_not_object */

/**
 * Free an ecma-value number
 */
inline void JERRY_ATTR_ALWAYS_INLINE
ecma_free_number (ecma_value_t value) /**< value description */
{
  JERRY_ASSERT (ecma_is_value_number (value));

  if (ecma_is_value_float_number (value))
  {
    ecma_number_t *number_p = (ecma_number_t *) ecma_get_pointer_from_ecma_value (value);
    ecma_dealloc_number (number_p);
  }
} /* ecma_free_number */

/**
 * Get the literal id associated with the given ecma_value type.
 * This operation is equivalent to the JavaScript 'typeof' operator.
 *
 * @returns one of the following value:
 *          - LIT_MAGIC_STRING_UNDEFINED
 *          - LIT_MAGIC_STRING_OBJECT
 *          - LIT_MAGIC_STRING_BOOLEAN
 *          - LIT_MAGIC_STRING_NUMBER
 *          - LIT_MAGIC_STRING_STRING
 *          - LIT_MAGIC_STRING_FUNCTION
 */
lit_magic_string_id_t
ecma_get_typeof_lit_id (ecma_value_t value) /**< input ecma value */
{
  lit_magic_string_id_t ret_value = LIT_MAGIC_STRING__EMPTY;

  if (ecma_is_value_undefined (value))
  {
    ret_value = LIT_MAGIC_STRING_UNDEFINED;
  }
  else if (ecma_is_value_null (value))
  {
    ret_value = LIT_MAGIC_STRING_OBJECT;
  }
  else if (ecma_is_value_boolean (value))
  {
    ret_value = LIT_MAGIC_STRING_BOOLEAN;
  }
  else if (ecma_is_value_number (value))
  {
    ret_value = LIT_MAGIC_STRING_NUMBER;
  }
  else if (ecma_is_value_string (value))
  {
    ret_value = LIT_MAGIC_STRING_STRING;
  }
#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
  else if (ecma_is_value_symbol (value))
  {
    ret_value = LIT_MAGIC_STRING_SYMBOL;
  }
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */
  else
  {
    JERRY_ASSERT (ecma_is_value_object (value));

    ret_value = ecma_op_is_callable (value) ? LIT_MAGIC_STRING_FUNCTION : LIT_MAGIC_STRING_OBJECT;
  }

  JERRY_ASSERT (ret_value != LIT_MAGIC_STRING__EMPTY);

  return ret_value;
} /* ecma_get_typeof_lit_id */

/**
 * @}
 * @}
 */
