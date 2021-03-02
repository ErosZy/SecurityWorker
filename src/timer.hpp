#ifndef JPROTECTOR_TIMER_HPP
#define JPROTECTOR_TIMER_HPP

#include <cstdint>
#include "marco.hpp"
#include "map.hpp"
#include "error.hpp"

extern "C" {
#include "jerryscript.h"

#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#endif
};

namespace ext {
  class timer {
    struct timer_pair {
      jerry_value_t func;
      uint32_t id;
    };

  public:
    static int init();

  private:
    static JERRY_EXTERNAL_FUNC(set_timeout);

    static JERRY_EXTERNAL_FUNC(set_interval);

    static JERRY_EXTERNAL_FUNC(clear_timer_async);

    static void async_call_handler(void *);

    static uint32_t tid;
    static map<int, ext::timer::timer_pair> async_call_map;
  };
}


#endif //JPROTECTOR_TIMER_HPP
