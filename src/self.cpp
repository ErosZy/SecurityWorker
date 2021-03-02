#include <jerryscript-core.h>
#include "self.hpp"

int ext::self::init() {
  jerry_value_t global_object = jerry_get_global_object();
  jerry_property_descriptor_t desc;
  jerry_init_property_descriptor_fields(&desc);
  desc.is_enumerable_defined = true;
  desc.is_enumerable = true;
  desc.is_configurable_defined = true;
  desc.is_configurable = true;
  desc.is_value_defined = false;
  desc.is_get_defined = true;
  desc.getter = jerry_create_external_function(ext::self::self_getter);

  jerry_value_t self_str = JERRY_STRING("self");
  jerry_define_own_property(global_object, self_str, &desc);
  
  jerry_release_value(self_str);
  jerry_free_property_descriptor_fields(&desc);
  jerry_release_value(global_object);
  return 0;
}

JERRY_EXTERNAL_FUNC(ext::self::self_getter) {
  return jerry_get_global_object();
}