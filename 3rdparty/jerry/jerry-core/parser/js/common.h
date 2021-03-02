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

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <setjmp.h>

/** \addtogroup parser Parser
 * @{
 *
 * \addtogroup jsparser JavaScript
 * @{
 *
 * \addtogroup jsparser_utils Utility
 * @{
 */

#include "ecma-globals.h"
#include "ecma-regexp-object.h"
#include "jmem.h"

/* Immediate management. */

/**
 * Literal types.
 *
 * The LEXER_UNUSED_LITERAL type is internal and
 * used for various purposes.
 */
typedef enum
{
  LEXER_IDENT_LITERAL = 0,          /**< identifier literal */
  LEXER_STRING_LITERAL = 1,         /**< string literal */
  LEXER_NUMBER_LITERAL = 2,         /**< number literal */
  LEXER_FUNCTION_LITERAL = 3,       /**< function literal */
  LEXER_REGEXP_LITERAL = 4,         /**< regexp literal */
  LEXER_UNUSED_LITERAL = 5,         /**< unused literal, can only be
                                         used by the byte code generator. */
} lexer_literal_type_t;

/**
 * Flag bits for status_flags member of lexer_literal_t.
 */
typedef enum
{
  LEXER_FLAG_VAR = (1 << 0), /**< local identifier (var, function arg) */
  LEXER_FLAG_NO_REG_STORE = (1 << 1), /**< this local identifier cannot be stored in register */
  LEXER_FLAG_INITIALIZED = (1 << 2), /**< this local identifier is initialized with a value */
  LEXER_FLAG_FUNCTION_ARGUMENT = (1 << 3), /**< this local identifier is a function argument */
  LEXER_FLAG_UNUSED_IDENT = (1 << 4), /**< this identifier is referenced by sub-functions,
                                       *   but not referenced by the currently parsed function */
  LEXER_FLAG_SOURCE_PTR = (1 << 5), /**< the literal is directly referenced in the source code
                                     *   (no need to allocate memory) */
  LEXER_FLAG_LATE_INIT = (1 << 6), /**< initialize this variable after the byte code is freed */
} lexer_literal_status_flags_t;

/**
 * Type of property length.
 */
#ifdef JERRY_CPOINTER_32_BIT
typedef uint32_t prop_length_t;
#else /* !JERRY_CPOINTER_32_BIT */
typedef uint16_t prop_length_t;
#endif /* JERRY_CPOINTER_32_BIT */

/**
 * Literal data.
 */
typedef struct
{
  union
  {
    ecma_value_t value;                  /**< literal value (not processed by the parser) */
    const uint8_t *char_p;               /**< character value */
    ecma_compiled_code_t *bytecode_p;    /**< compiled function or regexp pointer */
    uint32_t source_data;                /**< encoded source literal */
  } u;

#ifdef PARSER_DUMP_BYTE_CODE
  struct
#else /* !PARSER_DUMP_BYTE_CODE */
  union
#endif /* PARSER_DUMP_BYTE_CODE */
  {
    prop_length_t length;                /**< length of ident / string literal */
    uint16_t index;                      /**< real index during post processing */
  } prop;

  uint8_t type;                          /**< type of the literal */
  uint8_t status_flags;                  /**< status flags */
} lexer_literal_t;

void util_free_literal (lexer_literal_t *literal_p);

#ifdef PARSER_DUMP_BYTE_CODE
void util_print_literal (lexer_literal_t *);
#endif /* PARSER_DUMP_BYTE_CODE */

/* TRY/CATCH block */

#define PARSER_TRY_CONTEXT(context_name) \
  jmp_buf context_name

#define PARSER_THROW(context_name) \
  longjmp (context_name, 1);

#define PARSER_TRY(context_name) \
  { \
    if (!setjmp (context_name)) \
    { \

#define PARSER_CATCH \
    } \
    else \
    {

#define PARSER_TRY_END \
    } \
  }

/**
 * @}
 * @}
 * @}
 */

#endif /* !COMMON_H */
