#include "websocket.hpp"
#include <iostream>

uint32_t ext::websocket::id = 0;
map<uint32_t, ext::websocket_item> ext::websocket::websocket_item_map;

int ext::websocket::init() {
  jerry_value_t global_object = jerry_get_global_object();
#ifdef __EMSCRIPTEN__
  if(emscripten_websocket_is_supported()) {
#endif
  jerry_value_t websocket_constructor = jerry_create_external_function(ext::websocket::constructor);

  jerry_value_t websocket_proto = jerry_create_object();
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(websocket_proto, addEventListener, add_event_listener);
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(websocket_proto, removeEventListener, remove_event_listener);
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(websocket_proto, close, close);
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(websocket_proto, send, send);

  JERRY_SET_PROPERTY(websocket_constructor, prototype, websocket_proto);
  jerry_release_value(websocket_proto);

  JERRY_SET_PROPERTY(global_object, WebSocket, websocket_constructor);
  jerry_release_value(websocket_constructor);

#ifdef __EMSCRIPTEN__
  }
#endif
  jerry_release_value(global_object);
  return 0;
}

JERRY_EXTERNAL_FUNC(ext::websocket::constructor) {
  if (args_cnt == 0) {
    string constr("[ERROR] Failed to construct 'WebSocket': 1 argument required, but only 0 present.");
#ifdef __EMSCRIPTEN__
    emscripten_log(EM_LOG_ERROR, constr.c_str());
#endif
    return JERRY_UNDEFINED;
  }

  JERRY_CONV_STR_TO_CHAR_BUFFER(url, url_len, url_buffer, args_p);
  char *is_ws = strstr((char *) url_buffer, "ws://");
  char *is_wss = strstr((char *) url_buffer, "wss://");
  if (is_ws == nullptr && is_wss == nullptr) {
    string constr("Failed to construct 'WebSocket': The URL's scheme must be either 'ws' or 'wss'. ");
    constr << (char *) url_buffer << " is not allowed.";
#ifdef __EMSCRIPTEN__
    emscripten_log(EM_LOG_ERROR, constr.c_str());
#endif
    return JERRY_UNDEFINED;
  }

  JERRY_CONV_STR_TO_CHAR_BUFFER(protocol, protocol_len, protocol_buffer, (args_p + 1));

  JERRY_SET_PROPERTY(this_value, url, *args_p);
  JERRY_SET_PROPERTY(this_value, protocol, *(args_p + 1));

  jerry_value_t id = jerry_create_number(ext::websocket::id);
  JERRY_SET_PROPERTY(this_value, id, id);
  jerry_release_value(id);

  ext::websocket_item item;
  item.id = ext::websocket::id;
  item.url << (char *) url_buffer;
  item.protocol << (char *) protocol_buffer;
  item.this_val = this_value;
  jerry_acquire_value(this_value);

  ext::websocket::websocket_item_map.add(ext::websocket::id, item);
  auto item_ptr = ext::websocket::websocket_item_map.get(ext::websocket::id);

#ifdef __EMSCRIPTEN__
  EmscriptenWebSocketCreateAttributes attr;
  emscripten_websocket_init_create_attributes(&attr);
  attr.url = item_ptr->url.c_str();

  item_ptr->socket = emscripten_websocket_new(&attr);
  if (item_ptr->socket <= 0) {
    emscripten_log(EM_LOG_ERROR, "WebSocket creation failed");
    return JERRY_UNDEFINED;
  }

  emscripten_websocket_set_onopen_callback(item_ptr->socket, (void*)item_ptr, open_callback);
  emscripten_websocket_set_onclose_callback(item_ptr->socket, (void*)item_ptr, close_callback);
  emscripten_websocket_set_onerror_callback(item_ptr->socket, (void*)item_ptr, error_callback);
  emscripten_websocket_set_onmessage_callback(item_ptr->socket, (void*)item_ptr, message_callback);
#endif

  ext::websocket::id += 1;
  return JERRY_UNDEFINED;
}

JERRY_EXTERNAL_FUNC(ext::websocket::add_event_listener) {
  if (args_cnt < 2) {
    return JERRY_UNDEFINED;
  }

  if (!jerry_value_is_string(*args_p) || !jerry_value_is_function(*(args_p + 1))) {
    return JERRY_UNDEFINED;
  }

  JERRY_PROPERTY_BLOCK(this_value, id);
  auto _id = (uint32_t) jerry_get_number_value(id_prop);
  int32_t index = websocket_item_map.find(_id);
  if (index > -1) {
    auto item = websocket_item_map.get(_id);
    JERRY_CONV_STR_TO_CHAR_BUFFER(func_name, func_name_len, func_name_buffer, args_p);
    string func_name_str((char *) func_name_buffer);
    int32_t index = item->events.find(func_name_str);
    if (index == -1) {
      item->events.add(func_name_str, map<jerry_value_t, uint32_t>());
    }

    auto funcs = item->events.get(func_name_str);
    jerry_value_t func = *(args_p + 1);
    jerry_acquire_value(func);
    funcs->add(func, 0);
  }
  JERRY_PROPERTY_BLOCK_END(id);

  return JERRY_UNDEFINED;
}

JERRY_EXTERNAL_FUNC(ext::websocket::remove_event_listener) {
  if (args_cnt < 2) {
    return JERRY_UNDEFINED;
  }

  if (!jerry_value_is_string(*args_p) || !jerry_value_is_function(*(args_p + 1))) {
    return JERRY_UNDEFINED;
  }

  JERRY_PROPERTY_BLOCK(this_value, id);
  auto _id = (uint32_t) jerry_get_number_value(id_prop);
  int32_t index = websocket_item_map.find(_id);
  if (index > -1) {
    auto item = websocket_item_map.get(_id);
    JERRY_CONV_STR_TO_CHAR_BUFFER(func_name, func_name_len, func_name_buffer, args_p);
    string func_name_str((char *) func_name_buffer);
    int32_t index = item->events.find(func_name_str);
    if (index == -1) {
      return JERRY_UNDEFINED;
    }

    auto funcs = item->events.get(func_name_str);
    jerry_value_t func = *(args_p + 1);
    while (funcs->find(func) > -1) {
      jerry_release_value(func);
      funcs->remove(func);
    }
  }
  JERRY_PROPERTY_BLOCK_END(id);

  return JERRY_UNDEFINED;
}

JERRY_EXTERNAL_FUNC(ext::websocket::close) {
  JERRY_PROPERTY_BLOCK(this_value, id);
  auto _id = (uint32_t) jerry_get_number_value(id_prop);
  int32_t index = websocket_item_map.find(_id);
  if (index > -1) {
    auto item = websocket_item_map.get(_id);
#ifdef __EMSCRIPTEN__
    emscripten_websocket_close(item->socket, 0, 0);
#endif
  }
  JERRY_PROPERTY_BLOCK_END(id);

  return JERRY_UNDEFINED;
}

JERRY_EXTERNAL_FUNC(ext::websocket::send) {
  if (args_cnt == 0) {
    return JERRY_UNDEFINED;
  }

  JERRY_PROPERTY_BLOCK(this_value, id);
  auto _id = jerry_get_number_value(id_prop);
  int32_t index = websocket_item_map.find(_id);
  if (index == -1) {
    return JERRY_UNDEFINED;
  }

  auto item = websocket_item_map.get(_id);
  if (item->status != WEBSOCKET_OPEN_STATUS) {
    return JERRY_UNDEFINED;
  }

  if (jerry_value_is_string(*args_p)) {
    JERRY_CONV_STR_TO_CHAR_BUFFER(content, content_len, content_buffer, args_p);
#ifdef __EMSCRIPTEN__
    emscripten_websocket_send_utf8_text(item->socket, (char *)content_buffer);
#endif
  } else if (jerry_value_is_typedarray(*args_p)) {
    jerry_length_t len = jerry_get_typedarray_length(*args_p);
    jerry_value_t buffer = jerry_get_typedarray_buffer(*args_p, nullptr, &len);
    jerry_length_t bytes_len = jerry_get_arraybuffer_byte_length(buffer);
    uint8_t content[bytes_len];
    jerry_arraybuffer_read(buffer, 0, content, bytes_len);
#ifdef __EMSCRIPTEN__
    emscripten_websocket_send_binary(item->socket, content, bytes_len);
#endif
    jerry_release_value(buffer);
  }
  JERRY_PROPERTY_BLOCK_END(id);

  return JERRY_UNDEFINED;
}

void ext::websocket::define_property_get_set_func(jerry_value_t obj,
                                                  const char *func_name,
                                                  jerry_external_handler_t getter,
                                                  jerry_external_handler_t setter) {

  jerry_property_descriptor_t desc;
  jerry_init_property_descriptor_fields(&desc);
  desc.is_value_defined = false;
  desc.is_configurable_defined = true;
  desc.is_configurable = true;
  desc.is_enumerable_defined = true;
  desc.is_enumerable = true;
  desc.is_get_defined = true;
  desc.is_set_defined = true;
  desc.getter = jerry_create_external_function(getter);
  desc.setter = jerry_create_external_function(setter);

  jerry_value_t prop_name = JERRY_STRING(func_name);
  jerry_value_t prop = jerry_define_own_property(obj, prop_name, &desc);
  ext::error::log_runtime_error(prop);
  jerry_release_value(prop);
  jerry_release_value(prop_name);

  jerry_free_property_descriptor_fields(&desc);
}

#ifdef __EMSCRIPTEN__
EM_BOOL ext::websocket::open_callback(int eventType, const EmscriptenWebSocketOpenEvent *e, void *userData) {
  auto item = (ext::websocket_item *)userData;
  auto _id = item->id;
  int32_t index = websocket_item_map.find(_id);
  if(index > -1) {
    auto item = websocket_item_map.get(_id);
    auto events = item->events.get(string("open"));
    item->status = WEBSOCKET_OPEN_STATUS;

    if(events != nullptr) {
      events->foreach([](jerry_value_t k, uint32_t z, void *userData) -> void {
        auto this_val = (jerry_value_t *)userData;
        if(jerry_value_is_function(k)) {
          jerry_value_t retval = jerry_call_function(k, *this_val, nullptr, 0);
          ext::error::log_runtime_error(retval);
          jerry_release_value(retval);
        }
      }, &item->this_val);
    }
  }
  return 0;
}
#endif

#ifdef __EMSCRIPTEN__
EM_BOOL ext::websocket::close_callback(int eventType, const EmscriptenWebSocketCloseEvent *e, void *userData) {
  auto item = (ext::websocket_item *)userData;
  auto _id = item->id;
  int32_t index = websocket_item_map.find(_id);
  if(index > -1) {
    auto item = websocket_item_map.get(_id);
    item->status = WEBSOCKET_CLOSE_STATUS;

    item->events.foreach([](string key, map<jerry_value_t, uint32_t> value, void *this_val) -> void {
      bool emitted = key == string("close");
      if (emitted) {
        value.foreach([](jerry_value_t k, uint32_t z, void *userData) -> void {
          auto tiv = (jerry_value_t *) userData;
          if(jerry_value_is_function(k)) {
            jerry_value_t retval = jerry_call_function(k, *tiv, nullptr, 0);
            ext::error::log_runtime_error(retval);
            jerry_release_value(retval);
            jerry_release_value(k);
          }
        }, this_val);
      } else {
        value.foreach([](jerry_value_t k, uint32_t zero, void *userData) -> void {
          jerry_release_value(k);
        }, nullptr);
      }
    }, &item->this_val);

    jerry_release_value(item->this_val);
    emscripten_websocket_delete(item->socket);
    websocket_item_map.remove(_id);
  }

  return 0;
}
#endif

#ifdef __EMSCRIPTEN__
EM_BOOL ext::websocket::error_callback(int eventType, const EmscriptenWebSocketErrorEvent *e, void *userData) {
  auto item = (ext::websocket_item *)userData;
  auto _id = item->id;
  int32_t index = websocket_item_map.find(_id);
  if(index > -1) {
    auto item = websocket_item_map.get(_id);
    auto events = item->events.get(string("error"));
    item->userData = (void *)eventType;

    if(events != nullptr) {
      events->foreach([](jerry_value_t k, uint32_t z, void *userData) -> void {
        auto item = (websocket_item *)userData;
        jerry_value_t args[1];
        args[0] = jerry_create_number(*((int *)item->userData));
        if(jerry_value_is_function(k)) {
          jerry_value_t retval = jerry_call_function(k, item->this_val, args, 1);
          ext::error::log_runtime_error(retval);
          jerry_release_value(retval);
        }
        jerry_release_value(args[0]);
      }, item);
    }
  }
  return 0;
}
#endif

#ifdef __EMSCRIPTEN__
EM_BOOL ext::websocket::message_callback(int eventType, const EmscriptenWebSocketMessageEvent *e, void *userData) {
  if(e->numBytes <= 0){
    return 0;
  }

  auto item = (ext::websocket_item *)userData;
  auto _id = item->id;
  int32_t index = websocket_item_map.find(_id);
  if(index > -1){
    auto item = websocket_item_map.get(_id);
    if(item->status != WEBSOCKET_OPEN_STATUS){
      return 0;
    }

    jerry_value_t arg;
    if(e->isText){
      arg = JERRY_STRING((char *)e->data);
    }else{
      jerry_value_t arraybuf = jerry_create_arraybuffer(e->numBytes);
      jerry_arraybuffer_write(arraybuf, 0, e->data, e->numBytes);
      arg = jerry_create_typedarray_for_arraybuffer(JERRY_TYPEDARRAY_UINT8, arraybuf);
      jerry_release_value(arraybuf);
    }

    auto events = item->events.get(string("message"));
    item->userData = (void *)&arg;
    if(events != nullptr) {
      events->foreach([](jerry_value_t k, uint32_t z, void *userData) -> void {
        auto item = (websocket_item *)userData;
        jerry_value_t args[1];
        args[0] = *((jerry_value_t *)item->userData);
        if(jerry_value_is_function(k)) {
          jerry_value_t retval = jerry_call_function(k, item->this_val, args, 1);
          ext::error::log_runtime_error(retval);
          jerry_release_value(retval);
        }
      }, item);
    }

    jerry_release_value(arg);
  }
  return 0;
}
#endif
