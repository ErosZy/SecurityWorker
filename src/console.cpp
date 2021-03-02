#include "console.hpp"

map<string, double> ext::console::time_label_map;

int ext::console::init() {
  jerry_value_t global_object = jerry_get_global_object();
  jerry_value_t console_object = jerry_create_object();

  JERRY_SET_EXTERNAL_FUNC_PROPERTY(console_object, debug, debug);
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(console_object, warn, warn);
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(console_object, info, info);
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(console_object, log, log);
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(console_object, error, error);
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(console_object, time, time);
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(console_object, timeEnd, time_end);

  JERRY_SET_PROPERTY(global_object, console, console_object);

  jerry_release_value(console_object);
  jerry_release_value(global_object);

  return 0;
}

string ext::console::concat_args_to_str(const char *level, const jerry_value_t *args_p, jerry_length_t args_cnt) {
  string constr(level);
  for (uint32_t i = 0; i < args_cnt; i++) {
    JERRY_CONV_STR_TO_CHAR_BUFFER(msg, msg_len, char_buffer, args_p + i);
    constr << (char *) char_buffer << " ";
  }
  return constr;
}

JERRY_EXTERNAL_FUNC(ext::console::debug) {
  if (args_cnt != 0) {
    string constr = concat_args_to_str("[DEBUG] ", args_p, args_cnt);
#ifdef __EMSCRIPTEN__
    emscripten_log(EM_LOG_CONSOLE, constr.c_str());
#endif
  }

  return JERRY_UNDEFINED;
}

JERRY_EXTERNAL_FUNC(ext::console::warn) {
  if (args_cnt != 0) {
    string constr = concat_args_to_str("[WARN] ", args_p, args_cnt);
#ifdef __EMSCRIPTEN__
    emscripten_log(EM_LOG_WARN, constr.c_str());
#endif
  }
  return JERRY_UNDEFINED;
}

JERRY_EXTERNAL_FUNC(ext::console::error) {
  if (args_cnt != 0) {
    string constr = concat_args_to_str("[ERROR] ", args_p, args_cnt);
#ifdef __EMSCRIPTEN__
    emscripten_log(EM_LOG_ERROR, constr.c_str());
#endif
  }

  return JERRY_UNDEFINED;
}

JERRY_EXTERNAL_FUNC(ext::console::info) {
  if (args_cnt != 0) {
    string constr = concat_args_to_str("[INFO] ", args_p, args_cnt);
#ifdef __EMSCRIPTEN__
    emscripten_log(EM_LOG_CONSOLE, constr.c_str());
#endif
  }
  return JERRY_UNDEFINED;
}

JERRY_EXTERNAL_FUNC(ext::console::log) {
  if (args_cnt != 0) {
    string constr = concat_args_to_str("[LOG] ", args_p, args_cnt);
#ifdef __EMSCRIPTEN__
    emscripten_log(EM_LOG_CONSOLE, constr.c_str());
#endif
  }
  return JERRY_UNDEFINED;
}

JERRY_EXTERNAL_FUNC(ext::console::time) {
  JERRY_CONV_STR_TO_CHAR_BUFFER(arg, arg_len, char_buffer, args_p);
  string key((char *) char_buffer);
  double now = jerry_port_get_current_time();
  time_label_map.add(key, now);
  return JERRY_UNDEFINED;
}

JERRY_EXTERNAL_FUNC(ext::console::time_end) {
  JERRY_CONV_STR_TO_CHAR_BUFFER(arg, arg_len, char_buffer, args_p);
  string key((char *) char_buffer);
  int32_t index = time_label_map.find(key);

  string constr;
  if (index == -1) {
    constr << "Timer '" << key << "' does not exist";
#ifdef __EMSCRIPTEN__
    emscripten_log(EM_LOG_WARN, constr.c_str());
#endif
  } else {
    double now = jerry_port_get_current_time();
    jerry_value_t speed_time = jerry_create_number(now - *time_label_map.get(key));
    JERRY_CONV_STR_TO_CHAR_BUFFER(msg, msg_len, time_char_buffer, &speed_time);
    jerry_release_value(speed_time);
    constr << "[TIMER] " << key << ": " << (char *) time_char_buffer << "ms";
#ifdef __EMSCRIPTEN__
    emscripten_log(EM_LOG_CONSOLE, constr.c_str());
#endif
  }

  return JERRY_UNDEFINED;
}
