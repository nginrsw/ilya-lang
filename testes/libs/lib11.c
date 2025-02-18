#include "ilya.h"

/* fn from lib1.c */
int lib1_export (ilya_State *L);

ILYAMOD_API int ilyaopen_lib11 (ilya_State *L) {
  return lib1_export(L);
}


