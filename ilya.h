/*
** $Id: ilya.h $
** Ilya - A Scripting Language
** github.com/nginrsw, KRS-Bdg, Indonesia
** See Copyright Notice at the end of this file
*/


#ifndef ilya_h
#define ilya_h

#include <stdarg.h>
#include <stddef.h>


#define ILYA_COPYRIGHT	ILYA_RELEASE "  Copyright (C) 2025 github.com/nginrsw/ilya-lang, KRS-Bdg"
#define ILYA_AUTHORS	"Gillar Ajie Prasatya"


#define ILYA_VERSION_MAJOR_N	0
#define ILYA_VERSION_MINOR_N	0
#define ILYA_VERSION_RELEASE_N	1

#define ILYA_VERSION_NUM  (ILYA_VERSION_MAJOR_N * 100 + ILYA_VERSION_MINOR_N)
#define ILYA_VERSION_RELEASE_NUM  (ILYA_VERSION_NUM * 100 + ILYA_VERSION_RELEASE_N)


#include "ilyaconf.h"


/* mark for precompiled code ('<esc>Ilya') */
#define ILYA_SIGNATURE	"\x1bLua"

/* option for multiple returns in 'ilya_pcall' and 'ilya_call' */
#define ILYA_MULTRET	(-1)


/*
** Pseudo-indices
** (-LUAI_MAXSTACK is the minimum valid index; we keep some free empty
** space after that to help overflow detection)
*/
#define ILYA_REGISTRYINDEX	(-LUAI_MAXSTACK - 1000)
#define ilya_upvalueindex(i)	(ILYA_REGISTRYINDEX - (i))


/* thread status */
#define ILYA_OK		0
#define ILYA_YIELD	1
#define ILYA_ERRRUN	2
#define ILYA_ERRSYNTAX	3
#define ILYA_ERRMEM	4
#define ILYA_ERRERR	5


typedef struct ilya_State ilya_State;


/*
** basic types
*/
#define ILYA_TNONE		(-1)

#define ILYA_TNIL		0
#define ILYA_TBOOLEAN		1
#define ILYA_TLIGHTUSERDATA	2
#define ILYA_TNUMBER		3
#define ILYA_TSTRING		4
#define ILYA_TTABLE		5
#define ILYA_TFUNCTION		6
#define ILYA_TUSERDATA		7
#define ILYA_TTHREAD		8

#define ILYA_NUMTYPES		9



/* minimum Ilya stack available to a C fn */
#define ILYA_MINSTACK	20


/* predefined values in the registry */
/* index 1 is reserved for the reference mechanism */
#define ILYA_RIDX_GLOBALS	2
#define ILYA_RIDX_MAINTHREAD	3
#define ILYA_RIDX_LAST		3


/* type of numbers in Ilya */
typedef ILYA_NUMBER ilya_Number;


/* type for integer functions */
typedef ILYA_INTEGER ilya_Integer;

/* unsigned integer type */
typedef ILYA_UNSIGNED ilya_Unsigned;

/* type for continuation-fn contexts */
typedef ILYA_KCONTEXT ilya_KContext;


/*
** Type for C functions registered with Ilya
*/
typedef int (*ilya_CFunction) (ilya_State *L);

/*
** Type for continuation functions
*/
typedef int (*ilya_KFunction) (ilya_State *L, int status, ilya_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping Ilya chunks
*/
typedef const char * (*ilya_Reader) (ilya_State *L, void *ud, size_t *sz);

typedef int (*ilya_Writer) (ilya_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*ilya_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*ilya_WarnFunction) (void *ud, const char *msg, int tocont);


/*
** Type used by the debug API to collect debug information
*/
typedef struct ilya_Debug ilya_Debug;


/*
** Functions to be called by the debugger in specific events
*/
typedef void (*ilya_Hook) (ilya_State *L, ilya_Debug *ar);


/*
** generic extra include file
*/
#if defined(ILYA_USER_H)
#include ILYA_USER_H
#endif


/*
** RCS ident string
*/
extern const char ilya_ident[];


/*
** state manipulation
*/
ILYA_API ilya_State *(ilya_newstate) (ilya_Alloc f, void *ud, unsigned seed);
ILYA_API void       (ilya_close) (ilya_State *L);
ILYA_API ilya_State *(ilya_newthread) (ilya_State *L);
ILYA_API int        (ilya_closethread) (ilya_State *L, ilya_State *from);

ILYA_API ilya_CFunction (ilya_atpanic) (ilya_State *L, ilya_CFunction panicf);


ILYA_API ilya_Number (ilya_version) (ilya_State *L);


/*
** basic stack manipulation
*/
ILYA_API int   (ilya_absindex) (ilya_State *L, int idx);
ILYA_API int   (ilya_gettop) (ilya_State *L);
ILYA_API void  (ilya_settop) (ilya_State *L, int idx);
ILYA_API void  (ilya_pushvalue) (ilya_State *L, int idx);
ILYA_API void  (ilya_rotate) (ilya_State *L, int idx, int n);
ILYA_API void  (ilya_copy) (ilya_State *L, int fromidx, int toidx);
ILYA_API int   (ilya_checkstack) (ilya_State *L, int n);

ILYA_API void  (ilya_xmove) (ilya_State *from, ilya_State *to, int n);


/*
** access functions (stack -> C)
*/

ILYA_API int             (ilya_isnumber) (ilya_State *L, int idx);
ILYA_API int             (ilya_isstring) (ilya_State *L, int idx);
ILYA_API int             (ilya_iscfunction) (ilya_State *L, int idx);
ILYA_API int             (ilya_isinteger) (ilya_State *L, int idx);
ILYA_API int             (ilya_isuserdata) (ilya_State *L, int idx);
ILYA_API int             (ilya_type) (ilya_State *L, int idx);
ILYA_API const char     *(ilya_typename) (ilya_State *L, int tp);

ILYA_API ilya_Number      (ilya_tonumberx) (ilya_State *L, int idx, int *isnum);
ILYA_API ilya_Integer     (ilya_tointegerx) (ilya_State *L, int idx, int *isnum);
ILYA_API int             (ilya_toboolean) (ilya_State *L, int idx);
ILYA_API const char     *(ilya_tolstring) (ilya_State *L, int idx, size_t *len);
ILYA_API ilya_Unsigned    (ilya_rawlen) (ilya_State *L, int idx);
ILYA_API ilya_CFunction   (ilya_tocfunction) (ilya_State *L, int idx);
ILYA_API void	       *(ilya_touserdata) (ilya_State *L, int idx);
ILYA_API ilya_State      *(ilya_tothread) (ilya_State *L, int idx);
ILYA_API const void     *(ilya_topointer) (ilya_State *L, int idx);


/*
** Comparison and arithmetic functions
*/

#define ILYA_OPADD	0	/* ORDER TM, ORDER OP */
#define ILYA_OPSUB	1
#define ILYA_OPMUL	2
#define ILYA_OPMOD	3
#define ILYA_OPPOW	4
#define ILYA_OPDIV	5
#define ILYA_OPIDIV	6
#define ILYA_OPBAND	7
#define ILYA_OPBOR	8
#define ILYA_OPBXOR	9
#define ILYA_OPSHL	10
#define ILYA_OPSHR	11
#define ILYA_OPUNM	12
#define ILYA_OPBNOT	13

ILYA_API void  (ilya_arith) (ilya_State *L, int op);

#define ILYA_OPEQ	0
#define ILYA_OPLT	1
#define ILYA_OPLE	2

ILYA_API int   (ilya_rawequal) (ilya_State *L, int idx1, int idx2);
ILYA_API int   (ilya_compare) (ilya_State *L, int idx1, int idx2, int op);


/*
** push functions (C -> stack)
*/
ILYA_API void        (ilya_pushnil) (ilya_State *L);
ILYA_API void        (ilya_pushnumber) (ilya_State *L, ilya_Number n);
ILYA_API void        (ilya_pushinteger) (ilya_State *L, ilya_Integer n);
ILYA_API const char *(ilya_pushlstring) (ilya_State *L, const char *s, size_t len);
ILYA_API const char *(ilya_pushexternalstring) (ilya_State *L,
		const char *s, size_t len, ilya_Alloc falloc, void *ud);
ILYA_API const char *(ilya_pushstring) (ilya_State *L, const char *s);
ILYA_API const char *(ilya_pushvfstring) (ilya_State *L, const char *fmt,
                                                      va_list argp);
ILYA_API const char *(ilya_pushfstring) (ilya_State *L, const char *fmt, ...);
ILYA_API void  (ilya_pushcclosure) (ilya_State *L, ilya_CFunction fn, int n);
ILYA_API void  (ilya_pushboolean) (ilya_State *L, int b);
ILYA_API void  (ilya_pushlightuserdata) (ilya_State *L, void *p);
ILYA_API int   (ilya_pushthread) (ilya_State *L);


/*
** get functions (Ilya -> stack)
*/
ILYA_API int (ilya_getglobal) (ilya_State *L, const char *name);
ILYA_API int (ilya_gettable) (ilya_State *L, int idx);
ILYA_API int (ilya_getfield) (ilya_State *L, int idx, const char *k);
ILYA_API int (ilya_geti) (ilya_State *L, int idx, ilya_Integer n);
ILYA_API int (ilya_rawget) (ilya_State *L, int idx);
ILYA_API int (ilya_rawgeti) (ilya_State *L, int idx, ilya_Integer n);
ILYA_API int (ilya_rawgetp) (ilya_State *L, int idx, const void *p);

ILYA_API void  (ilya_createtable) (ilya_State *L, int narr, int nrec);
ILYA_API void *(ilya_newuserdatauv) (ilya_State *L, size_t sz, int nuvalue);
ILYA_API int   (ilya_getmetatable) (ilya_State *L, int objindex);
ILYA_API int  (ilya_getiuservalue) (ilya_State *L, int idx, int n);


/*
** set functions (stack -> Ilya)
*/
ILYA_API void  (ilya_setglobal) (ilya_State *L, const char *name);
ILYA_API void  (ilya_settable) (ilya_State *L, int idx);
ILYA_API void  (ilya_setfield) (ilya_State *L, int idx, const char *k);
ILYA_API void  (ilya_seti) (ilya_State *L, int idx, ilya_Integer n);
ILYA_API void  (ilya_rawset) (ilya_State *L, int idx);
ILYA_API void  (ilya_rawseti) (ilya_State *L, int idx, ilya_Integer n);
ILYA_API void  (ilya_rawsetp) (ilya_State *L, int idx, const void *p);
ILYA_API int   (ilya_setmetatable) (ilya_State *L, int objindex);
ILYA_API int   (ilya_setiuservalue) (ilya_State *L, int idx, int n);


/*
** 'load' and 'call' functions (load and run Ilya code)
*/
ILYA_API void  (ilya_callk) (ilya_State *L, int nargs, int nresults,
                           ilya_KContext ctx, ilya_KFunction k);
#define ilya_call(L,n,r)		ilya_callk(L, (n), (r), 0, NULL)

ILYA_API int   (ilya_pcallk) (ilya_State *L, int nargs, int nresults, int errfunc,
                            ilya_KContext ctx, ilya_KFunction k);
#define ilya_pcall(L,n,r,f)	ilya_pcallk(L, (n), (r), (f), 0, NULL)

ILYA_API int   (ilya_load) (ilya_State *L, ilya_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

ILYA_API int (ilya_dump) (ilya_State *L, ilya_Writer writer, void *data, int strip);


/*
** coroutine functions
*/
ILYA_API int  (ilya_yieldk)     (ilya_State *L, int nresults, ilya_KContext ctx,
                               ilya_KFunction k);
ILYA_API int  (ilya_resume)     (ilya_State *L, ilya_State *from, int narg,
                               int *nres);
ILYA_API int  (ilya_status)     (ilya_State *L);
ILYA_API int (ilya_isyieldable) (ilya_State *L);

#define ilya_yield(L,n)		ilya_yieldk(L, (n), 0, NULL)


/*
** Warning-related functions
*/
ILYA_API void (ilya_setwarnf) (ilya_State *L, ilya_WarnFunction f, void *ud);
ILYA_API void (ilya_warning)  (ilya_State *L, const char *msg, int tocont);


/*
** garbage-collection options
*/

#define ILYA_GCSTOP		0
#define ILYA_GCRESTART		1
#define ILYA_GCCOLLECT		2
#define ILYA_GCCOUNT		3
#define ILYA_GCCOUNTB		4
#define ILYA_GCSTEP		5
#define ILYA_GCISRUNNING		6
#define ILYA_GCGEN		7
#define ILYA_GCINC		8
#define ILYA_GCPARAM		9


/*
** garbage-collection parameters
*/
/* parameters for generational mode */
#define ILYA_GCPMINORMUL		0  /* control minor collections */
#define ILYA_GCPMAJORMINOR	1  /* control shift major->minor */
#define ILYA_GCPMINORMAJOR	2  /* control shift minor->major */

/* parameters for incremental mode */
#define ILYA_GCPPAUSE		3  /* size of pause between successive GCs */
#define ILYA_GCPSTEPMUL		4  /* GC "speed" */
#define ILYA_GCPSTEPSIZE		5  /* GC granularity */

/* number of parameters */
#define ILYA_GCPN		6


ILYA_API int (ilya_gc) (ilya_State *L, int what, ...);


/*
** miscellaneous functions
*/

ILYA_API int   (ilya_error) (ilya_State *L);

ILYA_API int   (ilya_next) (ilya_State *L, int idx);

ILYA_API void  (ilya_concat) (ilya_State *L, int n);
ILYA_API void  (ilya_len)    (ilya_State *L, int idx);

#define ILYA_N2SBUFFSZ	64
ILYA_API unsigned  (ilya_numbertocstring) (ilya_State *L, int idx, char *buff);
ILYA_API size_t  (ilya_stringtonumber) (ilya_State *L, const char *s);

ILYA_API ilya_Alloc (ilya_getallocf) (ilya_State *L, void **ud);
ILYA_API void      (ilya_setallocf) (ilya_State *L, ilya_Alloc f, void *ud);

ILYA_API void (ilya_toclose) (ilya_State *L, int idx);
ILYA_API void (ilya_closeslot) (ilya_State *L, int idx);


/*
** {==============================================================
** some useful macros
** ===============================================================
*/

#define ilya_getextraspace(L)	((void *)((char *)(L) - ILYA_EXTRASPACE))

#define ilya_tonumber(L,i)	ilya_tonumberx(L,(i),NULL)
#define ilya_tointeger(L,i)	ilya_tointegerx(L,(i),NULL)

#define ilya_pop(L,n)		ilya_settop(L, -(n)-1)

#define ilya_newtable(L)		ilya_createtable(L, 0, 0)

#define ilya_register(L,n,f) (ilya_pushcfunction(L, (f)), ilya_setglobal(L, (n)))

#define ilya_pushcfunction(L,f)	ilya_pushcclosure(L, (f), 0)

#define ilya_isfunction(L,n)	(ilya_type(L, (n)) == ILYA_TFUNCTION)
#define ilya_istable(L,n)	(ilya_type(L, (n)) == ILYA_TTABLE)
#define ilya_islightuserdata(L,n)	(ilya_type(L, (n)) == ILYA_TLIGHTUSERDATA)
#define ilya_isnil(L,n)		(ilya_type(L, (n)) == ILYA_TNIL)
#define ilya_isboolean(L,n)	(ilya_type(L, (n)) == ILYA_TBOOLEAN)
#define ilya_isthread(L,n)	(ilya_type(L, (n)) == ILYA_TTHREAD)
#define ilya_isnone(L,n)		(ilya_type(L, (n)) == ILYA_TNONE)
#define ilya_isnoneornil(L, n)	(ilya_type(L, (n)) <= 0)

#define ilya_pushliteral(L, s)	ilya_pushstring(L, "" s)

#define ilya_pushglobaltable(L)  \
	((void)ilya_rawgeti(L, ILYA_REGISTRYINDEX, ILYA_RIDX_GLOBALS))

#define ilya_tostring(L,i)	ilya_tolstring(L, (i), NULL)


#define ilya_insert(L,idx)	ilya_rotate(L, (idx), 1)

#define ilya_remove(L,idx)	(ilya_rotate(L, (idx), -1), ilya_pop(L, 1))

#define ilya_replace(L,idx)	(ilya_copy(L, -1, (idx)), ilya_pop(L, 1))

/* }============================================================== */


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/
#if defined(ILYA_COMPAT_APIINTCASTS)

#define ilya_pushunsigned(L,n)	ilya_pushinteger(L, (ilya_Integer)(n))
#define ilya_tounsignedx(L,i,is)	((ilya_Unsigned)ilya_tointegerx(L,i,is))
#define ilya_tounsigned(L,i)	ilya_tounsignedx(L,(i),NULL)

#endif

#define ilya_newuserdata(L,s)	ilya_newuserdatauv(L,s,1)
#define ilya_getuservalue(L,idx)	ilya_getiuservalue(L,idx,1)
#define ilya_setuservalue(L,idx)	ilya_setiuservalue(L,idx,1)

#define ilya_resetthread(L)	ilya_closethread(L,NULL)

/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define ILYA_HOOKCALL	0
#define ILYA_HOOKRET	1
#define ILYA_HOOKLINE	2
#define ILYA_HOOKCOUNT	3
#define ILYA_HOOKTAILCALL 4


/*
** Event masks
*/
#define ILYA_MASKCALL	(1 << ILYA_HOOKCALL)
#define ILYA_MASKRET	(1 << ILYA_HOOKRET)
#define ILYA_MASKLINE	(1 << ILYA_HOOKLINE)
#define ILYA_MASKCOUNT	(1 << ILYA_HOOKCOUNT)


ILYA_API int (ilya_getstack) (ilya_State *L, int level, ilya_Debug *ar);
ILYA_API int (ilya_getinfo) (ilya_State *L, const char *what, ilya_Debug *ar);
ILYA_API const char *(ilya_getlocal) (ilya_State *L, const ilya_Debug *ar, int n);
ILYA_API const char *(ilya_setlocal) (ilya_State *L, const ilya_Debug *ar, int n);
ILYA_API const char *(ilya_getupvalue) (ilya_State *L, int funcindex, int n);
ILYA_API const char *(ilya_setupvalue) (ilya_State *L, int funcindex, int n);

ILYA_API void *(ilya_upvalueid) (ilya_State *L, int fidx, int n);
ILYA_API void  (ilya_upvaluejoin) (ilya_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

ILYA_API void (ilya_sethook) (ilya_State *L, ilya_Hook func, int mask, int count);
ILYA_API ilya_Hook (ilya_gethook) (ilya_State *L);
ILYA_API int (ilya_gethookmask) (ilya_State *L);
ILYA_API int (ilya_gethookcount) (ilya_State *L);


struct ilya_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'lock', 'field', 'method' */
  const char *what;	/* (S) 'Ilya', 'C', 'main', 'tail' */
  const char *source;	/* (S) */
  size_t srclen;	/* (S) */
  int currentline;	/* (l) */
  int linedefined;	/* (S) */
  int lastlinedefined;	/* (S) */
  unsigned char nups;	/* (u) number of upvalues */
  unsigned char nparams;/* (u) number of parameters */
  char isvararg;        /* (u) */
  unsigned char extraargs;  /* (t) number of extra arguments */
  char istailcall;	/* (t) */
  int ftransfer;   /* (r) index of first value transferred */
  int ntransfer;   /* (r) number of transferred values */
  char short_src[ILYA_IDSIZE]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active fn */
};

/* }====================================================================== */


#define LUAI_TOSTRAUX(x)	#x
#define LUAI_TOSTR(x)		LUAI_TOSTRAUX(x)

#define ILYA_VERSION_MAJOR	LUAI_TOSTR(ILYA_VERSION_MAJOR_N)
#define ILYA_VERSION_MINOR	LUAI_TOSTR(ILYA_VERSION_MINOR_N)
#define ILYA_VERSION_RELEASE	LUAI_TOSTR(ILYA_VERSION_RELEASE_N)

#define ILYA_VERSION	"Ilya " ILYA_VERSION_MAJOR "." ILYA_VERSION_MINOR
#define ILYA_RELEASE	ILYA_VERSION "." ILYA_VERSION_RELEASE


/******************************************************************************
* Copyright (C) 2025 github.com/nginrsw/ilya, KRS-Bdg.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/


#endif
