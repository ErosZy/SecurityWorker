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

#include "common.h"

#include "ecma-alloc.h"
#include "ecma-array-object.h"
#include "ecma-builtins.h"
#include "ecma-comparison.h"
#include "ecma-conversion.h"
#include "ecma-exceptions.h"
#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-helpers.h"
#include "ecma-lcache.h"
#include "ecma-lex-env.h"
#include "ecma-objects.h"
#include "ecma-objects-general.h"
#include "ecma-regexp-object.h"
#include "ecma-try-catch-macro.h"
#include "jcontext.h"
#include "opcodes.h"
#include "vm.h"
#include "vm-stack.h"

/** \addtogroup vm Virtual machine
 * @{
 *
 * \addtogroup vm_executor Executor
 * @{
 */

/*
 * Check VM recursion depth limit
 */
#ifdef VM_RECURSION_LIMIT
JERRY_STATIC_ASSERT (VM_RECURSION_LIMIT > 0, vm_recursion_limit_must_be_greater_than_zero);
#endif /* VM_RECURSION_LIMIT */

/**
 * Get the value of object[property].
 *
 * @return ecma value
 */
static ecma_value_t
vm_op_get_value (ecma_value_t object, /**< base object */
                 ecma_value_t property) /**< property name */
{
  if (ecma_is_value_object (object))
  {
    ecma_string_t *property_name_p = NULL;

    if (ecma_is_value_integer_number (property))
    {
      ecma_integer_value_t int_value = ecma_get_integer_from_value (property);

      if (int_value >= 0 && int_value <= ECMA_DIRECT_STRING_MAX_IMM)
      {
        property_name_p = (ecma_string_t *) ECMA_CREATE_DIRECT_STRING (ECMA_DIRECT_STRING_UINT,
                                                                       (uintptr_t) int_value);
      }
    }
    else if (ecma_is_value_string (property))
    {
      property_name_p = ecma_get_string_from_value (property);
    }

#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
    if (ecma_is_value_symbol (property))
    {
      property_name_p = ecma_get_symbol_from_value (property);
    }
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */

    if (property_name_p != NULL)
    {
#ifndef CONFIG_ECMA_LCACHE_DISABLE
      ecma_object_t *object_p = ecma_get_object_from_value (object);
      ecma_property_t *property_p = ecma_lcache_lookup (object_p, property_name_p);

      if (property_p != NULL &&
          ECMA_PROPERTY_GET_TYPE (*property_p) == ECMA_PROPERTY_TYPE_NAMEDDATA)
      {
        return ecma_fast_copy_value (ECMA_PROPERTY_VALUE_PTR (property_p)->value);
      }
#endif /* !CONFIG_ECMA_LCACHE_DISABLE */

      /* There is no need to free the name. */
      return ecma_op_object_get (ecma_get_object_from_value (object), property_name_p);
    }
  }

  if (JERRY_UNLIKELY (ecma_is_value_undefined (object) || ecma_is_value_null (object)))
  {
#ifdef JERRY_ENABLE_ERROR_MESSAGES
    ecma_value_t error_value = ecma_raise_standard_error_with_format (ECMA_ERROR_TYPE,
                                                                      "Cannot read property '%' of %",
                                                                      property,
                                                                      object);
#else /* !JERRY_ENABLE_ERROR_MESSAGES */
    ecma_value_t error_value = ecma_raise_type_error (NULL);
#endif /* JERRY_ENABLE_ERROR_MESSAGES */
    return error_value;
  }

  ecma_string_t *property_name_p = ecma_op_to_prop_name (property);

  if (property_name_p == NULL)
  {
    return ECMA_VALUE_ERROR;
  }

  ecma_value_t get_value_result = ecma_op_get_value_object_base (object, property_name_p);

  ecma_deref_ecma_string (property_name_p);
  return get_value_result;
} /* vm_op_get_value */

/**
 * Set the value of object[property].
 *
 * Note:
 *  this function frees its object and property arguments
 *
 * @return an ecma value which contains an error
 *         if the property setting is unsuccessful
 */
static ecma_value_t
vm_op_set_value (ecma_value_t object, /**< base object */
                 ecma_value_t property, /**< property name */
                 ecma_value_t value, /**< ecma value */
                 bool is_strict) /**< strict mode */
{
  if (JERRY_UNLIKELY (!ecma_is_value_object (object)))
  {
    ecma_value_t to_object = ecma_op_to_object (object);
    ecma_free_value (object);

    if (ECMA_IS_VALUE_ERROR (to_object))
    {
#ifdef JERRY_ENABLE_ERROR_MESSAGES
      ecma_free_value (to_object);
      ecma_free_value (JERRY_CONTEXT (error_value));

      ecma_value_t error_value = ecma_raise_standard_error_with_format (ECMA_ERROR_TYPE,
                                                                        "Cannot set property '%' of %",
                                                                        property,
                                                                        object);
      ecma_free_value (property);

      return error_value;
#else /* !JERRY_ENABLE_ERROR_MESSAGES */
      ecma_free_value (property);
      return to_object;
#endif /* JERRY_ENABLE_ERROR_MESSAGES */
    }

    object = to_object;
  }

  ecma_string_t *property_p;
  ecma_object_t *object_p = ecma_get_object_from_value (object);

  if (!ecma_is_value_prop_name (property))
  {
    property_p = ecma_op_to_prop_name (property);
    ecma_fast_free_value (property);

    if (JERRY_UNLIKELY (property_p == NULL))
    {
      ecma_deref_object (object_p);
      return ECMA_VALUE_ERROR;
    }
  }
  else
  {
    property_p = ecma_get_prop_name_from_value (property);
  }

  ecma_value_t completion_value = ECMA_VALUE_EMPTY;

  if (!ecma_is_lexical_environment (object_p))
  {
    completion_value = ecma_op_object_put (object_p,
                                           property_p,
                                           value,
                                           is_strict);
  }
  else
  {
    completion_value = ecma_op_set_mutable_binding (object_p,
                                                    property_p,
                                                    value,
                                                    is_strict);
  }

  ecma_deref_object (object_p);
  ecma_deref_ecma_string (property_p);

  return completion_value;
} /* vm_op_set_value */

/** Compact bytecode define */
#define CBC_OPCODE(arg1, arg2, arg3, arg4) arg4,

/**
 * Decode table for both opcodes and extended opcodes.
 */
static const uint16_t vm_decode_table[] JERRY_CONST_DATA =
{
  CBC_OPCODE_LIST
  CBC_EXT_OPCODE_LIST
};

#undef CBC_OPCODE

/**
 * Run global code
 *
 * Note:
 *      returned value must be freed with ecma_free_value, when it is no longer needed.
 *
 * @return ecma value
 */
ecma_value_t
vm_run_global (const ecma_compiled_code_t *bytecode_p) /**< pointer to bytecode to run */
{
  ecma_object_t *glob_obj_p = ecma_builtin_get_global ();

  return vm_run (bytecode_p,
                 ecma_make_object_value (glob_obj_p),
                 ecma_get_global_environment (),
                 false,
                 NULL,
                 0);
} /* vm_run_global */

/**
 * Run specified eval-mode bytecode
 *
 * @return ecma value
 */
ecma_value_t
vm_run_eval (ecma_compiled_code_t *bytecode_data_p, /**< byte-code data */
             uint32_t parse_opts) /**< ecma_parse_opts_t option bits */
{
  ecma_value_t this_binding;
  ecma_object_t *lex_env_p;

  /* ECMA-262 v5, 10.4.2 */
  if (parse_opts & ECMA_PARSE_DIRECT_EVAL)
  {
    this_binding = ecma_copy_value (JERRY_CONTEXT (vm_top_context_p)->this_binding);
    lex_env_p = JERRY_CONTEXT (vm_top_context_p)->lex_env_p;

#ifdef JERRY_DEBUGGER
    uint32_t chain_index = parse_opts >> ECMA_PARSE_CHAIN_INDEX_SHIFT;

    while (chain_index != 0)
    {
      lex_env_p = ecma_get_lex_env_outer_reference (lex_env_p);

      if (JERRY_UNLIKELY (lex_env_p == NULL))
      {
        return ecma_raise_range_error (ECMA_ERR_MSG ("Invalid scope chain index for eval"));
      }

      if ((ecma_get_lex_env_type (lex_env_p) == ECMA_LEXICAL_ENVIRONMENT_THIS_OBJECT_BOUND)
          || (ecma_get_lex_env_type (lex_env_p) == ECMA_LEXICAL_ENVIRONMENT_DECLARATIVE))
      {
        chain_index--;
      }
    }
#endif
  }
  else
  {
    ecma_object_t *global_obj_p = ecma_builtin_get_global ();
    ecma_ref_object (global_obj_p);
    this_binding = ecma_make_object_value (global_obj_p);
    lex_env_p = ecma_get_global_environment ();
  }

  ecma_ref_object (lex_env_p);

  if ((bytecode_data_p->status_flags & CBC_CODE_FLAGS_STRICT_MODE) != 0)
  {
    ecma_object_t *strict_lex_env_p = ecma_create_decl_lex_env (lex_env_p);
    ecma_deref_object (lex_env_p);

    lex_env_p = strict_lex_env_p;
  }

  ecma_value_t completion_value = vm_run (bytecode_data_p,
                                          this_binding,
                                          lex_env_p,
                                          parse_opts,
                                          NULL,
                                          0);

  ecma_deref_object (lex_env_p);
  ecma_free_value (this_binding);

#ifdef JERRY_ENABLE_SNAPSHOT_EXEC
  if (!(bytecode_data_p->status_flags & CBC_CODE_FLAGS_STATIC_FUNCTION))
  {
    ecma_bytecode_deref (bytecode_data_p);
  }
#else /* !JERRY_ENABLE_SNAPSHOT_EXEC */
  ecma_bytecode_deref (bytecode_data_p);
#endif /* JERRY_ENABLE_SNAPSHOT_EXEC */

  return completion_value;
} /* vm_run_eval */

/**
 * Construct object
 *
 * @return object value
 */
static ecma_value_t
vm_construct_literal_object (vm_frame_ctx_t *frame_ctx_p, /**< frame context */
                             ecma_value_t lit_value) /**< literal */
{
  ecma_compiled_code_t *bytecode_p;

#ifdef JERRY_ENABLE_SNAPSHOT_EXEC
  if (JERRY_LIKELY (!(frame_ctx_p->bytecode_header_p->status_flags & CBC_CODE_FLAGS_STATIC_FUNCTION)))
  {
#endif /* JERRY_ENABLE_SNAPSHOT_EXEC */
    bytecode_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_compiled_code_t,
                                                  lit_value);
#ifdef JERRY_ENABLE_SNAPSHOT_EXEC
  }
  else
  {
    uint8_t *byte_p = ((uint8_t *) frame_ctx_p->bytecode_header_p) + lit_value;
    bytecode_p = (ecma_compiled_code_t *) byte_p;
  }
#endif /* JERRY_ENABLE_SNAPSHOT_EXEC */

#ifndef CONFIG_DISABLE_REGEXP_BUILTIN
  if (!(bytecode_p->status_flags & CBC_CODE_FLAGS_FUNCTION))
  {
    ecma_value_t ret_value;
    ret_value = ecma_op_create_regexp_object_from_bytecode ((re_compiled_code_t *) bytecode_p);

    if (ECMA_IS_VALUE_ERROR (ret_value))
    {
      /* TODO: throw exception instead of define an 'undefined' value. */
      return ECMA_VALUE_UNDEFINED;
    }

    return ret_value;
  }
#endif /* !CONFIG_DISABLE_REGEXP_BUILTIN */

  JERRY_ASSERT (bytecode_p->status_flags & CBC_CODE_FLAGS_FUNCTION);

  ecma_object_t *func_obj_p;

#ifndef CONFIG_DISABLE_ES2015_ARROW_FUNCTION
  if (!(bytecode_p->status_flags & CBC_CODE_FLAGS_ARROW_FUNCTION))
  {
#endif /* !CONFIG_DISABLE_ES2015_ARROW_FUNCTION */
    func_obj_p = ecma_op_create_function_object (frame_ctx_p->lex_env_p,
                                                 bytecode_p);
#ifndef CONFIG_DISABLE_ES2015_ARROW_FUNCTION
  }
  else
  {
    func_obj_p = ecma_op_create_arrow_function_object (frame_ctx_p->lex_env_p,
                                                       bytecode_p,
                                                       frame_ctx_p->this_binding);
  }
#endif /* !CONFIG_DISABLE_ES2015_ARROW_FUNCTION */

  return ecma_make_object_value (func_obj_p);
} /* vm_construct_literal_object */

/**
 * Get implicit this value
 *
 * @return true - if the implicit 'this' value is updated,
 *         false - otherwise
 */
static inline bool JERRY_ATTR_ALWAYS_INLINE
vm_get_implicit_this_value (ecma_value_t *this_value_p) /**< [in,out] this value */
{
  if (ecma_is_value_object (*this_value_p))
  {
    ecma_object_t *this_obj_p = ecma_get_object_from_value (*this_value_p);

    if (ecma_is_lexical_environment (this_obj_p))
    {
      ecma_value_t completion_value = ecma_op_implicit_this_value (this_obj_p);

      JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (completion_value));

      *this_value_p = completion_value;
      return true;
    }
  }
  return false;
} /* vm_get_implicit_this_value */

/**
 * Special bytecode sequence for error handling while the vm_loop
 * is preserved for an execute operation
 */
static const uint8_t vm_error_byte_code_p[] =
{
  CBC_EXT_OPCODE, CBC_EXT_ERROR
};

#ifndef CONFIG_DISABLE_ES2015_CLASS
/**
 * 'super(...)' function call handler.
 */
static void
vm_super_call (vm_frame_ctx_t *frame_ctx_p) /**< frame context */
{
  JERRY_ASSERT (frame_ctx_p->call_operation == VM_EXEC_SUPER_CALL);
  JERRY_ASSERT (frame_ctx_p->byte_code_p[0] == CBC_EXT_OPCODE);

  uint8_t *byte_code_p = frame_ctx_p->byte_code_p + 3;
  uint8_t opcode = byte_code_p[-2];
  uint32_t arguments_list_len = byte_code_p[-1];

  ecma_value_t *stack_top_p = frame_ctx_p->stack_top_p - arguments_list_len;

  ecma_value_t func_value = stack_top_p[-1];
  ecma_value_t completion_value;
  ecma_op_set_super_called (frame_ctx_p->lex_env_p);
  ecma_value_t this_value = ecma_op_get_class_this_binding (frame_ctx_p->lex_env_p);

  if (!ecma_is_constructor (func_value))
  {
    completion_value = ecma_raise_type_error ("Class extends value is not a constructor.");
  }
  else
  {
    completion_value = ecma_op_function_construct (ecma_get_object_from_value (func_value),
                                                   this_value,
                                                   stack_top_p,
                                                   arguments_list_len);

    if (this_value != completion_value && ecma_is_value_object (completion_value))
    {
      ecma_op_set_class_prototype (completion_value, this_value);
      ecma_op_set_class_this_binding (frame_ctx_p->lex_env_p, completion_value);
    }
  }

  /* Free registers. */
  for (uint32_t i = 0; i < arguments_list_len; i++)
  {
    ecma_fast_free_value (stack_top_p[i]);
  }

  if (JERRY_UNLIKELY (ECMA_IS_VALUE_ERROR (completion_value)))
  {
#ifdef JERRY_DEBUGGER
    JERRY_CONTEXT (debugger_exception_byte_code_p) = frame_ctx_p->byte_code_p;
#endif /* JERRY_DEBUGGER */
    frame_ctx_p->byte_code_p = (uint8_t *) vm_error_byte_code_p;
  }
  else
  {
    frame_ctx_p->byte_code_p = byte_code_p;
    ecma_free_value (*(--stack_top_p));
    uint32_t opcode_data = vm_decode_table[(CBC_END + 1) + opcode];

    if (!(opcode_data & (VM_OC_PUT_STACK | VM_OC_PUT_BLOCK)))
    {
      ecma_fast_free_value (completion_value);
    }
    else if (opcode_data & VM_OC_PUT_STACK)
    {
      *stack_top_p++ = completion_value;
    }
    else
    {
      ecma_fast_free_value (frame_ctx_p->block_result);
      frame_ctx_p->block_result = completion_value;
    }
  }

  frame_ctx_p->stack_top_p = stack_top_p;
} /* vm_super_call */
#endif /* !CONFIG_DISABLE_ES2015_CLASS */

/**
 * 'Function call' opcode handler.
 *
 * See also: ECMA-262 v5, 11.2.3
 */
static void
opfunc_call (vm_frame_ctx_t *frame_ctx_p) /**< frame context */
{
  uint8_t *byte_code_p = frame_ctx_p->byte_code_p + 1;
  uint8_t opcode = byte_code_p[-1];
  uint32_t arguments_list_len;

  if (opcode >= CBC_CALL0)
  {
    arguments_list_len = (unsigned int) ((opcode - CBC_CALL0) / 6);
  }
  else
  {
    arguments_list_len = *byte_code_p++;
  }

  bool is_call_prop = ((opcode - CBC_CALL) % 6) >= 3;

  ecma_value_t *stack_top_p = frame_ctx_p->stack_top_p - arguments_list_len;
  ecma_value_t this_value = is_call_prop ? stack_top_p[-3] : ECMA_VALUE_UNDEFINED;
  ecma_value_t func_value = stack_top_p[-1];
  ecma_value_t completion_value;

  if (!ecma_op_is_callable (func_value))
  {
    completion_value = ecma_raise_type_error (ECMA_ERR_MSG ("Expected a function."));
  }
  else
  {
    ecma_object_t *func_obj_p = ecma_get_object_from_value (func_value);

    completion_value = ecma_op_function_call (func_obj_p,
                                              this_value,
                                              stack_top_p,
                                              arguments_list_len);
  }

  JERRY_CONTEXT (status_flags) &= (uint32_t) ~ECMA_STATUS_DIRECT_EVAL;

  /* Free registers. */
  for (uint32_t i = 0; i < arguments_list_len; i++)
  {
    ecma_fast_free_value (stack_top_p[i]);
  }

  if (is_call_prop)
  {
    ecma_free_value (*(--stack_top_p));
    ecma_free_value (*(--stack_top_p));
  }

  if (JERRY_UNLIKELY (ECMA_IS_VALUE_ERROR (completion_value)))
  {
#ifdef JERRY_DEBUGGER
    JERRY_CONTEXT (debugger_exception_byte_code_p) = frame_ctx_p->byte_code_p;
#endif /* JERRY_DEBUGGER */
    frame_ctx_p->byte_code_p = (uint8_t *) vm_error_byte_code_p;
  }
  else
  {
    frame_ctx_p->byte_code_p = byte_code_p;
    ecma_free_value (*(--stack_top_p));
    uint32_t opcode_data = vm_decode_table[opcode];

    if (!(opcode_data & (VM_OC_PUT_STACK | VM_OC_PUT_BLOCK)))
    {
      ecma_fast_free_value (completion_value);
    }
    else if (opcode_data & VM_OC_PUT_STACK)
    {
      *stack_top_p++ = completion_value;
    }
    else
    {
      ecma_fast_free_value (frame_ctx_p->block_result);
      frame_ctx_p->block_result = completion_value;
    }
  }

  frame_ctx_p->stack_top_p = stack_top_p;
} /* opfunc_call */

/**
 * 'Constructor call' opcode handler.
 *
 * See also: ECMA-262 v5, 11.2.2
 */
static void
opfunc_construct (vm_frame_ctx_t *frame_ctx_p) /**< frame context */
{
  uint8_t *byte_code_p = frame_ctx_p->byte_code_p + 1;
  uint8_t opcode = byte_code_p[-1];
  unsigned int arguments_list_len;

  if (opcode >= CBC_NEW0)
  {
    arguments_list_len = (unsigned int) (opcode - CBC_NEW0);
  }
  else
  {
    arguments_list_len = *byte_code_p++;
  }

  ecma_value_t *stack_top_p = frame_ctx_p->stack_top_p - arguments_list_len;
  ecma_value_t constructor_value = stack_top_p[-1];
  ecma_value_t completion_value;

  if (!ecma_is_constructor (constructor_value))
  {
    completion_value = ecma_raise_type_error (ECMA_ERR_MSG ("Expected a constructor."));
  }
  else
  {
    ecma_object_t *constructor_obj_p = ecma_get_object_from_value (constructor_value);

    completion_value = ecma_op_function_construct (constructor_obj_p,
                                                   ECMA_VALUE_UNDEFINED,
                                                   stack_top_p,
                                                   arguments_list_len);
  }

  /* Free registers. */
  for (uint32_t i = 0; i < arguments_list_len; i++)
  {
    ecma_fast_free_value (stack_top_p[i]);
  }

  if (JERRY_UNLIKELY (ECMA_IS_VALUE_ERROR (completion_value)))
  {
#ifdef JERRY_DEBUGGER
    JERRY_CONTEXT (debugger_exception_byte_code_p) = frame_ctx_p->byte_code_p;
#endif /* JERRY_DEBUGGER */
    frame_ctx_p->byte_code_p = (uint8_t *) vm_error_byte_code_p;
  }
  else
  {
    ecma_free_value (stack_top_p[-1]);
    frame_ctx_p->byte_code_p = byte_code_p;
    stack_top_p[-1] = completion_value;
  }

  frame_ctx_p->stack_top_p = stack_top_p;
} /* opfunc_construct */

/**
 * Read literal index from the byte code stream into destination.
 *
 * @param destination destination
 */
#define READ_LITERAL_INDEX(destination) \
  do \
  { \
    (destination) = *byte_code_p++; \
    if ((destination) >= encoding_limit) \
    { \
      (destination) = (uint16_t) ((((destination) << 8) | *byte_code_p++) - encoding_delta); \
    } \
  } \
  while (0)

/**
 * Get literal value by literal index.
 *
 * @param literal_index literal index
 * @param target_value target value
 *
 * TODO: For performance reasons, we define this as a macro.
 * When we are able to construct a function with similar speed,
 * we can remove this macro.
 */
#define READ_LITERAL(literal_index, target_value) \
  do \
  { \
    if ((literal_index) < ident_end) \
    { \
      if ((literal_index) < register_end) \
      { \
        /* Note: There should be no specialization for arguments. */ \
        (target_value) = ecma_fast_copy_value (frame_ctx_p->registers_p[literal_index]); \
      } \
      else \
      { \
        ecma_string_t *name_p = ecma_get_string_from_value (literal_start_p[literal_index]); \
        \
        result = ecma_op_resolve_reference_value (frame_ctx_p->lex_env_p, \
                                                  name_p); \
        \
        if (ECMA_IS_VALUE_ERROR (result)) \
        { \
          goto error; \
        } \
        (target_value) = result; \
      } \
    } \
    else if (literal_index < const_literal_end) \
    { \
      (target_value) = ecma_fast_copy_value (literal_start_p[literal_index]); \
    } \
    else \
    { \
      /* Object construction. */ \
      (target_value) = vm_construct_literal_object (frame_ctx_p, \
                                                    literal_start_p[literal_index]); \
    } \
  } \
  while (0)

/**
 * Run initializer byte codes.
 *
 * @return ecma value
 */
static void
vm_init_loop (vm_frame_ctx_t *frame_ctx_p) /**< frame context */
{
  const ecma_compiled_code_t *bytecode_header_p = frame_ctx_p->bytecode_header_p;
  uint8_t *byte_code_p = frame_ctx_p->byte_code_p;
  uint16_t encoding_limit;
  uint16_t encoding_delta;
  uint16_t register_end;
  ecma_value_t *literal_start_p = frame_ctx_p->literal_start_p;
  bool is_strict = ((frame_ctx_p->bytecode_header_p->status_flags & CBC_CODE_FLAGS_STRICT_MODE) != 0);

  /* Prepare. */
  if (!(bytecode_header_p->status_flags & CBC_CODE_FLAGS_FULL_LITERAL_ENCODING))
  {
    encoding_limit = 255;
    encoding_delta = 0xfe01;
  }
  else
  {
    encoding_limit = 128;
    encoding_delta = 0x8000;
  }

  if (frame_ctx_p->bytecode_header_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
  {
    cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) (frame_ctx_p->bytecode_header_p);
    register_end = args_p->register_end;
  }
  else
  {
    cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) (frame_ctx_p->bytecode_header_p);
    register_end = args_p->register_end;
  }

  while (true)
  {
    switch (*byte_code_p)
    {
      case CBC_DEFINE_VARS:
      {
        uint32_t literal_index_end;
        uint32_t literal_index = register_end;

        byte_code_p++;
        READ_LITERAL_INDEX (literal_index_end);

        while (literal_index <= literal_index_end)
        {
          ecma_string_t *name_p = ecma_get_string_from_value (literal_start_p[literal_index]);
          vm_var_decl (frame_ctx_p, name_p);
          literal_index++;
        }
        break;
      }

      case CBC_INITIALIZE_VAR:
      case CBC_INITIALIZE_VARS:
      {
        uint8_t type = *byte_code_p;
        uint32_t literal_index;
        uint32_t literal_index_end;

        byte_code_p++;
        READ_LITERAL_INDEX (literal_index);

        if (type == CBC_INITIALIZE_VAR)
        {
          literal_index_end = literal_index;
        }
        else
        {
          READ_LITERAL_INDEX (literal_index_end);
        }

        while (literal_index <= literal_index_end)
        {
          uint32_t value_index;
          ecma_value_t lit_value;

          READ_LITERAL_INDEX (value_index);

          if (value_index < register_end)
          {
            lit_value = frame_ctx_p->registers_p[value_index];
          }
          else
          {
            lit_value = vm_construct_literal_object (frame_ctx_p,
                                                     literal_start_p[value_index]);
          }

          if (literal_index < register_end)
          {
            frame_ctx_p->registers_p[literal_index] = lit_value;
          }
          else
          {
            ecma_string_t *name_p = ecma_get_string_from_value (literal_start_p[literal_index]);

            vm_var_decl (frame_ctx_p, name_p);

            ecma_object_t *ref_base_lex_env_p = ecma_op_resolve_reference_base (frame_ctx_p->lex_env_p, name_p);

            ecma_value_t put_value_result = ecma_op_put_value_lex_env_base (ref_base_lex_env_p,
                                                                            name_p,
                                                                            is_strict,
                                                                            lit_value);

            JERRY_ASSERT (ecma_is_value_boolean (put_value_result)
                          || ecma_is_value_empty (put_value_result)
                          || ECMA_IS_VALUE_ERROR (put_value_result));

            if (ECMA_IS_VALUE_ERROR (put_value_result))
            {
              ecma_free_value (JERRY_CONTEXT (error_value));
            }

            if (value_index >= register_end)
            {
              ecma_free_value (lit_value);
            }
          }

          literal_index++;
        }
        break;
      }

#ifdef JERRY_ENABLE_SNAPSHOT_EXEC
      case CBC_SET_BYTECODE_PTR:
      {
        memcpy (&byte_code_p, byte_code_p + 1, sizeof (uint8_t *));
        frame_ctx_p->byte_code_start_p = byte_code_p;
        break;
      }
#endif /* JERRY_ENABLE_SNAPSHOT_EXEC */

      default:
      {
        frame_ctx_p->byte_code_p = byte_code_p;
        return;
      }
    }
  }
} /* vm_init_loop */

/**
 * Run generic byte code.
 *
 * @return ecma value
 */
static ecma_value_t JERRY_ATTR_NOINLINE
vm_loop (vm_frame_ctx_t *frame_ctx_p) /**< frame context */
{
  const ecma_compiled_code_t *bytecode_header_p = frame_ctx_p->bytecode_header_p;
  uint8_t *byte_code_p = frame_ctx_p->byte_code_p;
  ecma_value_t *literal_start_p = frame_ctx_p->literal_start_p;

  ecma_value_t *stack_top_p;
  uint16_t encoding_limit;
  uint16_t encoding_delta;
  uint16_t register_end;
  uint16_t ident_end;
  uint16_t const_literal_end;
  int32_t branch_offset = 0;
  uint8_t branch_offset_length = 0;
  ecma_value_t left_value;
  ecma_value_t right_value;
  ecma_value_t result = ECMA_VALUE_EMPTY;
  bool is_strict = ((frame_ctx_p->bytecode_header_p->status_flags & CBC_CODE_FLAGS_STRICT_MODE) != 0);

  /* Prepare for byte code execution. */
  if (!(bytecode_header_p->status_flags & CBC_CODE_FLAGS_FULL_LITERAL_ENCODING))
  {
    encoding_limit = 255;
    encoding_delta = 0xfe01;
  }
  else
  {
    encoding_limit = 128;
    encoding_delta = 0x8000;
  }

  if (bytecode_header_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
  {
    cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) (bytecode_header_p);
    register_end = args_p->register_end;
    ident_end = args_p->ident_end;
    const_literal_end = args_p->const_literal_end;
  }
  else
  {
    cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) (bytecode_header_p);
    register_end = args_p->register_end;
    ident_end = args_p->ident_end;
    const_literal_end = args_p->const_literal_end;
  }

  stack_top_p = frame_ctx_p->stack_top_p;

  /* Outer loop for exception handling. */
  while (true)
  {
    /* Internal loop for byte code execution. */
    while (true)
    {
      uint8_t *byte_code_start_p = byte_code_p;
      uint8_t opcode = *byte_code_p++;
      uint32_t opcode_data = opcode;

      if (opcode == CBC_EXT_OPCODE)
      {
        opcode = *byte_code_p++;
        opcode_data = (uint32_t) ((CBC_END + 1) + opcode);
      }

      opcode_data = vm_decode_table[opcode_data];

      left_value = ECMA_VALUE_UNDEFINED;
      right_value = ECMA_VALUE_UNDEFINED;

      uint32_t operands = VM_OC_GET_ARGS_INDEX (opcode_data);

      if (operands >= VM_OC_GET_LITERAL)
      {
        uint16_t literal_index;
        READ_LITERAL_INDEX (literal_index);
        READ_LITERAL (literal_index, left_value);

        if (operands != VM_OC_GET_LITERAL)
        {
          switch (operands)
          {
            case VM_OC_GET_LITERAL_LITERAL:
            {
              uint16_t second_literal_index;
              READ_LITERAL_INDEX (second_literal_index);
              READ_LITERAL (second_literal_index, right_value);
              break;
            }
            case VM_OC_GET_STACK_LITERAL:
            {
              JERRY_ASSERT (stack_top_p > frame_ctx_p->registers_p + register_end);
              right_value = left_value;
              left_value = *(--stack_top_p);
              break;
            }
            default:
            {
              JERRY_ASSERT (operands == VM_OC_GET_THIS_LITERAL);

              right_value = left_value;
              left_value = ecma_copy_value (frame_ctx_p->this_binding);
              break;
            }
          }
        }
      }
      else if (operands >= VM_OC_GET_STACK)
      {
        JERRY_ASSERT (operands == VM_OC_GET_STACK
                      || operands == VM_OC_GET_STACK_STACK);

        JERRY_ASSERT (stack_top_p > frame_ctx_p->registers_p + register_end);
        left_value = *(--stack_top_p);

        if (operands == VM_OC_GET_STACK_STACK)
        {
          JERRY_ASSERT (stack_top_p > frame_ctx_p->registers_p + register_end);
          right_value = left_value;
          left_value = *(--stack_top_p);
        }
      }
      else if (operands == VM_OC_GET_BRANCH)
      {
        branch_offset_length = CBC_BRANCH_OFFSET_LENGTH (opcode);
        JERRY_ASSERT (branch_offset_length >= 1 && branch_offset_length <= 3);

        branch_offset = *(byte_code_p++);

        if (JERRY_UNLIKELY (branch_offset_length != 1))
        {
          branch_offset <<= 8;
          branch_offset |= *(byte_code_p++);

          if (JERRY_UNLIKELY (branch_offset_length == 3))
          {
            branch_offset <<= 8;
            branch_offset |= *(byte_code_p++);
          }
        }

        if (opcode_data & VM_OC_BACKWARD_BRANCH)
        {
#ifdef JERRY_VM_EXEC_STOP
          if (JERRY_CONTEXT (vm_exec_stop_cb) != NULL
              && --JERRY_CONTEXT (vm_exec_stop_counter) == 0)
          {
            result = JERRY_CONTEXT (vm_exec_stop_cb) (JERRY_CONTEXT (vm_exec_stop_user_p));

            if (ecma_is_value_undefined (result))
            {
              JERRY_CONTEXT (vm_exec_stop_counter) = JERRY_CONTEXT (vm_exec_stop_frequency);
            }
            else
            {
              JERRY_CONTEXT (vm_exec_stop_counter) = 1;

              if (!ecma_is_value_error_reference (result))
              {
                JERRY_CONTEXT (error_value) = result;
              }
              else
              {
                JERRY_CONTEXT (error_value) = ecma_clear_error_reference (result, false);
              }

              JERRY_CONTEXT (status_flags) &= (uint32_t) ~ECMA_STATUS_EXCEPTION;
              result = ECMA_VALUE_ERROR;
              goto error;
            }
          }
#endif /* JERRY_VM_EXEC_STOP */

          branch_offset = -branch_offset;
        }
      }

      switch (VM_OC_GROUP_GET_INDEX (opcode_data))
      {
        case VM_OC_POP:
        {
          JERRY_ASSERT (stack_top_p > frame_ctx_p->registers_p + register_end);
          ecma_free_value (*(--stack_top_p));
          continue;
        }
        case VM_OC_POP_BLOCK:
        {
          ecma_fast_free_value (frame_ctx_p->block_result);
          frame_ctx_p->block_result = *(--stack_top_p);
          continue;
        }
        case VM_OC_PUSH:
        {
          *stack_top_p++ = left_value;
          continue;
        }
        case VM_OC_PUSH_TWO:
        {
          *stack_top_p++ = left_value;
          *stack_top_p++ = right_value;
          continue;
        }
        case VM_OC_PUSH_THREE:
        {
          uint16_t literal_index;

          *stack_top_p++ = left_value;
          left_value = ECMA_VALUE_UNDEFINED;

          READ_LITERAL_INDEX (literal_index);
          READ_LITERAL (literal_index, left_value);

          *stack_top_p++ = right_value;
          *stack_top_p++ = left_value;
          continue;
        }
        case VM_OC_PUSH_UNDEFINED:
        {
          *stack_top_p++ = ECMA_VALUE_UNDEFINED;
          continue;
        }
        case VM_OC_PUSH_TRUE:
        {
          *stack_top_p++ = ECMA_VALUE_TRUE;
          continue;
        }
        case VM_OC_PUSH_FALSE:
        {
          *stack_top_p++ = ECMA_VALUE_FALSE;
          continue;
        }
        case VM_OC_PUSH_NULL:
        {
          *stack_top_p++ = ECMA_VALUE_NULL;
          continue;
        }
        case VM_OC_PUSH_THIS:
        {
          *stack_top_p++ = ecma_copy_value (frame_ctx_p->this_binding);
          continue;
        }
        case VM_OC_PUSH_0:
        {
          *stack_top_p++ = ecma_make_integer_value (0);
          continue;
        }
        case VM_OC_PUSH_POS_BYTE:
        {
          ecma_integer_value_t number = *byte_code_p++;
          *stack_top_p++ = ecma_make_integer_value (number + 1);
          continue;
        }
        case VM_OC_PUSH_NEG_BYTE:
        {
          ecma_integer_value_t number = *byte_code_p++;
          *stack_top_p++ = ecma_make_integer_value (-(number + 1));
          continue;
        }
        case VM_OC_PUSH_LIT_0:
        {
          stack_top_p[0] = left_value;
          stack_top_p[1] = ecma_make_integer_value (0);
          stack_top_p += 2;
          continue;
        }
        case VM_OC_PUSH_LIT_POS_BYTE:
        {
          ecma_integer_value_t number = *byte_code_p++;
          stack_top_p[0] = left_value;
          stack_top_p[1] = ecma_make_integer_value (number + 1);
          stack_top_p += 2;
          continue;
        }
        case VM_OC_PUSH_LIT_NEG_BYTE:
        {
          ecma_integer_value_t number = *byte_code_p++;
          stack_top_p[0] = left_value;
          stack_top_p[1] = ecma_make_integer_value (-(number + 1));
          stack_top_p += 2;
          continue;
        }
        case VM_OC_PUSH_OBJECT:
        {
          ecma_object_t *obj_p = ecma_create_object (ecma_builtin_get (ECMA_BUILTIN_ID_OBJECT_PROTOTYPE),
                                                     0,
                                                     ECMA_OBJECT_TYPE_GENERAL);

          *stack_top_p++ = ecma_make_object_value (obj_p);
          continue;
        }
        case VM_OC_PUSH_NAMED_FUNC_EXPR:
        {
          ecma_object_t *func_p = ecma_get_object_from_value (left_value);

          JERRY_ASSERT (ecma_get_object_type (func_p) == ECMA_OBJECT_TYPE_FUNCTION);

          ecma_extended_object_t *ext_func_p = (ecma_extended_object_t *) func_p;

          JERRY_ASSERT (frame_ctx_p->lex_env_p == ECMA_GET_INTERNAL_VALUE_POINTER (ecma_object_t,
                                                                                   ext_func_p->u.function.scope_cp));

          ecma_object_t *name_lex_env = ecma_create_decl_lex_env (frame_ctx_p->lex_env_p);

          ecma_op_create_immutable_binding (name_lex_env, ecma_get_string_from_value (right_value), left_value);

          ECMA_SET_INTERNAL_VALUE_POINTER (ext_func_p->u.function.scope_cp, name_lex_env);

          ecma_free_value (right_value);
          ecma_deref_object (name_lex_env);
          *stack_top_p++ = left_value;
          continue;
        }
#ifndef CONFIG_DISABLE_ES2015_OBJECT_INITIALIZER
        case VM_OC_SET_COMPUTED_PROPERTY:
        {
          /* Swap values. */
          left_value ^= right_value;
          right_value ^= left_value;
          left_value ^= right_value;
          /* FALLTHRU */
        }
#endif /* !CONFIG_DISABLE_ES2015_OBJECT_INITIALIZER */
        case VM_OC_SET_PROPERTY:
        {
          JERRY_STATIC_ASSERT (VM_OC_NON_STATIC_FLAG == VM_OC_BACKWARD_BRANCH,
                               vm_oc_non_static_flag_must_be_equal_to_vm_oc_backward_branch);

          JERRY_ASSERT ((opcode_data >> VM_OC_NON_STATIC_SHIFT) <= 0x1);

          result = right_value;

          if (JERRY_UNLIKELY (!ecma_is_value_string (right_value)))
          {
            result = ecma_op_to_string (right_value);

            if (ECMA_IS_VALUE_ERROR (result))
            {
              goto error;
            }
          }

          ecma_string_t *prop_name_p = ecma_get_string_from_value (result);

#ifndef CONFIG_DISABLE_ES2015_CLASS
          if (JERRY_UNLIKELY (ecma_compare_ecma_string_to_magic_id (prop_name_p, LIT_MAGIC_STRING_PROTOTYPE))
              && !(opcode_data & VM_OC_NON_STATIC_FLAG))
          {
            if (!ecma_is_value_string (right_value))
            {
              ecma_deref_ecma_string (prop_name_p);
            }

            result = ecma_raise_type_error (ECMA_ERR_MSG ("prototype property of a class is non-configurable"));
            goto error;
          }

          const int index = (int) (opcode_data >> VM_OC_NON_STATIC_SHIFT) - 2;
#else /* CONFIG_DISABLE_ES2015_CLASS */
          const int index = -1;
#endif /* !CONFIG_DISABLE_ES2015_CLASS */

          ecma_object_t *object_p = ecma_get_object_from_value (stack_top_p[index]);
          ecma_property_t *property_p = ecma_find_named_property (object_p, prop_name_p);

          if (property_p != NULL
              && ECMA_PROPERTY_GET_TYPE (*property_p) != ECMA_PROPERTY_TYPE_NAMEDDATA)
          {
            ecma_delete_property (object_p, ECMA_PROPERTY_VALUE_PTR (property_p));
            property_p = NULL;
          }

          ecma_property_value_t *prop_value_p;

          if (property_p == NULL)
          {
            prop_value_p = ecma_create_named_data_property (object_p,
                                                            prop_name_p,
                                                            ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE,
                                                            NULL);
          }
          else
          {
            prop_value_p = ECMA_PROPERTY_VALUE_PTR (property_p);
          }

          ecma_named_data_property_assign_value (object_p, prop_value_p, left_value);

          if (!ecma_is_value_string (right_value))
          {
            ecma_deref_ecma_string (prop_name_p);
          }

          goto free_both_values;
        }
        case VM_OC_SET_GETTER:
        case VM_OC_SET_SETTER:
        {
          JERRY_ASSERT ((opcode_data >> VM_OC_NON_STATIC_SHIFT) <= 0x1);

          result = left_value;

          if (JERRY_UNLIKELY (!ecma_is_value_string (left_value)))
          {
            result = ecma_op_to_string (left_value);

            if (ECMA_IS_VALUE_ERROR (result))
            {
              goto error;
            }
          }

          ecma_string_t *prop_name_p = ecma_get_string_from_value (result);

#ifndef CONFIG_DISABLE_ES2015_CLASS
          if (JERRY_UNLIKELY (ecma_compare_ecma_string_to_magic_id (prop_name_p, LIT_MAGIC_STRING_PROTOTYPE))
              && !(opcode_data & VM_OC_NON_STATIC_FLAG))
          {
            if (!ecma_is_value_string (left_value))
            {
              ecma_deref_ecma_string (prop_name_p);
            }

            result = ecma_raise_type_error (ECMA_ERR_MSG ("prototype property of a class is non-configurable"));
            goto error;
          }

          const int index = (int) (opcode_data >> VM_OC_NON_STATIC_SHIFT) - 2;
#else /* CONFIG_DISABLE_ES2015_CLASS */
          const int index = -1;
#endif /* !CONFIG_DISABLE_ES2015_CLASS */

          opfunc_set_accessor (VM_OC_GROUP_GET_INDEX (opcode_data) == VM_OC_SET_GETTER,
                               stack_top_p[index],
                               prop_name_p,
                               right_value);

          if (!ecma_is_value_string (left_value))
          {
            ecma_deref_ecma_string (prop_name_p);
          }

          goto free_both_values;
        }
        case VM_OC_PUSH_ARRAY:
        {
          result = ecma_op_create_array_object (NULL, 0, false);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          continue;
        }
#ifndef CONFIG_DISABLE_ES2015_CLASS
        case VM_OC_SUPER_CALL:
        {
          frame_ctx_p->call_operation = VM_EXEC_SUPER_CALL;
          frame_ctx_p->byte_code_p = byte_code_start_p;
          frame_ctx_p->stack_top_p = stack_top_p;
          return ECMA_VALUE_UNDEFINED;
        }
        case VM_OC_CLASS_HERITAGE:
        {
          ecma_value_t super_value = *(--stack_top_p);
          ecma_object_t *super_class_p;
          branch_offset += (int32_t) (byte_code_start_p - frame_ctx_p->byte_code_start_p);

          JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p);

          if (ecma_is_value_null (super_value))
          {
            super_class_p = ecma_create_object (ecma_builtin_get (ECMA_BUILTIN_ID_OBJECT_PROTOTYPE),
                                                0,
                                                ECMA_OBJECT_TYPE_GENERAL);
          }
          else
          {
            result = ecma_op_to_object (super_value);
            ecma_free_value (super_value);

            if (ECMA_IS_VALUE_ERROR (result) || !ecma_is_constructor (result))
            {
              if (ECMA_IS_VALUE_ERROR (result))
              {
                ecma_free_value (JERRY_CONTEXT (error_value));
              }

              ecma_free_value (result);

              result = ecma_raise_type_error ("Value provided by class extends is not an object or null.");
              goto error;
            }
            else
            {
              super_class_p = ecma_get_object_from_value (result);
            }
          }

          ecma_object_t *super_env_p = ecma_create_object_lex_env (frame_ctx_p->lex_env_p,
                                                                   super_class_p,
                                                                   ECMA_LEXICAL_ENVIRONMENT_SUPER_OBJECT_BOUND);

          ecma_deref_object (super_class_p);

          VM_PLUS_EQUAL_U16 (frame_ctx_p->context_depth, PARSER_SUPER_CLASS_CONTEXT_STACK_ALLOCATION);
          stack_top_p += PARSER_SUPER_CLASS_CONTEXT_STACK_ALLOCATION;

          stack_top_p[-1] = VM_CREATE_CONTEXT (VM_CONTEXT_SUPER_CLASS, branch_offset);

          frame_ctx_p->lex_env_p = super_env_p;

          continue;
        }
        case VM_OC_CLASS_INHERITANCE:
        {
          ecma_value_t child_value = stack_top_p[-2];
          ecma_value_t child_prototype_value = stack_top_p[-1];

          ecma_object_t *child_class_p = ecma_get_object_from_value (child_value);
          ecma_object_t *child_prototype_class_p = ecma_get_object_from_value (child_prototype_value);
          ecma_property_value_t *prop_value_p;

          prop_value_p = ecma_create_named_data_property (child_prototype_class_p,
                                                          ecma_get_magic_string (LIT_MAGIC_STRING_CONSTRUCTOR),
                                                          ECMA_PROPERTY_CONFIGURABLE_WRITABLE,
                                                          NULL);

          ecma_named_data_property_assign_value (child_prototype_class_p, prop_value_p, child_value);

          ecma_object_t *super_class_p = ecma_get_lex_env_binding_object (frame_ctx_p->lex_env_p);

          if (ecma_get_object_prototype (super_class_p))
          {
            ecma_value_t super_prototype_value = ecma_op_object_get_by_magic_id (super_class_p,
                                                                                 LIT_MAGIC_STRING_PROTOTYPE);
            if (ecma_get_object_type (super_class_p) == ECMA_OBJECT_TYPE_BOUND_FUNCTION
                && !ecma_is_value_object (super_prototype_value))
            {
              ecma_free_value (super_prototype_value);
              result = ecma_raise_type_error (ECMA_ERR_MSG ("Class extends value does not have valid "
                                                            "prototype property."));
              goto error;
            }
            if (!(ECMA_IS_VALUE_ERROR (super_prototype_value) || !ecma_is_value_object (super_prototype_value)))
            {
              ecma_object_t *super_prototype_class_p = ecma_get_object_from_value (super_prototype_value);

              ECMA_SET_POINTER (child_prototype_class_p->prototype_or_outer_reference_cp, super_prototype_class_p);
              ECMA_SET_POINTER (child_class_p->prototype_or_outer_reference_cp, super_class_p);

            }
            ecma_free_value (super_prototype_value);
          }

          continue;
        }
        case VM_OC_PUSH_CLASS_CONSTRUCTOR:
        {
          ecma_object_t *prototype_obj_p = ecma_builtin_get (ECMA_BUILTIN_ID_FUNCTION_PROTOTYPE);

          ecma_object_t *function_obj_p = ecma_create_object (prototype_obj_p,
                                                              sizeof (ecma_extended_object_t),
                                                              ECMA_OBJECT_TYPE_EXTERNAL_FUNCTION);

          ecma_extended_object_t *ext_func_obj_p = (ecma_extended_object_t *) function_obj_p;
          ext_func_obj_p->u.external_handler_cb = ecma_op_function_implicit_constructor_handler_cb;

          *stack_top_p++ = ecma_make_object_value (function_obj_p);

          continue;
        }
        case VM_OC_SET_CLASS_CONSTRUCTOR:
        {
          ecma_object_t *new_constructor_obj_p = ecma_get_object_from_value (left_value);
          ecma_object_t *current_constructor_obj_p = ecma_get_object_from_value (stack_top_p[-2]);

          ecma_extended_object_t *new_ext_func_obj_p = (ecma_extended_object_t *) new_constructor_obj_p;
          ecma_extended_object_t *current_ext_func_obj_p = (ecma_extended_object_t *) current_constructor_obj_p;

          uint16_t type_flags_refs = current_constructor_obj_p->type_flags_refs;
          const int new_type = ECMA_OBJECT_TYPE_FUNCTION - ECMA_OBJECT_TYPE_EXTERNAL_FUNCTION;
          current_constructor_obj_p->type_flags_refs = (uint16_t) (type_flags_refs + new_type);

          ecma_compiled_code_t *bytecode_p;
          bytecode_p = (ecma_compiled_code_t *) ecma_op_function_get_compiled_code (new_ext_func_obj_p);
          bytecode_p->status_flags |= CBC_CODE_FLAGS_CONSTRUCTOR;
          ecma_bytecode_ref ((ecma_compiled_code_t *) bytecode_p);
          ECMA_SET_INTERNAL_VALUE_POINTER (current_ext_func_obj_p->u.function.bytecode_cp,
                                           bytecode_p);
          ECMA_SET_INTERNAL_VALUE_POINTER (current_ext_func_obj_p->u.function.scope_cp,
                                           ECMA_GET_INTERNAL_VALUE_POINTER (const ecma_object_t,
                                                                            new_ext_func_obj_p->u.function.scope_cp));
          ecma_deref_object (new_constructor_obj_p);
          continue;
        }
        case VM_OC_PUSH_IMPL_CONSTRUCTOR:
        {
          ecma_object_t *current_constructor_obj_p = ecma_get_object_from_value (stack_top_p[-2]);

          uint16_t type_flags_refs = current_constructor_obj_p->type_flags_refs;
          const int new_type = ECMA_OBJECT_TYPE_BOUND_FUNCTION - ECMA_OBJECT_TYPE_EXTERNAL_FUNCTION;
          current_constructor_obj_p->type_flags_refs = (uint16_t) (type_flags_refs + new_type);

          ecma_extended_object_t *ext_function_p = (ecma_extended_object_t *) current_constructor_obj_p;
          ecma_object_t *super_obj_p = ecma_op_resolve_super_reference_value (frame_ctx_p->lex_env_p);

          ECMA_SET_INTERNAL_VALUE_POINTER (ext_function_p->u.bound_function.target_function,
                                           super_obj_p);
          ext_function_p->u.bound_function.args_len_or_this = ECMA_VALUE_IMPLICIT_CONSTRUCTOR;

          continue;
        }
        case VM_OC_CLASS_EXPR_CONTEXT_END:
        {
          JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p - 1);

          JERRY_ASSERT (VM_GET_CONTEXT_TYPE (stack_top_p[-2]) == VM_CONTEXT_SUPER_CLASS);
          stack_top_p = vm_stack_context_abort (frame_ctx_p, stack_top_p - 1);

          JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p);
          stack_top_p++;
          stack_top_p[-1] = *stack_top_p;
          continue;
        }
        case VM_OC_CLASS_EVAL:
        {
          ECMA_SET_SUPER_EVAL_PARSER_OPTS (*byte_code_p++);
          continue;
        }
        case VM_OC_PUSH_CONSTRUCTOR_SUPER:
        {
          JERRY_ASSERT (byte_code_start_p[0] == CBC_EXT_OPCODE);

          bool is_super_called = ecma_op_is_super_called (frame_ctx_p->lex_env_p);

          if (byte_code_start_p[1] != CBC_EXT_PUSH_CONSTRUCTOR_SUPER_PROP)
          {
            /* Calling super(...) */
            if (is_super_called)
            {
              result = ecma_raise_reference_error (ECMA_ERR_MSG ("Super constructor may only be called once."));

              goto error;
            }
          }
          else if (!is_super_called)
          {
            /* Reference to super.method or super["method"] */
            result = ecma_raise_reference_error (ECMA_ERR_MSG ("Must call super constructor in derived class before "
                                                               "accessing 'super'."));
            goto error;
          }

          /* FALLTHRU */
        }
        case VM_OC_PUSH_SUPER:
        {
          JERRY_ASSERT (byte_code_start_p[0] == CBC_EXT_OPCODE);

          if (byte_code_start_p[1] == CBC_EXT_PUSH_SUPER
              || byte_code_start_p[1] == CBC_EXT_PUSH_CONSTRUCTOR_SUPER_PROP)
          {
            ecma_object_t *super_class_p = ecma_op_resolve_super_reference_value (frame_ctx_p->lex_env_p);

            ecma_value_t super_prototype = ecma_op_object_get_by_magic_id (super_class_p,
                                                                           LIT_MAGIC_STRING_PROTOTYPE);

            if (ECMA_IS_VALUE_ERROR (super_prototype))
            {
              result = super_prototype;
              goto error;
            }

            *stack_top_p++ = super_prototype;
          }
          else
          {
            ecma_object_t *super_class_p = ecma_op_resolve_super_reference_value (frame_ctx_p->lex_env_p);
            ecma_ref_object (super_class_p);
            *stack_top_p++ = ecma_make_object_value (super_class_p);
          }

          continue;
        }
        case VM_OC_PUSH_CONSTRUCTOR_THIS:
        {
          if (!ecma_op_is_super_called (frame_ctx_p->lex_env_p))
          {
            result = ecma_raise_reference_error (ECMA_ERR_MSG ("Must call super constructor in derived class before "
                                                               "accessing 'this' or returning from it."));
            goto error;
          }

          *stack_top_p++ = ecma_copy_value (ecma_op_get_class_this_binding (frame_ctx_p->lex_env_p));
          continue;
        }
        case VM_OC_SUPER_PROP_REFERENCE:
        {
          const int index = (byte_code_start_p[1] == CBC_EXT_SUPER_PROP_ASSIGN) ? -1 : -3;
          ecma_free_value (stack_top_p[index]);
          stack_top_p[index] = ecma_copy_value (frame_ctx_p->this_binding);
          continue;
        }
        case VM_OC_CONSTRUCTOR_RET:
        {
          result = left_value;
          left_value = ECMA_VALUE_UNDEFINED;

          if (!ecma_is_value_object (result))
          {
            if (ecma_is_value_undefined (result))
            {
              if (!ecma_op_is_super_called (frame_ctx_p->lex_env_p))
              {
                result = ecma_raise_reference_error (ECMA_ERR_MSG ("Must call super constructor in derived class "
                                                                   "before returning from derived constructor"));
              }
            }
            else
            {
              ecma_free_value (result);
              result = ecma_raise_type_error (ECMA_ERR_MSG ("Derived constructors may only "
                                                            "return object or undefined."));
            }
          }

          goto error;
        }
#endif /* !CONFIG_DISABLE_ES2015_CLASS */
        case VM_OC_PUSH_ELISON:
        {
          *stack_top_p++ = ECMA_VALUE_ARRAY_HOLE;
          continue;
        }
        case VM_OC_APPEND_ARRAY:
        {
          ecma_object_t *array_obj_p;
          uint32_t length_num;
          uint32_t values_length = *byte_code_p++;

          stack_top_p -= values_length;

          array_obj_p = ecma_get_object_from_value (stack_top_p[-1]);
          ecma_extended_object_t *ext_array_obj_p = (ecma_extended_object_t *) array_obj_p;

          length_num = ext_array_obj_p->u.array.length;

          for (uint32_t i = 0; i < values_length; i++)
          {
            if (!ecma_is_value_array_hole (stack_top_p[i]))
            {
              ecma_string_t *index_str_p = ecma_new_ecma_string_from_uint32 (length_num);

              ecma_property_value_t *prop_value_p;

              prop_value_p = ecma_create_named_data_property (array_obj_p,
                                                              index_str_p,
                                                              ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE,
                                                              NULL);

              JERRY_ASSERT (ecma_is_value_undefined (prop_value_p->value));
              prop_value_p->value = stack_top_p[i];

              /* The reference is moved so no need to free stack_top_p[i] except for objects. */
              if (ecma_is_value_object (stack_top_p[i]))
              {
                ecma_free_value (stack_top_p[i]);
              }

              ecma_deref_ecma_string (index_str_p);
            }

            length_num++;
          }

          ext_array_obj_p->u.array.length = length_num;
          continue;
        }
        case VM_OC_PUSH_UNDEFINED_BASE:
        {
          stack_top_p[0] = stack_top_p[-1];
          stack_top_p[-1] = ECMA_VALUE_UNDEFINED;
          stack_top_p++;
          continue;
        }
        case VM_OC_IDENT_REFERENCE:
        {
          uint16_t literal_index;

          READ_LITERAL_INDEX (literal_index);

          JERRY_ASSERT (literal_index < ident_end);

          if (literal_index < register_end)
          {
            *stack_top_p++ = ECMA_VALUE_REGISTER_REF;
            *stack_top_p++ = literal_index;
            *stack_top_p++ = ecma_fast_copy_value (frame_ctx_p->registers_p[literal_index]);
          }
          else
          {
            ecma_string_t *name_p = ecma_get_string_from_value (literal_start_p[literal_index]);

            ecma_object_t *ref_base_lex_env_p;
            ref_base_lex_env_p = ecma_op_resolve_reference_base (frame_ctx_p->lex_env_p,
                                                                 name_p);

            result = ecma_op_get_value_lex_env_base (ref_base_lex_env_p,
                                                     name_p,
                                                     is_strict);

            if (ECMA_IS_VALUE_ERROR (result))
            {
              goto error;
            }

            ecma_ref_object (ref_base_lex_env_p);
            ecma_ref_ecma_string (name_p);
            *stack_top_p++ = ecma_make_object_value (ref_base_lex_env_p);
            *stack_top_p++ = ecma_make_string_value (name_p);
            *stack_top_p++ = result;
          }
          continue;
        }
        case VM_OC_PROP_GET:
        {
          result = vm_op_get_value (left_value, right_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_PROP_REFERENCE:
        {
          /* Forms with reference requires preserving the base and offset. */

          if (opcode == CBC_PUSH_PROP_REFERENCE)
          {
            left_value = stack_top_p[-2];
            right_value = stack_top_p[-1];
          }
          else if (opcode == CBC_PUSH_PROP_LITERAL_REFERENCE)
          {
            *stack_top_p++ = left_value;
            right_value = left_value;
            left_value = stack_top_p[-2];
          }
          else
          {
            JERRY_ASSERT (opcode == CBC_PUSH_PROP_LITERAL_LITERAL_REFERENCE
                          || opcode == CBC_PUSH_PROP_THIS_LITERAL_REFERENCE);
            *stack_top_p++ = left_value;
            *stack_top_p++ = right_value;
          }
          /* FALLTHRU */
        }
        case VM_OC_PROP_PRE_INCR:
        case VM_OC_PROP_PRE_DECR:
        case VM_OC_PROP_POST_INCR:
        case VM_OC_PROP_POST_DECR:
        {
          result = vm_op_get_value (left_value,
                                    right_value);

          if (opcode < CBC_PRE_INCR)
          {
            left_value = ECMA_VALUE_UNDEFINED;
            right_value = ECMA_VALUE_UNDEFINED;
          }

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          if (opcode < CBC_PRE_INCR)
          {
            break;
          }

          stack_top_p += 2;
          left_value = result;
          right_value = ECMA_VALUE_UNDEFINED;
          /* FALLTHRU */
        }
        case VM_OC_PRE_INCR:
        case VM_OC_PRE_DECR:
        case VM_OC_POST_INCR:
        case VM_OC_POST_DECR:
        {
          uint32_t opcode_flags = VM_OC_GROUP_GET_INDEX (opcode_data) - VM_OC_PROP_PRE_INCR;

          byte_code_p = byte_code_start_p + 1;

          if (ecma_is_value_integer_number (left_value))
          {
            result = left_value;
            left_value = ECMA_VALUE_UNDEFINED;

            ecma_integer_value_t int_value = (ecma_integer_value_t) result;
            ecma_integer_value_t int_increase = 0;

            if (opcode_flags & VM_OC_DECREMENT_OPERATOR_FLAG)
            {
              if (int_value > ECMA_INTEGER_NUMBER_MIN_SHIFTED)
              {
                int_increase = -(1 << ECMA_DIRECT_SHIFT);
              }
            }
            else if (int_value < ECMA_INTEGER_NUMBER_MAX_SHIFTED)
            {
              int_increase = 1 << ECMA_DIRECT_SHIFT;
            }

            if (JERRY_LIKELY (int_increase != 0))
            {
              /* Postfix operators require the unmodifed number value. */
              if (opcode_flags & VM_OC_POST_INCR_DECR_OPERATOR_FLAG)
              {
                if (opcode_data & VM_OC_PUT_STACK)
                {
                  if (opcode_flags & VM_OC_IDENT_INCR_DECR_OPERATOR_FLAG)
                  {
                    JERRY_ASSERT (opcode == CBC_POST_INCR_IDENT_PUSH_RESULT
                                  || opcode == CBC_POST_DECR_IDENT_PUSH_RESULT);

                    *stack_top_p++ = result;
                  }
                  else
                  {
                    /* The parser ensures there is enough space for the
                     * extra value on the stack. See js-parser-expr.c. */

                    JERRY_ASSERT (opcode == CBC_POST_INCR_PUSH_RESULT
                                  || opcode == CBC_POST_DECR_PUSH_RESULT);

                    stack_top_p++;
                    stack_top_p[-1] = stack_top_p[-2];
                    stack_top_p[-2] = stack_top_p[-3];
                    stack_top_p[-3] = result;
                  }
                  opcode_data &= (uint32_t)~VM_OC_PUT_STACK;
                }
                else if (opcode_data & VM_OC_PUT_BLOCK)
                {
                  ecma_free_value (frame_ctx_p->block_result);
                  frame_ctx_p->block_result = result;
                  opcode_data &= (uint32_t) ~VM_OC_PUT_BLOCK;
                }
              }

              result = (ecma_value_t) (int_value + int_increase);
              break;
            }
          }
          else if (ecma_is_value_float_number (left_value))
          {
            result = left_value;
            left_value = ECMA_VALUE_UNDEFINED;
          }
          else
          {
            result = ecma_op_to_number (left_value);

            if (ECMA_IS_VALUE_ERROR (result))
            {
              goto error;
            }
          }

          ecma_number_t increase = ECMA_NUMBER_ONE;
          ecma_number_t result_number = ecma_get_number_from_value (result);

          if (opcode_flags & VM_OC_DECREMENT_OPERATOR_FLAG)
          {
            /* For decrement operators */
            increase = ECMA_NUMBER_MINUS_ONE;
          }

          /* Post operators require the unmodifed number value. */
          if (opcode_flags & VM_OC_POST_INCR_DECR_OPERATOR_FLAG)
          {
            if (opcode_data & VM_OC_PUT_STACK)
            {
              if (opcode_flags & VM_OC_IDENT_INCR_DECR_OPERATOR_FLAG)
              {
                JERRY_ASSERT (opcode == CBC_POST_INCR_IDENT_PUSH_RESULT
                              || opcode == CBC_POST_DECR_IDENT_PUSH_RESULT);

                *stack_top_p++ = ecma_copy_value (result);
              }
              else
              {
                /* The parser ensures there is enough space for the
                 * extra value on the stack. See js-parser-expr.c. */

                JERRY_ASSERT (opcode == CBC_POST_INCR_PUSH_RESULT
                              || opcode == CBC_POST_DECR_PUSH_RESULT);

                stack_top_p++;
                stack_top_p[-1] = stack_top_p[-2];
                stack_top_p[-2] = stack_top_p[-3];
                stack_top_p[-3] = ecma_copy_value (result);
              }
              opcode_data &= (uint32_t)~VM_OC_PUT_STACK;
            }
            else if (opcode_data & VM_OC_PUT_BLOCK)
            {
              ecma_free_value (frame_ctx_p->block_result);
              frame_ctx_p->block_result = ecma_copy_value (result);
              opcode_data &= (uint32_t) ~VM_OC_PUT_BLOCK;
            }
          }

          if (ecma_is_value_integer_number (result))
          {
            result = ecma_make_number_value (result_number + increase);
          }
          else
          {
            result = ecma_update_float_number (result, result_number + increase);
          }
          break;
        }
        case VM_OC_ASSIGN:
        {
          result = left_value;
          left_value = ECMA_VALUE_UNDEFINED;
          break;
        }
        case VM_OC_ASSIGN_PROP:
        {
          result = stack_top_p[-1];
          stack_top_p[-1] = left_value;
          left_value = ECMA_VALUE_UNDEFINED;
          break;
        }
        case VM_OC_ASSIGN_PROP_THIS:
        {
          result = stack_top_p[-1];
          stack_top_p[-1] = ecma_copy_value (frame_ctx_p->this_binding);
          *stack_top_p++ = left_value;
          left_value = ECMA_VALUE_UNDEFINED;
          break;
        }
        case VM_OC_RET:
        {
          JERRY_ASSERT (opcode == CBC_RETURN
                        || opcode == CBC_RETURN_WITH_BLOCK
                        || opcode == CBC_RETURN_WITH_LITERAL);

          if (opcode == CBC_RETURN_WITH_BLOCK)
          {
            left_value = frame_ctx_p->block_result;
            frame_ctx_p->block_result = ECMA_VALUE_UNDEFINED;
          }

          result = left_value;
          left_value = ECMA_VALUE_UNDEFINED;
          goto error;
        }
        case VM_OC_THROW:
        {
          JERRY_CONTEXT (error_value) = left_value;
          JERRY_CONTEXT (status_flags) |= ECMA_STATUS_EXCEPTION;

          result = ECMA_VALUE_ERROR;
          left_value = ECMA_VALUE_UNDEFINED;
          goto error;
        }
        case VM_OC_THROW_REFERENCE_ERROR:
        {
          result = ecma_raise_reference_error (ECMA_ERR_MSG ("Undefined reference."));
          goto error;
        }
        case VM_OC_EVAL:
        {
          JERRY_CONTEXT (status_flags) |= ECMA_STATUS_DIRECT_EVAL;
          JERRY_ASSERT (*byte_code_p >= CBC_CALL && *byte_code_p <= CBC_CALL2_PROP_BLOCK);
          continue;
        }
        case VM_OC_CALL:
        {
          frame_ctx_p->call_operation = VM_EXEC_CALL;
          frame_ctx_p->byte_code_p = byte_code_start_p;
          frame_ctx_p->stack_top_p = stack_top_p;
          return ECMA_VALUE_UNDEFINED;
        }
        case VM_OC_NEW:
        {
          frame_ctx_p->call_operation = VM_EXEC_CONSTRUCT;
          frame_ctx_p->byte_code_p = byte_code_start_p;
          frame_ctx_p->stack_top_p = stack_top_p;
          return ECMA_VALUE_UNDEFINED;
        }
        case VM_OC_ERROR:
        {
          JERRY_ASSERT (frame_ctx_p->byte_code_p[1] == CBC_EXT_ERROR);
#ifdef JERRY_DEBUGGER
          frame_ctx_p->byte_code_p = JERRY_CONTEXT (debugger_exception_byte_code_p);
#endif /* JERRY_DEBUGGER */

          result = ECMA_VALUE_ERROR;
          goto error;
        }
        case VM_OC_RESOLVE_BASE_FOR_CALL:
        {
          ecma_value_t this_value = stack_top_p[-3];

          if (this_value == ECMA_VALUE_REGISTER_REF)
          {
            /* Lexical environment cannot be 'this' value. */
            stack_top_p[-2] = ECMA_VALUE_UNDEFINED;
            stack_top_p[-3] = ECMA_VALUE_UNDEFINED;
          }
          else if (vm_get_implicit_this_value (&this_value))
          {
            ecma_free_value (stack_top_p[-3]);
            stack_top_p[-3] = this_value;
          }

          continue;
        }
        case VM_OC_PROP_DELETE:
        {
          result = vm_op_delete_prop (left_value, right_value, is_strict);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          JERRY_ASSERT (ecma_is_value_boolean (result));

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_DELETE:
        {
          uint16_t literal_index;

          READ_LITERAL_INDEX (literal_index);

          if (literal_index < register_end)
          {
            *stack_top_p++ = ECMA_VALUE_FALSE;
            continue;
          }

          result = vm_op_delete_var (literal_start_p[literal_index],
                                     frame_ctx_p->lex_env_p);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          JERRY_ASSERT (ecma_is_value_boolean (result));

          *stack_top_p++ = result;
          continue;
        }
        case VM_OC_JUMP:
        {
          byte_code_p = byte_code_start_p + branch_offset;
          continue;
        }
        case VM_OC_BRANCH_IF_STRICT_EQUAL:
        {
          ecma_value_t value = *(--stack_top_p);

          JERRY_ASSERT (stack_top_p > frame_ctx_p->registers_p + register_end);

          if (ecma_op_strict_equality_compare (value, stack_top_p[-1]))
          {
            byte_code_p = byte_code_start_p + branch_offset;
            ecma_free_value (*--stack_top_p);
          }
          ecma_free_value (value);
          continue;
        }
        case VM_OC_BRANCH_IF_TRUE:
        case VM_OC_BRANCH_IF_FALSE:
        case VM_OC_BRANCH_IF_LOGICAL_TRUE:
        case VM_OC_BRANCH_IF_LOGICAL_FALSE:
        {
          uint32_t opcode_flags = VM_OC_GROUP_GET_INDEX (opcode_data) - VM_OC_BRANCH_IF_TRUE;
          ecma_value_t value = *(--stack_top_p);

          bool boolean_value = ecma_op_to_boolean (value);

          if (opcode_flags & VM_OC_BRANCH_IF_FALSE_FLAG)
          {
            boolean_value = !boolean_value;
          }

          if (boolean_value)
          {
            byte_code_p = byte_code_start_p + branch_offset;
            if (opcode_flags & VM_OC_LOGICAL_BRANCH_FLAG)
            {
              /* "Push" the value back to the stack. */
              ++stack_top_p;
              continue;
            }
          }

          ecma_fast_free_value (value);
          continue;
        }
        case VM_OC_PLUS:
        case VM_OC_MINUS:
        {
          result = opfunc_unary_operation (left_value, VM_OC_GROUP_GET_INDEX (opcode_data) == VM_OC_PLUS);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_left_value;
        }
        case VM_OC_NOT:
        {
          *stack_top_p++ = ecma_make_boolean_value (!ecma_op_to_boolean (left_value));
          JERRY_ASSERT (ecma_is_value_boolean (stack_top_p[-1]));
          goto free_left_value;
        }
        case VM_OC_BIT_NOT:
        {
          JERRY_STATIC_ASSERT (ECMA_DIRECT_TYPE_MASK == ((1 << ECMA_DIRECT_SHIFT) - 1),
                               direct_type_mask_must_fill_all_bits_before_the_value_starts);

          if (ecma_is_value_integer_number (left_value))
          {
            *stack_top_p++ = (~left_value) & (ecma_value_t) (~ECMA_DIRECT_TYPE_MASK);
            goto free_left_value;
          }

          result = do_number_bitwise_logic (NUMBER_BITWISE_NOT,
                                            left_value,
                                            left_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_left_value;
        }
        case VM_OC_VOID:
        {
          *stack_top_p++ = ECMA_VALUE_UNDEFINED;
          goto free_left_value;
        }
        case VM_OC_TYPEOF_IDENT:
        {
          uint16_t literal_index;

          READ_LITERAL_INDEX (literal_index);

          JERRY_ASSERT (literal_index < ident_end);

          if (literal_index < register_end)
          {
            left_value = ecma_copy_value (frame_ctx_p->registers_p[literal_index]);
          }
          else
          {
            ecma_string_t *name_p = ecma_get_string_from_value (literal_start_p[literal_index]);

            ecma_object_t *ref_base_lex_env_p = ecma_op_resolve_reference_base (frame_ctx_p->lex_env_p,
                                                                                name_p);

            if (ref_base_lex_env_p == NULL)
            {
              result = ECMA_VALUE_UNDEFINED;
            }
            else
            {
              result = ecma_op_get_value_lex_env_base (ref_base_lex_env_p,
                                                       name_p,
                                                       is_strict);

            }

            if (ECMA_IS_VALUE_ERROR (result))
            {
              goto error;
            }

            left_value = result;
          }
          /* FALLTHRU */
        }
        case VM_OC_TYPEOF:
        {
          result = opfunc_typeof (left_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_left_value;
        }
        case VM_OC_ADD:
        {
          if (ecma_are_values_integer_numbers (left_value, right_value))
          {
            ecma_integer_value_t left_integer = ecma_get_integer_from_value (left_value);
            ecma_integer_value_t right_integer = ecma_get_integer_from_value (right_value);
            *stack_top_p++ = ecma_make_int32_value ((int32_t) (left_integer + right_integer));
            continue;
          }

          if (ecma_is_value_float_number (left_value)
              && ecma_is_value_number (right_value))
          {
            ecma_number_t new_value = (ecma_get_float_from_value (left_value) +
                                       ecma_get_number_from_value (right_value));

            *stack_top_p++ = ecma_update_float_number (left_value, new_value);
            ecma_free_number (right_value);
            continue;
          }

          if (ecma_is_value_float_number (right_value)
              && ecma_is_value_integer_number (left_value))
          {
            ecma_number_t new_value = ((ecma_number_t) ecma_get_integer_from_value (left_value) +
                                       ecma_get_float_from_value (right_value));

            *stack_top_p++ = ecma_update_float_number (right_value, new_value);
            continue;
          }

          result = opfunc_addition (left_value, right_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_SUB:
        {
          JERRY_STATIC_ASSERT (ECMA_INTEGER_NUMBER_MAX * 2 <= INT32_MAX
                               && ECMA_INTEGER_NUMBER_MIN * 2 >= INT32_MIN,
                               doubled_ecma_numbers_must_fit_into_int32_range);

          JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (left_value)
                        && !ECMA_IS_VALUE_ERROR (right_value));

          if (ecma_are_values_integer_numbers (left_value, right_value))
          {
            ecma_integer_value_t left_integer = ecma_get_integer_from_value (left_value);
            ecma_integer_value_t right_integer = ecma_get_integer_from_value (right_value);
            *stack_top_p++ = ecma_make_int32_value ((int32_t) (left_integer - right_integer));
            continue;
          }

          if (ecma_is_value_float_number (left_value)
              && ecma_is_value_number (right_value))
          {
            ecma_number_t new_value = (ecma_get_float_from_value (left_value) -
                                       ecma_get_number_from_value (right_value));

            *stack_top_p++ = ecma_update_float_number (left_value, new_value);
            ecma_free_number (right_value);
            continue;
          }

          if (ecma_is_value_float_number (right_value)
              && ecma_is_value_integer_number (left_value))
          {
            ecma_number_t new_value = ((ecma_number_t) ecma_get_integer_from_value (left_value) -
                                       ecma_get_float_from_value (right_value));

            *stack_top_p++ = ecma_update_float_number (right_value, new_value);
            continue;
          }

          result = do_number_arithmetic (NUMBER_ARITHMETIC_SUBSTRACTION,
                                         left_value,
                                         right_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_MUL:
        {
          JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (left_value)
                        && !ECMA_IS_VALUE_ERROR (right_value));

          JERRY_STATIC_ASSERT (ECMA_INTEGER_MULTIPLY_MAX * ECMA_INTEGER_MULTIPLY_MAX <= ECMA_INTEGER_NUMBER_MAX
                               && -(ECMA_INTEGER_MULTIPLY_MAX * ECMA_INTEGER_MULTIPLY_MAX) >= ECMA_INTEGER_NUMBER_MIN,
                               square_of_integer_multiply_max_must_fit_into_integer_value_range);

          if (ecma_are_values_integer_numbers (left_value, right_value))
          {
            ecma_integer_value_t left_integer = ecma_get_integer_from_value (left_value);
            ecma_integer_value_t right_integer = ecma_get_integer_from_value (right_value);

            if (-ECMA_INTEGER_MULTIPLY_MAX <= left_integer
                && left_integer <= ECMA_INTEGER_MULTIPLY_MAX
                && -ECMA_INTEGER_MULTIPLY_MAX <= right_integer
                && right_integer <= ECMA_INTEGER_MULTIPLY_MAX
                && left_value != 0
                && right_value != 0)
            {
              *stack_top_p++ = ecma_integer_multiply (left_integer, right_integer);
              continue;
            }

            ecma_number_t multiply = (ecma_number_t) left_integer * (ecma_number_t) right_integer;
            *stack_top_p++ = ecma_make_number_value (multiply);
            continue;
          }

          if (ecma_is_value_float_number (left_value)
              && ecma_is_value_number (right_value))
          {
            ecma_number_t new_value = (ecma_get_float_from_value (left_value) *
                                       ecma_get_number_from_value (right_value));

            *stack_top_p++ = ecma_update_float_number (left_value, new_value);
            ecma_free_number (right_value);
            continue;
          }

          if (ecma_is_value_float_number (right_value)
              && ecma_is_value_integer_number (left_value))
          {
            ecma_number_t new_value = ((ecma_number_t) ecma_get_integer_from_value (left_value) *
                                       ecma_get_float_from_value (right_value));

            *stack_top_p++ = ecma_update_float_number (right_value, new_value);
            continue;
          }

          result = do_number_arithmetic (NUMBER_ARITHMETIC_MULTIPLICATION,
                                         left_value,
                                         right_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_DIV:
        {
          JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (left_value)
                        && !ECMA_IS_VALUE_ERROR (right_value));

          result = do_number_arithmetic (NUMBER_ARITHMETIC_DIVISION,
                                         left_value,
                                         right_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_MOD:
        {
          JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (left_value)
                        && !ECMA_IS_VALUE_ERROR (right_value));

          if (ecma_are_values_integer_numbers (left_value, right_value))
          {
            ecma_integer_value_t left_integer = ecma_get_integer_from_value (left_value);
            ecma_integer_value_t right_integer = ecma_get_integer_from_value (right_value);

            if (right_integer != 0)
            {
              ecma_integer_value_t mod_result = left_integer % right_integer;

              if (mod_result != 0 || left_integer >= 0)
              {
                *stack_top_p++ = ecma_make_integer_value (mod_result);
                continue;
              }
            }
          }

          result = do_number_arithmetic (NUMBER_ARITHMETIC_REMAINDER,
                                         left_value,
                                         right_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_EQUAL:
        {
          result = opfunc_equality (left_value, right_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_NOT_EQUAL:
        {
          result = opfunc_equality (left_value, right_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = ecma_invert_boolean_value (result);
          goto free_both_values;
        }
        case VM_OC_STRICT_EQUAL:
        {
          bool is_equal = ecma_op_strict_equality_compare (left_value, right_value);

          result = ecma_make_boolean_value (is_equal);

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_STRICT_NOT_EQUAL:
        {
          bool is_equal = ecma_op_strict_equality_compare (left_value, right_value);

          result = ecma_make_boolean_value (!is_equal);

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_BIT_OR:
        {
          JERRY_STATIC_ASSERT (ECMA_DIRECT_TYPE_MASK == ((1 << ECMA_DIRECT_SHIFT) - 1),
                               direct_type_mask_must_fill_all_bits_before_the_value_starts);

          if (ecma_are_values_integer_numbers (left_value, right_value))
          {
            *stack_top_p++ = left_value | right_value;
            continue;
          }

          result = do_number_bitwise_logic (NUMBER_BITWISE_LOGIC_OR,
                                            left_value,
                                            right_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_BIT_XOR:
        {
          JERRY_STATIC_ASSERT (ECMA_DIRECT_TYPE_MASK == ((1 << ECMA_DIRECT_SHIFT) - 1),
                               direct_type_mask_must_fill_all_bits_before_the_value_starts);

          if (ecma_are_values_integer_numbers (left_value, right_value))
          {
            *stack_top_p++ = (left_value ^ right_value) & (ecma_value_t) (~ECMA_DIRECT_TYPE_MASK);
            continue;
          }

          result = do_number_bitwise_logic (NUMBER_BITWISE_LOGIC_XOR,
                                            left_value,
                                            right_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_BIT_AND:
        {
          JERRY_STATIC_ASSERT (ECMA_DIRECT_TYPE_MASK == ((1 << ECMA_DIRECT_SHIFT) - 1),
                               direct_type_mask_must_fill_all_bits_before_the_value_starts);

          if (ecma_are_values_integer_numbers (left_value, right_value))
          {
            *stack_top_p++ = left_value & right_value;
            continue;
          }

          result = do_number_bitwise_logic (NUMBER_BITWISE_LOGIC_AND,
                                            left_value,
                                            right_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_LEFT_SHIFT:
        {
          JERRY_STATIC_ASSERT (ECMA_DIRECT_TYPE_MASK == ((1 << ECMA_DIRECT_SHIFT) - 1),
                               direct_type_mask_must_fill_all_bits_before_the_value_starts);

          if (ecma_are_values_integer_numbers (left_value, right_value))
          {
            ecma_integer_value_t left_integer = ecma_get_integer_from_value (left_value);
            ecma_integer_value_t right_integer = ecma_get_integer_from_value (right_value);
            *stack_top_p++ = ecma_make_int32_value ((int32_t) (left_integer << (right_integer & 0x1f)));
            continue;
          }

          result = do_number_bitwise_logic (NUMBER_BITWISE_SHIFT_LEFT,
                                            left_value,
                                            right_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_RIGHT_SHIFT:
        {
          JERRY_STATIC_ASSERT (ECMA_DIRECT_TYPE_MASK == ((1 << ECMA_DIRECT_SHIFT) - 1),
                               direct_type_mask_must_fill_all_bits_before_the_value_starts);

          if (ecma_are_values_integer_numbers (left_value, right_value))
          {
            ecma_integer_value_t left_integer = ecma_get_integer_from_value (left_value);
            ecma_integer_value_t right_integer = ecma_get_integer_from_value (right_value);
            *stack_top_p++ = ecma_make_integer_value (left_integer >> (right_integer & 0x1f));
            continue;
          }

          result = do_number_bitwise_logic (NUMBER_BITWISE_SHIFT_RIGHT,
                                            left_value,
                                            right_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_UNS_RIGHT_SHIFT:
        {
          JERRY_STATIC_ASSERT (ECMA_DIRECT_TYPE_MASK == ((1 << ECMA_DIRECT_SHIFT) - 1),
                               direct_type_mask_must_fill_all_bits_before_the_value_starts);

          if (ecma_are_values_integer_numbers (left_value, right_value))
          {
            uint32_t left_uint32 = (uint32_t) ecma_get_integer_from_value (left_value);
            ecma_integer_value_t right_integer = ecma_get_integer_from_value (right_value);
            *stack_top_p++ = ecma_make_uint32_value (left_uint32 >> (right_integer & 0x1f));
            continue;
          }

          result = do_number_bitwise_logic (NUMBER_BITWISE_SHIFT_URIGHT,
                                            left_value,
                                            right_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_LESS:
        {
          if (ecma_are_values_integer_numbers (left_value, right_value))
          {
            bool is_less = (ecma_integer_value_t) left_value < (ecma_integer_value_t) right_value;

            /* This is a lookahead to the next opcode to improve performance.
             * If it is CBC_BRANCH_IF_TRUE_BACKWARD, execute it. */
            if (*byte_code_p <= CBC_BRANCH_IF_TRUE_BACKWARD_3 && *byte_code_p >= CBC_BRANCH_IF_TRUE_BACKWARD)
            {
              byte_code_start_p = byte_code_p++;
              branch_offset_length = CBC_BRANCH_OFFSET_LENGTH (*byte_code_start_p);
              JERRY_ASSERT (branch_offset_length >= 1 && branch_offset_length <= 3);

              if (is_less)
              {
                branch_offset = *(byte_code_p++);

                if (JERRY_UNLIKELY (branch_offset_length != 1))
                {
                  branch_offset <<= 8;
                  branch_offset |= *(byte_code_p++);
                  if (JERRY_UNLIKELY (branch_offset_length == 3))
                  {
                    branch_offset <<= 8;
                    branch_offset |= *(byte_code_p++);
                  }
                }

                /* Note: The opcode is a backward branch. */
                byte_code_p = byte_code_start_p - branch_offset;
              }
              else
              {
                byte_code_p += branch_offset_length;
              }

              continue;
            }

            *stack_top_p++ = ecma_make_boolean_value (is_less);
            continue;
          }

          if (ecma_is_value_number (left_value) && ecma_is_value_number (right_value))
          {
            ecma_number_t left_number = ecma_get_number_from_value (left_value);
            ecma_number_t right_number = ecma_get_number_from_value (right_value);

            *stack_top_p++ = ecma_make_boolean_value (left_number < right_number);
            goto free_both_values;
          }

          result = opfunc_relation (left_value, right_value, true, false);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_GREATER:
        {
          if (ecma_are_values_integer_numbers (left_value, right_value))
          {
            ecma_integer_value_t left_integer = (ecma_integer_value_t) left_value;
            ecma_integer_value_t right_integer = (ecma_integer_value_t) right_value;

            *stack_top_p++ = ecma_make_boolean_value (left_integer > right_integer);
            continue;
          }

          if (ecma_is_value_number (left_value) && ecma_is_value_number (right_value))
          {
            ecma_number_t left_number = ecma_get_number_from_value (left_value);
            ecma_number_t right_number = ecma_get_number_from_value (right_value);

            *stack_top_p++ = ecma_make_boolean_value (left_number > right_number);
            goto free_both_values;
          }

          result = opfunc_relation (left_value, right_value, false, false);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_LESS_EQUAL:
        {
          if (ecma_are_values_integer_numbers (left_value, right_value))
          {
            ecma_integer_value_t left_integer = (ecma_integer_value_t) left_value;
            ecma_integer_value_t right_integer = (ecma_integer_value_t) right_value;

            *stack_top_p++ = ecma_make_boolean_value (left_integer <= right_integer);
            continue;
          }

          if (ecma_is_value_number (left_value) && ecma_is_value_number (right_value))
          {
            ecma_number_t left_number = ecma_get_number_from_value (left_value);
            ecma_number_t right_number = ecma_get_number_from_value (right_value);

            *stack_top_p++ = ecma_make_boolean_value (left_number <= right_number);
            goto free_both_values;
          }

          result = opfunc_relation (left_value, right_value, false, true);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_GREATER_EQUAL:
        {
          if (ecma_are_values_integer_numbers (left_value, right_value))
          {
            ecma_integer_value_t left_integer = (ecma_integer_value_t) left_value;
            ecma_integer_value_t right_integer = (ecma_integer_value_t) right_value;

            *stack_top_p++ = ecma_make_boolean_value (left_integer >= right_integer);
            continue;
          }

          if (ecma_is_value_number (left_value) && ecma_is_value_number (right_value))
          {
            ecma_number_t left_number = ecma_get_number_from_value (left_value);
            ecma_number_t right_number = ecma_get_number_from_value (right_value);

            *stack_top_p++ = ecma_make_boolean_value (left_number >= right_number);
            goto free_both_values;
          }

          result = opfunc_relation (left_value, right_value, true, true);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_IN:
        {
          result = opfunc_in (left_value, right_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_INSTANCEOF:
        {
          result = opfunc_instanceof (left_value, right_value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          *stack_top_p++ = result;
          goto free_both_values;
        }
        case VM_OC_WITH:
        {
          ecma_value_t value = *(--stack_top_p);
          ecma_object_t *object_p;
          ecma_object_t *with_env_p;

          branch_offset += (int32_t) (byte_code_start_p - frame_ctx_p->byte_code_start_p);

          JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p);

          result = ecma_op_to_object (value);
          ecma_free_value (value);

          if (ECMA_IS_VALUE_ERROR (result))
          {
            goto error;
          }

          object_p = ecma_get_object_from_value (result);

          with_env_p = ecma_create_object_lex_env (frame_ctx_p->lex_env_p,
                                                   object_p,
                                                   ECMA_LEXICAL_ENVIRONMENT_THIS_OBJECT_BOUND);
          ecma_deref_object (object_p);

          VM_PLUS_EQUAL_U16 (frame_ctx_p->context_depth, PARSER_WITH_CONTEXT_STACK_ALLOCATION);
          stack_top_p += PARSER_WITH_CONTEXT_STACK_ALLOCATION;

          stack_top_p[-1] = VM_CREATE_CONTEXT (VM_CONTEXT_WITH, branch_offset);

          frame_ctx_p->lex_env_p = with_env_p;
          continue;
        }
        case VM_OC_FOR_IN_CREATE_CONTEXT:
        {
          ecma_value_t value = *(--stack_top_p);

          JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p);

          ecma_value_t expr_obj_value = ECMA_VALUE_UNDEFINED;
          ecma_collection_chunk_t *prop_names_p = opfunc_for_in (value, &expr_obj_value);
          ecma_free_value (value);

          if (prop_names_p == NULL)
          {
            byte_code_p = byte_code_start_p + branch_offset;
            continue;
          }

          branch_offset += (int32_t) (byte_code_start_p - frame_ctx_p->byte_code_start_p);

          VM_PLUS_EQUAL_U16 (frame_ctx_p->context_depth, PARSER_FOR_IN_CONTEXT_STACK_ALLOCATION);
          stack_top_p += PARSER_FOR_IN_CONTEXT_STACK_ALLOCATION;
          stack_top_p[-1] = (ecma_value_t) VM_CREATE_CONTEXT (VM_CONTEXT_FOR_IN, branch_offset);
          ECMA_SET_INTERNAL_VALUE_ANY_POINTER (stack_top_p[-2], prop_names_p);
          stack_top_p[-3] = 0;
          stack_top_p[-4] = expr_obj_value;

          continue;
        }
        case VM_OC_FOR_IN_GET_NEXT:
        {
          ecma_value_t *context_top_p = frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth;

          ecma_collection_chunk_t *chunk_p;
          chunk_p = ECMA_GET_INTERNAL_VALUE_ANY_POINTER (ecma_collection_chunk_t, context_top_p[-2]);

          JERRY_ASSERT (VM_GET_CONTEXT_TYPE (context_top_p[-1]) == VM_CONTEXT_FOR_IN);

          uint32_t index = context_top_p[-3];

          JERRY_ASSERT (!ecma_is_value_pointer (chunk_p->items[index]));

          *stack_top_p++ = chunk_p->items[index];
          index++;

          if (JERRY_LIKELY (!ecma_is_value_pointer (chunk_p->items[index])))
          {
            context_top_p[-3] = index;
            continue;
          }

          context_top_p[-3] = 0;

          ecma_collection_chunk_t *next_chunk_p;
          next_chunk_p = (ecma_collection_chunk_t *) ecma_get_pointer_from_value (chunk_p->items[index]);
          ECMA_SET_INTERNAL_VALUE_ANY_POINTER (context_top_p[-2], next_chunk_p);

          jmem_heap_free_block (chunk_p, sizeof (ecma_collection_chunk_t));
          continue;
        }
        case VM_OC_FOR_IN_HAS_NEXT:
        {
          JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p);

          ecma_collection_chunk_t *chunk_p;
          chunk_p = ECMA_GET_INTERNAL_VALUE_ANY_POINTER (ecma_collection_chunk_t, stack_top_p[-2]);

          uint32_t index = stack_top_p[-3];
          ecma_object_t *object_p = ecma_get_object_from_value (stack_top_p[-4]);

          while (true)
          {
            if (chunk_p == NULL)
            {
              ecma_deref_object (object_p);

              VM_MINUS_EQUAL_U16 (frame_ctx_p->context_depth, PARSER_FOR_IN_CONTEXT_STACK_ALLOCATION);
              stack_top_p -= PARSER_FOR_IN_CONTEXT_STACK_ALLOCATION;
              break;
            }

            ecma_string_t *prop_name_p = ecma_get_string_from_value (chunk_p->items[index]);

            if (JERRY_LIKELY (ecma_op_object_has_property (object_p, prop_name_p)))
            {
              byte_code_p = byte_code_start_p + branch_offset;
              break;
            }

            index++;
            ecma_value_t value = chunk_p->items[index];

            if (JERRY_LIKELY (!ecma_is_value_pointer (value)))
            {
              stack_top_p[-3] = index;
            }
            else
            {
              index = 0;
              stack_top_p[-3] = 0;

              ecma_collection_chunk_t *next_chunk_p;
              next_chunk_p = (ecma_collection_chunk_t *) ecma_get_pointer_from_value (value);
              ECMA_SET_INTERNAL_VALUE_ANY_POINTER (stack_top_p[-2], next_chunk_p);

              jmem_heap_free_block (chunk_p, sizeof (ecma_collection_chunk_t));
              chunk_p = next_chunk_p;
            }

            ecma_deref_ecma_string (prop_name_p);
          }
          continue;
        }
        case VM_OC_TRY:
        {
          /* Try opcode simply creates the try context. */
          branch_offset += (int32_t) (byte_code_start_p - frame_ctx_p->byte_code_start_p);

          JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p);

          VM_PLUS_EQUAL_U16 (frame_ctx_p->context_depth, PARSER_TRY_CONTEXT_STACK_ALLOCATION);
          stack_top_p += PARSER_TRY_CONTEXT_STACK_ALLOCATION;

          stack_top_p[-1] = (ecma_value_t) VM_CREATE_CONTEXT (VM_CONTEXT_TRY, branch_offset);
          continue;
        }
        case VM_OC_CATCH:
        {
          /* Catches are ignored and turned to jumps. */
          JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p);
          JERRY_ASSERT (VM_GET_CONTEXT_TYPE (stack_top_p[-1]) == VM_CONTEXT_TRY);

          byte_code_p = byte_code_start_p + branch_offset;
          continue;
        }
        case VM_OC_FINALLY:
        {
          branch_offset += (int32_t) (byte_code_start_p - frame_ctx_p->byte_code_start_p);

          JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p);

          JERRY_ASSERT (VM_GET_CONTEXT_TYPE (stack_top_p[-1]) == VM_CONTEXT_TRY
                        || VM_GET_CONTEXT_TYPE (stack_top_p[-1]) == VM_CONTEXT_CATCH);

          if (VM_GET_CONTEXT_TYPE (stack_top_p[-1]) == VM_CONTEXT_CATCH)
          {
            ecma_object_t *lex_env_p = frame_ctx_p->lex_env_p;
            frame_ctx_p->lex_env_p = ecma_get_lex_env_outer_reference (lex_env_p);
            ecma_deref_object (lex_env_p);
          }

          stack_top_p[-1] = (ecma_value_t) VM_CREATE_CONTEXT (VM_CONTEXT_FINALLY_JUMP, branch_offset);
          stack_top_p[-2] = (ecma_value_t) branch_offset;
          continue;
        }
        case VM_OC_CONTEXT_END:
        {
          JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p);

          switch (VM_GET_CONTEXT_TYPE (stack_top_p[-1]))
          {
            case VM_CONTEXT_FINALLY_JUMP:
            {
              uint32_t jump_target = stack_top_p[-2];

              VM_MINUS_EQUAL_U16 (frame_ctx_p->context_depth,
                                  PARSER_TRY_CONTEXT_STACK_ALLOCATION);
              stack_top_p -= PARSER_TRY_CONTEXT_STACK_ALLOCATION;

              if (vm_stack_find_finally (frame_ctx_p,
                                         &stack_top_p,
                                         VM_CONTEXT_FINALLY_JUMP,
                                         jump_target))
              {
                JERRY_ASSERT (VM_GET_CONTEXT_TYPE (stack_top_p[-1]) == VM_CONTEXT_FINALLY_JUMP);
                byte_code_p = frame_ctx_p->byte_code_p;
                stack_top_p[-2] = jump_target;
              }
              else
              {
                byte_code_p = frame_ctx_p->byte_code_start_p + jump_target;
              }
              break;
            }
            case VM_CONTEXT_FINALLY_THROW:
            {
              JERRY_CONTEXT (error_value) = stack_top_p[-2];
              JERRY_CONTEXT (status_flags) |= ECMA_STATUS_EXCEPTION;

              VM_MINUS_EQUAL_U16 (frame_ctx_p->context_depth,
                                  PARSER_TRY_CONTEXT_STACK_ALLOCATION);
              stack_top_p -= PARSER_TRY_CONTEXT_STACK_ALLOCATION;
              result = ECMA_VALUE_ERROR;

#ifdef JERRY_DEBUGGER
              JERRY_DEBUGGER_SET_FLAGS (JERRY_DEBUGGER_VM_EXCEPTION_THROWN);
#endif /* JERRY_DEBUGGER */
              goto error;
            }
            case VM_CONTEXT_FINALLY_RETURN:
            {
              result = stack_top_p[-2];

              VM_MINUS_EQUAL_U16 (frame_ctx_p->context_depth,
                                  PARSER_TRY_CONTEXT_STACK_ALLOCATION);
              stack_top_p -= PARSER_TRY_CONTEXT_STACK_ALLOCATION;
              goto error;
            }
            default:
            {
              stack_top_p = vm_stack_context_abort (frame_ctx_p, stack_top_p);
            }
          }

          JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p);
          continue;
        }
        case VM_OC_JUMP_AND_EXIT_CONTEXT:
        {
          JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p);

          branch_offset += (int32_t) (byte_code_start_p - frame_ctx_p->byte_code_start_p);

          if (vm_stack_find_finally (frame_ctx_p,
                                     &stack_top_p,
                                     VM_CONTEXT_FINALLY_JUMP,
                                     (uint32_t) branch_offset))
          {
            JERRY_ASSERT (VM_GET_CONTEXT_TYPE (stack_top_p[-1]) == VM_CONTEXT_FINALLY_JUMP);
            byte_code_p = frame_ctx_p->byte_code_p;
            stack_top_p[-2] = (uint32_t) branch_offset;
          }
          else
          {
            byte_code_p = frame_ctx_p->byte_code_start_p + branch_offset;
          }

          JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p);
          continue;
        }
#ifdef JERRY_DEBUGGER
        case VM_OC_BREAKPOINT_ENABLED:
        {
          if (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_VM_IGNORE)
          {
            continue;
          }

          JERRY_ASSERT (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED);

          JERRY_ASSERT (!(frame_ctx_p->bytecode_header_p->status_flags & CBC_CODE_FLAGS_DEBUGGER_IGNORE));

          frame_ctx_p->byte_code_p = byte_code_start_p;

          jerry_debugger_breakpoint_hit (JERRY_DEBUGGER_BREAKPOINT_HIT);
          if (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_VM_EXCEPTION_THROWN)
          {
            result = ECMA_VALUE_ERROR;
            goto error;
          }
          continue;
        }
        case VM_OC_BREAKPOINT_DISABLED:
        {
          if (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_VM_IGNORE)
          {
            continue;
          }

          JERRY_ASSERT (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED);

          JERRY_ASSERT (!(frame_ctx_p->bytecode_header_p->status_flags & CBC_CODE_FLAGS_DEBUGGER_IGNORE));

          frame_ctx_p->byte_code_p = byte_code_start_p;

          if ((JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_VM_STOP)
              && (JERRY_CONTEXT (debugger_stop_context) == NULL
                  || JERRY_CONTEXT (debugger_stop_context) == JERRY_CONTEXT (vm_top_context_p)))
          {
            jerry_debugger_breakpoint_hit (JERRY_DEBUGGER_BREAKPOINT_HIT);
            if (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_VM_EXCEPTION_THROWN)
            {
              result = ECMA_VALUE_ERROR;
              goto error;
            }
            continue;
          }

          if (JERRY_CONTEXT (debugger_message_delay) > 0)
          {
            JERRY_CONTEXT (debugger_message_delay)--;
            continue;
          }

          JERRY_CONTEXT (debugger_message_delay) = JERRY_DEBUGGER_MESSAGE_FREQUENCY;

          if (jerry_debugger_receive (NULL))
          {
            continue;
          }

          if ((JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_VM_STOP)
              && (JERRY_CONTEXT (debugger_stop_context) == NULL
                  || JERRY_CONTEXT (debugger_stop_context) == JERRY_CONTEXT (vm_top_context_p)))
          {
            jerry_debugger_breakpoint_hit (JERRY_DEBUGGER_BREAKPOINT_HIT);
            if (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_VM_EXCEPTION_THROWN)
            {
              result = ECMA_VALUE_ERROR;
              goto error;
            }
          }
          continue;
        }
#endif /* JERRY_DEBUGGER */
#ifdef JERRY_ENABLE_LINE_INFO
        case VM_OC_RESOURCE_NAME:
        {
          ecma_length_t formal_params_number = 0;

          if (CBC_NON_STRICT_ARGUMENTS_NEEDED (bytecode_header_p))
          {
            if (bytecode_header_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
            {
              cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) bytecode_header_p;

              formal_params_number = args_p->argument_end;
            }
            else
            {
              cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) bytecode_header_p;

              formal_params_number = args_p->argument_end;
            }
          }

          uint8_t *byte_p = (uint8_t *) bytecode_header_p;
          byte_p += ((size_t) bytecode_header_p->size) << JMEM_ALIGNMENT_LOG;

          ecma_value_t *resource_name_p = (ecma_value_t *) byte_p;
          resource_name_p -= formal_params_number;

          frame_ctx_p->resource_name = resource_name_p[-1];
          continue;
        }
        case VM_OC_LINE:
        {
          uint32_t value = 0;
          uint8_t byte;

          do
          {
            byte = *byte_code_p++;
            value = (value << 7) | (byte & CBC_LOWER_SEVEN_BIT_MASK);
          }
          while (byte & CBC_HIGHEST_BIT_MASK);

          frame_ctx_p->current_line = value;
          continue;
        }
#endif /* JERRY_ENABLE_LINE_INFO */
        default:
        {
          JERRY_ASSERT (VM_OC_GROUP_GET_INDEX (opcode_data) == VM_OC_NONE);

          jerry_fatal (ERR_DISABLED_BYTE_CODE);
        }
      }

      JERRY_ASSERT (VM_OC_HAS_PUT_RESULT (opcode_data));

      if (opcode_data & VM_OC_PUT_IDENT)
      {
        uint16_t literal_index;

        READ_LITERAL_INDEX (literal_index);

        if (literal_index < register_end)
        {
          ecma_fast_free_value (frame_ctx_p->registers_p[literal_index]);

          frame_ctx_p->registers_p[literal_index] = result;

          if (opcode_data & (VM_OC_PUT_STACK | VM_OC_PUT_BLOCK))
          {
            result = ecma_fast_copy_value (result);
          }
        }
        else
        {
          ecma_string_t *var_name_str_p = ecma_get_string_from_value (literal_start_p[literal_index]);

          ecma_object_t *ref_base_lex_env_p = ecma_op_resolve_reference_base (frame_ctx_p->lex_env_p,
                                                                              var_name_str_p);

          ecma_value_t put_value_result = ecma_op_put_value_lex_env_base (ref_base_lex_env_p,
                                                                          var_name_str_p,
                                                                          is_strict,
                                                                          result);

          if (ECMA_IS_VALUE_ERROR (put_value_result))
          {
            ecma_free_value (result);
            result = put_value_result;
            goto error;
          }

          if (!(opcode_data & (VM_OC_PUT_STACK | VM_OC_PUT_BLOCK)))
          {
            ecma_fast_free_value (result);
          }
        }
      }
      else if (opcode_data & VM_OC_PUT_REFERENCE)
      {
        ecma_value_t property = *(--stack_top_p);
        ecma_value_t object = *(--stack_top_p);

        if (object == ECMA_VALUE_REGISTER_REF)
        {
          ecma_fast_free_value (frame_ctx_p->registers_p[property]);

          frame_ctx_p->registers_p[property] = result;

          if (!(opcode_data & (VM_OC_PUT_STACK | VM_OC_PUT_BLOCK)))
          {
            goto free_both_values;
          }
          result = ecma_fast_copy_value (result);
        }
        else
        {
          ecma_value_t set_value_result = vm_op_set_value (object,
                                                           property,
                                                           result,
                                                           is_strict);

          if (ECMA_IS_VALUE_ERROR (set_value_result))
          {
            ecma_free_value (result);
            result = set_value_result;
            goto error;
          }

          if (!(opcode_data & (VM_OC_PUT_STACK | VM_OC_PUT_BLOCK)))
          {
            ecma_fast_free_value (result);
            goto free_both_values;
          }
        }
      }

      if (opcode_data & VM_OC_PUT_STACK)
      {
        *stack_top_p++ = result;
      }
      else if (opcode_data & VM_OC_PUT_BLOCK)
      {
        ecma_fast_free_value (frame_ctx_p->block_result);
        frame_ctx_p->block_result = result;
      }

free_both_values:
      ecma_fast_free_value (right_value);
free_left_value:
      ecma_fast_free_value (left_value);
    }
error:

    ecma_fast_free_value (left_value);
    ecma_fast_free_value (right_value);

    if (ECMA_IS_VALUE_ERROR (result))
    {
      ecma_value_t *vm_stack_p = stack_top_p;

      for (vm_stack_p = frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth;
           vm_stack_p < stack_top_p;
           vm_stack_p++)
      {
        if (*vm_stack_p == ECMA_VALUE_REGISTER_REF)
        {
          JERRY_ASSERT (vm_stack_p < stack_top_p);
          vm_stack_p++;
        }
        else
        {
          ecma_free_value (*vm_stack_p);
        }
      }

      stack_top_p = frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth;
#ifdef JERRY_DEBUGGER
      const uint32_t dont_stop = (JERRY_DEBUGGER_VM_IGNORE_EXCEPTION
                                  | JERRY_DEBUGGER_VM_IGNORE
                                  | JERRY_DEBUGGER_VM_EXCEPTION_THROWN);

      if ((JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_CONNECTED)
          && !(frame_ctx_p->bytecode_header_p->status_flags
               & (CBC_CODE_FLAGS_DEBUGGER_IGNORE | CBC_CODE_FLAGS_STATIC_FUNCTION))
          && !(JERRY_CONTEXT (debugger_flags) & dont_stop))
      {
        /* Save the error to a local value, because the engine enters breakpoint mode after,
           therefore an evaluation error, or user-created error throw would overwrite it. */
        ecma_value_t current_error_value = JERRY_CONTEXT (error_value);

        if (jerry_debugger_send_exception_string ())
        {
          jerry_debugger_breakpoint_hit (JERRY_DEBUGGER_EXCEPTION_HIT);

          if (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_VM_EXCEPTION_THROWN)
          {
            ecma_free_value (current_error_value);
          }
          else
          {
            JERRY_CONTEXT (error_value) = current_error_value;
          }

          JERRY_DEBUGGER_SET_FLAGS (JERRY_DEBUGGER_VM_EXCEPTION_THROWN);
        }
      }
#endif /* JERRY_DEBUGGER */
    }

    JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p);

    if (frame_ctx_p->context_depth == 0)
    {
      /* In most cases there is no context. */

      ecma_fast_free_value (frame_ctx_p->block_result);
      frame_ctx_p->call_operation = VM_NO_EXEC_OP;
      return result;
    }

    if (!ECMA_IS_VALUE_ERROR (result))
    {
      if (vm_stack_find_finally (frame_ctx_p,
                                 &stack_top_p,
                                 VM_CONTEXT_FINALLY_RETURN,
                                 0))
      {
        JERRY_ASSERT (VM_GET_CONTEXT_TYPE (stack_top_p[-1]) == VM_CONTEXT_FINALLY_RETURN);
        JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p);

        byte_code_p = frame_ctx_p->byte_code_p;
        stack_top_p[-2] = result;
        continue;
      }
    }
    else if (JERRY_CONTEXT (status_flags) & ECMA_STATUS_EXCEPTION)
    {
      if (vm_stack_find_finally (frame_ctx_p,
                                 &stack_top_p,
                                 VM_CONTEXT_FINALLY_THROW,
                                 0))
      {
        JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p);

#ifdef JERRY_DEBUGGER
        JERRY_DEBUGGER_CLEAR_FLAGS (JERRY_DEBUGGER_VM_EXCEPTION_THROWN);
#endif /* JERRY_DEBUGGER */

        byte_code_p = frame_ctx_p->byte_code_p;

        if (VM_GET_CONTEXT_TYPE (stack_top_p[-1]) == VM_CONTEXT_CATCH)
        {
          *stack_top_p++ = JERRY_CONTEXT (error_value);

          JERRY_ASSERT (byte_code_p[0] == CBC_ASSIGN_SET_IDENT);

          uint32_t literal_index = byte_code_p[1];

          if (literal_index >= encoding_limit)
          {
            literal_index = ((literal_index << 8) | byte_code_p[2]) - encoding_delta;
          }

          ecma_object_t *catch_env_p = ecma_create_decl_lex_env (frame_ctx_p->lex_env_p);
#ifdef JERRY_DEBUGGER
          catch_env_p->type_flags_refs |= (uint16_t) ECMA_OBJECT_FLAG_NON_CLOSURE;
#endif /* JERRY_DEBUGGER */

          ecma_string_t *catch_name_p = ecma_get_string_from_value (literal_start_p[literal_index]);
          ecma_op_create_mutable_binding (catch_env_p, catch_name_p, false);

          frame_ctx_p->lex_env_p = catch_env_p;
        }
        else
        {
          JERRY_ASSERT (VM_GET_CONTEXT_TYPE (stack_top_p[-1]) == VM_CONTEXT_FINALLY_THROW);
          stack_top_p[-2] = JERRY_CONTEXT (error_value);
        }

        continue;
      }
    }
    else
    {
      do
      {
        JERRY_ASSERT (frame_ctx_p->registers_p + register_end + frame_ctx_p->context_depth == stack_top_p);

        stack_top_p = vm_stack_context_abort (frame_ctx_p, stack_top_p);
      }
      while (frame_ctx_p->context_depth > 0);
    }

    ecma_free_value (frame_ctx_p->block_result);
    frame_ctx_p->call_operation = VM_NO_EXEC_OP;

    return result;
  }
} /* vm_loop */

#undef READ_LITERAL
#undef READ_LITERAL_INDEX

/**
 * Execute code block.
 *
 * @return ecma value
 */
static ecma_value_t JERRY_ATTR_NOINLINE
vm_execute (vm_frame_ctx_t *frame_ctx_p, /**< frame context */
            const ecma_value_t *arg_p, /**< arguments list */
            ecma_length_t arg_list_len) /**< length of arguments list */
{
  const ecma_compiled_code_t *bytecode_header_p = frame_ctx_p->bytecode_header_p;
  ecma_value_t completion_value;
  uint16_t argument_end;
  uint16_t register_end;

  if (bytecode_header_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
  {
    cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) bytecode_header_p;

    argument_end = args_p->argument_end;
    register_end = args_p->register_end;
  }
  else
  {
    cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) bytecode_header_p;

    argument_end = args_p->argument_end;
    register_end = args_p->register_end;
  }

  frame_ctx_p->stack_top_p = frame_ctx_p->registers_p + register_end;

#ifndef CONFIG_DISABLE_ES2015_FUNCTION_REST_PARAMETER
  uint32_t function_call_argument_count = arg_list_len;
#endif /* !CONFIG_DISABLE_ES2015_FUNCTION_REST_PARAMETER */

  if (arg_list_len > argument_end)
  {
    arg_list_len = argument_end;
  }

  for (uint32_t i = 0; i < arg_list_len; i++)
  {
    frame_ctx_p->registers_p[i] = ecma_fast_copy_value (arg_p[i]);
  }

  /* The arg_list_len contains the end of the copied arguments.
   * Fill everything else with undefined. */
  if (register_end > arg_list_len)
  {
    ecma_value_t *stack_p = frame_ctx_p->registers_p + arg_list_len;

    for (uint32_t i = arg_list_len; i < register_end; i++)
    {
      *stack_p++ = ECMA_VALUE_UNDEFINED;
    }
  }

#ifndef CONFIG_DISABLE_ES2015_FUNCTION_REST_PARAMETER
  if (bytecode_header_p->status_flags & CBC_CODE_FLAGS_REST_PARAMETER)
  {
    JERRY_ASSERT (function_call_argument_count >= arg_list_len);
    ecma_value_t new_array = ecma_op_create_array_object (arg_p + arg_list_len,
                                                          function_call_argument_count - arg_list_len,
                                                          false);
    JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (new_array));
    frame_ctx_p->registers_p[argument_end] = new_array;
    arg_list_len++;
  }
#endif /* !CONFIG_DISABLE_ES2015_FUNCTION_REST_PARAMETER */

  JERRY_CONTEXT (status_flags) &= (uint32_t) ~ECMA_STATUS_DIRECT_EVAL;

  JERRY_CONTEXT (vm_top_context_p) = frame_ctx_p;

  vm_init_loop (frame_ctx_p);

  while (true)
  {
    completion_value = vm_loop (frame_ctx_p);

    switch (frame_ctx_p->call_operation)
    {
      case VM_EXEC_CALL:
      {
        opfunc_call (frame_ctx_p);
        break;
      }
#ifndef CONFIG_DISABLE_ES2015_CLASS
      case VM_EXEC_SUPER_CALL:
      {
        vm_super_call (frame_ctx_p);
        break;
      }
#endif /* !CONFIG_DISABLE_ES2015_CLASS */
      case VM_EXEC_CONSTRUCT:
      {
        opfunc_construct (frame_ctx_p);
        break;
      }
      default:
      {
        JERRY_ASSERT (frame_ctx_p->call_operation == VM_NO_EXEC_OP);

        /* Free arguments and registers */
        for (uint32_t i = 0; i < register_end; i++)
        {
          ecma_fast_free_value (frame_ctx_p->registers_p[i]);
        }

#ifdef JERRY_DEBUGGER
        if (JERRY_CONTEXT (debugger_stop_context) == JERRY_CONTEXT (vm_top_context_p))
        {
          /* The engine will stop when the next breakpoint is reached. */
          JERRY_ASSERT (JERRY_CONTEXT (debugger_flags) & JERRY_DEBUGGER_VM_STOP);
          JERRY_CONTEXT (debugger_stop_context) = NULL;
        }
#endif /* JERRY_DEBUGGER */

#ifdef VM_RECURSION_LIMIT
        JERRY_CONTEXT (vm_recursion_counter)++;
#endif /* VM_RECURSION_LIMIT */

        JERRY_CONTEXT (vm_top_context_p) = frame_ctx_p->prev_context_p;
        return completion_value;
      }
    }
  }
} /* vm_execute */

/**
 * Run the code.
 *
 * @return ecma value
 */
ecma_value_t
vm_run (const ecma_compiled_code_t *bytecode_header_p, /**< byte-code data header */
        ecma_value_t this_binding_value, /**< value of 'ThisBinding' */
        ecma_object_t *lex_env_p, /**< lexical environment to use */
        uint32_t parse_opts, /**< ecma_parse_opts_t option bits */
        const ecma_value_t *arg_list_p, /**< arguments list */
        ecma_length_t arg_list_len) /**< length of arguments list */
{
#ifdef VM_RECURSION_LIMIT
  if (JERRY_UNLIKELY (JERRY_CONTEXT (vm_recursion_counter) == 0))
  {
    return ecma_raise_range_error (ECMA_ERR_MSG ("VM recursion limit is exceeded."));
  }
  else
  {
    JERRY_CONTEXT (vm_recursion_counter)--;
  }
#endif /* VM_RECURSION_LIMIT */

  ecma_value_t *literal_p;
  vm_frame_ctx_t frame_ctx;
  uint32_t call_stack_size;

  if (bytecode_header_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
  {
    cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) bytecode_header_p;
    call_stack_size = (uint32_t) (args_p->register_end + args_p->stack_limit);

    literal_p = (ecma_value_t *) ((uint8_t *) bytecode_header_p + sizeof (cbc_uint16_arguments_t));
    literal_p -= args_p->register_end;
    frame_ctx.literal_start_p = literal_p;
    literal_p += args_p->literal_end;
  }
  else
  {
    cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) bytecode_header_p;
    call_stack_size = (uint32_t) (args_p->register_end + args_p->stack_limit);

    literal_p = (ecma_value_t *) ((uint8_t *) bytecode_header_p + sizeof (cbc_uint8_arguments_t));
    literal_p -= args_p->register_end;
    frame_ctx.literal_start_p = literal_p;
    literal_p += args_p->literal_end;
  }

  frame_ctx.bytecode_header_p = bytecode_header_p;
  frame_ctx.byte_code_p = (uint8_t *) literal_p;
  frame_ctx.byte_code_start_p = (uint8_t *) literal_p;
  frame_ctx.lex_env_p = lex_env_p;
  frame_ctx.prev_context_p = JERRY_CONTEXT (vm_top_context_p);
  frame_ctx.this_binding = this_binding_value;
  frame_ctx.block_result = ECMA_VALUE_UNDEFINED;
#ifdef JERRY_ENABLE_LINE_INFO
  frame_ctx.resource_name = ECMA_VALUE_UNDEFINED;
  frame_ctx.current_line = 0;
#endif /* JERRY_ENABLE_LINE_INFO */
  frame_ctx.context_depth = 0;
  frame_ctx.is_eval_code = parse_opts & ECMA_PARSE_DIRECT_EVAL;

  /* Use JERRY_MAX() to avoid array declaration with size 0. */
  JERRY_VLA (ecma_value_t, stack, JERRY_MAX (call_stack_size, 1));
  frame_ctx.registers_p = stack;

  return vm_execute (&frame_ctx, arg_list_p, arg_list_len);
} /* vm_run */

/**
 * @}
 * @}
 */
