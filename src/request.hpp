#ifndef JPROTECTOR_REQUEST_HPP
#define JPROTECTOR_REQUEST_HPP

#include <cstdint>
#include "marco.hpp"
#include "string.hpp"
#include "map.hpp"
#include "error.hpp"

extern "C" {
#include "jerryscript.h"

#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#include "emscripten/fetch.h"
#endif
};

#define MAX_HEADERS_LEN 128

namespace ext {
  class request {
    struct request_item {
      jerry_value_t this_val;
      jerry_value_t onsuccess;
      jerry_value_t onerror;
    };

  public:
    static int init();

  private:
    static JERRY_EXTERNAL_FUNC(request_wrap);

#ifdef __EMSCRIPTEN__
    static jerry_value_t conv_response_data(emscripten_fetch_t *fetch);
    static void onsuccess(emscripten_fetch_t *fetch);
    static void onerror(emscripten_fetch_t *fetch);
#endif

    static map<unsigned int, request_item> request_map;
  };
}

#endif //JPROTECTOR_REQUEST_HPP
