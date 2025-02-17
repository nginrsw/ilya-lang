/*
** $Id: lundump.h $
** load precompiled Irin chunks
** See Copyright Notice in irin.h
*/

#ifndef lundump_h
#define lundump_h

#include <limits.h>

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"


/* data to catch conversion errors */
#define LUAC_DATA	"\x19\x93\r\n\x1a\n"

#define LUAC_INT	0x5678
#define LUAC_NUM	cast_num(370.5)

/*
** Encode major-minor version in one byte, one nibble for each
*/
#define LUAC_VERSION	(IRIN_VERSION_MAJOR_N*16+IRIN_VERSION_MINOR_N)

#define LUAC_FORMAT	0	/* this is the official format */


/* load one chunk; from lundump.c */
LUAI_FUNC LClosure* luaU_undump (irin_State* L, ZIO* Z, const char* name,
                                               int fixed);

/* dump one chunk; from ldump.c */
LUAI_FUNC int luaU_dump (irin_State* L, const Proto* f, irin_Writer w,
                         void* data, int strip);

#endif
