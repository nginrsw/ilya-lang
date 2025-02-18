/*
** $Id: lauxlib.h $
** Auxiliary functions for building Ilya libraries
** See Copyright Notice in ilya.h
*/


#ifndef lauxlib_h
#define lauxlib_h


#include <stddef.h>
#include <stdio.h>

#include "ilyaconf.h"
#include "ilya.h"


/* global table */
#define ILYA_GNAME	"_G"


typedef struct ilyaL_Buffer ilyaL_Buffer;


/* extra error code for 'ilyaL_loadfilex' */
#define ILYA_ERRFILE     (ILYA_ERRERR+1)


/* key, in the registry, for table of loaded modules */
#define ILYA_LOADED_TABLE	"_LOADED"


/* key, in the registry, for table of preloaded loaders */
#define ILYA_PRELOAD_TABLE	"_PRELOAD"


typedef struct ilyaL_Reg {
  const char *name;
  ilya_CFunction func;
} ilyaL_Reg;


#define ILYAL_NUMSIZES	(sizeof(ilya_Integer)*16 + sizeof(ilya_Number))

ILYALIB_API void (ilyaL_checkversion_) (ilya_State *L, ilya_Number ver, size_t sz);
#define ilyaL_checkversion(L)  \
	  ilyaL_checkversion_(L, ILYA_VERSION_NUM, ILYAL_NUMSIZES)

ILYALIB_API int (ilyaL_getmetafield) (ilya_State *L, int obj, const char *e);
ILYALIB_API int (ilyaL_callmeta) (ilya_State *L, int obj, const char *e);
ILYALIB_API const char *(ilyaL_tolstring) (ilya_State *L, int idx, size_t *len);
ILYALIB_API int (ilyaL_argerror) (ilya_State *L, int arg, const char *extramsg);
ILYALIB_API int (ilyaL_typeerror) (ilya_State *L, int arg, const char *tname);
ILYALIB_API const char *(ilyaL_checklstring) (ilya_State *L, int arg,
                                                          size_t *l);
ILYALIB_API const char *(ilyaL_optlstring) (ilya_State *L, int arg,
                                          const char *def, size_t *l);
ILYALIB_API ilya_Number (ilyaL_checknumber) (ilya_State *L, int arg);
ILYALIB_API ilya_Number (ilyaL_optnumber) (ilya_State *L, int arg, ilya_Number def);

ILYALIB_API ilya_Integer (ilyaL_checkinteger) (ilya_State *L, int arg);
ILYALIB_API ilya_Integer (ilyaL_optinteger) (ilya_State *L, int arg,
                                          ilya_Integer def);

ILYALIB_API void (ilyaL_checkstack) (ilya_State *L, int sz, const char *msg);
ILYALIB_API void (ilyaL_checktype) (ilya_State *L, int arg, int t);
ILYALIB_API void (ilyaL_checkany) (ilya_State *L, int arg);

ILYALIB_API int   (ilyaL_newmetatable) (ilya_State *L, const char *tname);
ILYALIB_API void  (ilyaL_setmetatable) (ilya_State *L, const char *tname);
ILYALIB_API void *(ilyaL_testudata) (ilya_State *L, int ud, const char *tname);
ILYALIB_API void *(ilyaL_checkudata) (ilya_State *L, int ud, const char *tname);

ILYALIB_API void (ilyaL_where) (ilya_State *L, int lvl);
ILYALIB_API int (ilyaL_error) (ilya_State *L, const char *fmt, ...);

ILYALIB_API int (ilyaL_checkoption) (ilya_State *L, int arg, const char *def,
                                   const char *const lst[]);

ILYALIB_API int (ilyaL_fileresult) (ilya_State *L, int stat, const char *fname);
ILYALIB_API int (ilyaL_execresult) (ilya_State *L, int stat);


/* predefined references */
#define ILYA_NOREF       (-2)
#define ILYA_REFNIL      (-1)

ILYALIB_API int (ilyaL_ref) (ilya_State *L, int t);
ILYALIB_API void (ilyaL_unref) (ilya_State *L, int t, int ref);

ILYALIB_API int (ilyaL_loadfilex) (ilya_State *L, const char *filename,
                                               const char *mode);

#define ilyaL_loadfile(L,f)	ilyaL_loadfilex(L,f,NULL)

ILYALIB_API int (ilyaL_loadbufferx) (ilya_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
ILYALIB_API int (ilyaL_loadstring) (ilya_State *L, const char *s);

ILYALIB_API ilya_State *(ilyaL_newstate) (void);

ILYALIB_API unsigned ilyaL_makeseed (ilya_State *L);

ILYALIB_API ilya_Integer (ilyaL_len) (ilya_State *L, int idx);

ILYALIB_API void (ilyaL_addgsub) (ilyaL_Buffer *b, const char *s,
                                     const char *p, const char *r);
ILYALIB_API const char *(ilyaL_gsub) (ilya_State *L, const char *s,
                                    const char *p, const char *r);

ILYALIB_API void (ilyaL_setfuncs) (ilya_State *L, const ilyaL_Reg *l, int nup);

ILYALIB_API int (ilyaL_getsubtable) (ilya_State *L, int idx, const char *fname);

ILYALIB_API void (ilyaL_traceback) (ilya_State *L, ilya_State *L1,
                                  const char *msg, int level);

ILYALIB_API void (ilyaL_requiref) (ilya_State *L, const char *modname,
                                 ilya_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define ilyaL_newlibtable(L,l)	\
  ilya_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define ilyaL_newlib(L,l)  \
  (ilyaL_checkversion(L), ilyaL_newlibtable(L,l), ilyaL_setfuncs(L,l,0))

#define ilyaL_argcheck(L, cond,arg,extramsg)	\
	((void)(ilyai_likely(cond) || ilyaL_argerror(L, (arg), (extramsg))))

#define ilyaL_argexpected(L,cond,arg,tname)	\
	((void)(ilyai_likely(cond) || ilyaL_typeerror(L, (arg), (tname))))

#define ilyaL_checkstring(L,n)	(ilyaL_checklstring(L, (n), NULL))
#define ilyaL_optstring(L,n,d)	(ilyaL_optlstring(L, (n), (d), NULL))

#define ilyaL_typename(L,i)	ilya_typename(L, ilya_type(L,(i)))

#define ilyaL_dofile(L, fn) \
	(ilyaL_loadfile(L, fn) || ilya_pcall(L, 0, ILYA_MULTRET, 0))

#define ilyaL_dostring(L, s) \
	(ilyaL_loadstring(L, s) || ilya_pcall(L, 0, ILYA_MULTRET, 0))

#define ilyaL_getmetatable(L,n)	(ilya_getfield(L, ILYA_REGISTRYINDEX, (n)))

#define ilyaL_opt(L,f,n,d)	(ilya_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define ilyaL_loadbuffer(L,s,sz,n)	ilyaL_loadbufferx(L,s,sz,n,NULL)


/*
** Perform arithmetic operations on ilya_Integer values with wrap-around
** semantics, as the Ilya core does.
*/
#define ilyaL_intop(op,v1,v2)  \
	((ilya_Integer)((ilya_Unsigned)(v1) op (ilya_Unsigned)(v2)))


/* push the value used to represent failure/error */
#define ilyaL_pushfail(L)	ilya_pushnil(L)



/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct ilyaL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  ilya_State *L;
  union {
    ILYAI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[ILYAL_BUFFERSIZE];  /* initial buffer */
  } init;
};


#define ilyaL_bufflen(bf)	((bf)->n)
#define ilyaL_buffaddr(bf)	((bf)->b)


#define ilyaL_addchar(B,c) \
  ((void)((B)->n < (B)->size || ilyaL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define ilyaL_addsize(B,s)	((B)->n += (s))

#define ilyaL_buffsub(B,s)	((B)->n -= (s))

ILYALIB_API void (ilyaL_buffinit) (ilya_State *L, ilyaL_Buffer *B);
ILYALIB_API char *(ilyaL_prepbuffsize) (ilyaL_Buffer *B, size_t sz);
ILYALIB_API void (ilyaL_addlstring) (ilyaL_Buffer *B, const char *s, size_t l);
ILYALIB_API void (ilyaL_addstring) (ilyaL_Buffer *B, const char *s);
ILYALIB_API void (ilyaL_addvalue) (ilyaL_Buffer *B);
ILYALIB_API void (ilyaL_pushresult) (ilyaL_Buffer *B);
ILYALIB_API void (ilyaL_pushresultsize) (ilyaL_Buffer *B, size_t sz);
ILYALIB_API char *(ilyaL_buffinitsize) (ilya_State *L, ilyaL_Buffer *B, size_t sz);

#define ilyaL_prepbuffer(B)	ilyaL_prepbuffsize(B, ILYAL_BUFFERSIZE)

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'ILYA_FILEHANDLE' and
** initial structure 'ilyaL_Stream' (it may contain other fields
** after that initial structure).
*/

#define ILYA_FILEHANDLE          "FILE*"


typedef struct ilyaL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  ilya_CFunction closef;  /* to close stream (NULL for closed streams) */
} ilyaL_Stream;

/* }====================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(ILYA_COMPAT_APIINTCASTS)

#define ilyaL_checkunsigned(L,a)	((ilya_Unsigned)ilyaL_checkinteger(L,a))
#define ilyaL_optunsigned(L,a,d)	\
	((ilya_Unsigned)ilyaL_optinteger(L,a,(ilya_Integer)(d)))

#define ilyaL_checkint(L,n)	((int)ilyaL_checkinteger(L, (n)))
#define ilyaL_optint(L,n,d)	((int)ilyaL_optinteger(L, (n), (d)))

#define ilyaL_checklong(L,n)	((long)ilyaL_checkinteger(L, (n)))
#define ilyaL_optlong(L,n,d)	((long)ilyaL_optinteger(L, (n), (d)))

#endif
/* }============================================================ */



#endif


