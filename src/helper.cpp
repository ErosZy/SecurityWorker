#include "helper.hpp"

int ext::helper::init() {
  jerry_value_t global_object = jerry_get_global_object();
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(global_object, btoa, btoa);
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(global_object, atob, atob);
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(global_object, __post_message_bridge__, post_message_bridge);
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(global_object, $$, $$);
  jerry_release_value(global_object);

  string post_message_code;
  post_message_code << "function postMessage(o) {"
                    << "  o = {message: o};"
                    << "  __post_message_bridge__(JSON.stringify(o));"
                    << "};"
                    << "function __onmessage__(o) {"
                    << "  if(typeof onmessage == 'function'){"
                    << "    try{"
                    << "      o = JSON.parse(o);"
                    << "      onmessage(o.message);"
                    << "    }catch(e){"
                    << "      console.error(e.toString());"
                    << "    }"
                    << "  }"
                    << "};";

  auto code = (jerry_char_t *) post_message_code.c_str();
  jerry_value_t retval = jerry_eval(code, post_message_code.size(), JERRY_PARSE_NO_OPTS);
  ext::error::log_compile_error(retval);
  jerry_release_value(retval);

  return 0;
}

JERRY_EXTERNAL_FUNC(ext::helper::btoa) {
  if (args_cnt == 0 || !jerry_value_is_string(*args_p)) {
    return JERRY_STRING("");
  }

  JERRY_CONV_STR_TO_CHAR_BUFFER(str, str_len, char_buffer, args_p);
  if (str_len == 0) {
    return JERRY_STRING("");
  }

  char *encode_str = b64_encode((const unsigned char *) char_buffer, str_len);
  jerry_value_t retval = JERRY_STRING(encode_str);
  delete encode_str;
  encode_str = nullptr;

  return retval;
}

JERRY_EXTERNAL_FUNC(ext::helper::atob) {
  if (args_cnt == 0) {
    return JERRY_STRING("");
  }

  JERRY_CONV_STR_TO_CHAR_BUFFER(str, str_len, char_buffer, args_p);
  if (str_len == 0) {
    return JERRY_STRING("");
  }

  unsigned char *decode_str = b64_decode((const char *) char_buffer, str_len);
  jerry_value_t retval = JERRY_STRING(decode_str);
  delete decode_str;
  decode_str = nullptr;

  return retval;
}

JERRY_EXTERNAL_FUNC(ext::helper::post_message_bridge) {
  if (args_cnt == 0 || !jerry_value_is_string(*args_p)) {
    return JERRY_UNDEFINED;
  }

  JERRY_CONV_STR_TO_CHAR_BUFFER(arg, arg_len, arg_buffer, args_p);
  string code;
  code << "if(typeof __post_message_bridge__ == 'function' ) {"
       << "__post_message_bridge__("
       << (char *) arg_buffer
       << ");};";

#ifdef __EMSCRIPTEN__
  emscripten_run_script(code.c_str());
#endif

  return JERRY_UNDEFINED;
}

JERRY_EXTERNAL_FUNC(ext::helper::$$) {
  if (args_cnt == 0 || !jerry_value_is_string(*args_p)) {
    return JERRY_UNDEFINED;
  }

  JERRY_CONV_STR_TO_CHAR_BUFFER(arg, arg_len, arg_buffer, args_p);
#ifdef __EMSCRIPTEN__
  char *result = emscripten_run_script_string((char *)arg_buffer);
  return JERRY_STRING(result);
#endif
}