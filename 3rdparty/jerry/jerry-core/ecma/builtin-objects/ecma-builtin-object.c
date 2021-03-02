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
#include "ecma-objects.h"
#include "ecma-objects-general.h"
#include "ecma-try-catch-macro.h"
#include "jrt.h"

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

#define BUILTIN_INC_HEADER_NAME "ecma-builtin-object.inc.h"
#define BUILTIN_UNDERSCORED_ID object
#include "ecma-builtin-internal-routines-template.inc.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltins
 * @{
 *
 * \addtogroup object ECMA Object object built-in
 * @{
 */

/**
 * Handle calling [[Call]] of built-in Object object
 *
 * @return ecma value
 */
ecma_value_t
ecma_builtin_object_dispatch_call (const ecma_value_t *arguments_list_p, /**< arguments list */
                                   ecma_length_t arguments_list_len) /**< number of arguments */
{
  JERRY_ASSERT (arguments_list_len == 0 || arguments_list_p != NULL);

  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  if (arguments_list_len == 0
      || ecma_is_value_undefined (arguments_list_p[0])
      || ecma_is_value_null (arguments_list_p[0]))
  {
    ret_value = ecma_builtin_object_dispatch_construct (arguments_list_p, arguments_list_len);
  }
  else
  {
    ret_value = ecma_op_to_object (arguments_list_p[0]);
  }

  return ret_value;
} /* ecma_builtin_object_dispatch_call */

/**
 * Handle calling [[Construct]] of built-in Object object
 *
 * @return ecma value
 */
ecma_value_t
ecma_builtin_object_dispatch_construct (const ecma_value_t *arguments_list_p, /**< arguments list */
                                        ecma_length_t arguments_list_len) /**< number of arguments */
{
  JERRY_ASSERT (arguments_list_len == 0 || arguments_list_p != NULL);

  if (arguments_list_len == 0)
  {
    ecma_object_t *obj_p = ecma_op_create_object_object_noarg ();

    return ecma_make_object_value (obj_p);
  }
  else
  {
    return ecma_op_create_object_object_arg (arguments_list_p[0]);
  }
} /* ecma_builtin_object_dispatch_construct */

/**
 * The Object object's 'getPrototypeOf' routine
 *
 * See also:
 *          ECMA-262 v5, 15.2.3.2
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_object_get_prototype_of (ecma_value_t this_arg, /**< 'this' argument */
                                             ecma_value_t arg) /**< routine's argument */
{
  JERRY_UNUSED (this_arg);
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;
  bool was_object = ecma_is_value_object (arg);

  /* 1. */
  if (!was_object)
  {
#ifndef CONFIG_DISABLE_ES2015_BUILTIN
    arg = ecma_op_to_object (arg);
    if (ECMA_IS_VALUE_ERROR (arg))
    {
      return arg;
    }
#else /* CONFIG_DISABLE_ES2015_BUILTIN */
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument is not an object."));
#endif /* !CONFIG_DISABLE_ES2015_BUILTIN */
  }
  /* 2. */
  ecma_object_t *obj_p = ecma_get_object_from_value (arg);
  ecma_object_t *prototype_p = ecma_get_object_prototype (obj_p);

  if (prototype_p)
  {
    ret_value = ecma_make_object_value (prototype_p);
    ecma_ref_object (prototype_p);
  }
  else
  {
    ret_value = ECMA_VALUE_NULL;
  }

#ifndef CONFIG_DISABLE_ES2015_BUILTIN
  if (!was_object)
  {
    ecma_free_value (arg);
  }
#endif /* !CONFIG_DISABLE_ES2015_BUILTIN */

  return ret_value;
} /* ecma_builtin_object_object_get_prototype_of */

#ifndef CONFIG_DISABLE_ES2015_BUILTIN
/**
 * [[SetPrototypeOf]]
 *
 * See also:
 *          ES2015 9.1.2
 *
 * @return true  - if success
 *         false - otherwise
 */
static bool
ecma_set_prototype_of (ecma_value_t o_value, /**< O */
                       ecma_value_t v_value) /**< V */
{
  /* 1. */
  JERRY_ASSERT (ecma_is_value_object (o_value));
  JERRY_ASSERT (ecma_is_value_object (v_value) || ecma_is_value_null (v_value));

  ecma_object_t *o_p = ecma_get_object_from_value (o_value);
  ecma_object_t *v_p = ecma_is_value_null (v_value) ? NULL : ecma_get_object_from_value (v_value);

  /* 3., 4. */
  if (v_p == ecma_get_object_prototype (o_p))
  {
    return true;
  }

  /* 2., 5. */
  if (!ecma_get_object_extensible (o_p))
  {
    return false;
  }

  /* 6., 7., 8. */
  ecma_object_t *p_p = v_p;
  while (true)
  {
    /* a. */
    if (p_p == NULL)
    {
      break;
    }

    /* b. */
    if (p_p == o_p)
    {
      return false;
    }

    /* c.i. TODO: es2015-subset profile does not support having a different
     * [[GetPrototypeOf]] internal method */

    /* c.ii. */
    p_p = ecma_get_object_prototype (p_p);
  }

  /* 9. */
  ECMA_SET_POINTER (o_p->prototype_or_outer_reference_cp, v_p);

  /* 10. */
  return true;
} /* ecma_set_prototype_of */

/**
 * The Object object's 'setPrototypeOf' routine
 *
 * See also:
 *          ES2015 19.1.2.18
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_object_set_prototype_of (ecma_value_t this_arg, /**< 'this' argument */
                                             ecma_value_t arg1, /**< routine's first argument */
                                             ecma_value_t arg2) /**< routine's second argument */
{
  JERRY_UNUSED (this_arg);
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  /* 1., 2. */
  ECMA_TRY_CATCH (unused_value,
                  ecma_op_check_object_coercible (arg1),
                  ret_value);

  /* 3. */
  if (!ecma_is_value_object (arg2) && !ecma_is_value_null (arg2))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("proto is neither Object nor Null."));
  }
  else
  {
    /* 4. */
    if (!ecma_is_value_object (arg1))
    {
      ret_value = ecma_copy_value (arg1);
    }
    else
    {
      /* 5. */
      bool status = ecma_set_prototype_of (arg1, arg2);

      /* 6. TODO: es2015-subset profile does not support having a different
       * [[SetPrototypeOf]] internal method */

      /* 7. */
      if (!status)
      {
        ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("cannot set prototype."));
      }
      else
      {
        /* 8. */
        ret_value = ecma_copy_value (arg1);
      }
    }
  }

  ECMA_FINALIZE (unused_value);

  return ret_value;
} /* ecma_builtin_object_object_set_prototype_of */
#endif /* !CONFIG_DISABLE_ES2015_BUILTIN */

/**
 * The Object object's 'getOwnPropertyNames' routine
 *
 * See also:
 *          ECMA-262 v5, 15.2.3.4
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_object_get_own_property_names (ecma_value_t this_arg, /**< 'this' argument */
                                                   ecma_value_t arg) /**< routine's argument */
{
  JERRY_UNUSED (this_arg);
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  if (!ecma_is_value_object (arg))
  {
    /* 1. */
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Argument is not an object."));
  }
  else
  {
    ecma_object_t *obj_p = ecma_get_object_from_value (arg);
    /* 2-5. */
    ret_value = ecma_builtin_helper_object_get_properties (obj_p, ECMA_LIST_NO_OPTS);
  }

  return ret_value;
} /* ecma_builtin_object_object_get_own_property_names */

#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
/**
 * The Object object's 'getOwnPropertySymbols' routine
 *
 * See also:
 *          ECMA-262 v6, 19.1.2.7
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_object_get_own_property_symbols (ecma_value_t this_arg, /**< 'this' argument */
                                                     ecma_value_t arg) /**< routine's argument */
{
  JERRY_UNUSED (this_arg);

  if (!ecma_is_value_object (arg))
  {
    /* 1. */
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument is not an object."));
  }

  /* 2 - 5. */
  ecma_object_t *obj_p = ecma_get_object_from_value (arg);

  return ecma_builtin_helper_object_get_properties (obj_p, ECMA_LIST_SYMBOLS);
} /* ecma_builtin_object_object_get_own_property_symbols */
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */

/**
 * The Object object's 'seal' routine
 *
 * See also:
 *          ECMA-262 v5, 15.2.3.8
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_object_seal (ecma_value_t this_arg, /**< 'this' argument */
                                 ecma_value_t arg) /**< routine's argument */
{
  JERRY_UNUSED (this_arg);
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  /* 1. */
  if (!ecma_is_value_object (arg))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Argument is not an object."));
  }
  else
  {
    /* 2. */
    ecma_object_t *obj_p = ecma_get_object_from_value (arg);

    ecma_collection_header_t *props_p = ecma_op_object_get_property_names (obj_p, ECMA_LIST_NO_OPTS);

    ecma_value_t *ecma_value_p = ecma_collection_iterator_init (props_p);

    while (ecma_value_p != NULL && ecma_is_value_empty (ret_value))
    {
      ecma_string_t *property_name_p = ecma_get_string_from_value (*ecma_value_p);
      ecma_value_p = ecma_collection_iterator_next (ecma_value_p);

      /* 2.a */
      ecma_property_descriptor_t prop_desc;

      if (!ecma_op_object_get_own_property_descriptor (obj_p, property_name_p, &prop_desc))
      {
        continue;
      }

      /* 2.b */
      prop_desc.is_configurable = false;

      /* 2.c */
      ECMA_TRY_CATCH (define_own_prop_ret,
                      ecma_op_object_define_own_property (obj_p,
                                                          property_name_p,
                                                          &prop_desc,
                                                          true),
                      ret_value);
      ECMA_FINALIZE (define_own_prop_ret);

      ecma_free_property_descriptor (&prop_desc);
    }

    ecma_free_values_collection (props_p, 0);

    if (ecma_is_value_empty (ret_value))
    {
      /* 3. */
      ecma_set_object_extensible (obj_p, false);

      /* 4. */
      ret_value = ecma_copy_value (arg);
    }
  }

  return ret_value;
} /* ecma_builtin_object_object_seal */

/**
 * The Object object's 'freeze' routine
 *
 * See also:
 *          ECMA-262 v5, 15.2.3.9
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_object_freeze (ecma_value_t this_arg, /**< 'this' argument */
                                   ecma_value_t arg) /**< routine's argument */
{
  JERRY_UNUSED (this_arg);
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  /* 1. */
  if (!ecma_is_value_object (arg))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Argument is not an object."));
  }
  else
  {
    /* 2. */
    ecma_object_t *obj_p = ecma_get_object_from_value (arg);

    ecma_collection_header_t *props_p = ecma_op_object_get_property_names (obj_p, ECMA_LIST_NO_OPTS);

    ecma_value_t *ecma_value_p = ecma_collection_iterator_init (props_p);

    while (ecma_value_p != NULL && ecma_is_value_empty (ret_value))
    {
      ecma_string_t *property_name_p = ecma_get_string_from_value (*ecma_value_p);
      ecma_value_p = ecma_collection_iterator_next (ecma_value_p);

      /* 2.a */
      ecma_property_descriptor_t prop_desc;

      if (!ecma_op_object_get_own_property_descriptor (obj_p, property_name_p, &prop_desc))
      {
        continue;
      }

      /* 2.b */
      if (prop_desc.is_writable_defined && prop_desc.is_writable)
      {
        prop_desc.is_writable = false;
      }

      /* 2.c */
      prop_desc.is_configurable = false;

      /* 2.d */
      ECMA_TRY_CATCH (define_own_prop_ret,
                      ecma_op_object_define_own_property (obj_p,
                                                          property_name_p,
                                                          &prop_desc,
                                                          true),
                      ret_value);
      ECMA_FINALIZE (define_own_prop_ret);

      ecma_free_property_descriptor (&prop_desc);
    }

    ecma_free_values_collection (props_p, 0);

    if (ecma_is_value_empty (ret_value))
    {
      /* 3. */
      ecma_set_object_extensible (obj_p, false);

      /* 4. */
      ret_value = ecma_copy_value (arg);
    }
  }

  return ret_value;
} /* ecma_builtin_object_object_freeze */

/**
 * The Object object's 'preventExtensions' routine
 *
 * See also:
 *          ECMA-262 v5, 15.2.3.10
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_object_prevent_extensions (ecma_value_t this_arg, /**< 'this' argument */
                                               ecma_value_t arg) /**< routine's argument */
{
  JERRY_UNUSED (this_arg);
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  if (!ecma_is_value_object (arg))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Argument is not an object."));
  }
  else
  {
    ecma_object_t *obj_p = ecma_get_object_from_value (arg);
    ecma_set_object_extensible (obj_p, false);

    ret_value = ecma_copy_value (arg);
  }

  return ret_value;
} /* ecma_builtin_object_object_prevent_extensions */

/**
 * Common helper code for the 'isFrozen' and 'isSealed' fuctions of Object.
 *
 * See also:
 *         Object.isFrozen
 *         Object.isSealed
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_frozen_or_sealed_helper (ecma_value_t this_arg, /**< 'this' argument */
                                             ecma_value_t arg, /**< routine's argument */
                                             bool frozen_mode) /**< routine mode */
{
  JERRY_UNUSED (this_arg);
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  /* 1. */
  if (!ecma_is_value_object (arg))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Argument is not an object."));
  }
  else
  {
    ecma_object_t *obj_p = ecma_get_object_from_value (arg);

    /* 3. */
    if (ecma_get_object_extensible (obj_p))
    {
      ret_value = ECMA_VALUE_FALSE;
    }
    else
    {
      /* the value can be updated in the loop below */
      ret_value = ECMA_VALUE_TRUE;

      /* 2. */
      ecma_collection_header_t *props_p = ecma_op_object_get_property_names (obj_p, ECMA_LIST_NO_OPTS);

      ecma_value_t *ecma_value_p = ecma_collection_iterator_init (props_p);

      while (ecma_value_p != NULL)
      {
        ecma_string_t *property_name_p = ecma_get_string_from_value (*ecma_value_p);
        ecma_value_p = ecma_collection_iterator_next (ecma_value_p);

        /* 2.a */
        ecma_property_t property = ecma_op_object_get_own_property (obj_p,
                                                                    property_name_p,
                                                                    NULL,
                                                                    ECMA_PROPERTY_GET_NO_OPTIONS);

        /* 2.b for isFrozen */
        if (frozen_mode
            && ECMA_PROPERTY_GET_TYPE (property) != ECMA_PROPERTY_TYPE_NAMEDACCESSOR
            && ecma_is_property_writable (property))
        {
          ret_value = ECMA_VALUE_FALSE;
          break;
        }

        /* 2.b for isSealed, 2.c for isFrozen */
        if (ecma_is_property_configurable (property))
        {
          ret_value = ECMA_VALUE_FALSE;
          break;
        }
      }

      ecma_free_values_collection (props_p, 0);
    }
  }

  return ret_value;
} /* ecma_builtin_object_frozen_or_sealed_helper */

/**
 * The Object object's 'isSealed' routine
 *
 * See also:
 *          ECMA-262 v5, 15.2.3.11
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_object_is_sealed (ecma_value_t this_arg, /**< 'this' argument */
                                      ecma_value_t arg) /**< routine's argument */
{
  return ecma_builtin_object_frozen_or_sealed_helper (this_arg, arg, false);
} /* ecma_builtin_object_object_is_sealed */

/**
 * The Object object's 'isFrozen' routine
 *
 * See also:
 *          ECMA-262 v5, 15.2.3.12
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_object_is_frozen (ecma_value_t this_arg, /**< 'this' argument */
                                      ecma_value_t arg) /**< routine's argument */
{
  return ecma_builtin_object_frozen_or_sealed_helper (this_arg, arg, true);
} /* ecma_builtin_object_object_is_frozen */

/**
 * The Object object's 'isExtensible' routine
 *
 * See also:
 *          ECMA-262 v5, 15.2.3.13
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_object_is_extensible (ecma_value_t this_arg, /**< 'this' argument */
                                          ecma_value_t arg) /**< routine's argument */
{
  JERRY_UNUSED (this_arg);

  if (!ecma_is_value_object (arg))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument is not an object."));
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (arg);
  return ecma_make_boolean_value (ecma_get_object_extensible (obj_p));
} /* ecma_builtin_object_object_is_extensible */

/**
 * The Object object's 'keys' routine
 *
 * See also:
 *          ECMA-262 v5, 15.2.3.14
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_object_keys (ecma_value_t this_arg, /**< 'this' argument */
                                 ecma_value_t arg) /**< routine's argument */
{
  JERRY_UNUSED (this_arg);
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  if (!ecma_is_value_object (arg))
  {
    /* 1. */
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Argument is not an object."));
  }
  else
  {
    ecma_object_t *obj_p = ecma_get_object_from_value (arg);
    /* 3-6. */
    ret_value = ecma_builtin_helper_object_get_properties (obj_p, ECMA_LIST_ENUMERABLE);
  }

  return ret_value;
} /* ecma_builtin_object_object_keys */

/**
 * The Object object's 'getOwnPropertyDescriptor' routine
 *
 * See also:
 *          ECMA-262 v5, 15.2.3.3
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_object_get_own_property_descriptor (ecma_value_t this_arg, /**< 'this' argument */
                                                        ecma_value_t arg1, /**< routine's first argument */
                                                        ecma_value_t arg2) /**< routine's second argument */
{
  JERRY_UNUSED (this_arg);
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  /* 1. */
  if (!ecma_is_value_object (arg1))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument is not an object."));
  }

  ecma_string_t *name_str_p = ecma_op_to_prop_name (arg2);

  if (JERRY_UNLIKELY (name_str_p == NULL))
  {
    return ECMA_VALUE_ERROR;
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (arg1);

  /* 3. */
  ecma_property_descriptor_t prop_desc;

  if (ecma_op_object_get_own_property_descriptor (obj_p, name_str_p, &prop_desc))
  {
    /* 4. */
    ecma_object_t *desc_obj_p = ecma_op_from_property_descriptor (&prop_desc);

    ecma_free_property_descriptor (&prop_desc);

    ret_value = ecma_make_object_value (desc_obj_p);
  }
  else
  {
    ret_value = ECMA_VALUE_UNDEFINED;
  }

  ecma_deref_ecma_string (name_str_p);

  return ret_value;
} /* ecma_builtin_object_object_get_own_property_descriptor */

/**
 * The Object object's 'create' routine
 *
 * See also:
 *          ECMA-262 v5, 15.2.3.5
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_object_create (ecma_value_t this_arg, /**< 'this' argument */
                                   ecma_value_t arg1, /**< routine's first argument */
                                   ecma_value_t arg2) /**< routine's second argument */
{
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  /* 1. */
  if (!ecma_is_value_object (arg1) && !ecma_is_value_null (arg1))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Argument is not an object."));
  }
  else
  {
    ecma_object_t *obj_p = NULL;

    if (!ecma_is_value_null (arg1))
    {
      obj_p = ecma_get_object_from_value (arg1);
    }
    /* 2-3. */
    ecma_object_t *result_obj_p = ecma_op_create_object_object_noarg_and_set_prototype (obj_p);

    /* 4. */
    if (!ecma_is_value_undefined (arg2))
    {
      ECMA_TRY_CATCH (obj,
                      ecma_builtin_object_object_define_properties (this_arg,
                                                                    ecma_make_object_value (result_obj_p),
                                                                    arg2),
                      ret_value);
      ECMA_FINALIZE (obj);
    }

    /* 5. */
    if (ecma_is_value_empty (ret_value))
    {
      ret_value = ecma_copy_value (ecma_make_object_value (result_obj_p));
    }

    ecma_deref_object (result_obj_p);
  }

  return ret_value;
} /* ecma_builtin_object_object_create */

/**
 * The Object object's 'defineProperties' routine
 *
 * See also:
 *          ECMA-262 v5, 15.2.3.7
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_object_define_properties (ecma_value_t this_arg, /**< 'this' argument */
                                              ecma_value_t arg1, /**< routine's first argument */
                                              ecma_value_t arg2) /**< routine's second argument */
{
  JERRY_UNUSED (this_arg);
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  /* 1. */
  if (!ecma_is_value_object (arg1))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Argument is not an object."));
  }
  else
  {
    ecma_object_t *obj_p = ecma_get_object_from_value (arg1);

    /* 2. */
    ECMA_TRY_CATCH (props,
                    ecma_op_to_object (arg2),
                    ret_value);

    ecma_object_t *props_p = ecma_get_object_from_value (props);
    /* 3. */
    ecma_collection_header_t *prop_names_p = ecma_op_object_get_property_names (props_p, ECMA_LIST_ENUMERABLE);
    uint32_t property_number = prop_names_p->item_count;

    ecma_value_t *ecma_value_p = ecma_collection_iterator_init (prop_names_p);

    /* 4. */
    JMEM_DEFINE_LOCAL_ARRAY (property_descriptors, property_number, ecma_property_descriptor_t);

    uint32_t property_descriptor_number = 0;

    while (ecma_value_p != NULL && ecma_is_value_empty (ret_value))
    {
      /* 5.a */
      ECMA_TRY_CATCH (desc_obj,
                      ecma_op_object_get (props_p, ecma_get_string_from_value (*ecma_value_p)),
                      ret_value);

      /* 5.b */
      ECMA_TRY_CATCH (conv_result,
                      ecma_op_to_property_descriptor (desc_obj,
                                                      &property_descriptors[property_descriptor_number]),
                      ret_value);

      property_descriptor_number++;

      ECMA_FINALIZE (conv_result);
      ECMA_FINALIZE (desc_obj);

      ecma_value_p = ecma_collection_iterator_next (ecma_value_p);
    }

    /* 6. */
    ecma_value_p = ecma_collection_iterator_init (prop_names_p);

    for (uint32_t index = 0;
         index < property_number && ecma_is_value_empty (ret_value);
         index++)
    {
      ECMA_TRY_CATCH (define_own_prop_ret,
                      ecma_op_object_define_own_property (obj_p,
                                                          ecma_get_string_from_value (*ecma_value_p),
                                                          &property_descriptors[index],
                                                          true),
                      ret_value);

      ECMA_FINALIZE (define_own_prop_ret);

      ecma_value_p = ecma_collection_iterator_next (ecma_value_p);
    }

    /* Clean up. */
    for (uint32_t index = 0;
         index < property_descriptor_number;
         index++)
    {
      ecma_free_property_descriptor (&property_descriptors[index]);
    }

    JMEM_FINALIZE_LOCAL_ARRAY (property_descriptors);

    ecma_free_values_collection (prop_names_p, 0);

    /* 7. */
    if (ecma_is_value_empty (ret_value))
    {
      ret_value = ecma_copy_value (arg1);
    }

    ECMA_FINALIZE (props);
  }

  return ret_value;
} /* ecma_builtin_object_object_define_properties */

/**
 * The Object object's 'defineProperty' routine
 *
 * See also:
 *          ECMA-262 v5, 15.2.3.6
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_object_define_property (ecma_value_t this_arg, /**< 'this' argument */
                                            ecma_value_t arg1, /**< routine's first argument */
                                            ecma_value_t arg2, /**< routine's second argument */
                                            ecma_value_t arg3) /**< routine's third argument */
{
  JERRY_UNUSED (this_arg);
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  if (!ecma_is_value_object (arg1))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Argument is not an object."));
  }

  ecma_string_t *name_str_p = ecma_op_to_prop_name (arg2);

  if (JERRY_UNLIKELY (name_str_p == NULL))
  {
    return ECMA_VALUE_ERROR;
  }

  ecma_object_t *obj_p = ecma_get_object_from_value (arg1);

  ecma_property_descriptor_t prop_desc;

  ECMA_TRY_CATCH (conv_result,
                  ecma_op_to_property_descriptor (arg3, &prop_desc),
                  ret_value);

  ECMA_TRY_CATCH (define_own_prop_ret,
                  ecma_op_object_define_own_property (obj_p,
                                                      name_str_p,
                                                      &prop_desc,
                                                      true),
                  ret_value);

  ret_value = ecma_copy_value (arg1);

  ECMA_FINALIZE (define_own_prop_ret);
  ecma_free_property_descriptor (&prop_desc);
  ECMA_FINALIZE (conv_result);
  ecma_deref_ecma_string (name_str_p);

  return ret_value;
} /* ecma_builtin_object_object_define_property */

#ifndef CONFIG_DISABLE_ES2015_BUILTIN
/**
 * The Object object's 'assign' routine
 *
 * See also:
 *          ECMA-262 v6, 19.1.2.1
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_object_object_assign (ecma_value_t this_arg, /**< 'this' argument */
                                   const ecma_value_t arguments_list_p[], /**< arguments list */
                                   ecma_length_t arguments_list_len) /**< number of arguments */
{
  JERRY_UNUSED (this_arg);

  ecma_value_t target = arguments_list_len > 0 ? arguments_list_p[0] : ECMA_VALUE_UNDEFINED;

  /* 1. */
  ecma_value_t to_value = ecma_op_to_object (target);

  if (ECMA_IS_VALUE_ERROR (to_value))
  {
    return to_value;
  }

  ecma_object_t *to_obj_p = ecma_get_object_from_value (to_value);

  /* 2. */
  if (arguments_list_len == 1)
  {
    return to_value;
  }

  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  /* 4-5. */
  for (uint32_t i = 1; i < arguments_list_len && ecma_is_value_empty (ret_value); i++)
  {
    ecma_value_t next_source = arguments_list_p[i];

    /* 5.a */
    if (ecma_is_value_undefined (next_source) || ecma_is_value_null (next_source))
    {
      continue;
    }

    /* 5.b.i */
    ecma_value_t from_value = ecma_op_to_object (next_source);
    /* null and undefied cases are handled above, so this must be a valid object */
    JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (from_value));

    ecma_object_t *from_obj_p = ecma_get_object_from_value (from_value);

    /* 5.b.iii */
    /* TODO: extends this collection if symbols will be supported */
    ecma_collection_header_t *props_p = ecma_op_object_get_property_names (from_obj_p, ECMA_LIST_ENUMERABLE);

    ecma_value_t *ecma_value_p = ecma_collection_iterator_init (props_p);

    while (ecma_value_p != NULL && ecma_is_value_empty (ret_value))
    {
      ecma_string_t *property_name_p = ecma_get_string_from_value (*ecma_value_p);

      /* 5.c.i-ii */
      ecma_property_descriptor_t prop_desc;

      if (!ecma_op_object_get_own_property_descriptor (from_obj_p, property_name_p, &prop_desc))
      {
        continue;
      }

      /* 5.c.iii */
      if (prop_desc.is_enumerable
          && ((prop_desc.is_value_defined && !ecma_is_value_undefined (prop_desc.value))
              || prop_desc.is_get_defined))
      {
        /* 5.c.iii.1 */
        ecma_value_t prop_value = ecma_op_object_get (from_obj_p, property_name_p);

        /* 5.c.iii.2 */
        if (ECMA_IS_VALUE_ERROR (prop_value))
        {
          ret_value = prop_value;
        }
        else
        {
          /* 5.c.iii.3 */
          ecma_value_t status = ecma_op_object_put (to_obj_p, property_name_p, prop_value, true);

          /* 5.c.iii.4 */
          if (ECMA_IS_VALUE_ERROR (status))
          {
            ret_value = status;
          }
        }

        ecma_free_value (prop_value);
        ecma_free_property_descriptor (&prop_desc);
      }

      ecma_value_p = ecma_collection_iterator_next (ecma_value_p);
    }

    ecma_deref_object (from_obj_p);
    ecma_free_values_collection (props_p, 0);
  }

  /* 6. */
  if (ecma_is_value_empty (ret_value))
  {
    return to_value;
  }

  ecma_deref_object (to_obj_p);
  return ret_value;
} /* ecma_builtin_object_object_assign */
#endif /* !CONFIG_DISABLE_ES2015_BUILTIN */

/**
 * @}
 * @}
 * @}
 */
