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

#include "ecma-builtin-helpers-macro-defines.inc.h"

#ifndef CONFIG_DISABLE_ES2015_PROMISE_BUILTIN

/* Object properties:
 *  (property name, object pointer getter) */

OBJECT_VALUE (LIT_MAGIC_STRING_CONSTRUCTOR,
              ECMA_BUILTIN_ID_PROMISE,
              ECMA_PROPERTY_CONFIGURABLE_WRITABLE)

NUMBER_VALUE (LIT_MAGIC_STRING_LENGTH,
              1,
              ECMA_PROPERTY_FLAG_WRITABLE)

#ifndef CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN
/* ECMA-262 v6, 25.4.5.4 */
STRING_VALUE (LIT_GLOBAL_SYMBOL_TO_STRING_TAG,
              LIT_MAGIC_STRING_PROMISE_UL,
              ECMA_PROPERTY_FLAG_CONFIGURABLE)
#endif /* !CONFIG_DISABLE_ES2015_SYMBOL_BUILTIN */

ROUTINE (LIT_MAGIC_STRING_THEN, ecma_builtin_promise_prototype_then, 2, 2)
ROUTINE (LIT_MAGIC_STRING_CATCH, ecma_builtin_promise_prototype_catch, 1, 1)

#endif /* !CONFIG_DISABLE_ES2015_PROMISE_BUILTIN */

#include "ecma-builtin-helpers-macro-undefs.inc.h"
