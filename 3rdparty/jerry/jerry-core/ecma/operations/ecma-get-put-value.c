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

/**
 * Implementation of ECMA GetValue and PutValue
 */

#include "ecma-builtins.h"
#include "ecma-exceptions.h"
#include "ecma-gc.h"
#include "ecma-helpers.h"
#include "ecma-lex-env.h"
#include "ecma-objects.h"
#include "ecma-function-object.h"
#include "ecma-objects-general.h"
#include "ecma-try-catch-macro.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup lexicalenvironment Lexical environment
 * @{
 */

/**
 * GetValue operation part (lexical environment base or unresolvable reference).
 *
 * See also: ECMA-262 v5, 8.7.1, sections 3 and 5
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_op_get_value_lex_env_base (ecma_object_t *ref_base_lex_env_p, /**< reference's base (lexical environment) */
                                ecma_string_t *var_name_string_p, /**< variable name */
                                bool is_strict) /**< flag indicating strict mode */
{
  const bool is_unresolvable_reference = (ref_base_lex_env_p == NULL);

  /* 3. */
  if (JERRY_UNLIKELY (is_unresolvable_reference))
  {
#ifdef JERRY_ENABLE_ERROR_MESSAGES
    ecma_value_t var_name_val = ecma_make_string_value (var_name_string_p);
    ecma_value_t error_value = ecma_raise_standard_error_with_format (ECMA_ERROR_REFERENCE,
                                                                      "% is not defined",
                                                                      var_name_val);
#else /* !JERRY_ENABLE_ERROR_MESSAGES */
    ecma_value_t error_value = ecma_raise_reference_error (NULL);
#endif /* JERRY_ENABLE_ERROR_MESSAGES */
    return error_value;
  }

  /* 5. */
  JERRY_ASSERT (ref_base_lex_env_p != NULL
                && ecma_is_lexical_environment (ref_base_lex_env_p));

  /* 5.a */
  return ecma_op_get_binding_value (ref_base_lex_env_p,
                                    var_name_string_p,
                                    is_strict);
} /* ecma_op_get_value_lex_env_base */

/**
 * GetValue operation part (object base).
 *
 * See also: ECMA-262 v5, 8.7.1, section 4
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_op_get_value_object_base (ecma_value_t base_value, /**< base value */
                               ecma_string_t *property_name_p) /**< property name */
{
  if (ecma_is_value_object (base_value))
  {
    ecma_object_t *obj_p = ecma_get_object_from_value (base_value);
    JERRY_ASSERT (obj_p != NULL
                  && !ecma_is_lexical_environment (obj_p));

    return ecma_op_object_get (obj_p, property_name_p);
  }

  JERRY_ASSERT (ecma_is_value_boolean (base_value)
                || ecma_is_value_number (base_value)
                || ECMA_ASSERT_VALUE_IS_SYMBOL (base_value)
                || ecma_is_value_string (base_value));

  /* Fast path for the strings length property. The length of the string
     can be obtained directly from the ecma-string. */
  if (ecma_is_value_string (base_value) && ecma_string_is_length (property_name_p))
  {
    ecma_string_t *string_p = ecma_get_string_from_value (base_value);

    return ecma_make_uint32_value (ecma_string_get_length (string_p));
  }

  ecma_value_t object_base = ecma_op_to_object (base_value);
  JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (object_base));

  ecma_object_t *object_p = ecma_get_object_from_value (object_base);
  JERRY_ASSERT (object_p != NULL
                && !ecma_is_lexical_environment (object_p));

  ecma_value_t ret_value = ECMA_VALUE_UNDEFINED;

  /* Circular reference is possible in JavaScript and testing it is complicated. */
  int max_depth = ECMA_PROPERTY_SEARCH_DEPTH_LIMIT;

  do
  {
    ecma_value_t value = ecma_op_object_find_own (base_value, object_p, property_name_p);

    if (ecma_is_value_found (value))
    {
      ret_value = value;
      break;
    }

    if (--max_depth == 0)
    {
      break;
    }

    object_p = ecma_get_object_prototype (object_p);
  }
  while (object_p != NULL);

  ecma_free_value (object_base);

  return ret_value;
} /* ecma_op_get_value_object_base */

/**
 * PutValue operation part (lexical environment base or unresolvable reference).
 *
 * See also: ECMA-262 v5, 8.7.2, sections 3 and 5
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
ecma_value_t
ecma_op_put_value_lex_env_base (ecma_object_t *ref_base_lex_env_p, /**< reference's base (lexical environment) */
                                ecma_string_t *var_name_string_p, /**< variable name */
                                bool is_strict, /**< flag indicating strict mode */
                                ecma_value_t value) /**< ECMA-value */
{
  const bool is_unresolvable_reference = (ref_base_lex_env_p == NULL);

  /* 3. */
  if (JERRY_UNLIKELY (is_unresolvable_reference))
  {
    /* 3.a. */
    if (is_strict)
    {
#ifdef JERRY_ENABLE_ERROR_MESSAGES
      ecma_value_t var_name_val = ecma_make_string_value (var_name_string_p);
      ecma_value_t error_value = ecma_raise_standard_error_with_format (ECMA_ERROR_REFERENCE,
                                                                        "% is not defined",
                                                                        var_name_val);
#else /* !JERRY_ENABLE_ERROR_MESSAGES */
      ecma_value_t error_value = ecma_raise_reference_error (NULL);
#endif /* JERRY_ENABLE_ERROR_MESSAGES */
      return error_value;
    }
    else
    {
      /* 3.b. */
      ecma_object_t *global_object_p = ecma_builtin_get_global ();

      ecma_value_t completion = ecma_op_object_put (global_object_p,
                                                    var_name_string_p,
                                                    value,
                                                    false);

      JERRY_ASSERT (ecma_is_value_boolean (completion));

      return ECMA_VALUE_EMPTY;
    }
  }

  /* 5. */
  JERRY_ASSERT (ref_base_lex_env_p != NULL
                && ecma_is_lexical_environment (ref_base_lex_env_p));

  /* 5.a */
  return ecma_op_set_mutable_binding (ref_base_lex_env_p,
                                      var_name_string_p,
                                      value,
                                      is_strict);
} /* ecma_op_put_value_lex_env_base */

/**
 * @}
 * @}
 */
