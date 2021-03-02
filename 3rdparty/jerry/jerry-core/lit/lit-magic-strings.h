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

#ifndef LIT_MAGIC_STRINGS_H
#define LIT_MAGIC_STRINGS_H

#include "lit-globals.h"

/**
 * Identifiers of ECMA and implementation-defined magic string constants
 */
typedef enum
{
/** @cond doxygen_suppress */
#define LIT_MAGIC_STRING_FIRST_STRING_WITH_SIZE(size, id)
#define LIT_MAGIC_STRING_DEF(id, ascii_zt_string) \
     id,
#include "lit-magic-strings.inc.h"
#undef LIT_MAGIC_STRING_DEF
#undef LIT_MAGIC_STRING_FIRST_STRING_WITH_SIZE
/** @endcond */
  LIT_NON_INTERNAL_MAGIC_STRING__COUNT, /**< number of non-internal magic strings */
  LIT_INTERNAL_MAGIC_STRING_PROMISE = LIT_NON_INTERNAL_MAGIC_STRING__COUNT, /**<  [[Promise]] of promise
                                                                             *    reject or resolve functions */
  LIT_INTERNAL_MAGIC_STRING_ALREADY_RESOLVED, /**< [[AlreadyResolved]] of promise reject or resolve functions */
  LIT_INTERNAL_MAGIC_STRING_RESOLVE_FUNCTION, /**< the resolve funtion of the promise object */
  LIT_INTERNAL_MAGIC_STRING_REJECT_FUNCTION, /**< the reject function of the promise object */
  LIT_INTERNAL_MAGIC_STRING_PROMISE_PROPERTY_PROMISE, /**< [[Promise]] property */
  LIT_INTERNAL_MAGIC_STRING_PROMISE_PROPERTY_RESOLVE, /**< [[Resolve]] property */
  LIT_INTERNAL_MAGIC_STRING_PROMISE_PROPERTY_REJECT, /**< [[Reject]] property */
  LIT_INTERNAL_MAGIC_STRING_PROMISE_PROPERTY_CAPABILITY, /**< [[Capability]] property */
  LIT_INTERNAL_MAGIC_STRING_PROMISE_PROPERTY_HANDLER, /**< [[Handler]] property */
  LIT_INTERNAL_MAGIC_STRING_PROMISE_PROPERTY_ALREADY_CALLED, /**< [[AlreadyCalled]] property */
  LIT_INTERNAL_MAGIC_STRING_PROMISE_PROPERTY_INDEX, /**< [[Index]] property */
  LIT_INTERNAL_MAGIC_STRING_PROMISE_PROPERTY_VALUE, /**< [[Values]] property */
  LIT_INTERNAL_MAGIC_STRING_PROMISE_PROPERTY_REMAINING_ELEMENT, /**< [[RemainingElement]] property */
  /* List of well known symbols */
  LIT_GLOBAL_SYMBOL_HAS_INSTANCE, /**< @@hasInstance well known symbol */
  LIT_GLOBAL_SYMBOL_IS_CONCAT_SPREADABLE, /**< @@isConcatSpreadable well known symbol */
  LIT_GLOBAL_SYMBOL_ITERATOR, /**< @@iterator well known symbol */
  LIT_GLOBAL_SYMBOL_MATCH, /**< @@match well known symbol */
  LIT_GLOBAL_SYMBOL_REPLACE, /**< @@replace well known symbol */
  LIT_GLOBAL_SYMBOL_SEARCH, /**< @@search well known symbol */
  LIT_GLOBAL_SYMBOL_SPECIES, /**< @@species well known symbol */
  LIT_GLOBAL_SYMBOL_SPLIT, /**< @@split well known symbol */
  LIT_GLOBAL_SYMBOL_TO_PRIMITIVE, /**< @@toPrimitive well known symbol */
  LIT_GLOBAL_SYMBOL_TO_STRING_TAG, /**< @@toStringTag well known symbol */
  LIT_GLOBAL_SYMBOL_UNSCOPABLES, /**< @@unscopables well known symbol */
  LIT_GC_MARK_REQUIRED_MAGIC_STRING__COUNT,  /**< number of internal magic strings which will be used as
                                              *   property names, and their values need to be marked during gc. */
  LIT_INTERNAL_MAGIC_STRING_DELETED = LIT_GC_MARK_REQUIRED_MAGIC_STRING__COUNT, /**< special value for
                                                                                 *   deleted properties */

  LIT_INTERNAL_MAGIC_STRING_NATIVE_POINTER, /**< native pointer info associated with an object */
  LIT_FIRST_INTERNAL_MAGIC_STRING = LIT_INTERNAL_MAGIC_STRING_NATIVE_POINTER,  /**< first index of internal
                                                                                *   magic strings */
  LIT_INTERNAL_MAGIC_STRING_CLASS_THIS_BINDING, /**< the this binding of the class constructor */
  LIT_MAGIC_STRING__COUNT /**< number of magic strings */
} lit_magic_string_id_t;

/**
 * Identifiers of implementation-defined external magic string constants
 */
typedef uint32_t lit_magic_string_ex_id_t;

uint32_t lit_get_magic_string_ex_count (void);

const lit_utf8_byte_t *lit_get_magic_string_utf8 (lit_magic_string_id_t id);
lit_utf8_size_t lit_get_magic_string_size (lit_magic_string_id_t id);

const lit_utf8_byte_t *lit_get_magic_string_ex_utf8 (lit_magic_string_ex_id_t id);
lit_utf8_size_t lit_get_magic_string_ex_size (lit_magic_string_ex_id_t id);

void lit_magic_strings_ex_set (const lit_utf8_byte_t * const *ex_str_items,
                               uint32_t count,
                               const lit_utf8_size_t *ex_str_sizes);

lit_magic_string_id_t lit_is_utf8_string_magic (const lit_utf8_byte_t *string_p, lit_utf8_size_t string_size);
lit_magic_string_id_t lit_is_utf8_string_pair_magic (const lit_utf8_byte_t *string1_p, lit_utf8_size_t string1_size,
                                                     const lit_utf8_byte_t *string2_p, lit_utf8_size_t string2_size);

lit_magic_string_ex_id_t lit_is_ex_utf8_string_magic (const lit_utf8_byte_t *string_p, lit_utf8_size_t string_size);
lit_magic_string_ex_id_t lit_is_ex_utf8_string_pair_magic (const lit_utf8_byte_t *string1_p,
                                                           lit_utf8_size_t string1_size,
                                                           const lit_utf8_byte_t *string2_p,
                                                           lit_utf8_size_t string2_size);

lit_utf8_byte_t *lit_copy_magic_string_to_buffer (lit_magic_string_id_t id, lit_utf8_byte_t *buffer_p,
                                                  lit_utf8_size_t buffer_size);

#endif /* !LIT_MAGIC_STRINGS_H */
