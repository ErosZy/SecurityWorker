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
#include "ecma-builtins.h"
#include "ecma-exceptions.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects.h"
#include "ecma-objects-general.h"
#include "ecma-symbol-object.h"

#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmasymbolobject ECMA Symbol object related routines
 * @{
 */

/**
 * Symbol creation operation.
 *
 * See also: ECMA-262 v6, 6.1.5.1
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_create_symbol (const ecma_value_t *arguments_list_p, /**< list of arguments */
                       ecma_length_t arguments_list_len) /**< length of the arguments' list */
{
  JERRY_ASSERT (arguments_list_len == 0 || arguments_list_p != NULL);

  ecma_value_t string_desc;

  /* 1-3. */
  if (arguments_list_len == 0)
  {
    string_desc = ecma_make_magic_string_value (LIT_MAGIC_STRING__EMPTY);
  }
  else
  {
    string_desc = ecma_op_to_string (arguments_list_p[0]);

    /* 4. */
    if (ECMA_IS_VALUE_ERROR (string_desc))
    {
      return string_desc;
    }

    JERRY_ASSERT (ecma_is_value_string (string_desc));
  }

  /* 5. */
  return ecma_make_symbol_value (ecma_new_symbol_from_descriptor_string (string_desc));
} /* ecma_op_create_symbol */

/**
 * Symbol object creation operation.
 *
 * See also: ECMA-262 v6, 19.4.1
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_op_create_symbol_object (const ecma_value_t value) /**< symbol value */
{
  JERRY_ASSERT (ecma_is_value_symbol (value));

#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
  ecma_object_t *prototype_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_SYMBOL_PROTOTYPE);
#else /* CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */
  ecma_object_t *prototype_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_OBJECT_PROTOTYPE);
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */

  ecma_object_t *object_p = ecma_create_object (prototype_obj_p,
                                                sizeof (ecma_extended_object_t),
                                                ECMA_OBJECT_TYPE_CLASS);

  ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;
  ext_object_p->u.class_prop.class_id = LIT_MAGIC_STRING_SYMBOL_UL;
  ext_object_p->u.class_prop.u.value = ecma_copy_value (value);

  return ecma_make_object_value (object_p);
} /* ecma_op_create_symbol_object */

/**
 * Get the symbol descriptor ecma-string from an ecma-symbol
 *
 * @return pointer to ecma-string descriptor
 */
ecma_string_t *
ecma_get_symbol_description (ecma_string_t *symbol_p) /**< ecma-symbol */
{
  JERRY_ASSERT (symbol_p != NULL);
  JERRY_ASSERT (ecma_prop_name_is_symbol (symbol_p));

  return ecma_get_string_from_value (symbol_p->u.symbol_descriptor);
} /* ecma_get_symbol_description */

/**
 * Get the descriptive string of the Symbol.
 *
 * See also: ECMA-262 v6, 19.4.3.2.1
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_get_symbol_descriptive_string (ecma_value_t symbol_value) /**< symbol to stringify */
{
  /* 1. */
  JERRY_ASSERT (ecma_is_value_symbol (symbol_value));

  /* 2 - 3. */
  ecma_string_t *symbol_p = ecma_get_symbol_from_value (symbol_value);
  ecma_string_t *string_desc_p = ecma_get_symbol_description (symbol_p);

  /* 5. */
  ecma_string_t *concat_p = ecma_concat_ecma_strings (ecma_get_magic_string (LIT_MAGIC_STRING_SYMBOL_LEFT_PAREN_UL),
                                                      string_desc_p);

  ecma_string_t *final_str_p = ecma_append_magic_string_to_string (concat_p, LIT_MAGIC_STRING_RIGHT_PAREN);

  return ecma_make_string_value (final_str_p);
} /* ecma_get_symbol_descriptive_string */

/**
 * Helper for Symbol.prototype.{toString, valueOf} routines
 *
 * See also: 19.4.3.2, 19.4.3.3
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_symbol_to_string_helper (ecma_value_t this_arg, /**< this argument value */
                              bool is_to_string) /**< true - perform the 'toString' routine steps
                                                  *   false - perform the 'valueOf' routine steps */
{
  if (ecma_is_value_symbol (this_arg))
  {
    return is_to_string ? ecma_get_symbol_descriptive_string (this_arg) : ecma_copy_value (this_arg);
  }

  if (ecma_is_value_object (this_arg))
  {
    ecma_object_t *object_p = ecma_get_object_from_value (this_arg);

    if (ecma_get_object_type (object_p) == ECMA_OBJECT_TYPE_CLASS)
    {
      ecma_extended_object_t *ext_object_p = (ecma_extended_object_t *) object_p;

      if (ext_object_p->u.class_prop.class_id == LIT_MAGIC_STRING_SYMBOL_UL)
      {
        return (is_to_string ? ecma_get_symbol_descriptive_string (ext_object_p->u.class_prop.u.value)
                             : ecma_copy_value (ext_object_p->u.class_prop.u.value));
      }
    }
  }

  return ecma_raise_type_error (ECMA_ERR_MSG ("Argument 'this' is must be a Symbol."));
} /* ecma_symbol_to_string_helper */

#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */

/**
 * @}
 * @}
 */
