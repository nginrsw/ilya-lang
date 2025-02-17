#include "ilya.h"


int luaopen_lib2 (ilya_State *L);

LUAMOD_API int luaopen_lib21 (ilya_State *L) {
  return luaopen_lib2(L);
}


