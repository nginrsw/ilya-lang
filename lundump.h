/*
** $Id: lundump.h $
** load precompiled Ilya chunks
** See Copyright Notice in ilya.h
*/

#ifndef lundump_h
#define lundump_h

#include <limits.h>

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"


/* data to catch conversion errors */
#define ILYAC_DATA	"\x19\x93\r\n\x1a\n"

#define ILYAC_INT	0x5678
#define ILYAC_NUM	cast_num(370.5)

/*
** Encode major-minor version in one byte, one nibble for each
*/
#define ILYAC_VERSION	(ILYA_VERSION_MAJOR_N*16+ILYA_VERSION_MINOR_N)

#define ILYAC_FORMAT	0	/* this is the official format */


/* load one chunk; from lundump.c */
ILYAI_FUNC LClosure* ilyaU_undump (ilya_State* L, ZIO* Z, const char* name,
                                               int fixed);

/* dump one chunk; from ldump.c */
ILYAI_FUNC int ilyaU_dump (ilya_State* L, const Proto* f, ilya_Writer w,
                         void* data, int strip);

#endif
