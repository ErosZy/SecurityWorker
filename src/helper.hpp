#ifndef JPROTECTOR_HELPER_HPP
#define JPROTECTOR_HELPER_HPP

#include "error.hpp"
#include "marco.hpp"

extern "C" {
#include "jerryscript.h"
#include "b64.h"

#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#endif
}

namespace ext {
  class helper {
  public:
    static int init();

  private:
    static JERRY_EXTERNAL_FUNC(atob);

    static JERRY_EXTERNAL_FUNC(btoa);

    static JERRY_EXTERNAL_FUNC(post_message_bridge);

    static JERRY_EXTERNAL_FUNC($$);
  };
}


#endif //JPROTECTOR_HELPER_HPP
