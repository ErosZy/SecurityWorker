#include "error.hpp"

int ext::error::init() {
  jerry_value_t global_object = jerry_get_global_object();
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(global_object, _print_error_stack_, ext::error::printStack);
  jerry_release_value(global_object);

  string format_error;
  format_error << "var E = Error; "
               << "Error = function() {   "
               << "   E.apply(this, arguments);"
               << "   this.message = arguments[0];"
               << "   this.stack = null; "
               << "   delete this.stack; "
               << "}; "
               << "Error.prototype = E.prototype; "
               << "Error.prototype.toString = function(){ "
               << "   var stack = _print_error_stack_();"
               << "   stack.shift(); "
               << "   stack = stack.map(function(v){ return '            at ' + v; }); "
               << "   return 'Error: ' + this.message + '\\n' + stack.join('\\n'); "
               << "}; "
               << "Object.defineProperties(Error.prototype, { "
               << "   stack: {"
               << "       configurable: true, "
               << "       enumerable: false, "
               << "       get: function() { "
               << "           var stack = _print_error_stack_();"
               << "           stack.shift(); "
               << "           stack = stack.map(function(v){ return '            at ' + v; }); "
               << "           return 'Error \\n' + stack.join('\\n'); "
               << "       } "
               << "   } "
               << "}); ";

  auto code = (jerry_char_t *) format_error.c_str();
  jerry_value_t retval = jerry_eval(code, format_error.size(), JERRY_PARSE_NO_OPTS);
  ext::error::log_compile_error(retval);
  jerry_release_value(retval);

  return 0;
}

JERRY_EXTERNAL_FUNC(ext::error::printStack) {
  return jerry_get_backtrace(11);
}

void ext::error::log_runtime_error(jerry_value_t &retval) {
  if (jerry_value_is_error(retval)) {
    string error_str("[ERROR] ");
    jerry_value_clear_error_flag(&retval);

    jerry_value_t message_name = JERRY_STRING("message");
    jerry_value_t message_prop = jerry_get_property(retval, message_name);
    jerry_value_t message_prop_error = jerry_get_value_from_error(message_prop, false);
    jerry_value_t message_prop_error_val = jerry_value_to_string(message_prop_error);
    JERRY_CONV_STR_TO_CHAR_BUFFER(msg_str, msg_str_len, msg_buffer, &message_prop_error_val);
    error_str << "Error: " << (char *) msg_buffer << "\n";

    jerry_release_value(message_prop_error_val);
    jerry_release_value(message_prop_error);
    jerry_release_value(message_prop);
    jerry_release_value(message_name);

    jerry_value_t stack_name = JERRY_STRING("stack");
    jerry_value_t stack_prop = jerry_get_property(retval, stack_name);
    jerry_value_t stack_prop_error = jerry_get_value_from_error(stack_prop, false);
    uint32_t stack_len = jerry_get_array_length(stack_prop_error);
    for (uint32_t i = 0; i < stack_len; i++) {
      jerry_value_t stack_prop_error_val = jerry_get_property_by_index(stack_prop_error, i);
      JERRY_CONV_STR_TO_CHAR_BUFFER(stack_str, stack_str_len, stack_buffer, &stack_prop_error_val);
      error_str << "            at " << (char *) stack_buffer;
      if (i < stack_len - 1) {
        error_str << "\n";
      }
      jerry_release_value(stack_prop_error_val);
    }

    jerry_release_value(stack_prop_error);
    jerry_release_value(stack_prop);
    jerry_release_value(stack_name);

#ifdef __EMSCRIPTEN__
    emscripten_log(EM_LOG_ERROR, error_str.c_str());
#endif
  }
}


void ext::error::log_compile_error(jerry_value_t &retval) {
  if (jerry_value_is_error(retval)) {
    jerry_value_t parsed_error = jerry_get_value_from_error(retval, false);
    jerry_value_t parsed_error_str = jerry_value_to_string(parsed_error);
    JERRY_CONV_STR_TO_CHAR_BUFFER(error, error_len, error_buffer, &parsed_error_str);
    jerry_release_value(parsed_error_str);
    jerry_release_value(parsed_error);
#ifdef __EMSCRIPTEN__
    string error_str("[ERROR] ");
    error_str << (char *)error_buffer;
    emscripten_log(EM_LOG_ERROR, error_str.c_str());
#endif
  }
}