#ifndef JPROTECTOR_ERROR_HPP
#define JPROTECTOR_ERROR_HPP

#include "marco.hpp"
#include "string.hpp"

extern "C" {
#include "jerryscript.h"

#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#endif
};

namespace ext {
  class error {
  public:
    static int init();

    static void log_runtime_error(jerry_value_t &retval);

    static void log_compile_error(jerry_value_t &retval);

  private:
    static JERRY_EXTERNAL_FUNC(printStack);
  };
}


#endif //JPROTECTOR_ERROR_HPP
