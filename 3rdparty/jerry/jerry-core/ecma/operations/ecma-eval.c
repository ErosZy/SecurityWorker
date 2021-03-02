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

#include "ecma-builtins.h"
#include "ecma-exceptions.h"
#include "ecma-eval.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-lex-env.h"
#include "js-parser.h"
#include "vm.h"
#include "jcontext.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup eval eval
 * @{
 */

/**
 * Perform 'eval' with code stored in ecma-string
 *
 * See also:
 *          ecma_op_eval_chars_buffer
 *          ECMA-262 v5, 15.1.2.1 (steps 2 to 8)
 *
 * @return ecma value
 */
ecma_value_t
ecma_op_eval (ecma_string_t *code_p, /**< code string */
              uint32_t parse_opts) /**< ecma_parse_opts_t option bits */
{
  ecma_value_t ret_value;

  lit_utf8_size_t chars_num = ecma_string_get_size (code_p);
  if (chars_num == 0)
  {
    ret_value = ECMA_VALUE_UNDEFINED;
  }
  else
  {
    ECMA_STRING_TO_UTF8_STRING (code_p, code_utf8_buffer_p, code_utf8_buffer_size);

    ret_value = ecma_op_eval_chars_buffer (code_utf8_buffer_p,
                                           chars_num,
                                           parse_opts);

    ECMA_FINALIZE_UTF8_STRING (code_utf8_buffer_p, code_utf8_buffer_size);
  }

  return ret_value;
} /* ecma_op_eval */

/**
 * Perform 'eval' with code stored in continuous character buffer
 *
 * See also:
 *          ecma_op_eval
 *          ECMA-262 v5, 15.1.2.1 (steps 2 to 8)
 *
 * @return ecma value
 */
ecma_value_t
ecma_op_eval_chars_buffer (const lit_utf8_byte_t *code_p, /**< code characters buffer */
                           size_t code_buffer_size, /**< size of the buffer */
                           uint32_t parse_opts) /**< ecma_parse_opts_t option bits */
{
#ifndef JERRY_DISABLE_JS_PARSER
  JERRY_ASSERT (code_p != NULL);

  ecma_compiled_code_t *bytecode_data_p;

  uint32_t is_strict_call = ECMA_PARSE_STRICT_MODE | ECMA_PARSE_DIRECT_EVAL;

  if ((parse_opts & is_strict_call) != is_strict_call)
  {
    parse_opts &= (uint32_t) ~ECMA_PARSE_STRICT_MODE;
  }

#ifdef JERRY_ENABLE_LINE_INFO
  JERRY_CONTEXT (resource_name) = ecma_make_magic_string_value (LIT_MAGIC_STRING__EMPTY);
#endif /* JERRY_ENABLE_LINE_INFO */

#ifndef CONFIG_DISABLE_ES2015_CLASS
  parse_opts |= ECMA_GET_SUPER_EVAL_PARSER_OPTS ();

  ECMA_CLEAR_SUPER_EVAL_PARSER_OPTS ();
#endif /* !CONFIG_DISABLE_ES2015_CLASS */

  ecma_value_t parse_status = parser_parse_script (NULL,
                                                   0,
                                                   code_p,
                                                   code_buffer_size,
                                                   parse_opts,
                                                   &bytecode_data_p);

  if (ECMA_IS_VALUE_ERROR (parse_status))
  {
    return parse_status;
  }

  return vm_run_eval (bytecode_data_p, parse_opts);
#else /* JERRY_DISABLE_JS_PARSER */
  JERRY_UNUSED (code_p);
  JERRY_UNUSED (code_buffer_size);
  JERRY_UNUSED (parse_opts);

  return ecma_raise_syntax_error (ECMA_ERR_MSG ("The parser has been disabled."));
#endif /* !JERRY_DISABLE_JS_PARSER */
} /* ecma_op_eval_chars_buffer */

/**
 * @}
 * @}
 */
