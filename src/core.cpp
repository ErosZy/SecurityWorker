#include "string.hpp"
#include "console.hpp"
#include "timer.hpp"
#include "helper.hpp"
#include "marco.hpp"
#include "error.hpp"
#include "websocket.hpp"
#include "request.hpp"
#include "self.hpp"
#include "b64.h"
#include "aes.hpp"

#define ENKEY "dtaacJLo7XZi845WnNalLM6HvaUVmbtnpTVTKcriHpAh3dXk"
#define ENIV "NJC4ZR7spT6FD8AEDbpJCNJ2GTmgSgft2gB8rKPHc7BYNyZb"

bool is_alive = false;

char *decrypt(char *code, int len) {
  auto tmp = (char *) malloc((size_t) len + 1);
  memset(tmp, '\0', (size_t) len + 1);
  memcpy(tmp, code, (size_t) len);
  code = tmp;
  for (int i = 0, j = len - 1; i < j; i++, j--) {
    char k = code[i];
    code[i] = code[j];
    code[j] = k;
  }
  auto b64 = b64_decode(code, (size_t) len);
  free(tmp);
  return (char *) b64;
}

extern "C" {
#include "jerryscript.h"

int security_worker_onmessage(char *data) {
  string code(data);
  if (is_alive || code.size()) {
    jerry_value_t global_object = jerry_get_global_object();
    jerry_value_t onmessage_prop_name = JERRY_STRING("__onmessage__");
    jerry_value_t onmessage_prop = jerry_get_property(global_object, onmessage_prop_name);
    if (jerry_value_is_function(onmessage_prop)) {
      jerry_value_t args[1];
      args[0] = jerry_create_string((jerry_char_t *) code.c_str());
      jerry_value_t retval = jerry_call_function(onmessage_prop, JERRY_UNDEFINED, args, 1);
      ext::error::log_runtime_error(retval);
      jerry_release_value(retval);
      jerry_release_value(args[0]);
    }
    jerry_release_value(onmessage_prop);
    jerry_release_value(onmessage_prop_name);
    jerry_release_value(global_object);
  }
  return 0;
}

int security_worker_new(char *js_code, size_t b64_len, size_t real_len, size_t en_len, char *$$_code) {
  js_code = decrypt(js_code, (int) b64_len);

  char rkey[17] = {'\0'};
  char riv[17] = {'\0'};
  for (int i = 0; i < 16; i++) {
    rkey[i] = ENKEY[16 + i];
    riv[i] = ENIV[16 + i];
  }

  char codekey[17] = {'\0'};
  char codeiv[17] = {'\0'};

  for (int i = 0, j = 15; i < j; i++, j--) {
    char k = rkey[i];
    codekey[i] = rkey[j];
    codekey[j] = k;

    k = riv[i];
    codeiv[i] = riv[j];
    codeiv[j] = k;
  }

  struct AES_ctx ctx;
  AES_init_ctx_iv(&ctx, (uint8_t *) codekey, (uint8_t *) codeiv);
  AES_CBC_decrypt_buffer(&ctx, (uint8_t *) js_code, (uint32_t) en_len);

  int padding = js_code[en_len - 1];
  real_len = en_len - padding;

  auto tmp = (char *) malloc(real_len + 1);
  memset(tmp, '\0', real_len + 1);
  memcpy(tmp, js_code, real_len);

  string script(tmp);
  free(tmp);
  free(js_code);

  if (script.size()) {
    jerry_init(JERRY_INIT_EMPTY);

    string $$str;
    $$str << "var $ = " << $$_code;
    jerry_value_t evalret = jerry_eval((jerry_char_t *) $$str.c_str(), $$str.size(), JERRY_PARSE_NO_OPTS);
    ext::error::log_compile_error(evalret);
    jerry_release_value(evalret);

    ext::console::init();
    ext::timer::init();
    ext::helper::init();
    ext::error::init();
    ext::request::init();
    ext::websocket::init();
    ext::self::init();

    jerry_value_t parsed_code = jerry_parse((jerry_char_t *) "<anonymous>",
                                            11,
                                            (jerry_char_t *) script.c_str(),
                                            script.size(),
                                            JERRY_PARSE_NO_OPTS);

    if (!jerry_value_is_error(parsed_code)) {
      jerry_value_t retval = jerry_run(parsed_code);
      ext::error::log_runtime_error(retval);
      jerry_release_value(retval);
    } else {
      ext::error::log_compile_error(parsed_code);
    }

    jerry_release_value(parsed_code);
    is_alive = true;
#ifdef __EMSCRIPTEN__
    emscripten_run_script("typeof __ready_bridge__ == 'function' && __ready_bridge__()");
#endif
  }
  return 0;
}

int security_worker_exit() {
  if (is_alive) {
    is_alive = false;
    jerry_cleanup();
  }
  return 0;
}
}
