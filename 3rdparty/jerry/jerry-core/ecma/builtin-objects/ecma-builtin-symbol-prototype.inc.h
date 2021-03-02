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

/*
 * Symbol prototype built-in description
 */

#include "ecma-builtin-helpers-macro-defines.inc.h"

#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN

/* Object properties:
 *  (property name, object pointer getter) */

OBJECT_VALUE (LIT_MAGIC_STRING_CONSTRUCTOR,
              ECMA_BUILTIN_ID_SYMBOL,
              ECMA_PROPERTY_CONFIGURABLE_WRITABLE)

NUMBER_VALUE (LIT_MAGIC_STRING_LENGTH,
              0,
              ECMA_PROPERTY_FLAG_WRITABLE)

ROUTINE (LIT_MAGIC_STRING_TO_STRING_UL, ecma_builtin_symbol_prototype_object_to_string, 0, 0)
ROUTINE (LIT_MAGIC_STRING_VALUE_OF_UL, ecma_builtin_symbol_prototype_object_value_of, 0, 0)
ROUTINE_CONFIGURABLE_ONLY (LIT_GLOBAL_SYMBOL_TO_PRIMITIVE,
                           ecma_builtin_symbol_prototype_object_to_primitive,
                           0,
                           0)

/* ECMA-262 v6, 19.4.3.4 */
STRING_VALUE (LIT_GLOBAL_SYMBOL_TO_STRING_TAG,
              LIT_MAGIC_STRING_SYMBOL_UL,
              ECMA_PROPERTY_FLAG_CONFIGURABLE)

#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */

#include "ecma-builtin-helpers-macro-undefs.inc.h"
