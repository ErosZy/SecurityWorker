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

#include "ecma-conversion.h"
#include "ecma-exceptions.h"
#include "ecma-function-object.h"
#include "ecma-helpers.h"
#include "ecma-lex-env.h"
#include "ecma-literal-storage.h"
#include "jcontext.h"
#include "jerryscript.h"
#include "jerry-snapshot.h"
#include "js-parser.h"
#include "lit-char-helpers.h"
#include "re-compiler.h"

#if defined JERRY_ENABLE_SNAPSHOT_SAVE || defined JERRY_ENABLE_SNAPSHOT_EXEC

/**
 * Get snapshot configuration flags.
 *
 * @return configuration flags
 */
static inline uint32_t JERRY_ATTR_ALWAYS_INLINE
snapshot_get_global_flags (bool has_regex, /**< regex literal is present */
                           bool has_class) /**< class literal is present */
{
  JERRY_UNUSED (has_regex);
  JERRY_UNUSED (has_class);

  uint32_t flags = 0;

#ifndef CONFIG_DISABLE_REGEXP_BUILTIN
  flags |= (has_regex ? JERRY_SNAPSHOT_HAS_REGEX_LITERAL : 0);
#endif /* !CONFIG_DISABLE_REGEXP_BUILTIN */
#ifndef CONFIG_DISABLE_ES2015_CLASS
  flags |= (has_class ? JERRY_SNAPSHOT_HAS_CLASS_LITERAL : 0);
#endif /* !CONFIG_DISABLE_ES2015_CLASS */

  return flags;
} /* snapshot_get_global_flags */

/**
 * Checks whether the global_flags argument matches to the current feature set.
 *
 * @return true if global_flags accepted, false otherwise
 */
static inline bool JERRY_ATTR_ALWAYS_INLINE
snapshot_check_global_flags (uint32_t global_flags) /**< global flags */
{
#ifndef CONFIG_DISABLE_REGEXP_BUILTIN
  global_flags &= (uint32_t) ~JERRY_SNAPSHOT_HAS_REGEX_LITERAL;
#endif /* !CONFIG_DISABLE_REGEXP_BUILTIN */
#ifndef CONFIG_DISABLE_ES2015_CLASS
  global_flags &= (uint32_t) ~JERRY_SNAPSHOT_HAS_CLASS_LITERAL;
#endif /* !CONFIG_DISABLE_ES2015_CLASS */

  return global_flags == snapshot_get_global_flags (false, false);
} /* snapshot_check_global_flags */

#endif /* JERRY_ENABLE_SNAPSHOT_SAVE || JERRY_ENABLE_SNAPSHOT_EXEC */

#ifdef JERRY_ENABLE_SNAPSHOT_SAVE

/**
 * Variables required to take a snapshot.
 */
typedef struct
{
  size_t snapshot_buffer_write_offset;
  ecma_value_t snapshot_error;
  bool regex_found;
  bool class_found;
} snapshot_globals_t;

/** \addtogroup jerrysnapshot Jerry snapshot operations
 * @{
 */

/**
 * Write data into the specified buffer.
 *
 * Note:
 *      Offset is in-out and is incremented if the write operation completes successfully.
 *
 * @return true - if write was successful, i.e. offset + data_size doesn't exceed buffer size,
 *         false - otherwise
 */
static inline bool JERRY_ATTR_ALWAYS_INLINE
snapshot_write_to_buffer_by_offset (uint8_t *buffer_p, /**< buffer */
                                    size_t buffer_size, /**< size of buffer */
                                    size_t *in_out_buffer_offset_p,  /**< [in,out] offset to write to
                                                                      * incremented with data_size */
                                    const void *data_p, /**< data */
                                    size_t data_size) /**< size of the writable data */
{
  if (*in_out_buffer_offset_p + data_size > buffer_size)
  {
    return false;
  }

  memcpy (buffer_p + *in_out_buffer_offset_p, data_p, data_size);
  *in_out_buffer_offset_p += data_size;

  return true;
} /* snapshot_write_to_buffer_by_offset */

/**
 * Maximum snapshot write buffer offset.
 */
#if CONFIG_ECMA_NUMBER_TYPE == CONFIG_ECMA_NUMBER_FLOAT32
#define JERRY_SNAPSHOT_MAXIMUM_WRITE_OFFSET (0x7fffff >> 1)
#else
#define JERRY_SNAPSHOT_MAXIMUM_WRITE_OFFSET (UINT32_MAX >> 1)
#endif

/**
 * Save snapshot helper.
 *
 * @return start offset
 */
static uint32_t
snapshot_add_compiled_code (ecma_compiled_code_t *compiled_code_p, /**< compiled code */
                            uint8_t *snapshot_buffer_p, /**< snapshot buffer */
                            size_t snapshot_buffer_size, /**< snapshot buffer size */
                            snapshot_globals_t *globals_p) /**< snapshot globals */
{
  const jerry_char_t *error_buffer_too_small_p = (const jerry_char_t *) "Snapshot buffer too small.";

  if (!ecma_is_value_empty (globals_p->snapshot_error))
  {
    return 0;
  }

  JERRY_ASSERT ((globals_p->snapshot_buffer_write_offset & (JMEM_ALIGNMENT - 1)) == 0);

  if (globals_p->snapshot_buffer_write_offset > JERRY_SNAPSHOT_MAXIMUM_WRITE_OFFSET)
  {
    const char * const error_message_p = "Maximum snapshot size reached.";
    globals_p->snapshot_error = jerry_create_error (JERRY_ERROR_RANGE, (const jerry_char_t *) error_message_p);
    return 0;
  }

  /* The snapshot generator always parses a single file,
   * so the base always starts right after the snapshot header. */
  uint32_t start_offset = (uint32_t) (globals_p->snapshot_buffer_write_offset - sizeof (jerry_snapshot_header_t));

  uint8_t *copied_code_start_p = snapshot_buffer_p + globals_p->snapshot_buffer_write_offset;
  ecma_compiled_code_t *copied_code_p = (ecma_compiled_code_t *) copied_code_start_p;

#ifndef CONFIG_DISABLE_ES2015_CLASS
  if (compiled_code_p->status_flags & CBC_CODE_FLAGS_CONSTRUCTOR)
  {
    globals_p->class_found = true;
  }
#endif /* !CONFIG_DISABLE_ES2015_CLASS */

#ifndef CONFIG_DISABLE_REGEXP_BUILTIN
  if (!(compiled_code_p->status_flags & CBC_CODE_FLAGS_FUNCTION))
  {
    /* Regular expression. */
    if (globals_p->snapshot_buffer_write_offset + sizeof (ecma_compiled_code_t) > snapshot_buffer_size)
    {
      globals_p->snapshot_error = jerry_create_error (JERRY_ERROR_RANGE, error_buffer_too_small_p);
      return 0;
    }

    globals_p->snapshot_buffer_write_offset += sizeof (ecma_compiled_code_t);

    ecma_value_t pattern = ((re_compiled_code_t *) compiled_code_p)->pattern;
    ecma_string_t *pattern_string_p = ecma_get_string_from_value (pattern);

    ecma_length_t pattern_size = 0;

    ECMA_STRING_TO_UTF8_STRING (pattern_string_p, buffer_p, buffer_size);

    pattern_size = buffer_size;

    if (!snapshot_write_to_buffer_by_offset (snapshot_buffer_p,
                                             snapshot_buffer_size,
                                             &globals_p->snapshot_buffer_write_offset,
                                             buffer_p,
                                             buffer_size))
    {
      globals_p->snapshot_error = jerry_create_error (JERRY_ERROR_RANGE, error_buffer_too_small_p);
      /* cannot return inside ECMA_FINALIZE_UTF8_STRING */
    }

    ECMA_FINALIZE_UTF8_STRING (buffer_p, buffer_size);

    if (!ecma_is_value_empty (globals_p->snapshot_error))
    {
      return 0;
    }

    globals_p->regex_found = true;
    globals_p->snapshot_buffer_write_offset = JERRY_ALIGNUP (globals_p->snapshot_buffer_write_offset,
                                                             JMEM_ALIGNMENT);

    /* Regexp character size is stored in refs. */
    copied_code_p->refs = (uint16_t) pattern_size;

    pattern_size += (ecma_length_t) sizeof (ecma_compiled_code_t);
    copied_code_p->size = (uint16_t) ((pattern_size + JMEM_ALIGNMENT - 1) >> JMEM_ALIGNMENT_LOG);

    copied_code_p->status_flags = compiled_code_p->status_flags;

    return start_offset;
  }
#endif /* !CONFIG_DISABLE_REGEXP_BUILTIN */

  JERRY_ASSERT (compiled_code_p->status_flags & CBC_CODE_FLAGS_FUNCTION);

  if (!snapshot_write_to_buffer_by_offset (snapshot_buffer_p,
                                           snapshot_buffer_size,
                                           &globals_p->snapshot_buffer_write_offset,
                                           compiled_code_p,
                                           ((size_t) compiled_code_p->size) << JMEM_ALIGNMENT_LOG))
  {
    globals_p->snapshot_error = jerry_create_error (JERRY_ERROR_RANGE, error_buffer_too_small_p);
    return 0;
  }

  /* Sub-functions and regular expressions are stored recursively. */
  uint8_t *buffer_p = (uint8_t *) copied_code_p;
  ecma_value_t *literal_start_p;
  uint32_t const_literal_end;
  uint32_t literal_end;

  if (compiled_code_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
  {
    literal_start_p = (ecma_value_t *) (buffer_p + sizeof (cbc_uint16_arguments_t));

    cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) buffer_p;
    literal_end = (uint32_t) (args_p->literal_end - args_p->register_end);
    const_literal_end = (uint32_t) (args_p->const_literal_end - args_p->register_end);
  }
  else
  {
    literal_start_p = (ecma_value_t *) (buffer_p + sizeof (cbc_uint8_arguments_t));

    cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) buffer_p;
    literal_end = (uint32_t) (args_p->literal_end - args_p->register_end);
    const_literal_end = (uint32_t) (args_p->const_literal_end - args_p->register_end);
  }

  for (uint32_t i = const_literal_end; i < literal_end; i++)
  {
    ecma_compiled_code_t *bytecode_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_compiled_code_t,
                                                                        literal_start_p[i]);

    if (bytecode_p == compiled_code_p)
    {
      literal_start_p[i] = 0;
    }
    else
    {
      uint32_t offset = snapshot_add_compiled_code (bytecode_p,
                                                    snapshot_buffer_p,
                                                    snapshot_buffer_size,
                                                    globals_p);

      JERRY_ASSERT (!ecma_is_value_empty (globals_p->snapshot_error) || offset > start_offset);

      literal_start_p[i] = offset - start_offset;
    }
  }

  return start_offset;
} /* snapshot_add_compiled_code */

/**
 * Create unsupported literal error.
 */
static void
static_snapshot_error_unsupported_literal (snapshot_globals_t *globals_p, /**< snapshot globals */
                                           ecma_value_t literal) /**< literal form the literal pool */
{
  const lit_utf8_byte_t error_prefix[] = "Unsupported static snapshot literal: ";

  ecma_string_t *error_message_p = ecma_new_ecma_string_from_utf8 (error_prefix, sizeof (error_prefix) - 1);

  literal = ecma_op_to_string (literal);
  JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (literal));

  ecma_string_t *literal_string_p = ecma_get_string_from_value (literal);
  error_message_p = ecma_concat_ecma_strings (error_message_p, literal_string_p);
  ecma_deref_ecma_string (literal_string_p);

  ecma_object_t *error_object_p = ecma_new_standard_error_with_message (ECMA_ERROR_RANGE, error_message_p);
  ecma_deref_ecma_string (error_message_p);

  globals_p->snapshot_error = ecma_create_error_object_reference (error_object_p);
} /* static_snapshot_error_unsupported_literal */

/**
 * Save static snapshot helper.
 *
 * @return start offset
 */
static uint32_t
static_snapshot_add_compiled_code (ecma_compiled_code_t *compiled_code_p, /**< compiled code */
                                   uint8_t *snapshot_buffer_p, /**< snapshot buffer */
                                   size_t snapshot_buffer_size, /**< snapshot buffer size */
                                   snapshot_globals_t *globals_p) /**< snapshot globals */
{
  if (!ecma_is_value_empty (globals_p->snapshot_error))
  {
    return 0;
  }

  JERRY_ASSERT ((globals_p->snapshot_buffer_write_offset & (JMEM_ALIGNMENT - 1)) == 0);

  if (globals_p->snapshot_buffer_write_offset >= JERRY_SNAPSHOT_MAXIMUM_WRITE_OFFSET)
  {
    const char * const error_message_p = "Maximum snapshot size reached.";
    globals_p->snapshot_error = jerry_create_error (JERRY_ERROR_RANGE, (const jerry_char_t *) error_message_p);
    return 0;
  }

  /* The snapshot generator always parses a single file,
   * so the base always starts right after the snapshot header. */
  uint32_t start_offset = (uint32_t) (globals_p->snapshot_buffer_write_offset - sizeof (jerry_snapshot_header_t));

  uint8_t *copied_code_start_p = snapshot_buffer_p + globals_p->snapshot_buffer_write_offset;
  ecma_compiled_code_t *copied_code_p = (ecma_compiled_code_t *) copied_code_start_p;

  if (!(compiled_code_p->status_flags & CBC_CODE_FLAGS_FUNCTION))
  {
    /* Regular expression literals are not supported. */
    const char * const error_message_p = "Regular expression literals are not supported.";
    globals_p->snapshot_error = jerry_create_error (JERRY_ERROR_RANGE, (const jerry_char_t *) error_message_p);
    return 0;
  }

  if (!snapshot_write_to_buffer_by_offset (snapshot_buffer_p,
                                           snapshot_buffer_size,
                                           &globals_p->snapshot_buffer_write_offset,
                                           compiled_code_p,
                                           ((size_t) compiled_code_p->size) << JMEM_ALIGNMENT_LOG))
  {
    const char * const error_message_p = "Snapshot buffer too small.";
    globals_p->snapshot_error = jerry_create_error (JERRY_ERROR_RANGE, (const jerry_char_t *) error_message_p);
    return 0;
  }

  /* Sub-functions and regular expressions are stored recursively. */
  uint8_t *buffer_p = (uint8_t *) copied_code_p;
  ecma_value_t *literal_start_p;
  uint32_t argument_end;
  uint32_t const_literal_end;
  uint32_t literal_end;

  ((ecma_compiled_code_t *) copied_code_p)->status_flags |= CBC_CODE_FLAGS_STATIC_FUNCTION;

  if (compiled_code_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
  {
    literal_start_p = (ecma_value_t *) (buffer_p + sizeof (cbc_uint16_arguments_t));

    cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) buffer_p;
    argument_end = args_p->argument_end;
    literal_end = (uint32_t) (args_p->literal_end - args_p->register_end);
    const_literal_end = (uint32_t) (args_p->const_literal_end - args_p->register_end);
  }
  else
  {
    literal_start_p = (ecma_value_t *) (buffer_p + sizeof (cbc_uint8_arguments_t));

    cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) buffer_p;
    argument_end = args_p->argument_end;
    literal_end = (uint32_t) (args_p->literal_end - args_p->register_end);
    const_literal_end = (uint32_t) (args_p->const_literal_end - args_p->register_end);
  }

  for (uint32_t i = 0; i < const_literal_end; i++)
  {
    if (!ecma_is_value_direct (literal_start_p[i])
        && !ecma_is_value_direct_string (literal_start_p[i]))
    {
      static_snapshot_error_unsupported_literal (globals_p, literal_start_p[i]);
      return 0;
    }
  }

  for (uint32_t i = const_literal_end; i < literal_end; i++)
  {
    ecma_compiled_code_t *bytecode_p = ECMA_GET_INTERNAL_VALUE_POINTER (ecma_compiled_code_t,
                                                                        literal_start_p[i]);

    if (bytecode_p == compiled_code_p)
    {
      literal_start_p[i] = 0;
    }
    else
    {
      uint32_t offset = static_snapshot_add_compiled_code (bytecode_p,
                                                           snapshot_buffer_p,
                                                           snapshot_buffer_size,
                                                           globals_p);

      JERRY_ASSERT (!ecma_is_value_empty (globals_p->snapshot_error) || offset > start_offset);

      literal_start_p[i] = offset - start_offset;
    }
  }

  if (CBC_NON_STRICT_ARGUMENTS_NEEDED (compiled_code_p))
  {
    buffer_p += ((size_t) compiled_code_p->size) << JMEM_ALIGNMENT_LOG;
    literal_start_p = ((ecma_value_t *) buffer_p) - argument_end;

    for (uint32_t i = 0; i < argument_end; i++)
    {
      if (!ecma_is_value_direct_string (literal_start_p[i]))
      {
        static_snapshot_error_unsupported_literal (globals_p, literal_start_p[i]);
        return 0;
      }
    }
  }

  return start_offset;
} /* static_snapshot_add_compiled_code */

/**
 * Set the uint16_t offsets in the code area.
 */
static void
jerry_snapshot_set_offsets (uint32_t *buffer_p, /**< buffer */
                            uint32_t size, /**< buffer size */
                            lit_mem_to_snapshot_id_map_entry_t *lit_map_p) /**< literal map */
{
  JERRY_ASSERT (size > 0);

  do
  {
    ecma_compiled_code_t *bytecode_p = (ecma_compiled_code_t *) buffer_p;
    uint32_t code_size = ((uint32_t) bytecode_p->size) << JMEM_ALIGNMENT_LOG;

    if (bytecode_p->status_flags & CBC_CODE_FLAGS_FUNCTION)
    {
      ecma_value_t *literal_start_p;
      uint32_t argument_end;
      uint32_t const_literal_end;

      if (bytecode_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
      {
        literal_start_p = (ecma_value_t *) (((uint8_t *) buffer_p) + sizeof (cbc_uint16_arguments_t));

        cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) buffer_p;
        argument_end = args_p->argument_end;
        const_literal_end = (uint32_t) (args_p->const_literal_end - args_p->register_end);
      }
      else
      {
        literal_start_p = (ecma_value_t *) (((uint8_t *) buffer_p) + sizeof (cbc_uint8_arguments_t));

        cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) buffer_p;
        argument_end = args_p->argument_end;
        const_literal_end = (uint32_t) (args_p->const_literal_end - args_p->register_end);
      }

      for (uint32_t i = 0; i < const_literal_end; i++)
      {
        if (ecma_is_value_string (literal_start_p[i])
            || ecma_is_value_float_number (literal_start_p[i]))
        {
          lit_mem_to_snapshot_id_map_entry_t *current_p = lit_map_p;

          while (current_p->literal_id != literal_start_p[i])
          {
            current_p++;
          }

          literal_start_p[i] = current_p->literal_offset;
        }
      }

      if (CBC_NON_STRICT_ARGUMENTS_NEEDED (bytecode_p))
      {
        uint8_t *byte_p = (uint8_t *) bytecode_p;
        byte_p += ((size_t) bytecode_p->size) << JMEM_ALIGNMENT_LOG;
        literal_start_p = ((ecma_value_t *) byte_p) - argument_end;

        for (uint32_t i = 0; i < argument_end; i++)
        {
          if (literal_start_p[i] != ECMA_VALUE_EMPTY)
          {
            JERRY_ASSERT (ecma_is_value_string (literal_start_p[i]));

            lit_mem_to_snapshot_id_map_entry_t *current_p = lit_map_p;

            while (current_p->literal_id != literal_start_p[i])
            {
              current_p++;
            }

            literal_start_p[i] = current_p->literal_offset;
          }
        }
      }

      /* Set reference counter to 1. */
      bytecode_p->refs = 1;
    }

    JERRY_ASSERT ((code_size % sizeof (uint32_t)) == 0);
    buffer_p += code_size / sizeof (uint32_t);
    size -= code_size;
  }
  while (size > 0);
} /* jerry_snapshot_set_offsets */

#endif /* JERRY_ENABLE_SNAPSHOT_SAVE */

#ifdef JERRY_ENABLE_SNAPSHOT_EXEC

/**
 * Byte code blocks shorter than this threshold are always copied into the memory.
 * The memory / performance trade-of of byte code redirection does not worth
 * in such cases.
 */
#define BYTECODE_NO_COPY_THRESHOLD 8

/**
 * Load byte code from snapshot.
 *
 * @return byte code
 */
static ecma_compiled_code_t *
snapshot_load_compiled_code (const uint8_t *base_addr_p, /**< base address of the
                                                          *   current primary function */
                             const uint8_t *literal_base_p, /**< literal start */
                             bool copy_bytecode) /**< byte code should be copied to memory */
{
  ecma_compiled_code_t *bytecode_p = (ecma_compiled_code_t *) base_addr_p;
  uint32_t code_size = ((uint32_t) bytecode_p->size) << JMEM_ALIGNMENT_LOG;

#ifndef CONFIG_DISABLE_REGEXP_BUILTIN
  if (!(bytecode_p->status_flags & CBC_CODE_FLAGS_FUNCTION))
  {
    const re_compiled_code_t *re_bytecode_p = NULL;

    const uint8_t *regex_start_p = ((const uint8_t *) bytecode_p) + sizeof (ecma_compiled_code_t);

    /* Real size is stored in refs. */
    ecma_string_t *pattern_str_p = ecma_new_ecma_string_from_utf8 (regex_start_p,
                                                                   bytecode_p->refs);

    re_compile_bytecode (&re_bytecode_p,
                         pattern_str_p,
                         bytecode_p->status_flags);

    ecma_deref_ecma_string (pattern_str_p);

    return (ecma_compiled_code_t *) re_bytecode_p;
  }
#endif /* !CONFIG_DISABLE_REGEXP_BUILTIN */

  JERRY_ASSERT (bytecode_p->status_flags & CBC_CODE_FLAGS_FUNCTION);

  size_t header_size;
  uint32_t argument_end = 0;
  uint32_t const_literal_end;
  uint32_t literal_end;

  if (bytecode_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
  {
    uint8_t *byte_p = (uint8_t *) bytecode_p;
    cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) byte_p;

    if (CBC_NON_STRICT_ARGUMENTS_NEEDED (bytecode_p))
    {
      argument_end = args_p->argument_end;
    }

    const_literal_end = (uint32_t) (args_p->const_literal_end - args_p->register_end);
    literal_end = (uint32_t) (args_p->literal_end - args_p->register_end);
    header_size = sizeof (cbc_uint16_arguments_t);
  }
  else
  {
    uint8_t *byte_p = (uint8_t *) bytecode_p;
    cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) byte_p;

    if (CBC_NON_STRICT_ARGUMENTS_NEEDED (bytecode_p))
    {
      argument_end = args_p->argument_end;
    }

    const_literal_end = (uint32_t) (args_p->const_literal_end - args_p->register_end);
    literal_end = (uint32_t) (args_p->literal_end - args_p->register_end);
    header_size = sizeof (cbc_uint8_arguments_t);
  }

  if (copy_bytecode
      || (header_size + (literal_end * sizeof (uint16_t)) + BYTECODE_NO_COPY_THRESHOLD > code_size))
  {
    bytecode_p = (ecma_compiled_code_t *) jmem_heap_alloc_block (code_size);

#ifdef JMEM_STATS
    jmem_stats_allocate_byte_code_bytes (code_size);
#endif /* JMEM_STATS */

    memcpy (bytecode_p, base_addr_p, code_size);
  }
  else
  {
    uint32_t start_offset = (uint32_t) (header_size + literal_end * sizeof (ecma_value_t));

    uint8_t *real_bytecode_p = ((uint8_t *) bytecode_p) + start_offset;
    uint32_t new_code_size = (uint32_t) (start_offset + 1 + sizeof (uint8_t *));

    if (argument_end != 0)
    {
      new_code_size += (uint32_t) (argument_end * sizeof (ecma_value_t));
    }

    new_code_size = JERRY_ALIGNUP (new_code_size, JMEM_ALIGNMENT);

    bytecode_p = (ecma_compiled_code_t *) jmem_heap_alloc_block (new_code_size);

#ifdef JMEM_STATS
    jmem_stats_allocate_byte_code_bytes (new_code_size);
#endif /* JMEM_STATS */

    memcpy (bytecode_p, base_addr_p, start_offset);

    bytecode_p->size = (uint16_t) (new_code_size >> JMEM_ALIGNMENT_LOG);

    uint8_t *byte_p = (uint8_t *) bytecode_p;

    if (argument_end != 0)
    {
      uint32_t argument_size = (uint32_t) (argument_end * sizeof (ecma_value_t));
      memcpy (byte_p + new_code_size - argument_size,
              base_addr_p + code_size - argument_size,
              argument_size);
    }

    byte_p[start_offset] = CBC_SET_BYTECODE_PTR;
    memcpy (byte_p + start_offset + 1, &real_bytecode_p, sizeof (uint8_t *));

    code_size = new_code_size;
  }

  JERRY_ASSERT (bytecode_p->refs == 1);

#ifdef JERRY_DEBUGGER
  bytecode_p->status_flags = (uint16_t) (bytecode_p->status_flags | CBC_CODE_FLAGS_DEBUGGER_IGNORE);
#endif /* JERRY_DEBUGGER */

  ecma_value_t *literal_start_p = (ecma_value_t *) (((uint8_t *) bytecode_p) + header_size);

  for (uint32_t i = 0; i < const_literal_end; i++)
  {
    if ((literal_start_p[i] & ECMA_VALUE_TYPE_MASK) == ECMA_TYPE_SNAPSHOT_OFFSET)
    {
      literal_start_p[i] = ecma_snapshot_get_literal (literal_base_p, literal_start_p[i]);
    }
  }

  for (uint32_t i = const_literal_end; i < literal_end; i++)
  {
    size_t literal_offset = (size_t) literal_start_p[i];

    if (literal_offset == 0)
    {
      /* Self reference */
      ECMA_SET_INTERNAL_VALUE_POINTER (literal_start_p[i],
                                       bytecode_p);
    }
    else
    {
      ecma_compiled_code_t *literal_bytecode_p;
      literal_bytecode_p = snapshot_load_compiled_code (base_addr_p + literal_offset,
                                                        literal_base_p,
                                                        copy_bytecode);

      ECMA_SET_INTERNAL_VALUE_POINTER (literal_start_p[i],
                                       literal_bytecode_p);
    }
  }

  if (argument_end != 0)
  {
    literal_start_p = (ecma_value_t *) (((uint8_t *) bytecode_p) + code_size);
    literal_start_p -= argument_end;

    for (uint32_t i = 0; i < argument_end; i++)
    {
      if ((literal_start_p[i] & ECMA_VALUE_TYPE_MASK) == ECMA_TYPE_SNAPSHOT_OFFSET)
      {
        literal_start_p[i] = ecma_snapshot_get_literal (literal_base_p, literal_start_p[i]);
      }
    }
  }

  return bytecode_p;
} /* snapshot_load_compiled_code */

#endif /* JERRY_ENABLE_SNAPSHOT_EXEC */

#ifdef JERRY_ENABLE_SNAPSHOT_SAVE

/**
 * Generate snapshot from specified source and arguments
 *
 * @return size of snapshot (a number value), if it was generated succesfully
 *          (i.e. there are no syntax errors in source code, buffer size is sufficient,
 *           and snapshot support is enabled in current configuration through JERRY_ENABLE_SNAPSHOT_SAVE),
 *         error object otherwise
 */
static jerry_value_t
jerry_generate_snapshot_with_args (const jerry_char_t *resource_name_p, /**< script resource name */
                                   size_t resource_name_length, /**< script resource name length */
                                   const jerry_char_t *source_p, /**< script source */
                                   size_t source_size, /**< script source size */
                                   const jerry_char_t *args_p, /**< arguments string */
                                   size_t args_size, /**< arguments string size */
                                   uint32_t generate_snapshot_opts, /**< jerry_generate_snapshot_opts_t option bits */
                                   uint32_t *buffer_p, /**< buffer to save snapshot to */
                                   size_t buffer_size) /**< the buffer's size */
{
  /* Currently unused arguments. */
  JERRY_UNUSED (resource_name_p);
  JERRY_UNUSED (resource_name_length);

#ifdef JERRY_ENABLE_LINE_INFO
  JERRY_CONTEXT (resource_name) = ECMA_VALUE_UNDEFINED;
#endif /* JERRY_ENABLE_LINE_INFO */

  snapshot_globals_t globals;
  ecma_value_t parse_status;
  ecma_compiled_code_t *bytecode_data_p;
  const uint32_t aligned_header_size = JERRY_ALIGNUP (sizeof (jerry_snapshot_header_t),
                                                      JMEM_ALIGNMENT);

  globals.snapshot_buffer_write_offset = aligned_header_size;
  globals.snapshot_error = ECMA_VALUE_EMPTY;
  globals.regex_found = false;
  globals.class_found = false;

  parse_status = parser_parse_script (args_p,
                                      args_size,
                                      source_p,
                                      source_size,
                                      (generate_snapshot_opts & JERRY_SNAPSHOT_SAVE_STRICT) != 0,
                                      &bytecode_data_p);

  if (ECMA_IS_VALUE_ERROR (parse_status))
  {
    return ecma_create_error_reference (JERRY_CONTEXT (error_value), true);
  }

  JERRY_ASSERT (bytecode_data_p != NULL);

  if (generate_snapshot_opts & JERRY_SNAPSHOT_SAVE_STATIC)
  {
    static_snapshot_add_compiled_code (bytecode_data_p, (uint8_t *) buffer_p, buffer_size, &globals);
  }
  else
  {
    snapshot_add_compiled_code (bytecode_data_p, (uint8_t *) buffer_p, buffer_size, &globals);
  }

  if (!ecma_is_value_empty (globals.snapshot_error))
  {
    ecma_bytecode_deref (bytecode_data_p);
    return globals.snapshot_error;
  }

  jerry_snapshot_header_t header;
  header.magic = JERRY_SNAPSHOT_MAGIC;
  header.version = JERRY_SNAPSHOT_VERSION;
  header.global_flags = snapshot_get_global_flags (globals.regex_found, globals.class_found);
  header.lit_table_offset = (uint32_t) globals.snapshot_buffer_write_offset;
  header.number_of_funcs = 1;
  header.func_offsets[0] = aligned_header_size;

  lit_mem_to_snapshot_id_map_entry_t *lit_map_p = NULL;
  uint32_t literals_num = 0;

  if (!(generate_snapshot_opts & JERRY_SNAPSHOT_SAVE_STATIC))
  {
    ecma_collection_header_t *lit_pool_p = ecma_new_values_collection ();

    ecma_save_literals_add_compiled_code (bytecode_data_p, lit_pool_p);

    if (!ecma_save_literals_for_snapshot (lit_pool_p,
                                          buffer_p,
                                          buffer_size,
                                          &globals.snapshot_buffer_write_offset,
                                          &lit_map_p,
                                          &literals_num))
    {
      JERRY_ASSERT (lit_map_p == NULL);
      const char * const error_message_p = "Cannot allocate memory for literals.";
      ecma_bytecode_deref (bytecode_data_p);
      return jerry_create_error (JERRY_ERROR_COMMON, (const jerry_char_t *) error_message_p);
    }

    jerry_snapshot_set_offsets (buffer_p + (aligned_header_size / sizeof (uint32_t)),
                                (uint32_t) (header.lit_table_offset - aligned_header_size),
                                lit_map_p);
  }

  size_t header_offset = 0;

  snapshot_write_to_buffer_by_offset ((uint8_t *) buffer_p,
                                      buffer_size,
                                      &header_offset,
                                      &header,
                                      sizeof (header));

  if (lit_map_p != NULL)
  {
    jmem_heap_free_block (lit_map_p, literals_num * sizeof (lit_mem_to_snapshot_id_map_entry_t));
  }

  ecma_bytecode_deref (bytecode_data_p);

  return ecma_make_number_value ((ecma_number_t) globals.snapshot_buffer_write_offset);
} /* jerry_generate_snapshot_with_args */

#endif /* JERRY_ENABLE_SNAPSHOT_SAVE */

/**
 * Generate snapshot from specified source and arguments
 *
 * @return size of snapshot (a number value), if it was generated succesfully
 *          (i.e. there are no syntax errors in source code, buffer size is sufficient,
 *           and snapshot support is enabled in current configuration through JERRY_ENABLE_SNAPSHOT_SAVE),
 *         error object otherwise
 */
jerry_value_t
jerry_generate_snapshot (const jerry_char_t *resource_name_p, /**< script resource name */
                         size_t resource_name_length, /**< script resource name length */
                         const jerry_char_t *source_p, /**< script source */
                         size_t source_size, /**< script source size */
                         uint32_t generate_snapshot_opts, /**< jerry_generate_snapshot_opts_t option bits */
                         uint32_t *buffer_p, /**< buffer to save snapshot to */
                         size_t buffer_size) /**< the buffer's size */
{
#ifdef JERRY_ENABLE_SNAPSHOT_SAVE
  uint32_t allowed_opts = (JERRY_SNAPSHOT_SAVE_STATIC | JERRY_SNAPSHOT_SAVE_STRICT);

  if ((generate_snapshot_opts & ~(allowed_opts)) != 0)
  {
    const char * const error_message_p = "Unsupported generate snapshot flags specified.";
    return jerry_create_error (JERRY_ERROR_RANGE, (const jerry_char_t *) error_message_p);
  }

  return jerry_generate_snapshot_with_args (resource_name_p,
                                            resource_name_length,
                                            source_p,
                                            source_size,
                                            NULL,
                                            0,
                                            generate_snapshot_opts,
                                            buffer_p,
                                            buffer_size);
#else /* !JERRY_ENABLE_SNAPSHOT_SAVE */
  JERRY_UNUSED (resource_name_p);
  JERRY_UNUSED (resource_name_length);
  JERRY_UNUSED (source_p);
  JERRY_UNUSED (source_size);
  JERRY_UNUSED (generate_snapshot_opts);
  JERRY_UNUSED (buffer_p);
  JERRY_UNUSED (buffer_size);

  return 0;
#endif /* JERRY_ENABLE_SNAPSHOT_SAVE */
} /* jerry_generate_snapshot */

#ifdef JERRY_ENABLE_SNAPSHOT_EXEC
/**
 * Execute/load snapshot from specified buffer
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return result of bytecode - if run was successful
 *         thrown error - otherwise
 */
static jerry_value_t
jerry_snapshot_result (const uint32_t *snapshot_p, /**< snapshot */
                       size_t snapshot_size, /**< size of snapshot */
                       size_t func_index, /**< index of primary function */
                       uint32_t exec_snapshot_opts, /**< jerry_exec_snapshot_opts_t option bits */
                       bool as_function) /** < specify if the loaded snapshot should be returned as a function */
{
  JERRY_ASSERT (snapshot_p != NULL);

  uint32_t allowed_opts = (JERRY_SNAPSHOT_EXEC_COPY_DATA | JERRY_SNAPSHOT_EXEC_ALLOW_STATIC);

  if ((exec_snapshot_opts & ~(allowed_opts)) != 0)
  {
    ecma_raise_range_error (ECMA_ERR_MSG ("Unsupported exec snapshot flags specified."));
    return ecma_create_error_reference_from_context ();
  }

  const char * const invalid_version_error_p = "Invalid snapshot version or unsupported features present";
  const char * const invalid_format_error_p = "Invalid snapshot format";
  const uint8_t *snapshot_data_p = (uint8_t *) snapshot_p;

  if (snapshot_size <= sizeof (jerry_snapshot_header_t))
  {
    ecma_raise_type_error (invalid_format_error_p);
    return ecma_create_error_reference_from_context ();
  }

  const jerry_snapshot_header_t *header_p = (const jerry_snapshot_header_t *) snapshot_data_p;

  if (header_p->magic != JERRY_SNAPSHOT_MAGIC
      || header_p->version != JERRY_SNAPSHOT_VERSION
      || !snapshot_check_global_flags (header_p->global_flags))
  {
    ecma_raise_type_error (invalid_version_error_p);
    return ecma_create_error_reference_from_context ();
  }

  if (header_p->lit_table_offset > snapshot_size)
  {
    ecma_raise_type_error (invalid_version_error_p);
    return ecma_create_error_reference_from_context ();
  }

  if (func_index >= header_p->number_of_funcs)
  {
    ecma_raise_range_error (ECMA_ERR_MSG ("Function index is higher than maximum"));
    return ecma_create_error_reference_from_context ();
  }

  JERRY_ASSERT ((header_p->lit_table_offset % sizeof (uint32_t)) == 0);

  uint32_t func_offset = header_p->func_offsets[func_index];
  ecma_compiled_code_t *bytecode_p = (ecma_compiled_code_t *) (snapshot_data_p + func_offset);

  if (bytecode_p->status_flags & CBC_CODE_FLAGS_STATIC_FUNCTION)
  {
    if (!(exec_snapshot_opts & JERRY_SNAPSHOT_EXEC_ALLOW_STATIC))
    {
      ecma_raise_common_error (ECMA_ERR_MSG ("Static snapshots not allowed"));
      return ecma_create_error_reference_from_context ();
    }

    if (exec_snapshot_opts & JERRY_SNAPSHOT_EXEC_COPY_DATA)
    {
      ecma_raise_common_error (ECMA_ERR_MSG ("Static snapshots cannot be copied into memory"));
      return ecma_create_error_reference_from_context ();
    }
  }
  else
  {
    const uint8_t *literal_base_p = snapshot_data_p + header_p->lit_table_offset;

    bytecode_p = snapshot_load_compiled_code ((const uint8_t *) bytecode_p,
                                              literal_base_p,
                                              (exec_snapshot_opts & JERRY_SNAPSHOT_EXEC_COPY_DATA) != 0);

    if (bytecode_p == NULL)
    {
      return ecma_raise_type_error (invalid_format_error_p);
    }
  }

  ecma_value_t ret_val;

  if (as_function)
  {
    ecma_object_t *lex_env_p = ecma_get_global_environment ();
    ecma_object_t *func_obj_p = ecma_op_create_function_object (lex_env_p,
                                                                bytecode_p);

    if (!(bytecode_p->status_flags & CBC_CODE_FLAGS_STATIC_FUNCTION))
    {
      ecma_bytecode_deref (bytecode_p);
    }
    ret_val = ecma_make_object_value (func_obj_p);
  }
  else
  {
    ret_val = vm_run_global (bytecode_p);
    if (!(bytecode_p->status_flags & CBC_CODE_FLAGS_STATIC_FUNCTION))
    {
      ecma_bytecode_deref (bytecode_p);
    }
  }

  if (ECMA_IS_VALUE_ERROR (ret_val))
  {
    return ecma_create_error_reference_from_context ();
  }

  return ret_val;
} /* jerry_snapshot_result */
#endif /* JERRY_ENABLE_SNAPSHOT_EXEC */

/**
 * Execute snapshot from specified buffer
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return result of bytecode - if run was successful
 *         thrown error - otherwise
 */
jerry_value_t
jerry_exec_snapshot (const uint32_t *snapshot_p, /**< snapshot */
                     size_t snapshot_size, /**< size of snapshot */
                     size_t func_index, /**< index of primary function */
                     uint32_t exec_snapshot_opts) /**< jerry_exec_snapshot_opts_t option bits */
{
#ifdef JERRY_ENABLE_SNAPSHOT_EXEC
  return jerry_snapshot_result (snapshot_p, snapshot_size, func_index, exec_snapshot_opts, false);
#else /* !JERRY_ENABLE_SNAPSHOT_EXEC */
  JERRY_UNUSED (snapshot_p);
  JERRY_UNUSED (snapshot_size);
  JERRY_UNUSED (func_index);
  JERRY_UNUSED (exec_snapshot_opts);

  return ECMA_VALUE_FALSE;
#endif /* JERRY_ENABLE_SNAPSHOT_EXEC */
} /* jerry_exec_snapshot */

/**
 * @}
 */

#ifdef JERRY_ENABLE_SNAPSHOT_SAVE

/**
 * Collect all literals from a snapshot file.
 */
static void
scan_snapshot_functions (const uint8_t *buffer_p, /**< snapshot buffer start */
                         const uint8_t *buffer_end_p, /**< snapshot buffer end */
                         ecma_collection_header_t *lit_pool_p, /**< list of known values */
                         const uint8_t *literal_base_p) /**< start of literal data */
{
  JERRY_ASSERT (buffer_end_p > buffer_p);

  do
  {
    const ecma_compiled_code_t *bytecode_p = (ecma_compiled_code_t *) buffer_p;
    uint32_t code_size = ((uint32_t) bytecode_p->size) << JMEM_ALIGNMENT_LOG;

    if ((bytecode_p->status_flags & CBC_CODE_FLAGS_FUNCTION)
        && !(bytecode_p->status_flags & CBC_CODE_FLAGS_STATIC_FUNCTION))
    {
      const ecma_value_t *literal_start_p;
      uint32_t argument_end;
      uint32_t const_literal_end;

      if (bytecode_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
      {
        literal_start_p = (ecma_value_t *) (buffer_p + sizeof (cbc_uint16_arguments_t));

        cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) buffer_p;
        argument_end = args_p->argument_end;
        const_literal_end = (uint32_t) (args_p->const_literal_end - args_p->register_end);
      }
      else
      {
        literal_start_p = (ecma_value_t *) (buffer_p + sizeof (cbc_uint8_arguments_t));

        cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) buffer_p;
        argument_end = args_p->argument_end;
        const_literal_end = (uint32_t) (args_p->const_literal_end - args_p->register_end);
      }

      for (uint32_t i = 0; i < const_literal_end; i++)
      {
        if ((literal_start_p[i] & ECMA_VALUE_TYPE_MASK) == ECMA_TYPE_SNAPSHOT_OFFSET)
        {
          ecma_value_t lit_value = ecma_snapshot_get_literal (literal_base_p, literal_start_p[i]);
          ecma_save_literals_append_value (lit_value, lit_pool_p);
        }
      }

      if (CBC_NON_STRICT_ARGUMENTS_NEEDED (bytecode_p))
      {
        uint8_t *byte_p = (uint8_t *) bytecode_p;
        byte_p += ((size_t) bytecode_p->size) << JMEM_ALIGNMENT_LOG;
        literal_start_p = ((ecma_value_t *) byte_p) - argument_end;

        for (uint32_t i = 0; i < argument_end; i++)
        {
          if ((literal_start_p[i] & ECMA_VALUE_TYPE_MASK) == ECMA_TYPE_SNAPSHOT_OFFSET)
          {
            ecma_value_t lit_value = ecma_snapshot_get_literal (literal_base_p, literal_start_p[i]);
            ecma_save_literals_append_value (lit_value, lit_pool_p);
          }
        }
      }
    }

    buffer_p += code_size;
  }
  while (buffer_p < buffer_end_p);
} /* scan_snapshot_functions */

/**
 * Update all literal offsets in a snapshot data.
 */
static void
update_literal_offsets (uint8_t *buffer_p, /**< [in,out] snapshot buffer start */
                        const uint8_t *buffer_end_p, /**< snapshot buffer end */
                        const lit_mem_to_snapshot_id_map_entry_t *lit_map_p, /**< literal map */
                        const uint8_t *literal_base_p) /**< start of literal data */
{
  JERRY_ASSERT (buffer_end_p > buffer_p);

  do
  {
    const ecma_compiled_code_t *bytecode_p = (ecma_compiled_code_t *) buffer_p;
    uint32_t code_size = ((uint32_t) bytecode_p->size) << JMEM_ALIGNMENT_LOG;

    if ((bytecode_p->status_flags & CBC_CODE_FLAGS_FUNCTION)
        && !(bytecode_p->status_flags & CBC_CODE_FLAGS_STATIC_FUNCTION))
    {
      ecma_value_t *literal_start_p;
      uint32_t argument_end;
      uint32_t const_literal_end;

      if (bytecode_p->status_flags & CBC_CODE_FLAGS_UINT16_ARGUMENTS)
      {
        literal_start_p = (ecma_value_t *) (buffer_p + sizeof (cbc_uint16_arguments_t));

        cbc_uint16_arguments_t *args_p = (cbc_uint16_arguments_t *) buffer_p;
        argument_end = args_p->argument_end;
        const_literal_end = (uint32_t) (args_p->const_literal_end - args_p->register_end);
      }
      else
      {
        literal_start_p = (ecma_value_t *) (buffer_p + sizeof (cbc_uint8_arguments_t));

        cbc_uint8_arguments_t *args_p = (cbc_uint8_arguments_t *) buffer_p;
        argument_end = args_p->argument_end;
        const_literal_end = (uint32_t) (args_p->const_literal_end - args_p->register_end);
      }

      for (uint32_t i = 0; i < const_literal_end; i++)
      {
        if ((literal_start_p[i] & ECMA_VALUE_TYPE_MASK) == ECMA_TYPE_SNAPSHOT_OFFSET)
        {
          ecma_value_t lit_value = ecma_snapshot_get_literal (literal_base_p, literal_start_p[i]);
          const lit_mem_to_snapshot_id_map_entry_t *current_p = lit_map_p;

          while (current_p->literal_id != lit_value)
          {
            current_p++;
          }

          literal_start_p[i] = current_p->literal_offset;
        }
      }

      if (CBC_NON_STRICT_ARGUMENTS_NEEDED (bytecode_p))
      {
        uint8_t *byte_p = (uint8_t *) bytecode_p;
        byte_p += ((size_t) bytecode_p->size) << JMEM_ALIGNMENT_LOG;
        literal_start_p = ((ecma_value_t *) byte_p) - argument_end;

        for (uint32_t i = 0; i < argument_end; i++)
        {
          if ((literal_start_p[i] & ECMA_VALUE_TYPE_MASK) == ECMA_TYPE_SNAPSHOT_OFFSET)
          {
            ecma_value_t lit_value = ecma_snapshot_get_literal (literal_base_p, literal_start_p[i]);
            const lit_mem_to_snapshot_id_map_entry_t *current_p = lit_map_p;

            while (current_p->literal_id != lit_value)
            {
              current_p++;
            }

            literal_start_p[i] = current_p->literal_offset;
          }
        }
      }
    }

    buffer_p += code_size;
  }
  while (buffer_p < buffer_end_p);
} /* update_literal_offsets */

#endif /* JERRY_ENABLE_SNAPSHOT_SAVE */

/**
 * Merge multiple snapshots into a single buffer
 *
 * @return length of merged snapshot file
 *         0 on error
 */
size_t
jerry_merge_snapshots (const uint32_t **inp_buffers_p, /**< array of (pointers to start of) input buffers */
                       size_t *inp_buffer_sizes_p, /**< array of input buffer sizes */
                       size_t number_of_snapshots, /**< number of snapshots */
                       uint32_t *out_buffer_p, /**< output buffer */
                       size_t out_buffer_size, /**< output buffer size */
                       const char **error_p) /**< error description */
{
#ifdef JERRY_ENABLE_SNAPSHOT_SAVE
  uint32_t number_of_funcs = 0;
  uint32_t merged_global_flags = 0;
  size_t functions_size = sizeof (jerry_snapshot_header_t);

  if (number_of_snapshots < 2)
  {
    *error_p = "at least two snapshots must be passed";
    return 0;
  }

  ecma_collection_header_t *lit_pool_p = ecma_new_values_collection ();

  for (uint32_t i = 0; i < number_of_snapshots; i++)
  {
    if (inp_buffer_sizes_p[i] < sizeof (jerry_snapshot_header_t))
    {
      *error_p = "invalid snapshot file";
      ecma_free_values_collection (lit_pool_p, ECMA_COLLECTION_NO_COPY);
      return 0;
    }

    const jerry_snapshot_header_t *header_p = (const jerry_snapshot_header_t *) inp_buffers_p[i];

    if (header_p->magic != JERRY_SNAPSHOT_MAGIC
        || header_p->version != JERRY_SNAPSHOT_VERSION
        || !snapshot_check_global_flags (header_p->global_flags))
    {
      *error_p = "invalid snapshot version or unsupported features present";
      ecma_free_values_collection (lit_pool_p, ECMA_COLLECTION_NO_COPY);
      return 0;
    }

    merged_global_flags |= header_p->global_flags;

    uint32_t start_offset = header_p->func_offsets[0];
    const uint8_t *data_p = (const uint8_t *) inp_buffers_p[i];
    const uint8_t *literal_base_p = data_p + header_p->lit_table_offset;

    JERRY_ASSERT (header_p->number_of_funcs > 0);

    number_of_funcs += header_p->number_of_funcs;
    functions_size += header_p->lit_table_offset - start_offset;

    scan_snapshot_functions (data_p + start_offset,
                             literal_base_p,
                             lit_pool_p,
                             literal_base_p);
  }

  JERRY_ASSERT (number_of_funcs > 0);

  functions_size += JERRY_ALIGNUP ((number_of_funcs - 1) * sizeof (uint32_t), JMEM_ALIGNMENT);

  if (functions_size >= out_buffer_size)
  {
    *error_p = "output buffer is too small";
    ecma_free_values_collection (lit_pool_p, ECMA_COLLECTION_NO_COPY);
    return 0;
  }

  jerry_snapshot_header_t *header_p = (jerry_snapshot_header_t *) out_buffer_p;

  header_p->magic = JERRY_SNAPSHOT_MAGIC;
  header_p->version = JERRY_SNAPSHOT_VERSION;
  header_p->global_flags = merged_global_flags;
  header_p->lit_table_offset = (uint32_t) functions_size;
  header_p->number_of_funcs = number_of_funcs;

  lit_mem_to_snapshot_id_map_entry_t *lit_map_p;
  uint32_t literals_num;

  if (!ecma_save_literals_for_snapshot (lit_pool_p,
                                        out_buffer_p,
                                        out_buffer_size,
                                        &functions_size,
                                        &lit_map_p,
                                        &literals_num))
  {
    *error_p = "buffer is too small";
    return 0;
  }

  uint32_t *func_offset_p = header_p->func_offsets;
  uint8_t *dst_p = ((uint8_t *) out_buffer_p) + sizeof (jerry_snapshot_header_t);
  dst_p += JERRY_ALIGNUP ((number_of_funcs - 1) * sizeof (uint32_t), JMEM_ALIGNMENT);

  for (uint32_t i = 0; i < number_of_snapshots; i++)
  {
    const jerry_snapshot_header_t *current_header_p = (const jerry_snapshot_header_t *) inp_buffers_p[i];

    uint32_t start_offset = current_header_p->func_offsets[0];

    memcpy (dst_p,
            ((const uint8_t *) inp_buffers_p[i]) + start_offset,
            current_header_p->lit_table_offset - start_offset);

    const uint8_t *literal_base_p = ((const uint8_t *) inp_buffers_p[i]) + current_header_p->lit_table_offset;
    update_literal_offsets (dst_p,
                            dst_p + current_header_p->lit_table_offset - start_offset,
                            lit_map_p,
                            literal_base_p);

    uint32_t current_offset = (uint32_t) (dst_p - (uint8_t *) out_buffer_p) - start_offset;

    for (uint32_t j = 0; j < current_header_p->number_of_funcs; j++)
    {
      /* Updating offset without changing any flags. */
      *func_offset_p++ = current_header_p->func_offsets[j] + current_offset;
    }

    dst_p += current_header_p->lit_table_offset - start_offset;
  }

  JERRY_ASSERT ((uint32_t) (dst_p - (uint8_t *) out_buffer_p) == header_p->lit_table_offset);

  if (lit_map_p != NULL)
  {
    jmem_heap_free_block (lit_map_p, literals_num * sizeof (lit_mem_to_snapshot_id_map_entry_t));
  }

  *error_p = NULL;
  return functions_size;
#else /* !JERRY_ENABLE_SNAPSHOT_SAVE */
  JERRY_UNUSED (inp_buffers_p);
  JERRY_UNUSED (inp_buffer_sizes_p);
  JERRY_UNUSED (number_of_snapshots);
  JERRY_UNUSED (out_buffer_p);
  JERRY_UNUSED (out_buffer_size);
  JERRY_UNUSED (error_p);

  *error_p = "snapshot merge not supported";
  return 0;
#endif /* JERRY_ENABLE_SNAPSHOT_SAVE */
} /* jerry_merge_snapshots */

#ifdef JERRY_ENABLE_SNAPSHOT_SAVE

/**
 * ====================== Functions for literal saving ==========================
 */

/**
 * Compare two ecma_strings by size, then lexicographically.
 *
 * @return true - if the first string is less than the second one,
 *         false - otherwise
 */
static bool
jerry_save_literals_compare (ecma_string_t *literal1, /**< first literal */
                             ecma_string_t *literal2) /**< second literal */
{
  const lit_utf8_size_t lit1_size = ecma_string_get_size (literal1);
  const lit_utf8_size_t lit2_size = ecma_string_get_size (literal2);

  if (lit1_size == lit2_size)
  {
    return ecma_compare_ecma_strings_relational (literal1, literal2);
  }

  return (lit1_size < lit2_size);
} /* jerry_save_literals_compare */

/**
 * Helper function for the heapsort algorithm.
 *
 * @return index of the maximum value
 */
static lit_utf8_size_t
jerry_save_literals_heap_max (ecma_string_t *literals[], /**< array of literals */
                              lit_utf8_size_t num_of_nodes, /**< number of nodes */
                              lit_utf8_size_t node_idx, /**< index of parent node */
                              lit_utf8_size_t child_idx1, /**< index of the first child */
                              lit_utf8_size_t child_idx2) /**< index of the second child */
{
  lit_utf8_size_t max_idx = node_idx;

  if (child_idx1 < num_of_nodes
      && jerry_save_literals_compare (literals[max_idx], literals[child_idx1]))
  {
    max_idx = child_idx1;
  }

  if (child_idx2 < num_of_nodes
      && jerry_save_literals_compare (literals[max_idx], literals[child_idx2]))
  {
    max_idx = child_idx2;
  }

  return max_idx;
} /* jerry_save_literals_heap_max */

/**
 * Helper function for the heapsort algorithm.
 */
static void
jerry_save_literals_down_heap (ecma_string_t *literals[], /**< array of literals */
                               lit_utf8_size_t num_of_nodes, /**< number of nodes */
                               lit_utf8_size_t node_idx) /**< index of parent node */
{
  while (true)
  {
    lit_utf8_size_t max_idx = jerry_save_literals_heap_max (literals,
                                                            num_of_nodes,
                                                            node_idx,
                                                            2 * node_idx + 1,
                                                            2 * node_idx + 2);
    if (max_idx == node_idx)
    {
      break;
    }

    ecma_string_t *tmp_str_p  = literals[node_idx];
    literals[node_idx] = literals[max_idx];
    literals[max_idx] = tmp_str_p;

    node_idx = max_idx;
  }
} /* jerry_save_literals_down_heap */

/**
 * Helper function for a heapsort algorithm.
 */
static void
jerry_save_literals_sort (ecma_string_t *literals[], /**< array of literals */
                          lit_utf8_size_t num_of_literals) /**< number of literals */
{
  if (num_of_literals < 2)
  {
    return;
  }

  lit_utf8_size_t lit_idx = (num_of_literals - 2) / 2;

  while (lit_idx <= (num_of_literals - 2) / 2)
  {
    jerry_save_literals_down_heap (literals, num_of_literals, lit_idx--);
  }

  for (lit_idx = 0; lit_idx < num_of_literals; lit_idx++)
  {
    const lit_utf8_size_t last_idx = num_of_literals - lit_idx - 1;

    ecma_string_t *tmp_str_p = literals[last_idx];
    literals[last_idx] = literals[0];
    literals[0] = tmp_str_p;

    jerry_save_literals_down_heap (literals, last_idx, 0);
  }
} /* jerry_save_literals_sort */

/**
 * Append characters to the specified buffer.
 *
 * @return the position of the buffer pointer after copy.
 */
static uint8_t *
jerry_append_chars_to_buffer (uint8_t *buffer_p, /**< buffer */
                              uint8_t *buffer_end_p, /**< the end of the buffer */
                              const char *chars, /**< string */
                              lit_utf8_size_t string_size) /**< string size */
{
  if (buffer_p > buffer_end_p)
  {
    return buffer_p;
  }

  if (string_size == 0)
  {
    string_size = (lit_utf8_size_t) strlen (chars);
  }

  if (buffer_p + string_size <= buffer_end_p)
  {
    memcpy ((char *) buffer_p, chars, string_size);

    return buffer_p + string_size;
  }

  /* Move the pointer behind the buffer to prevent further writes. */
  return buffer_end_p + 1;
} /* jerry_append_chars_to_buffer */

/**
 * Append an ecma-string to the specified buffer.
 *
 * @return the position of the buffer pointer after copy.
 */
static uint8_t *
jerry_append_ecma_string_to_buffer (uint8_t *buffer_p, /**< buffer */
                                    uint8_t *buffer_end_p, /**< the end of the buffer */
                                    ecma_string_t *string_p) /**< ecma-string */
{
  ECMA_STRING_TO_UTF8_STRING (string_p, str_buffer_p, str_buffer_size);

  /* Append the string to the buffer. */
  uint8_t *new_buffer_p = jerry_append_chars_to_buffer (buffer_p,
                                                        buffer_end_p,
                                                        (const char *) str_buffer_p,
                                                        str_buffer_size);

  ECMA_FINALIZE_UTF8_STRING (str_buffer_p, str_buffer_size);

  return new_buffer_p;
} /* jerry_append_ecma_string_to_buffer */

/**
 * Append an unsigned number to the specified buffer.
 *
 * @return the position of the buffer pointer after copy.
 */
static uint8_t *
jerry_append_number_to_buffer (uint8_t *buffer_p, /**< buffer */
                               uint8_t *buffer_end_p, /**< the end of the buffer */
                               lit_utf8_size_t number) /**< number */
{
  lit_utf8_byte_t uint32_to_str_buffer[ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32];
  lit_utf8_size_t utf8_str_size = ecma_uint32_to_utf8_string (number,
                                                              uint32_to_str_buffer,
                                                              ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32);

  JERRY_ASSERT (utf8_str_size <= ECMA_MAX_CHARS_IN_STRINGIFIED_UINT32);

  return jerry_append_chars_to_buffer (buffer_p,
                                       buffer_end_p,
                                       (const char *) uint32_to_str_buffer,
                                       utf8_str_size);
} /* jerry_append_number_to_buffer */

/**
 * Check whether the passed ecma-string is a valid identifier.
 *
 * @return true - if the ecma-string is a valid identifier,
 *         false - otherwise
 */
static bool
ecma_string_is_valid_identifier (const ecma_string_t *string_p)
{
  bool result = false;

  ECMA_STRING_TO_UTF8_STRING (string_p, str_buffer_p, str_buffer_size);

  if (lit_char_is_identifier_start (str_buffer_p))
  {
    const uint8_t *str_start_p = str_buffer_p;
    const uint8_t *str_end_p = str_buffer_p + str_buffer_size;

    result = true;

    while (str_start_p < str_end_p)
    {
      if (!lit_char_is_identifier_part (str_start_p))
      {
        result = false;
        break;
      }
      lit_utf8_incr (&str_start_p);
    }
  }

  ECMA_FINALIZE_UTF8_STRING (str_buffer_p, str_buffer_size);

  return result;
} /* ecma_string_is_valid_identifier */

#endif /* JERRY_ENABLE_SNAPSHOT_SAVE */

/**
 * Get the literals from a snapshot. Copies certain string literals into the given
 * buffer in a specified format.
 *
 * Note:
 *      Only valid identifiers are saved in C format.
 *
 * @return size of the literal-list in bytes, at most equal to the buffer size,
 *         if the list of the literals isn't empty,
 *         0 - otherwise.
 */
size_t
jerry_get_literals_from_snapshot (const uint32_t *snapshot_p, /**< input snapshot buffer */
                                  size_t snapshot_size, /**< size of the input snapshot buffer */
                                  jerry_char_t *lit_buf_p, /**< [out] buffer to save literals to */
                                  size_t lit_buf_size, /**< the buffer's size */
                                  bool is_c_format) /**< format-flag */
{
#ifdef JERRY_ENABLE_SNAPSHOT_SAVE
  const uint8_t *snapshot_data_p = (uint8_t *) snapshot_p;
  const jerry_snapshot_header_t *header_p = (const jerry_snapshot_header_t *) snapshot_data_p;

  if (snapshot_size <= sizeof (jerry_snapshot_header_t)
      || header_p->magic != JERRY_SNAPSHOT_MAGIC
      || header_p->version != JERRY_SNAPSHOT_VERSION
      || !snapshot_check_global_flags (header_p->global_flags))
  {
    /* Invalid snapshot format */
    return 0;
  }

  JERRY_ASSERT ((header_p->lit_table_offset % sizeof (uint32_t)) == 0);
  const uint8_t *literal_base_p = snapshot_data_p + header_p->lit_table_offset;

  ecma_collection_header_t *lit_pool_p = ecma_new_values_collection ();
  scan_snapshot_functions (snapshot_data_p + header_p->func_offsets[0],
                           literal_base_p,
                           lit_pool_p,
                           literal_base_p);

  lit_utf8_size_t literal_count = 0;
  ecma_value_t *iterator_p = ecma_collection_iterator_init (lit_pool_p);

  /* Count the valid and non-magic identifiers in the list. */
  while (iterator_p != NULL)
  {
    if (ecma_is_value_string (*iterator_p))
    {
      ecma_string_t *literal_p = ecma_get_string_from_value (*iterator_p);

      /* NOTE:
       *      We don't save a literal (in C format) which isn't a valid
       *      identifier or it's a magic string.
       * TODO:
       *      Save all of the literals in C format as well.
       */
      if (ecma_get_string_magic (literal_p) == LIT_MAGIC_STRING__COUNT
          && (!is_c_format || ecma_string_is_valid_identifier (literal_p)))
      {
        literal_count++;
      }
    }

    iterator_p = ecma_collection_iterator_next (iterator_p);
  }

  if (literal_count == 0)
  {
    ecma_free_values_collection (lit_pool_p, ECMA_COLLECTION_NO_COPY);
    return 0;
  }

  jerry_char_t *const buffer_start_p = lit_buf_p;
  jerry_char_t *const buffer_end_p = lit_buf_p + lit_buf_size;

  JMEM_DEFINE_LOCAL_ARRAY (literal_array, literal_count, ecma_string_t *);
  lit_utf8_size_t literal_idx = 0;

  iterator_p = ecma_collection_iterator_init (lit_pool_p);

  while (iterator_p != NULL)
  {
    if (ecma_is_value_string (*iterator_p))
    {
      ecma_string_t *literal_p = ecma_get_string_from_value (*iterator_p);

      /* NOTE:
       *      We don't save a literal (in C format) which isn't a valid
       *      identifier or it's a magic string.
       * TODO:
       *      Save all of the literals in C format as well.
       */
      if (ecma_get_string_magic (literal_p) == LIT_MAGIC_STRING__COUNT
          && (!is_c_format || ecma_string_is_valid_identifier (literal_p)))
      {
        literal_array[literal_idx++] = literal_p;
      }
    }

    iterator_p = ecma_collection_iterator_next (iterator_p);
  }

  ecma_free_values_collection (lit_pool_p, ECMA_COLLECTION_NO_COPY);

  /* Sort the strings by size at first, then lexicographically. */
  jerry_save_literals_sort (literal_array, literal_count);

  if (is_c_format)
  {
    /* Save literal count. */
    lit_buf_p = jerry_append_chars_to_buffer (lit_buf_p,
                                              buffer_end_p,
                                              "jerry_length_t literal_count = ",
                                              0);

    lit_buf_p = jerry_append_number_to_buffer (lit_buf_p, buffer_end_p, literal_count);

    /* Save the array of literals. */
    lit_buf_p = jerry_append_chars_to_buffer (lit_buf_p,
                                              buffer_end_p,
                                              ";\n\njerry_char_t *literals[",
                                              0);

    lit_buf_p = jerry_append_number_to_buffer (lit_buf_p, buffer_end_p, literal_count);
    lit_buf_p = jerry_append_chars_to_buffer (lit_buf_p, buffer_end_p, "] =\n{\n", 0);

    for (lit_utf8_size_t i = 0; i < literal_count; i++)
    {
      lit_buf_p = jerry_append_chars_to_buffer (lit_buf_p, buffer_end_p, "  \"", 0);
      lit_buf_p = jerry_append_ecma_string_to_buffer (lit_buf_p, buffer_end_p, literal_array[i]);
      lit_buf_p = jerry_append_chars_to_buffer (lit_buf_p, buffer_end_p, "\"", 0);

      if (i < literal_count - 1)
      {
        lit_buf_p = jerry_append_chars_to_buffer (lit_buf_p, buffer_end_p, ",", 0);
      }

      lit_buf_p = jerry_append_chars_to_buffer (lit_buf_p, buffer_end_p, "\n", 0);
    }

    lit_buf_p = jerry_append_chars_to_buffer (lit_buf_p,
                                              buffer_end_p,
                                              "};\n\njerry_length_t literal_sizes[",
                                              0);

    lit_buf_p = jerry_append_number_to_buffer (lit_buf_p, buffer_end_p, literal_count);
    lit_buf_p = jerry_append_chars_to_buffer (lit_buf_p, buffer_end_p, "] =\n{\n", 0);
  }

  /* Save the literal sizes respectively. */
  for (lit_utf8_size_t i = 0; i < literal_count; i++)
  {
    lit_utf8_size_t str_size = ecma_string_get_size (literal_array[i]);

    if (is_c_format)
    {
      lit_buf_p = jerry_append_chars_to_buffer (lit_buf_p, buffer_end_p, "  ", 0);
    }

    lit_buf_p = jerry_append_number_to_buffer (lit_buf_p, buffer_end_p, str_size);
    lit_buf_p = jerry_append_chars_to_buffer (lit_buf_p, buffer_end_p, " ", 0);

    if (is_c_format)
    {
      /* Show the given string as a comment. */
      lit_buf_p = jerry_append_chars_to_buffer (lit_buf_p, buffer_end_p, "/* ", 0);
      lit_buf_p = jerry_append_ecma_string_to_buffer (lit_buf_p, buffer_end_p, literal_array[i]);
      lit_buf_p = jerry_append_chars_to_buffer (lit_buf_p, buffer_end_p, " */", 0);

      if (i < literal_count - 1)
      {
        lit_buf_p = jerry_append_chars_to_buffer (lit_buf_p, buffer_end_p, ",", 0);
      }
    }
    else
    {
      lit_buf_p = jerry_append_ecma_string_to_buffer (lit_buf_p, buffer_end_p, literal_array[i]);
    }

    lit_buf_p = jerry_append_chars_to_buffer (lit_buf_p, buffer_end_p, "\n", 0);
  }

  if (is_c_format)
  {
    lit_buf_p = jerry_append_chars_to_buffer (lit_buf_p, buffer_end_p, "};\n", 0);
  }

  JMEM_FINALIZE_LOCAL_ARRAY (literal_array);

  return lit_buf_p <= buffer_end_p ? (size_t) (lit_buf_p - buffer_start_p) : 0;
#else /* !JERRY_ENABLE_SNAPSHOT_SAVE */
  JERRY_UNUSED (snapshot_p);
  JERRY_UNUSED (snapshot_size);
  JERRY_UNUSED (lit_buf_p);
  JERRY_UNUSED (lit_buf_size);
  JERRY_UNUSED (is_c_format);

  return 0;
#endif /* JERRY_ENABLE_SNAPSHOT_SAVE */
} /* jerry_get_literals_from_snapshot */


/**
 * Generate snapshot function from specified source and arguments
 *
 * @return size of snapshot (a number value), if it was generated succesfully
 *          (i.e. there are no syntax errors in source code, buffer size is sufficient,
 *           and snapshot support is enabled in current configuration through JERRY_ENABLE_SNAPSHOT_SAVE),
 *         error object otherwise
 */
jerry_value_t
jerry_generate_function_snapshot (const jerry_char_t *resource_name_p, /**< script resource name */
                                  size_t resource_name_length, /**< script resource name length */
                                  const jerry_char_t *source_p, /**< script source */
                                  size_t source_size, /**< script source size */
                                  const jerry_char_t *args_p, /**< arguments string */
                                  size_t args_size, /**< arguments string size */
                                  uint32_t generate_snapshot_opts, /**< jerry_generate_snapshot_opts_t option bits */
                                  uint32_t *buffer_p, /**< buffer to save snapshot to */
                                  size_t buffer_size) /**< the buffer's size */
{
#ifdef JERRY_ENABLE_SNAPSHOT_SAVE
  uint32_t allowed_opts = (JERRY_SNAPSHOT_SAVE_STATIC | JERRY_SNAPSHOT_SAVE_STRICT);

  if ((generate_snapshot_opts & ~(allowed_opts)) != 0)
  {
    const char * const error_message_p = "Unsupported generate snapshot flags specified.";
    return jerry_create_error (JERRY_ERROR_RANGE, (const jerry_char_t *) error_message_p);
  }

  return jerry_generate_snapshot_with_args (resource_name_p,
                                            resource_name_length,
                                            source_p,
                                            source_size,
                                            args_p,
                                            args_size,
                                            generate_snapshot_opts,
                                            buffer_p,
                                            buffer_size);
#else /* !JERRY_ENABLE_SNAPSHOT_SAVE */
  JERRY_UNUSED (resource_name_p);
  JERRY_UNUSED (resource_name_length);
  JERRY_UNUSED (source_p);
  JERRY_UNUSED (source_size);
  JERRY_UNUSED (args_p);
  JERRY_UNUSED (args_size);
  JERRY_UNUSED (generate_snapshot_opts);
  JERRY_UNUSED (buffer_p);
  JERRY_UNUSED (buffer_size);

  return 0;
#endif /* JERRY_ENABLE_SNAPSHOT_SAVE */
} /* jerry_generate_function_snapshot */

/**
 * Load function from specified snapshot buffer
 *
 * Note:
 *      returned value must be freed with jerry_release_value, when it is no longer needed.
 *
 * @return result of bytecode - if run was successful
 *         thrown error - otherwise
 */
jerry_value_t
jerry_load_function_snapshot (const uint32_t *function_snapshot_p, /**< snapshot of the function(s) */
                              const size_t function_snapshot_size, /**< size of the snapshot */
                              size_t func_index, /**< index of the function to load */
                              uint32_t exec_snapshot_opts) /**< jerry_exec_snapshot_opts_t option bits */
{
#ifdef JERRY_ENABLE_SNAPSHOT_EXEC
  return jerry_snapshot_result (function_snapshot_p, function_snapshot_size, func_index, exec_snapshot_opts, true);
#else /* !JERRY_ENABLE_SNAPSHOT_EXEC */
  JERRY_UNUSED (function_snapshot_p);
  JERRY_UNUSED (function_snapshot_size);
  JERRY_UNUSED (func_index);
  JERRY_UNUSED (exec_snapshot_opts);

  return ECMA_VALUE_FALSE;
#endif /* JERRY_ENABLE_SNAPSHOT_EXEC */
} /* jerry_load_function_snapshot */
