#include "request.hpp"

map<unsigned int, ext::request::request_item> ext::request::request_map;

int ext::request::init() {
  jerry_value_t global_object = jerry_get_global_object();
  JERRY_SET_EXTERNAL_FUNC_PROPERTY(global_object, request, request_wrap);
  jerry_release_value(global_object);
  return 0;
}

JERRY_EXTERNAL_FUNC(ext::request::request_wrap) {
  if (args_cnt == 0 || !jerry_value_is_object(*args_p)) {
    return JERRY_UNDEFINED;
  }

  ext::request::request_item item{0};
  jerry_value_t param = jerry_value_to_object(*args_p);

  // request uri
  string uri_str;
  JERRY_SAFE_GET_OBJ_KEY_BLOCK(param, uri)
  if (jerry_value_is_undefined(uri_prop)) {
    return JERRY_UNDEFINED;
  } else {
    JERRY_CONV_STR_TO_CHAR_BUFFER(uri, uri_len, uri_chs, &uri_prop);
    uri_str << (char *) uri_chs;
  }
  JERRY_SAFE_GET_OBJ_KEY_BLOCK_END(uri)

  // callback retain
  jerry_acquire_value(param);
  item.this_val = param;
  JERRY_SAFE_GET_OBJ_KEY_BLOCK(param, success)
  if (jerry_value_is_function(success_prop)) {
    jerry_acquire_value(success_prop);
    item.onsuccess = success_prop;
  }
  JERRY_SAFE_GET_OBJ_KEY_BLOCK_END(success)

  JERRY_SAFE_GET_OBJ_KEY_BLOCK(param, error)
  if (jerry_value_is_function(error_prop)) {
    jerry_acquire_value(error_prop);
    item.onerror = error_prop;
  }
  JERRY_SAFE_GET_OBJ_KEY_BLOCK_END(error)

  // request method: GET/POST/HEAD...
  string method_str;
  JERRY_SAFE_GET_OBJ_KEY_BLOCK(param, method)
  if (jerry_value_is_undefined(method_prop)) {
    method_str << "GET";
  } else {
    JERRY_CONV_STR_TO_CHAR_BUFFER(method, method_len, method_chs, &method_prop);
    method_str << (char *) method_chs;
  }
  JERRY_SAFE_GET_OBJ_KEY_BLOCK_END(method)

  // request data
  string data_str;
  JERRY_SAFE_GET_OBJ_KEY_BLOCK(param, body)
  if (!jerry_value_is_undefined(body_prop)) {
    if (!(method_str == string("GET") || method_str == string("HEAD"))) {
      JERRY_CONV_STR_TO_CHAR_BUFFER(body, body_len, body_chs, &body_prop);
      data_str << (char *) body_chs;
    }
  }
  JERRY_SAFE_GET_OBJ_KEY_BLOCK_END(body)

  // request headers
  uint32_t headers_len = 0;
  string *headers[MAX_HEADERS_LEN];
  memset(headers, NULL, MAX_HEADERS_LEN * sizeof(string *));
  JERRY_SAFE_GET_OBJ_KEY_BLOCK(param, headers)
  if (!jerry_value_is_undefined(headers_prop)) {
    auto foreach_func = [](
            const jerry_value_t prop_name,
            const jerry_value_t prop_value,
            void *user_data_p
    ) -> bool {
      auto headers = (string **) user_data_p;
      int32_t index = -1;
      JERRY_CONV_STR_TO_CHAR_BUFFER(name, name_len, name_buffer, &prop_name);
      JERRY_CONV_STR_TO_CHAR_BUFFER(val, val_len, val_buffer, &prop_value);
      for (uint32_t i = 0; i < MAX_HEADERS_LEN; i++) {
        if (headers[i] == NULL) {
          index = i;
          break;
        }
      }

      if (index == -1) {
        return false;
      }

      headers[index] = new string((char *) name_buffer);
      headers[index + 1] = new string((char *) val_buffer);
      return true;
    };

    jerry_foreach_object_property(headers_prop, foreach_func, headers);
    for (uint32_t i = 0; i < MAX_HEADERS_LEN; i++) {
      if (headers[i] != NULL) {
        headers_len += 1;
      } else {
        break;
      }
    }
  }
  JERRY_SAFE_GET_OBJ_KEY_BLOCK_END(headers)

  // request timeout
  uint32_t timeout = 20 * 1000;
  JERRY_SAFE_GET_OBJ_KEY_BLOCK(param, timeout)
  if (!jerry_value_is_undefined(timeout_prop)) {
    jerry_value_t conv_timeout_prop = jerry_value_to_number(timeout_prop);
    timeout = (uint32_t) jerry_get_number_value(conv_timeout_prop);
    jerry_release_value(conv_timeout_prop);
  }
  JERRY_SAFE_GET_OBJ_KEY_BLOCK_END(timeout)

  // cors credentials
  bool with_credentials = false;
  JERRY_SAFE_GET_OBJ_KEY_BLOCK(param, withCredentials)
  if (!jerry_value_is_undefined(withCredentials_prop)) {
    with_credentials = jerry_value_to_boolean(withCredentials_prop);
  }
  JERRY_SAFE_GET_OBJ_KEY_BLOCK_END(withCredentials)

#ifdef __EMSCRIPTEN__
  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  strcpy(attr.requestMethod, method_str.c_str());

  if(data_str.size()){
    char *requestData = new char[data_str.size() + 1];
    memset(requestData, '\0', data_str.size() + 1);
    strcpy(requestData, data_str.c_str());
    attr.requestDataSize = data_str.size();
    attr.requestData = requestData;
    attr.userData = (void *)requestData;
  }

  attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_REPLACE;
  attr.timeoutMSecs = (unsigned long)timeout;
  attr.withCredentials = (EM_BOOL)with_credentials;

  char **headers_chs = new char *[headers_len + 1];
  headers_chs[headers_len] = nullptr;
  for(uint32_t i = 0; i < headers_len; i++){
    headers_chs[i] = new char[headers[i]->size() + 1];
    memset(headers_chs[i], '\0', headers[i]->size() + 1);
    memcpy(headers_chs[i], headers[i]->c_str(), headers[i]->size());
    delete headers[i];
    headers[i] = nullptr;
  }
  attr.requestHeaders = headers_chs;

  attr.onsuccess = ext::request::onsuccess;
  attr.onerror = ext::request::onerror;

  auto fetch = emscripten_fetch(&attr, uri_str.c_str());
  request_map.add(fetch->id, item);

  memset(&attr, 0, sizeof(attr));
  delete[] headers_chs;
  headers_chs = nullptr;
#endif

  jerry_release_value(param);

  return JERRY_UNDEFINED;
}

#ifdef __EMSCRIPTEN__
jerry_value_t ext::request::conv_response_data(emscripten_fetch_t *fetch) {
  jerry_value_t resp = jerry_create_object();

  jerry_value_t status_val = jerry_create_number((double)fetch->status);
  JERRY_SET_PROPERTY(resp, status, status_val);
  jerry_release_value(status_val);

  jerry_value_t status_text_val = JERRY_STRING(fetch->statusText);
  JERRY_SET_PROPERTY(resp, statusText, status_val);
  jerry_release_value(status_text_val);

  jerry_value_t total_bytes_val = jerry_create_number((double) fetch->totalBytes);
  JERRY_SET_PROPERTY(resp, totalBytes, total_bytes_val);
  jerry_release_value(total_bytes_val);

  char *data = new char[fetch->totalBytes + 1];
  memset(data, '\0', fetch->totalBytes + 1);
  memcpy(data, fetch->data, fetch->totalBytes);
  jerry_value_t data_val = JERRY_STRING(data);
  JERRY_SET_PROPERTY(resp, text, data_val);
  jerry_release_value(data_val);
  delete[] data;
  data = nullptr;

  return resp;
}
#endif

#ifdef __EMSCRIPTEN__
void ext::request::onsuccess(emscripten_fetch_t *fetch) {
  uint32_t index = request_map.find(fetch->id);
  if(index == -1){
    return;
  }

  request_item *item = request_map.get(fetch->id);
  if(jerry_value_is_function(item->onsuccess)){
    jerry_value_t resp = ext::request::conv_response_data(fetch);
    jerry_value_t retval = jerry_call_function(item->onsuccess, item->this_val, &resp, 1);
    ext::error::log_runtime_error(retval);
    jerry_release_value(retval);
    jerry_release_value(resp);
  }

  jerry_release_value(item->this_val);
  jerry_release_value(item->onsuccess);
  jerry_release_value(item->onerror);
  request_map.remove(fetch->id);
  emscripten_fetch_close(fetch);
}
#endif

#ifdef __EMSCRIPTEN__
void ext::request::onerror(emscripten_fetch_t *fetch) {
  uint32_t index = request_map.find(fetch->id);
  if(index == -1){
    return;
  }

  request_item item = *request_map.get(fetch->id);
  if(jerry_value_is_function(item.onerror)){
    jerry_value_t resp = ext::request::conv_response_data(fetch);
    jerry_value_t retval = jerry_call_function(item.onerror, item.this_val, &resp, 1);
    ext::error::log_runtime_error(retval);
    jerry_release_value(retval);
    jerry_release_value(resp);
  }

  jerry_release_value(item.this_val);
  jerry_release_value(item.onsuccess);
  jerry_release_value(item.onerror);
  request_map.remove(fetch->id);
  emscripten_fetch_close(fetch);
}
#endif
