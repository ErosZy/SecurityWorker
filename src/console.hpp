#ifndef JPROTECTOR_CONSOLE_HPP
#define JPROTECTOR_CONSOLE_HPP

#include <cstdint>
#include "string.hpp"
#include "map.hpp"
#include "marco.hpp"
#include "error.hpp"

extern "C" {
#include "jerryscript.h"

#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#endif
};

namespace ext {
  class console {
  public:
    static int init();

  private:
    static string concat_args_to_str(const char *level, const jerry_value_t *args_p, jerry_length_t args_cnt);

    static JERRY_EXTERNAL_FUNC(debug);

    static JERRY_EXTERNAL_FUNC(warn);

    static JERRY_EXTERNAL_FUNC(info);

    static JERRY_EXTERNAL_FUNC(log);

    static JERRY_EXTERNAL_FUNC(error);

    static JERRY_EXTERNAL_FUNC(time);

    static JERRY_EXTERNAL_FUNC(time_end);

    static map<string, double> time_label_map;
  };
}

#endif //JPROTECTOR_CONSOLE_HPP
