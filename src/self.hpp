#ifndef JPROTECTOR_SELF_HPP
#define JPROTECTOR_SELF_HPP

#include "string.hpp"
#include "error.hpp"

extern "C" {
#include "jerryscript.h"
};

namespace ext {
  class self {
  public:
    static int init();

  private:
    static JERRY_EXTERNAL_FUNC(self_getter);
  };
}


#endif //JPROTECTOR_SELF_HPP
