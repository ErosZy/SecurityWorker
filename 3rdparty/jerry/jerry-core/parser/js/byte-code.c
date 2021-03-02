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

#include "js-parser-internal.h"

JERRY_STATIC_ASSERT ((sizeof (cbc_uint8_arguments_t) % sizeof (jmem_cpointer_t)) == 0,
                     sizeof_cbc_uint8_arguments_t_must_be_divisible_by_sizeof_jmem_cpointer_t);

JERRY_STATIC_ASSERT ((sizeof (cbc_uint16_arguments_t) % sizeof (jmem_cpointer_t)) == 0,
                     sizeof_cbc_uint16_arguments_t_must_be_divisible_by_sizeof_jmem_cpointer_t);

#ifndef JERRY_DISABLE_JS_PARSER

/** \addtogroup parser Parser
 * @{
 *
 * \addtogroup jsparser JavaScript
 * @{
 *
 * \addtogroup jsparser_bytecode Bytecode
 * @{
 */

/**
 * Compact bytecode definition
 */
#define CBC_OPCODE(arg1, arg2, arg3, arg4) \
  ((arg2) | (((arg3) + CBC_STACK_ADJUST_BASE) << CBC_STACK_ADJUST_SHIFT)),

/**
 * Flags of the opcodes.
 */
const uint8_t cbc_flags[] JERRY_CONST_DATA =
{
  CBC_OPCODE_LIST
};

/**
 * Flags of the extended opcodes.
 */
const uint8_t cbc_ext_flags[] =
{
  CBC_EXT_OPCODE_LIST
};

#undef CBC_OPCODE

#ifdef PARSER_DUMP_BYTE_CODE

#define CBC_OPCODE(arg1, arg2, arg3, arg4) #arg1,

/**
 * Names of the opcodes.
 */
const char * const cbc_names[] =
{
  CBC_OPCODE_LIST
};

/**
 * Names of the extended opcodes.
 */
const char * const cbc_ext_names[] =
{
  CBC_EXT_OPCODE_LIST
};

#undef CBC_OPCODE

#endif /* PARSER_DUMP_BYTE_CODE */

/**
 * @}
 * @}
 * @}
 */

#endif /* !JERRY_DISABLE_JS_PARSER */
