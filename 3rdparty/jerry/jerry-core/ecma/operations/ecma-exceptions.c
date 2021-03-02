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

#include <stdarg.h>
#include "ecma-builtins.h"
#include "ecma-conversion.h"
#include "ecma-exceptions.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects.h"
#include "ecma-symbol-object.h"
#include "jcontext.h"
#include "jrt.h"

#ifdef JERRY_ENABLE_LINE_INFO
#include "vm.h"
#endif /* JERRY_ENABLE_LINE_INFO */

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup exceptions Exceptions
 * @{
 */

/**
 * Map error type to error prototype.
 */
typedef struct
{
  ecma_standard_error_t error_type; /**< Native error type */
  ecma_builtin_id_t error_prototype_id; /**< ID of the error prototype */
} ecma_error_mapping_t;

/**
 * List of error type mappings
 */
const ecma_error_mapping_t ecma_error_mappings[] =
{
#define ERROR_ELEMENT(TYPE, ID) { TYPE, ID }
  ERROR_ELEMENT (ECMA_ERROR_COMMON,      ECMA_BUILTIN_ID_ERROR_PROTOTYPE),

#ifndef CONFIG_DISABLE_ERROR_BUILTINS
  ERROR_ELEMENT (ECMA_ERROR_EVAL,        ECMA_BUILTIN_ID_EVAL_ERROR_PROTOTYPE),
  ERROR_ELEMENT (ECMA_ERROR_RANGE,       ECMA_BUILTIN_ID_RANGE_ERROR_PROTOTYPE),
  ERROR_ELEMENT (ECMA_ERROR_REFERENCE,   ECMA_BUILTIN_ID_REFERENCE_ERROR_PROTOTYPE),
  ERROR_ELEMENT (ECMA_ERROR_TYPE,        ECMA_BUILTIN_ID_TYPE_ERROR_PROTOTYPE),
  ERROR_ELEMENT (ECMA_ERROR_URI,         ECMA_BUILTIN_ID_URI_ERROR_PROTOTYPE),
  ERROR_ELEMENT (ECMA_ERROR_SYNTAX,      ECMA_BUILTIN_ID_SYNTAX_ERROR_PROTOTYPE),
#endif /* !CONFIG_DISABLE_ERROR_BUILTINS */

#undef ERROR_ELEMENT
};

/**
 * Standard ecma-error object constructor.
 *
 * Note:
 *    calling with ECMA_ERROR_NONE does not make sense thus it will
 *    cause a fault in the system.
 *
 * @return pointer to ecma-object representing specified error
 *         with reference counter set to one.
 */
ecma_object_t *
ecma_new_standard_error (ecma_standard_error_t error_type) /**< native error type */
{
#ifndef CONFIG_DISABLE_ERROR_BUILTINS
  ecma_builtin_id_t prototype_id = ECMA_BUILTIN_ID__COUNT;

  switch (error_type)
  {
    case ECMA_ERROR_EVAL:
    {
      prototype_id = ECMA_BUILTIN_ID_EVAL_ERROR_PROTOTYPE;
      break;
    }

    case ECMA_ERROR_RANGE:
    {
      prototype_id = ECMA_BUILTIN_ID_RANGE_ERROR_PROTOTYPE;
      break;
    }

    case ECMA_ERROR_REFERENCE:
    {
      prototype_id = ECMA_BUILTIN_ID_REFERENCE_ERROR_PROTOTYPE;
      break;
    }

    case ECMA_ERROR_TYPE:
    {
      prototype_id = ECMA_BUILTIN_ID_TYPE_ERROR_PROTOTYPE;
      break;
    }

    case ECMA_ERROR_URI:
    {
      prototype_id = ECMA_BUILTIN_ID_URI_ERROR_PROTOTYPE;
      break;
    }

    case ECMA_ERROR_SYNTAX:
    {
      prototype_id = ECMA_BUILTIN_ID_SYNTAX_ERROR_PROTOTYPE;
      break;
    }

    default:
    {
      JERRY_ASSERT (error_type == ECMA_ERROR_COMMON);

      prototype_id = ECMA_BUILTIN_ID_ERROR_PROTOTYPE;
      break;
    }
  }
#else
  JERRY_UNUSED (error_type);
  ecma_builtin_id_t prototype_id = ECMA_BUILTIN_ID_ERROR_PROTOTYPE;
#endif /* !CONFIG_DISABLE_ERROR_BUILTINS */

  ecma_object_t *prototype_obj_p = ecma_builtin_get (prototype_id);

  ecma_object_t *new_error_obj_p = ecma_create_object (prototype_obj_p,
                                                       sizeof (ecma_extended_object_t),
                                                       ECMA_OBJECT_TYPE_CLASS);

  ((ecma_extended_object_t *) new_error_obj_p)->u.class_prop.class_id = LIT_MAGIC_STRING_ERROR_UL;

#ifdef JERRY_ENABLE_LINE_INFO
  /* The "stack" identifier is not a magic string. */
  const char * const stack_id_p = "stack";

  ecma_string_t *stack_str_p = ecma_new_ecma_string_from_utf8 ((const lit_utf8_byte_t *) stack_id_p, 5);

  ecma_property_value_t *prop_value_p = ecma_create_named_data_property (new_error_obj_p,
                                                                         stack_str_p,
                                                                         ECMA_PROPERTY_CONFIGURABLE_WRITABLE,
                                                                         NULL);
  ecma_deref_ecma_string (stack_str_p);

  ecma_value_t backtrace_value = vm_get_backtrace (0);

  prop_value_p->value = backtrace_value;
  ecma_deref_object (ecma_get_object_from_value (backtrace_value));
#endif /* JERRY_ENABLE_LINE_INFO */

  return new_error_obj_p;
} /* ecma_new_standard_error */

/**
 * Return the error type for an Error object.
 *
 * @return one of the ecma_standard_error_t value
 *         if it is not an Error object then ECMA_ERROR_NONE will be returned
 */
ecma_standard_error_t
ecma_get_error_type (ecma_object_t *error_object) /**< possible error object */
{
  ecma_object_t *prototype_p = ecma_get_object_prototype (error_object);
  if (prototype_p != NULL)
  {
    uint8_t builtin_id = ecma_get_object_builtin_id (prototype_p);

    for (uint8_t idx = 0; idx < sizeof (ecma_error_mappings) / sizeof (ecma_error_mappings[0]); idx++)
    {
      if (ecma_error_mappings[idx].error_prototype_id == builtin_id)
      {
        return ecma_error_mappings[idx].error_type;
      }
    }
  }

  return ECMA_ERROR_NONE;
} /* ecma_get_error_type */

/**
 * Standard ecma-error object constructor.
 *
 * @return pointer to ecma-object representing specified error
 *         with reference counter set to one.
 */
ecma_object_t *
ecma_new_standard_error_with_message (ecma_standard_error_t error_type, /**< native error type */
                                      ecma_string_t *message_string_p) /**< message string */
{
  ecma_object_t *new_error_obj_p = ecma_new_standard_error (error_type);

  ecma_property_value_t *prop_value_p;
  prop_value_p = ecma_create_named_data_property (new_error_obj_p,
                                                  ecma_get_magic_string (LIT_MAGIC_STRING_MESSAGE),
                                                  ECMA_PROPERTY_CONFIGURABLE_WRITABLE,
                                                  NULL);

  ecma_ref_ecma_string (message_string_p);
  prop_value_p->value = ecma_make_string_value (message_string_p);

  return new_error_obj_p;
} /* ecma_new_standard_error_with_message */

/**
 * Raise a standard ecma-error with the given type and message.
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
static ecma_value_t
ecma_raise_standard_error (ecma_standard_error_t error_type, /**< error type */
                           const lit_utf8_byte_t *msg_p) /**< error message */
{
  ecma_object_t *error_obj_p;

  if (msg_p != NULL)
  {
    ecma_string_t *error_msg_p = ecma_new_ecma_string_from_utf8 (msg_p,
                                                                 lit_zt_utf8_string_size (msg_p));
    error_obj_p = ecma_new_standard_error_with_message (error_type, error_msg_p);
    ecma_deref_ecma_string (error_msg_p);
  }
  else
  {
    error_obj_p = ecma_new_standard_error (error_type);
  }

  JERRY_CONTEXT (error_value) = ecma_make_object_value (error_obj_p);
  JERRY_CONTEXT (status_flags) |= ECMA_STATUS_EXCEPTION;
  return ECMA_VALUE_ERROR;
} /* ecma_raise_standard_error */

#ifdef JERRY_ENABLE_ERROR_MESSAGES

/**
 * Raise a standard ecma-error with the given format string and arguments.
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_raise_standard_error_with_format (ecma_standard_error_t error_type, /**< error type */
                                       const char *format, /**< format string */
                                       ...) /**< ecma-values */
{
  JERRY_ASSERT (format != NULL);

  ecma_string_t *error_msg_p = ecma_get_magic_string (LIT_MAGIC_STRING__EMPTY);

  const char *start_p = format;
  const char *end_p = format;

  va_list args;

  va_start (args, format);

  while (*end_p)
  {
    if (*end_p == '%')
    {
      /* Concat template string. */
      if (end_p > start_p)
      {
        const lit_utf8_byte_t *chars_p = (const lit_utf8_byte_t *) start_p;
        lit_utf8_size_t chars_size = (lit_utf8_size_t) (end_p - start_p);

        error_msg_p = ecma_append_chars_to_string (error_msg_p,
                                                   chars_p,
                                                   chars_size,
                                                   lit_utf8_string_length (chars_p, chars_size));
      }

      /* Convert an argument to string without side effects. */
      ecma_string_t *arg_string_p;
      const ecma_value_t arg_val = va_arg (args, ecma_value_t);

      if (JERRY_UNLIKELY (ecma_is_value_object (arg_val)))
      {
        ecma_object_t *arg_object_p = ecma_get_object_from_value (arg_val);
        lit_magic_string_id_t class_name = ecma_object_get_class_name (arg_object_p);
        arg_string_p = ecma_get_magic_string (class_name);
      }
#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
      else if (ecma_is_value_symbol (arg_val))
      {
        ecma_value_t symbol_desc_value = ecma_get_symbol_descriptive_string (arg_val);
        arg_string_p = ecma_get_string_from_value (symbol_desc_value);
      }
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */
      else
      {
        ecma_value_t str_val = ecma_op_to_string (arg_val);
        arg_string_p = ecma_get_string_from_value (str_val);
      }

      /* Concat argument. */
      error_msg_p = ecma_concat_ecma_strings (error_msg_p, arg_string_p);
      ecma_deref_ecma_string (arg_string_p);

      start_p = end_p + 1;
    }

    end_p++;
  }

  va_end (args);

  /* Concat reset of template string. */
  if (start_p < end_p)
  {
    const lit_utf8_byte_t *chars_p = (const lit_utf8_byte_t *) start_p;
    lit_utf8_size_t chars_size = (lit_utf8_size_t) (end_p - start_p);

    error_msg_p = ecma_append_chars_to_string (error_msg_p,
                                               chars_p,
                                               chars_size,
                                               lit_utf8_string_length (chars_p, chars_size));
  }

  ecma_object_t *error_obj_p = ecma_new_standard_error_with_message (error_type, error_msg_p);
  ecma_deref_ecma_string (error_msg_p);

  JERRY_CONTEXT (error_value) = ecma_make_object_value (error_obj_p);
  JERRY_CONTEXT (status_flags) |= ECMA_STATUS_EXCEPTION;
  return ECMA_VALUE_ERROR;
} /* ecma_raise_standard_error_with_format */

#endif /* JERRY_ENABLE_ERROR_MESSAGES */

/**
 * Raise a common error with the given message.
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_raise_common_error (const char *msg_p) /**< error message */
{
  return ecma_raise_standard_error (ECMA_ERROR_COMMON, (const lit_utf8_byte_t *) msg_p);
} /* ecma_raise_common_error */

/**
 * Raise a RangeError with the given message.
 *
 * See also: ECMA-262 v5, 15.11.6.2
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_raise_range_error (const char *msg_p) /**< error message */
{
  return ecma_raise_standard_error (ECMA_ERROR_RANGE, (const lit_utf8_byte_t *) msg_p);
} /* ecma_raise_range_error */

/**
 * Raise a ReferenceError with the given message.
 *
 * See also: ECMA-262 v5, 15.11.6.3
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_raise_reference_error (const char *msg_p) /**< error message */
{
  return ecma_raise_standard_error (ECMA_ERROR_REFERENCE, (const lit_utf8_byte_t *) msg_p);
} /* ecma_raise_reference_error */

/**
 * Raise a SyntaxError with the given message.
 *
 * See also: ECMA-262 v5, 15.11.6.4
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_raise_syntax_error (const char *msg_p) /**< error message */
{
  return ecma_raise_standard_error (ECMA_ERROR_SYNTAX, (const lit_utf8_byte_t *) msg_p);
} /* ecma_raise_syntax_error */

/**
 * Raise a TypeError with the given message.
 *
* See also: ECMA-262 v5, 15.11.6.5
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_raise_type_error (const char *msg_p) /**< error message */
{
  return ecma_raise_standard_error (ECMA_ERROR_TYPE, (const lit_utf8_byte_t *) msg_p);
} /* ecma_raise_type_error */

/**
 * Raise a URIError with the given message.
 *
* See also: ECMA-262 v5, 15.11.6.6
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
ecma_raise_uri_error (const char *msg_p) /**< error message */
{
  return ecma_raise_standard_error (ECMA_ERROR_URI, (const lit_utf8_byte_t *) msg_p);
} /* ecma_raise_uri_error */

/**
 * @}
 * @}
 */
