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

#ifndef JS_PARSER_INTERNAL_H
#define JS_PARSER_INTERNAL_H

#include "common.h"

#include "byte-code.h"
#include "debugger.h"
#include "js-parser.h"
#include "js-parser-limits.h"
#include "js-lexer.h"

/** \addtogroup parser Parser
 * @{
 *
 * \addtogroup jsparser JavaScript
 * @{
 *
 * \addtogroup jsparser_internals Internals
 * @{
 */

/**
 * General parser flags.
 */
typedef enum
{
  PARSER_IS_STRICT = (1u << 0),               /**< strict mode code */
  PARSER_IS_FUNCTION = (1u << 1),             /**< function body is parsed */
  PARSER_IS_CLOSURE = (1u << 2),              /**< function body is encapsulated in {} block */
  PARSER_IS_FUNC_EXPRESSION = (1u << 3),      /**< a function expression is parsed */
  PARSER_IS_PROPERTY_GETTER = (1u << 4),      /**< a property getter function is parsed */
  PARSER_IS_PROPERTY_SETTER = (1u << 5),      /**< a property setter function is parsed */
#ifndef CONFIG_DISABLE_ES2015_FUNCTION_REST_PARAMETER
  PARSER_FUNCTION_HAS_REST_PARAM = (1u << 6), /**< function has rest parameter */
#endif /* !CONFIG_DISABLE_ES2015_FUNCTION_REST_PARAMETER */
  PARSER_HAS_NON_STRICT_ARG = (1u << 7),      /**< the function has arguments which
                                               *   are not supported in strict mode */
  PARSER_ARGUMENTS_NEEDED = (1u << 8),        /**< arguments object must be created */
  PARSER_ARGUMENTS_NOT_NEEDED = (1u << 9),    /**< arguments object must NOT be created */
  PARSER_LEXICAL_ENV_NEEDED = (1u << 10),     /**< lexical environment object must be created */
  PARSER_NO_REG_STORE = (1u << 11),           /**< all local variables must be stored
                                               *   in the lexical environment object */
  PARSER_INSIDE_WITH = (1u << 12),            /**< code block is inside a with statement */
  PARSER_RESOLVE_BASE_FOR_CALLS = (1u << 13), /**< the this object must be resolved when
                                               *   a function without a base object is called */
  PARSER_HAS_INITIALIZED_VARS = (1u << 14),   /**< a CBC_INITIALIZE_VARS instruction must be emitted */
  PARSER_HAS_LATE_LIT_INIT = (1u << 15),      /**< allocate memory for this string after
                                               *   the local parser data is freed */
  PARSER_NO_END_LABEL = (1u << 16),           /**< return instruction must be inserted
                                               *   after the last byte code */
  PARSER_DEBUGGER_BREAKPOINT_APPENDED = (1u << 17), /**< pending (unsent) breakpoint
                                                     *   info is available */
#ifndef CONFIG_DISABLE_ES2015_ARROW_FUNCTION
  PARSER_IS_ARROW_FUNCTION = (1u << 18),      /**< an arrow function is parsed */
  PARSER_ARROW_PARSE_ARGS = (1u << 19),       /**< parse the argument list of an arrow function */
#endif /* !CONFIG_DISABLE_ES2015_ARROW_FUNCTION */
#ifndef CONFIG_DISABLE_ES2015_CLASS
  /* These four status flags must be in this order. See PARSER_CLASS_PARSE_OPTS_OFFSET. */
  PARSER_CLASS_CONSTRUCTOR = (1u << 20),      /**< a class constructor is parsed (this value must be kept in
                                               *   in sync with ECMA_PARSE_CLASS_CONSTRUCTOR) */
  PARSER_CLASS_HAS_SUPER = (1u << 21),        /**< class has super reference */
  PARSER_CLASS_IMPLICIT_SUPER = (1u << 22),   /**< class has implicit parent class */
  PARSER_CLASS_STATIC_FUNCTION = (1u << 23),  /**< this function is a static class method */
  PARSER_CLASS_SUPER_PROP_REFERENCE = (1u << 24),  /**< super property call or assignment */
#endif /* !CONFIG_DISABLE_ES2015_CLASS */
} parser_general_flags_t;

/**
 * Expression parsing flags.
 */
typedef enum
{
  PARSE_EXPR = 0,                             /**< parse an expression without any special flags */
  PARSE_EXPR_STATEMENT = (1u << 0),           /**< discard the result of the expression */
  PARSE_EXPR_BLOCK = (1u << 1),               /**< copy the expression result into the block result */
  PARSE_EXPR_NO_COMMA = (1u << 2),            /**< do not parse comma operator */
  PARSE_EXPR_HAS_LITERAL = (1u << 3),         /**< a primary literal is provided by a
                                               *   CBC_PUSH_LITERAL instruction  */
} parser_expression_flags_t;

/**
 * Mask for strict mode code
 */
#define PARSER_STRICT_MODE_MASK 0x1

#ifndef CONFIG_DISABLE_ES2015_CLASS
/**
 * Offset between PARSER_CLASS_CONSTRUCTOR and ECMA_PARSE_CLASS_CONSTRUCTOR
 */
#define PARSER_CLASS_PARSE_OPTS_OFFSET \
  (JERRY_LOG2 (PARSER_CLASS_CONSTRUCTOR) - JERRY_LOG2 (ECMA_PARSE_CLASS_CONSTRUCTOR))

/**
 * Count of ecma_parse_opts_t class parsing options related bits
 */
#define PARSER_CLASS_PARSE_OPTS_COUNT \
  (JERRY_LOG2 (ECMA_PARSE_HAS_STATIC_SUPER) - JERRY_LOG2 (ECMA_PARSE_CLASS_CONSTRUCTOR))

/**
 * Mask for get class option bits from ecma_parse_opts_t
 */
#define PARSER_CLASS_ECMA_PARSE_OPTS_TO_PARSER_OPTS_MASK \
  (((1 << PARSER_CLASS_PARSE_OPTS_COUNT) - 1) << JERRY_LOG2 (ECMA_PARSE_CLASS_CONSTRUCTOR))

/**
 * Get class option bits from ecma_parse_opts_t
 */
#define PARSER_GET_CLASS_PARSER_OPTS(opts) \
  (((opts) & PARSER_CLASS_ECMA_PARSE_OPTS_TO_PARSER_OPTS_MASK) << PARSER_CLASS_PARSE_OPTS_OFFSET)

/**
 * Get class option bits from parser_general_flags_t
 */
#define PARSER_GET_CLASS_ECMA_PARSE_OPTS(opts) \
  ((uint16_t) (((opts) >> PARSER_CLASS_PARSE_OPTS_OFFSET) & PARSER_CLASS_ECMA_PARSE_OPTS_TO_PARSER_OPTS_MASK))

/**
 * Class constructor with heritage context representing bits
 */
#define PARSER_CLASS_CONSTRUCTOR_SUPER (PARSER_CLASS_CONSTRUCTOR | PARSER_CLASS_HAS_SUPER)

/**
 * Check the scope is a class constructor with heritage context
 */
#define PARSER_IS_CLASS_CONSTRUCTOR_SUPER(flag) \
  (((flag) & PARSER_CLASS_CONSTRUCTOR_SUPER) == PARSER_CLASS_CONSTRUCTOR_SUPER)
#endif /* !CONFIG_DISABLE_ES2015_CLASS */

/* The maximum of PARSER_CBC_STREAM_PAGE_SIZE is 127. */
#define PARSER_CBC_STREAM_PAGE_SIZE \
  ((uint32_t) (64 - sizeof (void *)))

#define PARSER_STACK_PAGE_SIZE \
  ((uint32_t) (((sizeof (void *) > 4) ? 128 : 64) - sizeof (void *)))

/* Avoid compiler warnings for += operations. */
#define PARSER_PLUS_EQUAL_U16(base, value) (base) = (uint16_t) ((base) + (value))
#define PARSER_MINUS_EQUAL_U16(base, value) (base) = (uint16_t) ((base) - (value))
#define PARSER_PLUS_EQUAL_LC(base, value) (base) = (parser_line_counter_t) ((base) + (value))

/**
 * Argument for a compact-byte code.
 */
typedef struct
{
  uint16_t literal_index;                     /**< literal index argument */
  uint16_t value;                             /**< other argument (second literal or byte). */
  uint16_t third_literal_index;               /**< literal index argument */
  uint8_t literal_type;                       /**< last literal type */
  uint8_t literal_object_type;                /**< last literal object type */
} cbc_argument_t;

/* Useful parser macros. */

#define PARSER_CBC_UNAVAILABLE CBC_EXT_OPCODE

#define PARSER_TO_EXT_OPCODE(opcode) ((uint16_t) ((opcode) + 256))
#define PARSER_GET_EXT_OPCODE(opcode) ((opcode) - 256)
#define PARSER_IS_BASIC_OPCODE(opcode) ((opcode) < 256)
#define PARSER_IS_PUSH_LITERAL(opcode) \
  ((opcode) == CBC_PUSH_LITERAL \
   || (opcode) == CBC_PUSH_TWO_LITERALS \
   || (opcode) == CBC_PUSH_THREE_LITERALS)
#define PARSER_IS_PUSH_NUMBER(opcode) \
  ((opcode) == CBC_PUSH_NUMBER_0 \
   || (opcode) == CBC_PUSH_NUMBER_POS_BYTE \
   || (opcode) == CBC_PUSH_NUMBER_NEG_BYTE \
   || (opcode) == PARSER_TO_EXT_OPCODE (CBC_EXT_PUSH_LITERAL_PUSH_NUMBER_0) \
   || (opcode) == PARSER_TO_EXT_OPCODE (CBC_EXT_PUSH_LITERAL_PUSH_NUMBER_POS_BYTE) \
   || (opcode) == PARSER_TO_EXT_OPCODE (CBC_EXT_PUSH_LITERAL_PUSH_NUMBER_NEG_BYTE))

#define PARSER_GET_LITERAL(literal_index) \
  ((lexer_literal_t *) parser_list_get (&context_p->literal_pool, (literal_index)))

#define PARSER_TO_BINARY_OPERATION_WITH_RESULT(opcode) \
  (PARSER_TO_EXT_OPCODE(opcode) - CBC_ASSIGN_ADD + CBC_EXT_ASSIGN_ADD_PUSH_RESULT)

#define PARSER_TO_BINARY_OPERATION_WITH_BLOCK(opcode) \
  ((uint16_t) (PARSER_TO_EXT_OPCODE(opcode) - CBC_ASSIGN_ADD + CBC_EXT_ASSIGN_ADD_BLOCK))

#define PARSER_GET_FLAGS(op) \
  (PARSER_IS_BASIC_OPCODE (op) ? cbc_flags[(op)] : cbc_ext_flags[PARSER_GET_EXT_OPCODE (op)])

#define PARSER_OPCODE_IS_RETURN(op) \
  ((op) == CBC_RETURN || (op) == CBC_RETURN_WITH_BLOCK || (op) == CBC_RETURN_WITH_LITERAL)

#define PARSER_ARGS_EQ(op, types) \
  ((PARSER_GET_FLAGS (op) & CBC_ARG_TYPES) == (types))

/**
 * All data allocated by the parser is
 * stored in parser_data_pages in the memory.
 */
typedef struct parser_mem_page_t
{
  struct parser_mem_page_t *next_p;           /**< next page */
  uint8_t bytes[1];                           /**< memory bytes */
} parser_mem_page_t;

/**
 * Structure for managing parser memory.
 */
typedef struct
{
  parser_mem_page_t *first_p;                 /**< first allocated page */
  parser_mem_page_t *last_p;                  /**< last allocated page */
  uint32_t last_position;                     /**< position of the last allocated byte */
} parser_mem_data_t;

/**
 * Parser memory list.
 */
typedef struct
{
  parser_mem_data_t data;                     /**< storage space */
  uint32_t page_size;                         /**< size of each page */
  uint32_t item_size;                         /**< size of each item */
  uint32_t item_count;                        /**< number of items on each page */
} parser_list_t;

/**
 * Iterator for parser memory list.
 */
typedef struct
{
  parser_list_t *list_p;                      /**< parser list */
  parser_mem_page_t *current_p;               /**< currently processed page */
  size_t current_position;                    /**< current position on the page */
} parser_list_iterator_t;

/**
 * Parser memory stack.
 */
typedef struct
{
  parser_mem_data_t data;                     /**< storage space */
  parser_mem_page_t *free_page_p;             /**< space for fast allocation */
} parser_stack_t;

/**
 * Iterator for parser memory stack.
 */
typedef struct
{
  parser_mem_page_t *current_p;               /**< currently processed page */
  size_t current_position;                    /**< current position on the page */
} parser_stack_iterator_t;

/**
 * Branch type.
 */
typedef struct
{
  parser_mem_page_t *page_p;                  /**< branch location page */
  uint32_t offset;                            /**< branch location offset */
} parser_branch_t;

/**
 * Branch chain type.
 */
typedef struct parser_branch_node_t
{
  struct parser_branch_node_t *next_p;        /**< next linked list node */
  parser_branch_t branch;                     /**< branch */
} parser_branch_node_t;

#ifdef JERRY_DEBUGGER
/**
 * Extra information for each breakpoint.
 */
typedef struct
{
  uint32_t value;                             /**< line or offset of the breakpoint */
} parser_breakpoint_info_t;

/**
 * Maximum number of breakpoint info.
 */
#define PARSER_MAX_BREAKPOINT_INFO_COUNT \
  (JERRY_DEBUGGER_TRANSPORT_MAX_BUFFER_SIZE / sizeof (parser_breakpoint_info_t))

#endif /* JERRY_DEBUGGER */

/**
 * Those members of a context which needs
 * to be saved when a sub-function is parsed.
 */
typedef struct parser_saved_context_t
{
  /* Parser members. */
  uint32_t status_flags;                      /**< parsing options */
  uint16_t stack_depth;                       /**< current stack depth */
  uint16_t stack_limit;                       /**< maximum stack depth */
  struct parser_saved_context_t *prev_context_p; /**< last saved context */
  parser_stack_iterator_t last_statement;     /**< last statement position */

  /* Literal types */
  uint16_t argument_count;                    /**< number of function arguments */
  uint16_t register_count;                    /**< number of registers */
  uint16_t literal_count;                     /**< number of literals */

  /* Memory storage members. */
  parser_mem_data_t byte_code;                /**< byte code buffer */
  uint32_t byte_code_size;                    /**< byte code size for branches */
  parser_mem_data_t literal_pool_data;        /**< literal list */

#ifndef JERRY_NDEBUG
  uint16_t context_stack_depth;               /**< current context stack depth */
#endif /* !JERRY_NDEBUG */
} parser_saved_context_t;

/**
 * Shared parser context.
 */
typedef struct
{
  PARSER_TRY_CONTEXT (try_buffer);            /**< try_buffer */
  parser_error_t error;                       /**< error code */
  void *allocated_buffer_p;                   /**< dinamically allocated buffer
                                               *   which needs to be freed on error */
  uint32_t allocated_buffer_size;             /**< size of the dinamically allocated buffer */

  /* Parser members. */
  uint32_t status_flags;                      /**< status flags */
  uint16_t stack_depth;                       /**< current stack depth */
  uint16_t stack_limit;                       /**< maximum stack depth */
  parser_saved_context_t *last_context_p;     /**< last saved context */
  parser_stack_iterator_t last_statement;     /**< last statement position */

  /* Lexer members. */
  lexer_token_t token;                        /**< current token */
  lexer_lit_object_t lit_object;              /**< current literal object */
  const uint8_t *source_p;                    /**< next source byte */
  const uint8_t *source_end_p;                /**< last source byte */
  parser_line_counter_t line;                 /**< current line */
  parser_line_counter_t column;               /**< current column */

  /* Compact byte code members. */
  cbc_argument_t last_cbc;                    /**< argument of the last cbc */
  uint16_t last_cbc_opcode;                   /**< opcode of the last cbc */

  /* Literal types */
  uint16_t argument_count;                    /**< number of function arguments */
  uint16_t register_count;                    /**< number of registers */
  uint16_t literal_count;                     /**< number of literals */

  /* Memory storage members. */
  parser_mem_data_t byte_code;                /**< byte code buffer */
  uint32_t byte_code_size;                    /**< current byte code size for branches */
  parser_list_t literal_pool;                 /**< literal list */
  parser_mem_data_t stack;                    /**< storage space */
  parser_mem_page_t *free_page_p;             /**< space for fast allocation */
  uint8_t stack_top_uint8;                    /**< top byte stored on the stack */

#ifndef JERRY_NDEBUG
  /* Variables for debugging / logging. */
  uint16_t context_stack_depth;               /**< current context stack depth */
#endif /* !JERRY_NDEBUG */

#ifdef PARSER_DUMP_BYTE_CODE
  int is_show_opcodes;                        /**< show opcodes */
  uint32_t total_byte_code_size;              /**< total byte code size */
#endif /* PARSER_DUMP_BYTE_CODE */

#ifdef JERRY_DEBUGGER
  parser_breakpoint_info_t breakpoint_info[PARSER_MAX_BREAKPOINT_INFO_COUNT]; /**< breakpoint info list */
  uint16_t breakpoint_info_count; /**< current breakpoint index */
  parser_line_counter_t last_breakpoint_line; /**< last line where breakpoint has been inserted */
#endif /* JERRY_DEBUGGER */

#ifdef JERRY_ENABLE_LINE_INFO
  parser_line_counter_t last_line_info_line; /**< last line where line info has been inserted */
#endif /* JERRY_ENABLE_LINE_INFO */
} parser_context_t;

/**
 * @}
 * @}
 * @}
 *
 * \addtogroup mem Memory allocation
 * @{
 *
 * \addtogroup mem_parser Parser memory manager
 * @{
 */

/* Memory management.
 * Note: throws an error if unsuccessful. */
void *parser_malloc (parser_context_t *context_p, size_t size);
void parser_free (void *ptr, size_t size);
void *parser_malloc_local (parser_context_t *context_p, size_t size);
void parser_free_local (void *ptr, size_t size);

/* Parser byte stream. */

void parser_cbc_stream_init (parser_mem_data_t *data_p);
void parser_cbc_stream_free (parser_mem_data_t *data_p);
void parser_cbc_stream_alloc_page (parser_context_t *context_p, parser_mem_data_t *data_p);

/* Parser list. Ensures pointer alignment. */

void parser_list_init (parser_list_t *list_p, uint32_t item_size, uint32_t item_count);
void parser_list_free (parser_list_t *list_p);
void parser_list_reset (parser_list_t *list_p);
void *parser_list_append (parser_context_t *context_p, parser_list_t *list_p);
void *parser_list_get (parser_list_t *list_p, size_t index);
void parser_list_iterator_init (parser_list_t *list_p, parser_list_iterator_t *iterator_p);
void *parser_list_iterator_next (parser_list_iterator_t *iterator_p);

/* Parser stack. Optimized for pushing bytes.
 * Pop functions never throws error. */

void parser_stack_init (parser_context_t *context_p);
void parser_stack_free (parser_context_t *context_p);
void parser_stack_push_uint8 (parser_context_t *context_p, uint8_t uint8_value);
void parser_stack_pop_uint8 (parser_context_t *context_p);
void parser_stack_push_uint16 (parser_context_t *context_p, uint16_t uint16_value);
uint16_t parser_stack_pop_uint16 (parser_context_t *context_p);
void parser_stack_push (parser_context_t *context_p, const void *data_p, uint32_t length);
void parser_stack_pop (parser_context_t *context_p, void *data_p, uint32_t length);
void parser_stack_iterator_skip (parser_stack_iterator_t *iterator, size_t length);
void parser_stack_iterator_read (parser_stack_iterator_t *iterator, void *data_p, size_t length);
void parser_stack_iterator_write (parser_stack_iterator_t *iterator, const void *data_p, size_t length);

/**
 * @}
 * @}
 *
 * \addtogroup parser Parser
 * @{
 *
 * \addtogroup jsparser JavaScript
 * @{
 *
 * \addtogroup jsparser_utils Utility
 * @{
 */

/* Compact byte code emitting functions. */

void parser_flush_cbc (parser_context_t *context_p);
void parser_emit_cbc (parser_context_t *context_p, uint16_t opcode);
void parser_emit_cbc_literal (parser_context_t *context_p, uint16_t opcode, uint16_t literal_index);
void parser_emit_cbc_literal_from_token (parser_context_t *context_p, uint16_t opcode);
void parser_emit_cbc_call (parser_context_t *context_p, uint16_t opcode, size_t call_arguments);
void parser_emit_cbc_push_number (parser_context_t *context_p, bool is_negative_number);
void parser_emit_cbc_forward_branch (parser_context_t *context_p, uint16_t opcode, parser_branch_t *branch_p);
parser_branch_node_t *parser_emit_cbc_forward_branch_item (parser_context_t *context_p, uint16_t opcode,
                                                           parser_branch_node_t *next_p);
void parser_emit_cbc_backward_branch (parser_context_t *context_p, uint16_t opcode, uint32_t offset);
void parser_set_branch_to_current_position (parser_context_t *context_p, parser_branch_t *branch_p);
void parser_set_breaks_to_current_position (parser_context_t *context_p, parser_branch_node_t *current_p);
void parser_set_continues_to_current_position (parser_context_t *context_p, parser_branch_node_t *current_p);

/* Convenience macros. */
#define parser_emit_cbc_ext(context_p, opcode) \
  parser_emit_cbc ((context_p), PARSER_TO_EXT_OPCODE (opcode))
#define parser_emit_cbc_ext_literal(context_p, opcode, literal_index) \
  parser_emit_cbc_literal ((context_p), PARSER_TO_EXT_OPCODE (opcode), (literal_index))
#define parser_emit_cbc_ext_call(context_p, opcode, call_arguments) \
  parser_emit_cbc_call ((context_p), PARSER_TO_EXT_OPCODE (opcode), (call_arguments))
#define parser_emit_cbc_ext_forward_branch(context_p, opcode, branch_p) \
  parser_emit_cbc_forward_branch ((context_p), PARSER_TO_EXT_OPCODE (opcode), (branch_p))
#define parser_emit_cbc_ext_backward_branch(context_p, opcode, offset) \
  parser_emit_cbc_backward_branch ((context_p), PARSER_TO_EXT_OPCODE (opcode), (offset))

/**
 * @}
 *
 * \addtogroup jsparser_lexer Lexer
 * @{
 */

/* Lexer functions */

void lexer_next_token (parser_context_t *context_p);
bool lexer_check_next_character (parser_context_t *context_p, lit_utf8_byte_t character);
#ifndef CONFIG_DISABLE_ES2015_CLASS
void lexer_skip_empty_statements (parser_context_t *context_p);
#endif /* !CONFIG_DISABLE_ES2015_CLASS */
#ifndef CONFIG_DISABLE_ES2015_ARROW_FUNCTION
lexer_token_type_t lexer_check_arrow (parser_context_t *context_p);
#endif /* !CONFIG_DISABLE_ES2015_ARROW_FUNCTION */
void lexer_parse_string (parser_context_t *context_p);
void lexer_expect_identifier (parser_context_t *context_p, uint8_t literal_type);
void lexer_scan_identifier (parser_context_t *context_p, bool property_name);
ecma_char_t lexer_hex_to_character (parser_context_t *context_p, const uint8_t *source_p, int length);
void lexer_expect_object_literal_id (parser_context_t *context_p, uint32_t ident_opts);
void lexer_construct_literal_object (parser_context_t *context_p, lexer_lit_location_t *literal_p,
                                     uint8_t literal_type);
bool lexer_construct_number_object (parser_context_t *context_p, bool is_expr, bool is_negative_number);
void lexer_convert_push_number_to_push_literal (parser_context_t *context_p);
uint16_t lexer_construct_function_object (parser_context_t *context_p, uint32_t extra_status_flags);
void lexer_construct_regexp_object (parser_context_t *context_p, bool parse_only);
bool lexer_compare_identifier_to_current (parser_context_t *context_p, const lexer_lit_location_t *right_ident_p);
bool lexer_compare_raw_identifier_to_current (parser_context_t *context_p, const char *right_ident_p,
                                              size_t right_ident_length);
uint8_t lexer_convert_binary_lvalue_token_to_binary (uint8_t token);

/**
 * @}
 *
 * \addtogroup jsparser_expr Expression parser
 * @{
 */

/* Parser functions. */

void parser_parse_expression (parser_context_t *context_p, int options);
#ifndef CONFIG_DISABLE_ES2015_CLASS
void parser_parse_class (parser_context_t *context_p, bool is_statement);
void parser_parse_super_class_context_start (parser_context_t *context_p);
void parser_parse_super_class_context_end (parser_context_t *context_p, bool is_statement);
#endif /* !CONFIG_DISABLE_ES2015_CLASS */

/**
 * @}
 *
 * \addtogroup jsparser_scanner Scanner
 * @{
 */

void parser_scan_until (parser_context_t *context_p, lexer_range_t *range_p, lexer_token_type_t end_type);

/**
 * @}
 *
 * \addtogroup jsparser_stmt Statement parser
 * @{
 */

void parser_parse_statements (parser_context_t *context_p);
void parser_free_jumps (parser_stack_iterator_t iterator);

/**
 * @}
 *
 * \addtogroup jsparser_parser Parser
 * @{
 */

ecma_compiled_code_t *parser_parse_function (parser_context_t *context_p, uint32_t status_flags);
#ifndef CONFIG_DISABLE_ES2015_ARROW_FUNCTION
ecma_compiled_code_t *parser_parse_arrow_function (parser_context_t *context_p, uint32_t status_flags);
#endif /* !CONFIG_DISABLE_ES2015_ARROW_FUNCTION */

/* Error management. */

void parser_raise_error (parser_context_t *context_p, parser_error_t error);

/* Debug functions. */

#ifdef JERRY_DEBUGGER

void parser_append_breakpoint_info (parser_context_t *context_p, jerry_debugger_header_type_t type, uint32_t value);

#endif /* JERRY_DEBUGGER */

#ifdef JERRY_ENABLE_LINE_INFO

void parser_emit_line_info (parser_context_t *context_p, uint32_t line, bool flush_cbc);

#endif /* JERRY_ENABLE_LINE_INFO */

/**
 * @}
 * @}
 * @}
 */

#endif /* !JS_PARSER_INTERNAL_H */
