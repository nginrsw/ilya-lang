#include "ilya.h"


int ilyaopen_lib2 (ilya_State *L);

ILYAMOD_API int ilyaopen_lib21 (ilya_State *L) {
  return ilyaopen_lib2(L);
}


