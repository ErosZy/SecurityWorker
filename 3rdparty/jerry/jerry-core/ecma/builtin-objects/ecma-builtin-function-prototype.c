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
#include "ecma-builtin-helpers.h"
#include "ecma-builtins.h"
#include "ecma-conversion.h"
#include "ecma-exceptions.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-function-object.h"
#include "ecma-objects.h"
#include "ecma-try-catch-macro.h"
#include "jrt.h"

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

#define BUILTIN_INC_HEADER_NAME "ecma-builtin-function-prototype.inc.h"
#define BUILTIN_UNDERSCORED_ID function_prototype
#include "ecma-builtin-internal-routines-template.inc.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltins
 * @{
 *
 * \addtogroup functionprototype ECMA Function.prototype object built-in
 * @{
 */

/**
 * Maximum number of arguments for an apply function.
 */
#define ECMA_FUNCTION_APPLY_ARGUMENT_COUNT_LIMIT 65535

/**
 * The Function.prototype object's 'toString' routine
 *
 * See also:
 *          ECMA-262 v5, 15.3.4.2
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_function_prototype_object_to_string (ecma_value_t this_arg) /**< this argument */
{
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  if (!ecma_op_is_callable (this_arg))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a function."));
  }
  else
  {
    ret_value = ecma_make_magic_string_value (LIT_MAGIC_STRING__FUNCTION_TO_STRING);
  }
  return ret_value;
} /* ecma_builtin_function_prototype_object_to_string */

/**
 * The Function.prototype object's 'apply' routine
 *
 * See also:
 *          ECMA-262 v5, 15.3.4.3
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_function_prototype_object_apply (ecma_value_t this_arg, /**< this argument */
                                              ecma_value_t arg1, /**< first argument */
                                              ecma_value_t arg2) /**< second argument */
{
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  /* 1. */
  if (!ecma_op_is_callable (this_arg))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a function."));
  }
  else
  {
    ecma_object_t *func_obj_p = ecma_get_object_from_value (this_arg);

    /* 2. */
    if (ecma_is_value_null (arg2) || ecma_is_value_undefined (arg2))
    {
      ret_value = ecma_op_function_call (func_obj_p, arg1, NULL, 0);
    }
    else
    {
      /* 3. */
      if (!ecma_is_value_object (arg2))
      {
        ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Argument is not an object."));
      }
      else
      {
        ecma_object_t *obj_p = ecma_get_object_from_value (arg2);

        /* 4. */
        ecma_value_t length_value = ecma_op_object_get_by_magic_id (obj_p, LIT_MAGIC_STRING_LENGTH);
        if (ECMA_IS_VALUE_ERROR (length_value))
        {
          return length_value;
        }

        ecma_number_t length_number;
        ecma_value_t get_result = ecma_get_number (length_value, &length_number);

        ecma_free_value (length_value);

        if (ECMA_IS_VALUE_ERROR (get_result))
        {
          return get_result;
        }
        JERRY_ASSERT (ecma_is_value_empty (get_result));

        /* 5. */
        const uint32_t length = ecma_number_to_uint32 (length_number);

        if (length >= ECMA_FUNCTION_APPLY_ARGUMENT_COUNT_LIMIT)
        {
          ret_value = ecma_raise_range_error (ECMA_ERR_MSG ("Too many arguments declared for Function.apply()."));
        }
        else
        {
          /* 6. */
          JMEM_DEFINE_LOCAL_ARRAY (arguments_list_p, length, ecma_value_t);
          uint32_t index = 0;

          /* 7. */
          for (index = 0; index < length; index++)
          {
            ecma_string_t *curr_idx_str_p = ecma_new_ecma_string_from_uint32 (index);
            ecma_value_t get_value = ecma_op_object_get (obj_p, curr_idx_str_p);
            ecma_deref_ecma_string (curr_idx_str_p);

            if (ECMA_IS_VALUE_ERROR (get_value))
            {
              ret_value = get_value;
              break;
            }

            arguments_list_p[index] = get_value;
          }

          if (ecma_is_value_empty (ret_value))
          {
            JERRY_ASSERT (index == length);
            ret_value = ecma_op_function_call (func_obj_p,
                                               arg1,
                                               arguments_list_p,
                                               length);
          }

          for (uint32_t remove_index = 0; remove_index < index; remove_index++)
          {
            ecma_free_value (arguments_list_p[remove_index]);
          }

          JMEM_FINALIZE_LOCAL_ARRAY (arguments_list_p);
        }
      }
    }
  }

  return ret_value;
} /* ecma_builtin_function_prototype_object_apply */

/**
 * The Function.prototype object's 'call' routine
 *
 * See also:
 *          ECMA-262 v5, 15.3.4.4
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_function_prototype_object_call (ecma_value_t this_arg, /**< this argument */
                                             const ecma_value_t *arguments_list_p, /**< list of arguments */
                                             ecma_length_t arguments_number) /**< number of arguments */
{
  if (!ecma_op_is_callable (this_arg))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a function."));
  }
  else
  {
    ecma_object_t *func_obj_p = ecma_get_object_from_value (this_arg);

    if (arguments_number == 0)
    {
      /* Even a 'this' argument is missing. */
      return ecma_op_function_call (func_obj_p,
                                    ECMA_VALUE_UNDEFINED,
                                    NULL,
                                    0);
    }
    else
    {
      return ecma_op_function_call (func_obj_p,
                                    arguments_list_p[0],
                                    arguments_list_p + 1,
                                    (ecma_length_t) (arguments_number - 1u));
    }
  }
} /* ecma_builtin_function_prototype_object_call */

/**
 * The Function.prototype object's 'bind' routine
 *
 * See also:
 *          ECMA-262 v5, 15.3.4.5
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_function_prototype_object_bind (ecma_value_t this_arg, /**< this argument */
                                             const ecma_value_t *arguments_list_p, /**< list of arguments */
                                             ecma_length_t arguments_number) /**< number of arguments */
{
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  /* 2. */
  if (!ecma_op_is_callable (this_arg))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is not a function."));
  }
  else
  {
    /* 4. 11. 18. */
    ecma_object_t *prototype_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_FUNCTION_PROTOTYPE);

    ecma_object_t *function_p;
    ecma_extended_object_t *ext_function_p;

    if (arguments_number == 0
        || (arguments_number == 1 && !ecma_is_value_integer_number (arguments_list_p[0])))
    {
      function_p = ecma_create_object (prototype_obj_p,
                                       sizeof (ecma_extended_object_t),
                                       ECMA_OBJECT_TYPE_BOUND_FUNCTION);

      /* 8. */
      ext_function_p = (ecma_extended_object_t *) function_p;
      ecma_object_t *this_arg_obj_p = ecma_get_object_from_value (this_arg);
      ECMA_SET_INTERNAL_VALUE_POINTER (ext_function_p->u.bound_function.target_function,
                                       this_arg_obj_p);

      ext_function_p->u.bound_function.args_len_or_this = ECMA_VALUE_UNDEFINED;

      if (arguments_number != 0)
      {
        ext_function_p->u.bound_function.args_len_or_this = ecma_copy_value_if_not_object (arguments_list_p[0]);
      }
    }
    else
    {
      JERRY_ASSERT (arguments_number > 0);

      size_t obj_size = sizeof (ecma_extended_object_t) + (arguments_number * sizeof (ecma_value_t));

      function_p = ecma_create_object (prototype_obj_p,
                                       obj_size,
                                       ECMA_OBJECT_TYPE_BOUND_FUNCTION);

      /* 8. */
      ext_function_p = (ecma_extended_object_t *) function_p;
      ecma_object_t *this_arg_obj_p = ecma_get_object_from_value (this_arg);
      ECMA_SET_INTERNAL_VALUE_POINTER (ext_function_p->u.bound_function.target_function,
                                       this_arg_obj_p);

      /* NOTE: This solution provides temporary false data about the object's size
         but prevents GC from freeing it until it's not fully initialized. */
      ext_function_p->u.bound_function.args_len_or_this = ECMA_VALUE_UNDEFINED;
      ecma_value_t *args_p = (ecma_value_t *) (ext_function_p + 1);

      for (ecma_length_t i = 0; i < arguments_number; i++)
      {
        *args_p++ = ecma_copy_value_if_not_object (arguments_list_p[i]);
      }

      ecma_value_t args_len_or_this = ecma_make_integer_value ((ecma_integer_value_t) arguments_number);
      ext_function_p->u.bound_function.args_len_or_this = args_len_or_this;
    }

    /*
     * [[Class]] property is not stored explicitly for objects of ECMA_OBJECT_TYPE_FUNCTION type.
     *
     * See also: ecma_object_get_class_name
     */

    /* 22. */
    ret_value = ecma_make_object_value (function_p);
  }

  return ret_value;
} /* ecma_builtin_function_prototype_object_bind */

/**
 * Handle calling [[Call]] of built-in Function.prototype object
 *
 * @return ecma value
 */
ecma_value_t
ecma_builtin_function_prototype_dispatch_call (const ecma_value_t *arguments_list_p, /**< arguments list */
                                               ecma_length_t arguments_list_len) /**< number of arguments */
{
  JERRY_ASSERT (arguments_list_len == 0 || arguments_list_p != NULL);

  return ECMA_VALUE_UNDEFINED;
} /* ecma_builtin_function_prototype_dispatch_call */

/**
 * Handle calling [[Construct]] of built-in Function.prototype object
 *
 * @return ecma value
 */
ecma_value_t
ecma_builtin_function_prototype_dispatch_construct (const ecma_value_t *arguments_list_p, /**< arguments list */
                                                    ecma_length_t arguments_list_len) /**< number of arguments */
{
  JERRY_ASSERT (arguments_list_len == 0 || arguments_list_p != NULL);

  return ecma_raise_type_error (ECMA_ERR_MSG ("'Function.prototype' is not a constructor."));
} /* ecma_builtin_function_prototype_dispatch_construct */

/**
 * @}
 * @}
 * @}
 */
