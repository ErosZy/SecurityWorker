#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "aes.h"
#include "mini_gzip.h"

#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#include "emscripten/websocket.h"
#include "emscripten/html5.h"
#endif

#define ENKEY "cC0NWtAI1n7kytT0"
#define ENIV "IF0ssl3fDwXWBh8b"
#define ENLEN 0
#define GZIP_SIZE 0
#define UNPACK_SIZE 0

int main() {
  uint8_t compress_bytes[] = {};
#ifdef __EMSCRIPTEN__
  int unchange = emscripten_run_script_int("(function(){eval('var rEFGxb=1;')}());typeof rEFGxb=='undefined';");
  if(!unchange){
    return 0;
  }
#endif

// Android4.4 不支持WebGL，但是其占比5.4%
// 暂时把WebGL的环境检测去掉
// #ifdef __EMSCRIPTEN__
//   EmscriptenWebGLContextAttributes attrs;
//   emscripten_webgl_init_context_attributes(&attrs);
//   attrs.depth = 0;
//   attrs.stencil = 0;
//   attrs.antialias = 0;

//   EM_ASM(
//     var canvas = document.createElement('canvas');
//     Module.canvas.parentElement.appendChild(canvas);
//     canvas.id = 'SecurityWorker';
//     canvas.width = canvas.height = 0;
//     canvas.style.position = 'absolute';
//     canvas.style.top = '-9999px';
//   );

//   EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = emscripten_webgl_create_context("SecurityWorker", &attrs);
//   EMSCRIPTEN_RESULT res = emscripten_webgl_make_context_current(context);
//   if(res != EMSCRIPTEN_RESULT_SUCCESS){
//     return 0;
//   }

//   int drawingBufferWidth = -1;
//   int drawingBufferHeight = -1;
//   res = emscripten_webgl_get_drawing_buffer_size(context, &drawingBufferWidth, &drawingBufferHeight);
//   if(res != EMSCRIPTEN_RESULT_SUCCESS){
//     return 0;
//   }

//   emscripten_webgl_destroy_context(context);

//   EM_ASM(
//     var canvas = document.getElementById('SecurityWorker');
//     canvas.parentElement.removeChild(canvas);
//   );
// #endif


#ifdef __EMSCRIPTEN__
  if(emscripten_websocket_is_supported()) {
#endif

  struct AES_ctx ctx;
  AES_init_ctx_iv(&ctx, (uint8_t *) ENKEY, (uint8_t *) ENIV);
  AES_CBC_decrypt_buffer(&ctx, compress_bytes, ENLEN);
  char *tmp = (char *) malloc(GZIP_SIZE);
  memset(tmp, 0, GZIP_SIZE);
  memcpy(tmp, compress_bytes, GZIP_SIZE);

  struct mini_gzip gz;
  uint8_t mem_out[UNPACK_SIZE + 1];
  memset(mem_out, '\0', UNPACK_SIZE + 1);
  int ret = mini_gz_start(&gz, tmp, GZIP_SIZE);
  int out_len = mini_gz_unpack(&gz, mem_out, UNPACK_SIZE);
  free(tmp);

#ifdef __EMSCRIPTEN__
  emscripten_run_script((char *)mem_out);
#endif

#ifdef __EMSCRIPTEN__
  }
#endif
  return 0;
}