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

#include "ecma-builtin-helpers.h"
#include "ecma-builtin-typedarray-helpers.h"
#include "ecma-builtins.h"
#include "ecma-exceptions.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects.h"
#include "ecma-conversion.h"
#include "ecma-function-object.h"
#include "ecma-typedarray-object.h"
#include "ecma-arraybuffer-object.h"
#include "ecma-try-catch-macro.h"
#include "jrt.h"
#include "jrt-libc-includes.h"
#include "ecma-gc.h"
#include "jmem.h"

#ifndef CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

#define BUILTIN_INC_HEADER_NAME "ecma-builtin-typedarray-prototype.inc.h"
#define BUILTIN_UNDERSCORED_ID typedarray_prototype
#include "ecma-builtin-internal-routines-template.inc.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltins
 * @{
 *
 * \addtogroup typedarrayprototype ECMA %TypedArray%.prototype object built-in
 * @{
 */

/**
 * The %TypedArray%.prototype.buffer accessor
 *
 * See also:
 *          ES2015, 22.2.3.1
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_buffer_getter (ecma_value_t this_arg) /**< this argument */
{
  if (ecma_is_typedarray (this_arg))
  {
    ecma_object_t *typedarray_p = ecma_get_object_from_value (this_arg);
    ecma_object_t *obj_p = ecma_typedarray_get_arraybuffer (typedarray_p);
    ecma_ref_object (obj_p);

    return ecma_make_object_value (obj_p);
  }

  return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a TypedArray."));
} /* ecma_builtin_typedarray_prototype_buffer_getter */

/**
 * The %TypedArray%.prototype.byteLength accessor
 *
 * See also:
 *          ES2015, 22.2.3.2
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_bytelength_getter (ecma_value_t this_arg) /**< this argument */
{
  if (ecma_is_typedarray (this_arg))
  {
    ecma_object_t *typedarray_p = ecma_get_object_from_value (this_arg);
    uint8_t shift = ecma_typedarray_get_element_size_shift (typedarray_p);

    return ecma_make_uint32_value (ecma_typedarray_get_length (typedarray_p) << shift);
  }

  return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a TypedArray."));
} /* ecma_builtin_typedarray_prototype_bytelength_getter */

/**
 * The %TypedArray%.prototype.byteOffset accessor
 *
 * See also:
 *          ES2015, 22.2.3.3
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_byteoffset_getter (ecma_value_t this_arg) /**< this argument */
{
  if (ecma_is_typedarray (this_arg))
  {
    ecma_object_t *typedarray_p = ecma_get_object_from_value (this_arg);

    return ecma_make_uint32_value (ecma_typedarray_get_offset (typedarray_p));
  }

  return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a TypedArray."));
} /* ecma_builtin_typedarray_prototype_byteoffset_getter */

/**
 * The %TypedArray%.prototype.length accessor
 *
 * See also:
 *          ES2015, 22.2.3.17
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_length_getter (ecma_value_t this_arg) /**< this argument */
{
  if (ecma_is_typedarray (this_arg))
  {
    ecma_object_t *typedarray_p = ecma_get_object_from_value (this_arg);

    return ecma_make_uint32_value (ecma_typedarray_get_length (typedarray_p));
  }

  return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a TypedArray."));
} /* ecma_builtin_typedarray_prototype_length_getter */

#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
/**
 * The %TypedArray%.prototype[Symbol.toStringTag] accessor
 *
 * See also:
 *          ES2015, 22.2.3.31
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_to_string_tag_getter (ecma_value_t this_arg) /**< this argument */
{
  if (!ecma_is_typedarray (this_arg))
  {
    return ECMA_VALUE_UNDEFINED;
  }

  return ecma_make_magic_string_value (ecma_object_get_class_name (ecma_get_object_from_value (this_arg)));
} /* ecma_builtin_typedarray_prototype_to_string_tag_getter */
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */

/**
 * Type of routine.
 */
typedef enum
{
  TYPEDARRAY_ROUTINE_EVERY, /**< routine: every ES2015, 22.2.3.7 */
  TYPEDARRAY_ROUTINE_SOME, /**< routine: some ES2015, 22.2.3.9 */
  TYPEDARRAY_ROUTINE_FOREACH, /**< routine: forEach ES2015, 15.4.4.18 */
  TYPEDARRAY_ROUTINE__COUNT /**< count of the modes */
} typedarray_routine_mode;

/**
 * The common function for 'every', 'some' and 'forEach'
 * because they have a similar structure.
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_exec_routine (ecma_value_t this_arg, /**< this argument */
                                                ecma_value_t cb_func_val, /**< callback function */
                                                ecma_value_t cb_this_arg, /**< 'this' of the callback function */
                                                typedarray_routine_mode mode) /**< mode: which routine */
{
  JERRY_ASSERT (mode < TYPEDARRAY_ROUTINE__COUNT);

  if (!ecma_is_typedarray (this_arg))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a TypedArray."));
  }

  if (!ecma_op_is_callable (cb_func_val))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Callback function is not callable."));
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (this_arg);
  uint32_t len = ecma_typedarray_get_length (obj_p);
  ecma_object_t *func_object_p = ecma_get_object_from_value (cb_func_val);
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  for (uint32_t index = 0; index < len && ecma_is_value_empty (ret_value); index++)
  {
    ecma_value_t current_index =  ecma_make_uint32_value (index);
    ecma_value_t get_value = ecma_op_typedarray_get_index_prop (obj_p, index);

    JERRY_ASSERT (ecma_is_value_number (get_value));

    ecma_value_t call_args[] = { get_value, current_index, this_arg };

    ECMA_TRY_CATCH (call_value, ecma_op_function_call (func_object_p, cb_this_arg, call_args, 3), ret_value);

    if (mode == TYPEDARRAY_ROUTINE_EVERY)
    {
      if (!ecma_op_to_boolean (call_value))
      {
        ret_value = ECMA_VALUE_FALSE;
      }
    }
    else if (mode == TYPEDARRAY_ROUTINE_SOME
             && ecma_op_to_boolean (call_value))
    {
      ret_value = ECMA_VALUE_TRUE;
    }

    ECMA_FINALIZE (call_value);

    ecma_fast_free_value (current_index);
    ecma_fast_free_value (get_value);
  }

  if (ecma_is_value_empty (ret_value))
  {
    if (mode == TYPEDARRAY_ROUTINE_EVERY)
    {
      ret_value = ECMA_VALUE_TRUE;
    }
    else if (mode == TYPEDARRAY_ROUTINE_SOME)
    {
      ret_value = ECMA_VALUE_FALSE;
    }
    else
    {
      ret_value = ECMA_VALUE_UNDEFINED;
    }
  }

  return ret_value;
} /* ecma_builtin_typedarray_prototype_exec_routine */

/**
 * The %TypedArray%.prototype object's 'every' routine
 *
 * See also:
 *          ES2015, 22.2.3.7
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_every (ecma_value_t this_arg, /**< this argument */
                                         ecma_value_t cb_func_val, /**< callback function */
                                         ecma_value_t cb_this_arg) /**< this' of the callback function */
{
  return ecma_builtin_typedarray_prototype_exec_routine (this_arg,
                                                         cb_func_val,
                                                         cb_this_arg,
                                                         TYPEDARRAY_ROUTINE_EVERY);
} /* ecma_builtin_typedarray_prototype_every */

/**
 * The %TypedArray%.prototype object's 'some' routine
 *
 * See also:
 *          ES2015, 22.2.3.9
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_some (ecma_value_t this_arg, /**< this argument */
                                        ecma_value_t cb_func_val, /**< callback function */
                                        ecma_value_t cb_this_arg) /**< this' of the callback function */
{
  return ecma_builtin_typedarray_prototype_exec_routine (this_arg,
                                                         cb_func_val,
                                                         cb_this_arg,
                                                         TYPEDARRAY_ROUTINE_SOME);
} /* ecma_builtin_typedarray_prototype_some */

/**
 * The %TypedArray%.prototype object's 'forEach' routine
 *
 * See also:
 *          ES2015, 15.4.4.18
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_for_each (ecma_value_t this_arg, /**< this argument */
                                            ecma_value_t cb_func_val, /**< callback function */
                                            ecma_value_t cb_this_arg) /**< this' of the callback function */
{
  return ecma_builtin_typedarray_prototype_exec_routine (this_arg,
                                                         cb_func_val,
                                                         cb_this_arg,
                                                         TYPEDARRAY_ROUTINE_FOREACH);
} /* ecma_builtin_typedarray_prototype_for_each */

/**
 * The %TypedArray%.prototype object's 'map' routine
 *
 * See also:
 *          ES2015, 22.2.3.8
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_map (ecma_value_t this_arg, /**< this argument */
                                       ecma_value_t cb_func_val, /**< callback function */
                                       ecma_value_t cb_this_arg) /**< this' of the callback function */
{
  if (!ecma_is_typedarray (this_arg))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a TypedArray."));
  }

  if (!ecma_op_is_callable (cb_func_val))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Callback function is not callable."));
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (this_arg);
  uint32_t len = ecma_typedarray_get_length (obj_p);
  ecma_object_t *func_object_p = ecma_get_object_from_value (cb_func_val);
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  ecma_value_t new_typedarray = ecma_op_create_typedarray_with_type_and_length (obj_p, len);

  if (ECMA_IS_VALUE_ERROR (new_typedarray))
  {
    return new_typedarray;
  }

  ecma_object_t *new_obj_p = ecma_get_object_from_value (new_typedarray);

  for (uint32_t index = 0; index < len && ecma_is_value_empty (ret_value); index++)
  {
    ecma_value_t current_index =  ecma_make_uint32_value (index);
    ecma_value_t get_value = ecma_op_typedarray_get_index_prop (obj_p, index);
    ecma_value_t call_args[] = { get_value, current_index, this_arg };

    ECMA_TRY_CATCH (mapped_value, ecma_op_function_call (func_object_p, cb_this_arg, call_args, 3), ret_value);

    bool set_status = ecma_op_typedarray_set_index_prop (new_obj_p, index, mapped_value);

    if (!set_status)
    {
      ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("error in typedarray set"));
    }

    ECMA_FINALIZE (mapped_value);

    ecma_fast_free_value (current_index);
    ecma_fast_free_value (get_value);
  }

  if (ecma_is_value_empty (ret_value))
  {
    ret_value = new_typedarray;
  }
  else
  {
    ecma_free_value (new_typedarray);
  }

  return ret_value;
} /* ecma_builtin_typedarray_prototype_map */

/**
 * Reduce and reduceRight routines share a similar structure.
 * And we use 'is_right' to distinguish between them.
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_reduce_with_direction (ecma_value_t this_arg, /**< this argument */
                                                         ecma_value_t cb_func_val, /**< callback function */
                                                         ecma_value_t initial_val, /**< initial value */
                                                         bool is_right) /**< choose order, true is reduceRight */
{
  if (!ecma_is_typedarray (this_arg))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a TypedArray."));
  }

  if (!ecma_op_is_callable (cb_func_val))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Callback function is not callable."));
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (this_arg);
  uint32_t len = ecma_typedarray_get_length (obj_p);

  if (len == 0)
  {
    if (ecma_is_value_undefined (initial_val))
    {
      return ecma_raise_type_error (ECMA_ERR_MSG ("Initial value cannot be undefined."));
    }

    return ecma_copy_value (initial_val);
  }

  JERRY_ASSERT (len > 0);

  ecma_value_t accumulator = ECMA_VALUE_UNDEFINED;
  uint32_t index = is_right ? (len - 1) : 0;

  if (ecma_is_value_undefined (initial_val))
  {
    accumulator = ecma_op_typedarray_get_index_prop (obj_p, index);

    JERRY_ASSERT (ecma_is_value_number (accumulator));

    if (is_right)
    {
      if (index == 0)
      {
        return accumulator;
      }

      index--;
    }
    else
    {
      index++;

      if (index == len)
      {
        return accumulator;
      }
    }
  }
  else
  {
    accumulator = ecma_copy_value (initial_val);
  }

  ecma_object_t *func_object_p = ecma_get_object_from_value (cb_func_val);

  while (true)
  {
    ecma_value_t current_index =  ecma_make_uint32_value (index);
    ecma_value_t get_value = ecma_op_typedarray_get_index_prop (obj_p, index);
    ecma_value_t call_args[] = { accumulator, get_value, current_index, this_arg };

    JERRY_ASSERT (ecma_is_value_number (get_value));

    ecma_value_t call_value = ecma_op_function_call (func_object_p,
                                                     ECMA_VALUE_UNDEFINED,
                                                     call_args,
                                                     4);

    ecma_fast_free_value (accumulator);
    ecma_fast_free_value (get_value);
    ecma_fast_free_value (current_index);

    if (ECMA_IS_VALUE_ERROR (call_value))
    {
      return call_value;
    }

    accumulator = call_value;

    if (is_right)
    {
      if (index == 0)
      {
        break;
      }

      index--;
    }
    else
    {
      index++;

      if (index == len)
      {
        break;
      }
    }
  }

  return accumulator;
} /* ecma_builtin_typedarray_prototype_reduce_with_direction */

/**
 * The %TypedArray%.prototype object's 'reduce' routine
 *
 * See also:
 *          ES2015, 22.2.3.19
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_reduce (ecma_value_t this_arg, /**< this argument */
                                          ecma_value_t cb_func_val, /**< callback function */
                                          ecma_value_t initial_val) /**< initial value */
{
  return ecma_builtin_typedarray_prototype_reduce_with_direction (this_arg,
                                                                  cb_func_val,
                                                                  initial_val,
                                                                  false);
} /* ecma_builtin_typedarray_prototype_reduce */

/**
 * The %TypedArray%.prototype object's 'reduceRight' routine
 *
 * See also:
 *          ES2015, 22.2.3.20
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_reduce_right (ecma_value_t this_arg, /**< this argument */
                                                ecma_value_t cb_func_val, /**< callback function */
                                                ecma_value_t initial_val) /**< initial value */
{
  return ecma_builtin_typedarray_prototype_reduce_with_direction (this_arg,
                                                                  cb_func_val,
                                                                  initial_val,
                                                                  true);
} /* ecma_builtin_typedarray_prototype_reduce_right */

/**
 * The %TypedArray%.prototype object's 'filter' routine
 *
 * See also:
 *          ES2015, 22.2.3.9
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_filter (ecma_value_t this_arg, /**< this argument */
                                          ecma_value_t cb_func_val, /**< callback function */
                                          ecma_value_t cb_this_arg) /**< 'this' of the callback function */
{
  if (!ecma_is_typedarray (this_arg))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a TypedArray."));
  }

  if (!ecma_op_is_callable (cb_func_val))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Callback function is not callable."));
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (this_arg);
  uint32_t len = ecma_typedarray_get_length (obj_p);
  lit_utf8_byte_t *buffer_p = ecma_typedarray_get_buffer (obj_p);
  uint8_t shift = ecma_typedarray_get_element_size_shift (obj_p);
  uint8_t element_size = (uint8_t) (1 << shift);
  ecma_object_t *func_object_p = ecma_get_object_from_value (cb_func_val);
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  if (len == 0)
  {
    return ecma_op_create_typedarray_with_type_and_length (obj_p, 0);
  }

  JMEM_DEFINE_LOCAL_ARRAY (pass_value_list_p, len * element_size, lit_utf8_byte_t);

  lit_utf8_byte_t *pass_value_p = pass_value_list_p;

  for (uint32_t index = 0; index < len && ecma_is_value_empty (ret_value); index++)
  {
    ecma_value_t current_index = ecma_make_uint32_value (index);
    ecma_value_t get_value = ecma_op_typedarray_get_index_prop (obj_p, index);

    JERRY_ASSERT (ecma_is_value_number (get_value));

    ecma_value_t call_args[] = { get_value, current_index, this_arg };

    ECMA_TRY_CATCH (call_value, ecma_op_function_call (func_object_p, cb_this_arg, call_args, 3), ret_value);

    if (ecma_op_to_boolean (call_value))
    {
      memcpy (pass_value_p, buffer_p, element_size);
      pass_value_p += element_size;
    }

    buffer_p += element_size;

    ECMA_FINALIZE (call_value);

    ecma_fast_free_value (current_index);
    ecma_fast_free_value (get_value);
  }

  if (ecma_is_value_empty (ret_value))
  {
    uint32_t pass_num = (uint32_t) ((pass_value_p - pass_value_list_p) >> shift);

    ret_value = ecma_op_create_typedarray_with_type_and_length (obj_p, pass_num);

    if (!ECMA_IS_VALUE_ERROR (ret_value))
    {
      obj_p = ecma_get_object_from_value (ret_value);

      JERRY_ASSERT (ecma_typedarray_get_offset (obj_p) == 0);

      memcpy (ecma_typedarray_get_buffer (obj_p),
              pass_value_list_p,
              (size_t) (pass_value_p - pass_value_list_p));
    }
  }

  JMEM_FINALIZE_LOCAL_ARRAY (pass_value_list_p);

  return ret_value;
} /* ecma_builtin_typedarray_prototype_filter */

/**
 * The %TypedArray%.prototype object's 'reverse' routine
 *
 * See also:
 *          ES2015, 22.2.3.21
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_builtin_typedarray_prototype_reverse (ecma_value_t this_arg) /**< this argument */
{
  if (!ecma_is_typedarray (this_arg))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a TypedArray."));
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (this_arg);
  uint32_t len = ecma_typedarray_get_length (obj_p);
  lit_utf8_byte_t *buffer_p = ecma_typedarray_get_buffer (obj_p);
  uint8_t shift = ecma_typedarray_get_element_size_shift (obj_p);
  uint8_t element_size = (uint8_t) (1 << shift);
  uint32_t middle = (len / 2) << shift;
  uint32_t buffer_last = (len << shift) - element_size;

  for (uint32_t lower = 0; lower < middle; lower += element_size)
  {
    uint32_t upper = buffer_last - lower;
    lit_utf8_byte_t *lower_p = buffer_p + lower;
    lit_utf8_byte_t *upper_p = buffer_p + upper;

    lit_utf8_byte_t tmp[8];
    memcpy (&tmp[0], lower_p, element_size);
    memcpy (lower_p, upper_p, element_size);
    memcpy (upper_p, &tmp[0], element_size);
  }

  return ecma_copy_value (this_arg);
} /* ecma_builtin_typedarray_prototype_reverse */

/**
 * The %TypedArray%.prototype object's 'set' routine for a typedArray source
 *
 * See also:
 *          ES2015, 22.2.3.22, 22.2.3.22.2
 *
 * @return ecma value of undefined if success, error otherwise.
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_op_typedarray_set_with_typedarray (ecma_value_t this_arg, /**< this argument */
                                        ecma_value_t arr_val, /**< typedarray object */
                                        ecma_value_t offset_val) /**< offset value */
{
  /* 6.~ 8. targetOffset */
  ecma_number_t target_offset_num;
  if (!ecma_is_value_empty (ecma_get_number (offset_val, &target_offset_num)))
  {
    return ecma_raise_range_error (ECMA_ERR_MSG ("Invalid offset"));
  }

  if (ecma_number_is_nan (target_offset_num))
  {
    target_offset_num = 0;
  }

  if (target_offset_num <= -1.0 || target_offset_num >= (ecma_number_t) UINT32_MAX + 0.5)
  {
    return ecma_raise_range_error (ECMA_ERR_MSG ("Invalid offset"));
  }

  ecma_object_t *target_typedarray_p = ecma_get_object_from_value (this_arg);
  ecma_object_t *src_typedarray_p = ecma_get_object_from_value (arr_val);

  /* 9. targetBuffer */
  ecma_object_t *target_arraybuffer_p = ecma_typedarray_get_arraybuffer (target_typedarray_p);
  lit_utf8_byte_t *target_buffer_p = ecma_typedarray_get_buffer (target_typedarray_p);

  /* 11. targetLength */
  ecma_length_t target_length = ecma_typedarray_get_length (target_typedarray_p);

  /* 12. srcBuffer */
  ecma_object_t *src_arraybuffer_p = ecma_typedarray_get_arraybuffer (src_typedarray_p);
  lit_utf8_byte_t *src_buffer_p = ecma_typedarray_get_buffer (src_typedarray_p);

  /* 15. targetType */
  lit_magic_string_id_t target_class_id = ecma_object_get_class_name (target_typedarray_p);

  /* 16. targetElementSize */
  uint8_t target_shift = ecma_typedarray_get_element_size_shift (target_typedarray_p);
  uint8_t target_element_size = (uint8_t) (1 << target_shift);

  /* 17. targetByteOffset */
  ecma_length_t target_byte_offset = ecma_typedarray_get_offset (target_typedarray_p);

  /* 19. srcType */
  lit_magic_string_id_t src_class_id = ecma_object_get_class_name (src_typedarray_p);

  /* 20. srcElementSize */
  uint8_t src_shift = ecma_typedarray_get_element_size_shift (src_typedarray_p);
  uint8_t src_element_size = (uint8_t) (1 << src_shift);

  /* 21. srcLength */
  ecma_length_t src_length = ecma_typedarray_get_length (src_typedarray_p);
  uint32_t src_length_uint32 = ecma_number_to_uint32 (src_length);

  if ((ecma_number_t) src_length_uint32 != src_length)
  {
    return ecma_raise_range_error (ECMA_ERR_MSG ("Invalid source length"));
  }

  /* 22. srcByteOffset */
  ecma_length_t src_byte_offset = ecma_typedarray_get_offset (src_typedarray_p);

  /* 23. */
  uint32_t target_offset_uint32 = ecma_number_to_uint32 (target_offset_num);

  if ((int64_t) src_length_uint32 + target_offset_uint32 > target_length)
  {
    return ecma_raise_range_error (ECMA_ERR_MSG ("Invalid range of index"));
  }

  /* 24.d, 25. srcByteIndex */
  ecma_length_t src_byte_index = 0;

  if (src_arraybuffer_p != target_arraybuffer_p)
  {
    src_byte_index = src_byte_offset;
  }

  /* 26. targetByteIndex */
  uint32_t target_byte_index = target_offset_uint32 * target_element_size + target_byte_offset;

  /* 27. limit */
  uint32_t limit = target_byte_index + target_element_size * src_length_uint32;

  if (src_class_id == target_class_id)
  {
    memmove (target_buffer_p + target_byte_index, src_buffer_p + src_byte_index,
             target_element_size * src_length_uint32);
  }
  else
  {
    while (target_byte_index < limit)
    {
      ecma_number_t elem_num = ecma_get_typedarray_element (src_buffer_p + src_byte_index, src_class_id);
      ecma_set_typedarray_element (target_buffer_p + target_byte_index, elem_num, target_class_id);
      src_byte_index += src_element_size;
      target_byte_index += target_element_size;
    }
  }

  return ECMA_VALUE_UNDEFINED;
} /* ecma_op_typedarray_set_with_typedarray */

/**
 * The %TypedArray%.prototype object's 'set' routine
 *
 * See also:
 *          ES2015, 22.2.3.22, 22.2.3.22.1
 *
 * @return ecma value of undefined if success, error otherwise.
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_builtin_typedarray_prototype_set (ecma_value_t this_arg, /**< this argument */
                                       ecma_value_t arr_val, /**< array object */
                                       ecma_value_t offset_val) /**< offset value */
{
  /* 2.~ 4. */
  if (!ecma_is_typedarray (this_arg))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a TypedArray."));
  }

  /* 1. */
  if (ecma_is_typedarray (arr_val))
  {
    /* 22.2.3.22.2 */
    return ecma_op_typedarray_set_with_typedarray (this_arg, arr_val, offset_val);
  }

  /* 6.~ 8. targetOffset */
  ecma_value_t ret_val = ECMA_VALUE_EMPTY;
  ECMA_OP_TO_NUMBER_TRY_CATCH (target_offset_num, offset_val, ret_val);
  if (ecma_number_is_nan (target_offset_num))
  {
    target_offset_num = 0;
  }
  if (target_offset_num <= -1.0 || target_offset_num >= (ecma_number_t) UINT32_MAX + 0.5)
  {
    return ecma_raise_range_error (ECMA_ERR_MSG ("Invalid offset"));
  }
  uint32_t target_offset_uint32 = ecma_number_to_uint32 (target_offset_num);

  /* 11. targetLength */
  ecma_object_t *typedarray_p = ecma_get_object_from_value (this_arg);
  ecma_length_t target_length = ecma_typedarray_get_length (typedarray_p);

  /* 13. targetElementSize */
  uint8_t shift = ecma_typedarray_get_element_size_shift (typedarray_p);
  uint8_t element_size = (uint8_t) (1 << shift);

  /* 14. targetType */
  lit_magic_string_id_t target_class_id = ecma_object_get_class_name (typedarray_p);

  /* 9., 15. */
  lit_utf8_byte_t *target_buffer_p = ecma_typedarray_get_buffer (typedarray_p);

  /* 16.~ 17. */
  ECMA_TRY_CATCH (source_obj, ecma_op_to_object (arr_val), ret_val);

  /* 18.~ 19. */
  ecma_object_t *source_obj_p = ecma_get_object_from_value (source_obj);

  ECMA_TRY_CATCH (source_length,
                  ecma_op_object_get_by_magic_id (source_obj_p, LIT_MAGIC_STRING_LENGTH),
                  ret_val);

  ECMA_OP_TO_NUMBER_TRY_CATCH (source_length_num, source_length, ret_val);

  if (ecma_number_is_nan (source_length_num) || source_length_num <= 0)
  {
    source_length_num = 0;
  }

  uint32_t source_length_uint32 = ecma_number_to_uint32 (source_length_num);

  if ((ecma_number_t) source_length_uint32 != source_length_num)
  {
    return ecma_raise_range_error (ECMA_ERR_MSG ("Invalid source length"));
  }

  /* 20. if srcLength + targetOffset > targetLength, throw a RangeError */
  if ((int64_t) source_length_uint32 + target_offset_uint32 > target_length)
  {
    ret_val = ecma_raise_range_error (ECMA_ERR_MSG ("Invalid range of index"));
  }

  /* 21.~ 25. */
  uint32_t target_byte_index = target_offset_uint32 * element_size;
  uint32_t k = 0;

  while (k < source_length_uint32 && ecma_is_value_empty (ret_val))
  {
    ecma_string_t *k_str_p = ecma_new_ecma_string_from_uint32 (k);

    ECMA_TRY_CATCH (elem,
                    ecma_op_object_get (source_obj_p, k_str_p),
                    ret_val);

    ECMA_OP_TO_NUMBER_TRY_CATCH (elem_num, elem, ret_val);

    ecma_set_typedarray_element (target_buffer_p + target_byte_index, elem_num, target_class_id);

    ECMA_OP_TO_NUMBER_FINALIZE (elem_num);
    ECMA_FINALIZE (elem);

    ecma_deref_ecma_string (k_str_p);

    k++;
    target_byte_index += element_size;
  }

  ECMA_OP_TO_NUMBER_FINALIZE (source_length_num);
  ECMA_FINALIZE (source_length);
  ECMA_FINALIZE (source_obj);
  ECMA_OP_TO_NUMBER_FINALIZE (target_offset_num);

  if (ecma_is_value_empty (ret_val))
  {
    ret_val = ECMA_VALUE_UNDEFINED;
  }
  return ret_val;
} /* ecma_builtin_typedarray_prototype_set */

/**
 * TypedArray.prototype's 'toString' single element operation routine based
 * on the Array.prototype's 'toString' single element operation routine
 *
 * See also:
 *          ECMA-262 v5.1, 15.4.4.2
 *
 * @return ecma_value_t value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_op_typedarray_get_to_string_at_index (ecma_object_t *obj_p, /**< this object */
                                           uint32_t index) /**< array index */
{
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;
  ecma_string_t *index_string_p = ecma_new_ecma_string_from_uint32 (index);
  ecma_value_t index_value = ecma_op_object_get (obj_p, index_string_p);
  ecma_deref_ecma_string (index_string_p);

  if (ECMA_IS_VALUE_ERROR (index_value))
  {
    return index_value;
  }

  if (ecma_is_value_undefined (index_value)
      || ecma_is_value_null (index_value))
  {
    ret_value = ecma_make_magic_string_value (LIT_MAGIC_STRING__EMPTY);
  }
  else
  {
    ret_value = ecma_op_to_string (index_value);
  }

  ecma_free_value (index_value);
  return ret_value;
} /* ecma_op_typedarray_get_to_string_at_index */

/**
 * The TypedArray.prototype.toString's separator creation routine based on
 * the Array.prototype.toString's separator routine
 *
 * See also:
 *          ECMA-262 v5.1, 15.4.4.2 4th step
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_op_typedarray_get_separator_string (ecma_value_t separator) /**< possible separator */
{
  if (ecma_is_value_undefined (separator))
  {
    return ecma_make_magic_string_value (LIT_MAGIC_STRING_COMMA_CHAR);
  }

  return ecma_op_to_string (separator);
} /* ecma_op_typedarray_get_separator_string */

/**
 * The TypedArray.prototype object's 'join' routine basen on
 * the Array.porottype object's 'join'
 *
 * See also:
 *          ECMA-262 v5, 15.4.4.5
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_join (ecma_value_t this_arg, /**< this argument */
                                        ecma_value_t separator_arg) /**< separator argument */
{
  /* 1. */
  ecma_value_t obj_value = ecma_op_to_object (this_arg);

  if (ECMA_IS_VALUE_ERROR (obj_value))
  {
    return obj_value;
  }
  ecma_object_t *obj_p = ecma_get_object_from_value (obj_value);

  /* 2. */
  ecma_value_t length_value = ecma_op_object_get_by_magic_id (obj_p, LIT_MAGIC_STRING_LENGTH);

  if (ECMA_IS_VALUE_ERROR (length_value))
  {
    ecma_free_value (obj_value);
    return length_value;
  }

  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  ECMA_OP_TO_NUMBER_TRY_CATCH (length_number,
                               length_value,
                               ret_value);

  /* 3. */
  uint32_t length = ecma_number_to_uint32 (length_number);
  /* 4-5. */
  ecma_value_t separator_value = ecma_op_typedarray_get_separator_string (separator_arg);
  if (ECMA_IS_VALUE_ERROR (separator_value))
  {
    ecma_free_value (length_value);
    ecma_free_value (obj_value);
    return separator_value;
  }

  if (length == 0)
  {
    /* 6. */
    ret_value = ecma_make_magic_string_value (LIT_MAGIC_STRING__EMPTY);
  }
  else
  {
    ecma_string_t *separator_string_p = ecma_get_string_from_value (separator_value);

    /* 7-8. */
    ecma_value_t first_value = ecma_op_typedarray_get_to_string_at_index (obj_p, 0);
    if (ECMA_IS_VALUE_ERROR (first_value))
    {
      ecma_free_value (separator_value);
      ecma_free_value (length_value);
      ecma_free_value (obj_value);
      return first_value;
    }

    ecma_string_t *return_string_p = ecma_get_string_from_value (first_value);
    ecma_ref_ecma_string (return_string_p);
    if (ecma_is_value_empty (ret_value))
    {
      /* 9-10. */
      for (uint32_t k = 1; k < length; k++)
      {
        /* 10.a */
        return_string_p = ecma_concat_ecma_strings (return_string_p, separator_string_p);

       /* 10.b, 10.c */
        ecma_value_t next_string_value = ecma_op_typedarray_get_to_string_at_index (obj_p, k);
        if (ECMA_IS_VALUE_ERROR (next_string_value))
        {
          ecma_deref_ecma_string (return_string_p);
          ecma_free_value (first_value);
          ecma_free_value (separator_value);
          ecma_free_value (length_value);
          ecma_free_value (obj_value);
          return next_string_value;
        }

        /* 10.d */
        ecma_string_t *next_string_p = ecma_get_string_from_value (next_string_value);
        return_string_p = ecma_concat_ecma_strings (return_string_p, next_string_p);

        ecma_free_value (next_string_value);
      }
      ret_value = ecma_make_string_value (return_string_p);
    }
    else
    {
      ecma_deref_ecma_string (return_string_p);
    }

    ecma_free_value (first_value);
  }
  ecma_free_value (separator_value);

  ECMA_OP_TO_NUMBER_FINALIZE (length_number);
  ecma_free_value (length_value);
  ecma_free_value (obj_value);
  return ret_value;
} /* ecma_builtin_typedarray_prototype_join */

/**
 * The TypedArray.prototype object's 'toString' routine basen on
 * the Array.porottype object's 'toString'
 *
 * See also:
 *          ECMA-262 v5, 15.4.4.2
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_object_to_string (ecma_value_t this_arg) /**< this argument */
{
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  /* 1. */
  ecma_value_t obj_this_value = ecma_op_to_object (this_arg);
  if (ECMA_IS_VALUE_ERROR (obj_this_value))
  {
    return obj_this_value;
  }
  ecma_object_t *obj_p = ecma_get_object_from_value (obj_this_value);

  /* 2. */
  ecma_value_t join_value = ecma_op_object_get_by_magic_id (obj_p, LIT_MAGIC_STRING_JOIN);
  if (ECMA_IS_VALUE_ERROR (join_value))
  {
    ecma_free_value (obj_this_value);
    return join_value;
  }

  if (!ecma_op_is_callable (join_value))
  {
    /* 3. */
    ret_value = ecma_builtin_helper_object_to_string (this_arg);
  }
  else
  {
    /* 4. */
    ecma_object_t *join_func_obj_p = ecma_get_object_from_value (join_value);

    ret_value = ecma_op_function_call (join_func_obj_p, this_arg, NULL, 0);
  }

  ecma_free_value (join_value);
  ecma_free_value (obj_this_value);

  return ret_value;
} /* ecma_builtin_typedarray_prototype_object_to_string */

/**
 * The %TypedArray%.prototype object's 'subarray' routine.
 *
 * See also:
 *          ES2015, 22.2.3.26
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_subarray (ecma_value_t this_arg, /**< this argument */
                                            ecma_value_t begin, /**< begin */
                                            ecma_value_t end) /**< end */
{
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  /* 2.~ 4. */
  if (!ecma_is_typedarray (this_arg))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a TypedArray."));
  }

  ecma_object_t *src_typedarray_p = ecma_get_object_from_value (this_arg);

  /* 5. buffer */
  ecma_object_t *src_typedarray_arraybuffer_p = ecma_typedarray_get_arraybuffer (src_typedarray_p);

  /* 6. srcLength */
  ecma_length_t src_length = ecma_typedarray_get_length (src_typedarray_p);

  /* 9. beginIndex, 12. endIndex */
  uint32_t begin_index_uint32 = 0, end_index_uint32 = 0;

  /* 7. relativeBegin */
  ECMA_OP_TO_NUMBER_TRY_CATCH (relative_begin, begin, ret_value);
  begin_index_uint32 = ecma_builtin_helper_array_index_normalize (relative_begin, src_length);

  if (ecma_is_value_undefined (end))
  {
    end_index_uint32 = (uint32_t) src_length;
  }
  else
  {
    /* 10. relativeEnd */
    ECMA_OP_TO_NUMBER_TRY_CATCH (relative_end, end, ret_value);

    end_index_uint32 = ecma_builtin_helper_array_index_normalize (relative_end, src_length);

    ECMA_OP_TO_NUMBER_FINALIZE (relative_end);
  }

  ECMA_OP_TO_NUMBER_FINALIZE (relative_begin);

  if (!ecma_is_value_empty (ret_value))
  {
    return ret_value;
  }

  /* 13. newLength */
  ecma_length_t subarray_length = 0;

  if (end_index_uint32 > begin_index_uint32)
  {
    subarray_length = end_index_uint32 - begin_index_uint32;
  }

  /* 15. elementSize */
  uint8_t shift = ecma_typedarray_get_element_size_shift (src_typedarray_p);
  uint8_t element_size = (uint8_t) (1 << shift);

  /* 16. srcByteOffset */
  ecma_length_t src_byte_offset = ecma_typedarray_get_offset (src_typedarray_p);

  /* 17. beginByteOffset */
  ecma_length_t begin_byte_offset = src_byte_offset + begin_index_uint32 * element_size;

  uint8_t src_builtin_id = ecma_typedarray_helper_get_builtin_id (src_typedarray_p);
  ecma_value_t arguments_p[3] =
  {
    ecma_make_object_value (src_typedarray_arraybuffer_p),
    ecma_make_uint32_value (begin_byte_offset),
    ecma_make_uint32_value (subarray_length)
  };

  ret_value = ecma_typedarray_helper_dispatch_construct (arguments_p, 3, src_builtin_id);

  ecma_free_value (arguments_p[1]);
  ecma_free_value (arguments_p[2]);
  return ret_value;
} /* ecma_builtin_typedarray_prototype_subarray */

/**
 * The %TypedArray%.prototype object's 'fill' routine.
 *
 * See also:
 *          ES2015, 22.2.3.8, 22.1.3.6
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_fill (ecma_value_t this_arg, /**< this argument */
                                        ecma_value_t value, /**< value */
                                        ecma_value_t begin, /**< begin */
                                        ecma_value_t end) /**< end */
{
  if (!ecma_is_typedarray (this_arg))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a TypedArray."));
  }

  ecma_number_t value_num;
  ecma_value_t ret_value = ecma_get_number (value, &value_num);

  if (!ecma_is_value_empty (ret_value))
  {
    return ret_value;
  }

  ecma_object_t *typedarray_p = ecma_get_object_from_value (this_arg);
  ecma_object_t *typedarray_arraybuffer_p = ecma_typedarray_get_arraybuffer (typedarray_p);
  lit_utf8_byte_t *buffer_p = ecma_arraybuffer_get_buffer (typedarray_arraybuffer_p);
  ecma_length_t length = ecma_typedarray_get_length (typedarray_p);

  uint32_t begin_index_uint32 = 0, end_index_uint32 = 0;

  ECMA_OP_TO_NUMBER_TRY_CATCH (relative_begin, begin, ret_value);
  begin_index_uint32 = ecma_builtin_helper_array_index_normalize (relative_begin, length);

  if (ecma_is_value_undefined (end))
  {
    end_index_uint32 = (uint32_t) length;
  }
  else
  {
    ECMA_OP_TO_NUMBER_TRY_CATCH (relative_end, end, ret_value);

    end_index_uint32 = ecma_builtin_helper_array_index_normalize (relative_end, length);

    ECMA_OP_TO_NUMBER_FINALIZE (relative_end);
  }

  ECMA_OP_TO_NUMBER_FINALIZE (relative_begin);

  if (!ecma_is_value_empty (ret_value))
  {
    return ret_value;
  }

  ecma_length_t subarray_length = 0;

  if (end_index_uint32 > begin_index_uint32)
  {
    subarray_length = end_index_uint32 - begin_index_uint32;
  }

  uint8_t shift = ecma_typedarray_get_element_size_shift (typedarray_p);
  ecma_length_t byte_offset = ecma_typedarray_get_offset (typedarray_p);
  lit_magic_string_id_t class_id = ecma_object_get_class_name (typedarray_p);

  uint8_t element_size = (uint8_t) (1 << shift);
  uint32_t byte_index = byte_offset + begin_index_uint32 * element_size;
  uint32_t limit = byte_index + subarray_length * element_size;

  while (byte_index < limit)
  {
    ecma_set_typedarray_element (buffer_p + byte_index, value_num, class_id);
    byte_index += element_size;
  }

  return ecma_copy_value (this_arg);
} /* ecma_builtin_typedarray_prototype_fill */

/**
 * SortCompare abstract method
 *
 * See also:
 *          ECMA-262 v5, 15.4.4.11
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_sort_compare_helper (ecma_value_t lhs, /**< left value */
                                                       ecma_value_t rhs, /**< right value */
                                                       ecma_value_t compare_func) /**< compare function */
{
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;
  ecma_number_t result = ECMA_NUMBER_ZERO;

  if (ecma_is_value_undefined (compare_func))
  {
    /* Default comparison when no comparefn is passed. */
    double lhs_value = (double) ecma_get_number_from_value (lhs);
    double rhs_value = (double) ecma_get_number_from_value (rhs);

    if (ecma_number_is_nan (lhs_value))
    {
      // Keep NaNs at the end of the array.
      result = ECMA_NUMBER_ONE;
    }
    else if (ecma_number_is_nan (rhs_value))
    {
      // Keep NaNs at the end of the array.
      result = ECMA_NUMBER_MINUS_ONE;
    }
    else if (lhs_value < rhs_value)
    {
      result = ECMA_NUMBER_MINUS_ONE;
    }
    else if (lhs_value > rhs_value)
    {
      result = ECMA_NUMBER_ONE;
    }
    else
    {
      result = ECMA_NUMBER_ZERO;
    }

    return ecma_make_number_value (result);
  }

  /*
   * compare_func, if not undefined, will always contain a callable function object.
   * We checked this previously, before this function was called.
   */
  JERRY_ASSERT (ecma_op_is_callable (compare_func));
  ecma_object_t *comparefn_obj_p = ecma_get_object_from_value (compare_func);

  ecma_value_t compare_args[] = { lhs, rhs };

  ECMA_TRY_CATCH (call_value,
                  ecma_op_function_call (comparefn_obj_p,
                                         ECMA_VALUE_UNDEFINED,
                                         compare_args,
                                         2),
                  ret_value);

  if (!ecma_is_value_number (call_value))
  {
    ECMA_OP_TO_NUMBER_TRY_CATCH (ret_num, call_value, ret_value);
    result = ret_num;
    ECMA_OP_TO_NUMBER_FINALIZE (ret_num);

    // If the coerced value can't be represented as a Number, compare them as equals.
    if (ecma_number_is_nan (result))
    {
      result = ECMA_NUMBER_ZERO;
    }
  }
  else
  {
    result = ecma_get_number_from_value (call_value);
  }

  ECMA_FINALIZE (call_value);

  if (ecma_is_value_empty (ret_value))
  {
    ret_value = ecma_make_number_value (result);
  }

  return ret_value;
} /* ecma_builtin_typedarray_prototype_sort_compare_helper */

/**
 * The %TypedArray%.prototype object's 'sort' routine.
 *
 * See also:
 *          ES2015, 22.2.3.25, 22.1.3.24
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_sort (ecma_value_t this_arg, /**< this argument */
                                        ecma_value_t compare_func) /**< comparator fn */
{
  if (!ecma_is_typedarray (this_arg))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a TypedArray."));
  }

  if (!ecma_is_value_undefined (compare_func) && !ecma_op_is_callable (compare_func))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Compare function is not callable."));
  }

  ecma_object_t *typedarray_p = ecma_get_object_from_value (this_arg);
  ecma_length_t typedarray_length = ecma_typedarray_get_length (typedarray_p);

  if (!typedarray_length)
  {
    return ecma_copy_value (this_arg);
  }

  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  JMEM_DEFINE_LOCAL_ARRAY (values_buffer, typedarray_length, ecma_value_t);

  lit_magic_string_id_t class_id = ecma_object_get_class_name (typedarray_p);
  lit_utf8_byte_t *typedarray_buffer_p = ecma_typedarray_get_buffer (typedarray_p);
  uint8_t shift = ecma_typedarray_get_element_size_shift (typedarray_p);
  uint8_t element_size = (uint8_t) (1 << shift);

  uint32_t byte_index = 0, buffer_index = 0;
  uint32_t limit = typedarray_length * element_size;

  /* Copy unsorted array into a native c array. */
  while (byte_index < limit)
  {
    JERRY_ASSERT (buffer_index < typedarray_length);
    ecma_number_t element_num = ecma_get_typedarray_element (typedarray_buffer_p + byte_index,
                                                             class_id);
    ecma_value_t element_value = ecma_make_number_value (element_num);
    values_buffer[buffer_index++] = element_value;
    byte_index += element_size;
  }

  JERRY_ASSERT (buffer_index == typedarray_length);

  const ecma_builtin_helper_sort_compare_fn_t sort_cb = &ecma_builtin_typedarray_prototype_sort_compare_helper;
  ECMA_TRY_CATCH (sort_value,
                  ecma_builtin_helper_array_heap_sort_helper (values_buffer,
                                                              (uint32_t) (typedarray_length - 1),
                                                              compare_func,
                                                              sort_cb),
                  ret_value);
  ECMA_FINALIZE (sort_value);

  if (ecma_is_value_empty (ret_value))
  {
    byte_index = 0;
    buffer_index = 0;
    limit = typedarray_length * element_size;
    /* Put sorted values from the native array back into the typedarray buffer. */
    while (byte_index < limit)
    {
      JERRY_ASSERT (buffer_index < typedarray_length);
      ecma_value_t element_value = values_buffer[buffer_index++];
      ecma_number_t element_num = ecma_get_number_from_value (element_value);
      ecma_set_typedarray_element (typedarray_buffer_p + byte_index, element_num, class_id);
      byte_index += element_size;
    }

    JERRY_ASSERT (buffer_index == typedarray_length);
  }

  /* Free values that were copied to the local array. */
  for (uint32_t index = 0; index < typedarray_length; index++)
  {
    ecma_free_value (values_buffer[index]);
  }

  JMEM_FINALIZE_LOCAL_ARRAY (values_buffer);

  if (ecma_is_value_empty (ret_value))
  {
    ret_value = ecma_copy_value (this_arg);
  }

  return ret_value;
} /* ecma_builtin_typedarray_prototype_sort */

/**
 * The %TypedArray%.prototype object's 'find' routine
 *
 * See also:
 *          ECMA-262 v6, 22.2.3.10
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_typedarray_prototype_find (ecma_value_t this_arg, /**< this argument */
                                        ecma_value_t predicate, /**< callback function */
                                        ecma_value_t predicate_this_arg) /**< this argument for
                                                                          *   invoke predicate */
{
  if (!ecma_is_typedarray (this_arg))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a TypedArray."));
  }

  if (!ecma_op_is_callable (predicate))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Callback function is not callable."));
  }

  JERRY_ASSERT (ecma_is_value_object (predicate));
  ecma_object_t *func_object_p = ecma_get_object_from_value (predicate);

  ecma_object_t *typedarray_p = ecma_get_object_from_value (this_arg);
  uint32_t typedarray_length = ecma_typedarray_get_length (typedarray_p);
  lit_magic_string_id_t class_id = ecma_object_get_class_name (typedarray_p);
  lit_utf8_byte_t *typedarray_buffer_p = ecma_typedarray_get_buffer (typedarray_p);
  uint8_t shift = ecma_typedarray_get_element_size_shift (typedarray_p);
  uint8_t element_size = (uint8_t) (1 << shift);

  uint32_t buffer_index = 0;
  uint32_t limit = typedarray_length * element_size;

  for (uint32_t byte_index = 0;  byte_index < limit; byte_index += element_size)
  {
    JERRY_ASSERT (buffer_index < typedarray_length);
    ecma_number_t element_num = ecma_get_typedarray_element (typedarray_buffer_p + byte_index, class_id);
    ecma_value_t element_value = ecma_make_number_value (element_num);

    ecma_value_t call_args[] = { element_value, ecma_make_uint32_value (buffer_index++), this_arg };

    ecma_value_t call_value = ecma_op_function_call (func_object_p, predicate_this_arg, call_args, 3);

    if (ECMA_IS_VALUE_ERROR (call_value))
    {
      ecma_free_value (element_value);
      return call_value;
    }

    bool call_result = ecma_op_to_boolean (call_value);
    ecma_free_value (call_value);

    if (call_result)
    {
      return element_value;
    }

    ecma_free_value (element_value);
  }

  return ECMA_VALUE_UNDEFINED;
} /* ecma_builtin_typedarray_prototype_find */

/**
 * @}
 * @}
 * @}
 */

#endif /* !CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN */
