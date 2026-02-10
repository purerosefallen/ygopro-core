#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>

extern "C" EMSCRIPTEN_KEEPALIVE void ocgcore_shutdown(int code) {
  emscripten_force_exit(code);
}
#endif 
