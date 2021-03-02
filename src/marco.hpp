#ifndef JPROTECTOR_MARCO_HPP
#define JPROTECTOR_MARCO_HPP

#include "memory.h"

extern "C" {
#include "jerryscript.h"
}

#define JERRY_EXTERNAL_FUNC(FUNC_NAME) jerry_value_t FUNC_NAME ( \
  const jerry_value_t func_value, \
  const jerry_value_t this_value, \
  const jerry_value_t *args_p, \
  const jerry_length_t args_cnt )

#define JERRY_UNDEFINED jerry_create_undefined()
#define JERRY_STRING(STR) jerry_create_string((jerry_char_t *) STR)

#define JERRY_SET_EXTERNAL_FUNC_PROPERTY(OBJ, FUNC_NAME, FUNC_HANDLER) \
jerry_value_t FUNC_NAME##_func = jerry_create_external_function(FUNC_HANDLER); \
jerry_value_t FUNC_NAME##_func_name = jerry_create_string((jerry_char_t *) #FUNC_NAME ); \
jerry_value_t FUNC_NAME##_retval = jerry_set_property(OBJ, FUNC_NAME##_func_name, FUNC_NAME##_func); \
jerry_release_value(FUNC_NAME##_retval); \
jerry_release_value(FUNC_NAME##_func_name); \
jerry_release_value(FUNC_NAME##_func);

#define JERRY_SET_PROPERTY(OBJ, PROP_NAME, PROP_VAL) \
jerry_value_t PROP_NAME##prop_name = jerry_create_string((jerry_char_t *) #PROP_NAME); \
jerry_value_t PROP_NAME##_retval = jerry_set_property(OBJ, PROP_NAME##prop_name, PROP_VAL); \
jerry_release_value(PROP_NAME##_retval); \
jerry_release_value(PROP_NAME##prop_name);

#define JERRY_GET_PROPERTY(OBJ, PROP_NAME, PROP_TYPE, VALUE_TYPE) \
jerry_value_t PROP_NAME##_prop_name = jerry_create_string((jerry_char_t *) #PROP_NAME); \
jerry_value_t PROP_NAME##_prop_value = jerry_get_property(OBJ, PROP_NAME##_prop_name); \
VALUE_TYPE PROP_NAME##_value = jerry_get_##PROP_TYPE##_value(PROP_NAME##_prop_value); \
jerry_release_value(PROP_NAME##_prop_value); \
jerry_release_value(PROP_NAME##_prop_name);

#define JERRY_CONV_STR_TO_CHAR_BUFFER(STR_PROP_NAME, LEN_PROP_NAME, CHARS, ARGS_PTR) \
jerry_value_t STR_PROP_NAME = jerry_value_to_string(*(ARGS_PTR)); \
jerry_length_t LEN_PROP_NAME = jerry_get_string_size(STR_PROP_NAME); \
jerry_char_t CHARS[LEN_PROP_NAME + 1]; \
memset(CHARS, 0, LEN_PROP_NAME + 1); \
jerry_string_to_char_buffer(STR_PROP_NAME, CHARS, LEN_PROP_NAME); \
jerry_release_value(STR_PROP_NAME);

#define JERRY_SAFE_GET_OBJ_KEY_BLOCK(VALUE, PROP_NAME) \
jerry_value_t PROP_NAME##_key = jerry_create_string((const jerry_char_t *) #PROP_NAME); \
jerry_value_t PROP_NAME##_prop = jerry_get_property(VALUE, PROP_NAME##_key);

#define JERRY_SAFE_GET_OBJ_KEY_BLOCK_END(PROP_NAME) \
jerry_release_value(PROP_NAME##_prop); \
jerry_release_value(PROP_NAME##_key);

#define JERRY_PROPERTY_BLOCK(OBJ, PROP_NAME) \
jerry_value_t PROP_NAME = JERRY_STRING(#PROP_NAME); \
jerry_value_t PROP_NAME##_prop = jerry_get_property(OBJ, PROP_NAME); \

#define JERRY_PROPERTY_BLOCK_END(PROP_NAME) \
jerry_release_value(PROP_NAME##_prop); \
jerry_release_value(PROP_NAME);

#endif //JPROTECTOR_MARCO_HPP
