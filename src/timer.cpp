#include "timer.hpp"

uint32_t ext::timer::tid = 0;
map<int, ext::timer::timer_pair> ext::timer::async_call_map;

void ext::timer::async_call_handler(void *tid) {
  auto id = (uint32_t *) tid;
  int32_t index = async_call_map.find(*id);
  if (index > -1) {
    const timer_pair pair = *async_call_map.get(*id);
    JERRY_GET_PROPERTY(pair.func, is_repeat, boolean, bool);
    jerry_value_t retval = jerry_call_function(pair.func, jerry_create_undefined(), nullptr, 0);
    ext::error::log_runtime_error(retval);
    jerry_release_value(retval);

    if (!is_repeat_value) {
      jerry_release_value(pair.func);
      async_call_map.remove(*id);
      delete id;
      id = nullptr;
    } else {
      JERRY_GET_PROPERTY(pair.func, timeout, number, double);
#ifdef __EMSCRIPTEN__
      emscripten_async_call(async_call_handler, tid, int(timeout_value));
#endif
    }
  }
}

int ext::timer::init() {
  jerry_value_t global_object = jerry_get_global_object();
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(global_object, setTimeout, ext::timer::set_timeout);
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(global_object, setInterval, ext::timer::set_interval);
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(global_object, clearTimeout, ext::timer::clear_timer_async);
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(global_object, clearInterval, ext::timer::clear_timer_async);
  jerry_release_value(global_object);
  return 0;
}

JERRY_EXTERNAL_FUNC(ext::timer::set_timeout) {
  double timeout = 0;
  if (args_cnt == 0) {
    return jerry_create_number(tid++);
  }

  if (jerry_value_is_function(*args_p)) {
    jerry_value_t func = jerry_acquire_value(args_p[0]);
    jerry_value_t is_repeat = jerry_create_boolean(false);
    JERRY_SET_PROPERTY(func, is_repeat, is_repeat);
    jerry_release_value(is_repeat);

    if (args_cnt >= 2 && jerry_value_is_number(args_p[1])) {
      timeout = jerry_get_number_value(args_p[1]);
    }

#ifdef __EMSCRIPTEN__
    emscripten_async_call(async_call_handler, (void *)new uint32_t(tid), int(timeout));
#endif

    async_call_map.add(tid, timer_pair{func, tid});
  }

  return jerry_create_number(tid++);
}

JERRY_EXTERNAL_FUNC(ext::timer::set_interval) {
  double timeout = 0;
  if (args_cnt == 0) {
    return jerry_create_number(tid++);
  }

  if (jerry_value_is_function(*args_p)) {
    jerry_value_t func = jerry_acquire_value(args_p[0]);
    jerry_value_t is_repeat = jerry_create_boolean(true);
    JERRY_SET_PROPERTY(func, is_repeat, is_repeat);
    jerry_release_value(is_repeat);

    if (args_cnt >= 2 && jerry_value_is_number(args_p[1])) {
      timeout = jerry_get_number_value(args_p[1]);
    }

    jerry_value_t timeout_prop = jerry_create_number(timeout);
    JERRY_SET_PROPERTY(func, timeout, timeout_prop);
    jerry_release_value(timeout_prop);

#ifdef __EMSCRIPTEN__
    emscripten_async_call(async_call_handler, (void *)new uint32_t(tid), int(timeout));
#endif

    async_call_map.add(tid, timer_pair{func, tid});
  }

  return jerry_create_number(tid++);
}

JERRY_EXTERNAL_FUNC(ext::timer::clear_timer_async) {
  if (args_cnt == 0) {
    return JERRY_UNDEFINED;
  }

  if (jerry_value_is_number(*args_p)) {
    auto id = (uint32_t) jerry_get_number_value(*args_p);
    int32_t index = async_call_map.find(id);
    if (index == -1) {
      return JERRY_UNDEFINED;
    }

    const timer_pair pair = *async_call_map.get(id);
    jerry_release_value(pair.func);
    async_call_map.remove(id);
  }

  return JERRY_UNDEFINED;
}