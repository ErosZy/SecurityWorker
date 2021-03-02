#ifndef JPROTECTOR_WEBSOCKET_HPP
#define JPROTECTOR_WEBSOCKET_HPP

#include <cstring>
#include "error.hpp"
#include "marco.hpp"
#include "string.hpp"
#include "map.hpp"

extern "C" {
#include "jerryscript.h"

#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#include "emscripten/websocket.h"
#endif
};

namespace ext {
  typedef enum {
    WEBSOCKET_INIT_STATUS = 0,
    WEBSOCKET_OPEN_STATUS,
    WEBSOCKET_CLOSE_STATUS,
  } websocket_status_t;

  struct websocket_item {
    websocket_item() : id(0),
                       url(string("")),
                       protocol(string("")),
                       status(WEBSOCKET_INIT_STATUS),
                       this_val(0),
                       events(map<string, map<jerry_value_t, uint32_t>>()),
                       userData(nullptr) {
    };

    websocket_item(const websocket_item &item) : id(item.id),
                                                 url(item.url),
                                                 protocol(item.protocol),
                                                 status(item.status),
                                                 this_val(item.this_val),
                                                 events(item.events),
                                                 userData(item.userData) {};

    websocket_item &operator=(const websocket_item &item) {
      id = item.id;
      url = item.url;
      protocol = item.protocol;
      status = item.status;
      this_val = item.this_val;
      events = item.events;
      userData = item.userData;
      return *this;
    }

    uint32_t id;
    string url;
    string protocol;
    uint32_t status;
    void *userData;
    jerry_value_t this_val;
    map<string, map<jerry_value_t, uint32_t>> events;
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_WEBSOCKET_T socket;
#endif
  };

  class websocket {
  public:
    static int init();

  private:
    static JERRY_EXTERNAL_FUNC(constructor);

    static JERRY_EXTERNAL_FUNC(add_event_listener);

    static JERRY_EXTERNAL_FUNC(remove_event_listener);

    static JERRY_EXTERNAL_FUNC(close);

    static JERRY_EXTERNAL_FUNC(send);

    static void define_property_get_set_func(jerry_value_t obj,
                                             const char *,
                                             jerry_external_handler_t,
                                             jerry_external_handler_t);

#ifdef __EMSCRIPTEN__
    static EM_BOOL open_callback(int eventType, const EmscriptenWebSocketOpenEvent *e, void *userData);
    static EM_BOOL close_callback(int eventType, const EmscriptenWebSocketCloseEvent *e, void *userData);
    static EM_BOOL error_callback(int eventType, const EmscriptenWebSocketErrorEvent *e, void *userData);
    static EM_BOOL message_callback(int eventType, const EmscriptenWebSocketMessageEvent *e, void *userData);
#endif
    static uint32_t id;
    static map<uint32_t, ext::websocket_item> websocket_item_map;
  };

}


#endif //JPROTECTOR_WEBSOCKET_HPP
