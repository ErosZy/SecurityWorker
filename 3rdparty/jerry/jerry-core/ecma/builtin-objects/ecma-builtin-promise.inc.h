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
 * Promose built-in description
 */

#include "ecma-builtin-helpers-macro-defines.inc.h"

#ifndef CONFIG_DISABLE_ES2015_PROMISE_BUILTIN

/* Number properties:
 *  (property name, number value, writable, enumerable, configurable) */

NUMBER_VALUE (LIT_MAGIC_STRING_LENGTH,
              1,
              ECMA_PROPERTY_FIXED)

/* Object properties:
 *  (property name, object pointer getter) */

OBJECT_VALUE (LIT_MAGIC_STRING_PROTOTYPE,
              ECMA_BUILTIN_ID_PROMISE_PROTOTYPE,
              ECMA_PROPERTY_FIXED)

/* Routine properties:
 *  (property name, C routine name, arguments number or NON_FIXED, value of the routine's length property) */
ROUTINE (LIT_MAGIC_STRING_REJECT, ecma_builtin_promise_reject, 1, 1)
ROUTINE (LIT_MAGIC_STRING_RESOLVE, ecma_builtin_promise_resolve, 1, 1)
ROUTINE (LIT_MAGIC_STRING_RACE, ecma_builtin_promise_race, 1, 1)
ROUTINE (LIT_MAGIC_STRING_ALL, ecma_builtin_promise_all, 1, 1)

#endif /* !CONFIG_DISABLE_ES2015_PROMISE_BUILTIN */

#include "ecma-builtin-helpers-macro-undefs.inc.h"
