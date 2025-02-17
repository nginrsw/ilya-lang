/*
** $Id: irin.h $
** Irin - A Scripting Language
** github.com/nginrsw, KRS-Bdg, Indonesia
** See Copyright Notice at the end of this file
*/


#ifndef irin_h
#define irin_h

#include <stdarg.h>
#include <stddef.h>


#define IRIN_COPYRIGHT	IRIN_RELEASE "  Copyright (C) 2025 Irin.org, KRS-Bdg"
#define IRIN_AUTHORS	"Gillar Ajie Prasatya"


#define IRIN_VERSION_MAJOR_N	0
#define IRIN_VERSION_MINOR_N	0
#define IRIN_VERSION_RELEASE_N	1

#define IRIN_VERSION_NUM  (IRIN_VERSION_MAJOR_N * 100 + IRIN_VERSION_MINOR_N)
#define IRIN_VERSION_RELEASE_NUM  (IRIN_VERSION_NUM * 100 + IRIN_VERSION_RELEASE_N)


#include "irinconf.h"


/* mark for precompiled code ('<esc>Irin') */
#define IRIN_SIGNATURE	"\x1bLua"

/* option for multiple returns in 'irin_pcall' and 'irin_call' */
#define IRIN_MULTRET	(-1)


/*
** Pseudo-indices
** (-LUAI_MAXSTACK is the minimum valid index; we keep some free empty
** space after that to help overflow detection)
*/
#define IRIN_REGISTRYINDEX	(-LUAI_MAXSTACK - 1000)
#define irin_upvalueindex(i)	(IRIN_REGISTRYINDEX - (i))


/* thread status */
#define IRIN_OK		0
#define IRIN_YIELD	1
#define IRIN_ERRRUN	2
#define IRIN_ERRSYNTAX	3
#define IRIN_ERRMEM	4
#define IRIN_ERRERR	5


typedef struct irin_State irin_State;


/*
** basic types
*/
#define IRIN_TNONE		(-1)

#define IRIN_TNIL		0
#define IRIN_TBOOLEAN		1
#define IRIN_TLIGHTUSERDATA	2
#define IRIN_TNUMBER		3
#define IRIN_TSTRING		4
#define IRIN_TTABLE		5
#define IRIN_TFUNCTION		6
#define IRIN_TUSERDATA		7
#define IRIN_TTHREAD		8

#define IRIN_NUMTYPES		9



/* minimum Irin stack available to a C fn */
#define IRIN_MINSTACK	20


/* predefined values in the registry */
/* index 1 is reserved for the reference mechanism */
#define IRIN_RIDX_GLOBALS	2
#define IRIN_RIDX_MAINTHREAD	3
#define IRIN_RIDX_LAST		3


/* type of numbers in Irin */
typedef IRIN_NUMBER irin_Number;


/* type for integer functions */
typedef IRIN_INTEGER irin_Integer;

/* unsigned integer type */
typedef IRIN_UNSIGNED irin_Unsigned;

/* type for continuation-fn contexts */
typedef IRIN_KCONTEXT irin_KContext;


/*
** Type for C functions registered with Irin
*/
typedef int (*irin_CFunction) (irin_State *L);

/*
** Type for continuation functions
*/
typedef int (*irin_KFunction) (irin_State *L, int status, irin_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping Irin chunks
*/
typedef const char * (*irin_Reader) (irin_State *L, void *ud, size_t *sz);

typedef int (*irin_Writer) (irin_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*irin_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*irin_WarnFunction) (void *ud, const char *msg, int tocont);


/*
** Type used by the debug API to collect debug information
*/
typedef struct irin_Debug irin_Debug;


/*
** Functions to be called by the debugger in specific events
*/
typedef void (*irin_Hook) (irin_State *L, irin_Debug *ar);


/*
** generic extra include file
*/
#if defined(IRIN_USER_H)
#include IRIN_USER_H
#endif


/*
** RCS ident string
*/
extern const char irin_ident[];


/*
** state manipulation
*/
IRIN_API irin_State *(irin_newstate) (irin_Alloc f, void *ud, unsigned seed);
IRIN_API void       (irin_close) (irin_State *L);
IRIN_API irin_State *(irin_newthread) (irin_State *L);
IRIN_API int        (irin_closethread) (irin_State *L, irin_State *from);

IRIN_API irin_CFunction (irin_atpanic) (irin_State *L, irin_CFunction panicf);


IRIN_API irin_Number (irin_version) (irin_State *L);


/*
** basic stack manipulation
*/
IRIN_API int   (irin_absindex) (irin_State *L, int idx);
IRIN_API int   (irin_gettop) (irin_State *L);
IRIN_API void  (irin_settop) (irin_State *L, int idx);
IRIN_API void  (irin_pushvalue) (irin_State *L, int idx);
IRIN_API void  (irin_rotate) (irin_State *L, int idx, int n);
IRIN_API void  (irin_copy) (irin_State *L, int fromidx, int toidx);
IRIN_API int   (irin_checkstack) (irin_State *L, int n);

IRIN_API void  (irin_xmove) (irin_State *from, irin_State *to, int n);


/*
** access functions (stack -> C)
*/

IRIN_API int             (irin_isnumber) (irin_State *L, int idx);
IRIN_API int             (irin_isstring) (irin_State *L, int idx);
IRIN_API int             (irin_iscfunction) (irin_State *L, int idx);
IRIN_API int             (irin_isinteger) (irin_State *L, int idx);
IRIN_API int             (irin_isuserdata) (irin_State *L, int idx);
IRIN_API int             (irin_type) (irin_State *L, int idx);
IRIN_API const char     *(irin_typename) (irin_State *L, int tp);

IRIN_API irin_Number      (irin_tonumberx) (irin_State *L, int idx, int *isnum);
IRIN_API irin_Integer     (irin_tointegerx) (irin_State *L, int idx, int *isnum);
IRIN_API int             (irin_toboolean) (irin_State *L, int idx);
IRIN_API const char     *(irin_tolstring) (irin_State *L, int idx, size_t *len);
IRIN_API irin_Unsigned    (irin_rawlen) (irin_State *L, int idx);
IRIN_API irin_CFunction   (irin_tocfunction) (irin_State *L, int idx);
IRIN_API void	       *(irin_touserdata) (irin_State *L, int idx);
IRIN_API irin_State      *(irin_tothread) (irin_State *L, int idx);
IRIN_API const void     *(irin_topointer) (irin_State *L, int idx);


/*
** Comparison and arithmetic functions
*/

#define IRIN_OPADD	0	/* ORDER TM, ORDER OP */
#define IRIN_OPSUB	1
#define IRIN_OPMUL	2
#define IRIN_OPMOD	3
#define IRIN_OPPOW	4
#define IRIN_OPDIV	5
#define IRIN_OPIDIV	6
#define IRIN_OPBAND	7
#define IRIN_OPBOR	8
#define IRIN_OPBXOR	9
#define IRIN_OPSHL	10
#define IRIN_OPSHR	11
#define IRIN_OPUNM	12
#define IRIN_OPBNOT	13

IRIN_API void  (irin_arith) (irin_State *L, int op);

#define IRIN_OPEQ	0
#define IRIN_OPLT	1
#define IRIN_OPLE	2

IRIN_API int   (irin_rawequal) (irin_State *L, int idx1, int idx2);
IRIN_API int   (irin_compare) (irin_State *L, int idx1, int idx2, int op);


/*
** push functions (C -> stack)
*/
IRIN_API void        (irin_pushnil) (irin_State *L);
IRIN_API void        (irin_pushnumber) (irin_State *L, irin_Number n);
IRIN_API void        (irin_pushinteger) (irin_State *L, irin_Integer n);
IRIN_API const char *(irin_pushlstring) (irin_State *L, const char *s, size_t len);
IRIN_API const char *(irin_pushexternalstring) (irin_State *L,
		const char *s, size_t len, irin_Alloc falloc, void *ud);
IRIN_API const char *(irin_pushstring) (irin_State *L, const char *s);
IRIN_API const char *(irin_pushvfstring) (irin_State *L, const char *fmt,
                                                      va_list argp);
IRIN_API const char *(irin_pushfstring) (irin_State *L, const char *fmt, ...);
IRIN_API void  (irin_pushcclosure) (irin_State *L, irin_CFunction fn, int n);
IRIN_API void  (irin_pushboolean) (irin_State *L, int b);
IRIN_API void  (irin_pushlightuserdata) (irin_State *L, void *p);
IRIN_API int   (irin_pushthread) (irin_State *L);


/*
** get functions (Irin -> stack)
*/
IRIN_API int (irin_getglobal) (irin_State *L, const char *name);
IRIN_API int (irin_gettable) (irin_State *L, int idx);
IRIN_API int (irin_getfield) (irin_State *L, int idx, const char *k);
IRIN_API int (irin_geti) (irin_State *L, int idx, irin_Integer n);
IRIN_API int (irin_rawget) (irin_State *L, int idx);
IRIN_API int (irin_rawgeti) (irin_State *L, int idx, irin_Integer n);
IRIN_API int (irin_rawgetp) (irin_State *L, int idx, const void *p);

IRIN_API void  (irin_createtable) (irin_State *L, int narr, int nrec);
IRIN_API void *(irin_newuserdatauv) (irin_State *L, size_t sz, int nuvalue);
IRIN_API int   (irin_getmetatable) (irin_State *L, int objindex);
IRIN_API int  (irin_getiuservalue) (irin_State *L, int idx, int n);


/*
** set functions (stack -> Irin)
*/
IRIN_API void  (irin_setglobal) (irin_State *L, const char *name);
IRIN_API void  (irin_settable) (irin_State *L, int idx);
IRIN_API void  (irin_setfield) (irin_State *L, int idx, const char *k);
IRIN_API void  (irin_seti) (irin_State *L, int idx, irin_Integer n);
IRIN_API void  (irin_rawset) (irin_State *L, int idx);
IRIN_API void  (irin_rawseti) (irin_State *L, int idx, irin_Integer n);
IRIN_API void  (irin_rawsetp) (irin_State *L, int idx, const void *p);
IRIN_API int   (irin_setmetatable) (irin_State *L, int objindex);
IRIN_API int   (irin_setiuservalue) (irin_State *L, int idx, int n);


/*
** 'load' and 'call' functions (load and run Irin code)
*/
IRIN_API void  (irin_callk) (irin_State *L, int nargs, int nresults,
                           irin_KContext ctx, irin_KFunction k);
#define irin_call(L,n,r)		irin_callk(L, (n), (r), 0, NULL)

IRIN_API int   (irin_pcallk) (irin_State *L, int nargs, int nresults, int errfunc,
                            irin_KContext ctx, irin_KFunction k);
#define irin_pcall(L,n,r,f)	irin_pcallk(L, (n), (r), (f), 0, NULL)

IRIN_API int   (irin_load) (irin_State *L, irin_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

IRIN_API int (irin_dump) (irin_State *L, irin_Writer writer, void *data, int strip);


/*
** coroutine functions
*/
IRIN_API int  (irin_yieldk)     (irin_State *L, int nresults, irin_KContext ctx,
                               irin_KFunction k);
IRIN_API int  (irin_resume)     (irin_State *L, irin_State *from, int narg,
                               int *nres);
IRIN_API int  (irin_status)     (irin_State *L);
IRIN_API int (irin_isyieldable) (irin_State *L);

#define irin_yield(L,n)		irin_yieldk(L, (n), 0, NULL)


/*
** Warning-related functions
*/
IRIN_API void (irin_setwarnf) (irin_State *L, irin_WarnFunction f, void *ud);
IRIN_API void (irin_warning)  (irin_State *L, const char *msg, int tocont);


/*
** garbage-collection options
*/

#define IRIN_GCSTOP		0
#define IRIN_GCRESTART		1
#define IRIN_GCCOLLECT		2
#define IRIN_GCCOUNT		3
#define IRIN_GCCOUNTB		4
#define IRIN_GCSTEP		5
#define IRIN_GCISRUNNING		6
#define IRIN_GCGEN		7
#define IRIN_GCINC		8
#define IRIN_GCPARAM		9


/*
** garbage-collection parameters
*/
/* parameters for generational mode */
#define IRIN_GCPMINORMUL		0  /* control minor collections */
#define IRIN_GCPMAJORMINOR	1  /* control shift major->minor */
#define IRIN_GCPMINORMAJOR	2  /* control shift minor->major */

/* parameters for incremental mode */
#define IRIN_GCPPAUSE		3  /* size of pause between successive GCs */
#define IRIN_GCPSTEPMUL		4  /* GC "speed" */
#define IRIN_GCPSTEPSIZE		5  /* GC granularity */

/* number of parameters */
#define IRIN_GCPN		6


IRIN_API int (irin_gc) (irin_State *L, int what, ...);


/*
** miscellaneous functions
*/

IRIN_API int   (irin_error) (irin_State *L);

IRIN_API int   (irin_next) (irin_State *L, int idx);

IRIN_API void  (irin_concat) (irin_State *L, int n);
IRIN_API void  (irin_len)    (irin_State *L, int idx);

#define IRIN_N2SBUFFSZ	64
IRIN_API unsigned  (irin_numbertocstring) (irin_State *L, int idx, char *buff);
IRIN_API size_t  (irin_stringtonumber) (irin_State *L, const char *s);

IRIN_API irin_Alloc (irin_getallocf) (irin_State *L, void **ud);
IRIN_API void      (irin_setallocf) (irin_State *L, irin_Alloc f, void *ud);

IRIN_API void (irin_toclose) (irin_State *L, int idx);
IRIN_API void (irin_closeslot) (irin_State *L, int idx);


/*
** {==============================================================
** some useful macros
** ===============================================================
*/

#define irin_getextraspace(L)	((void *)((char *)(L) - IRIN_EXTRASPACE))

#define irin_tonumber(L,i)	irin_tonumberx(L,(i),NULL)
#define irin_tointeger(L,i)	irin_tointegerx(L,(i),NULL)

#define irin_pop(L,n)		irin_settop(L, -(n)-1)

#define irin_newtable(L)		irin_createtable(L, 0, 0)

#define irin_register(L,n,f) (irin_pushcfunction(L, (f)), irin_setglobal(L, (n)))

#define irin_pushcfunction(L,f)	irin_pushcclosure(L, (f), 0)

#define irin_isfunction(L,n)	(irin_type(L, (n)) == IRIN_TFUNCTION)
#define irin_istable(L,n)	(irin_type(L, (n)) == IRIN_TTABLE)
#define irin_islightuserdata(L,n)	(irin_type(L, (n)) == IRIN_TLIGHTUSERDATA)
#define irin_isnil(L,n)		(irin_type(L, (n)) == IRIN_TNIL)
#define irin_isboolean(L,n)	(irin_type(L, (n)) == IRIN_TBOOLEAN)
#define irin_isthread(L,n)	(irin_type(L, (n)) == IRIN_TTHREAD)
#define irin_isnone(L,n)		(irin_type(L, (n)) == IRIN_TNONE)
#define irin_isnoneornil(L, n)	(irin_type(L, (n)) <= 0)

#define irin_pushliteral(L, s)	irin_pushstring(L, "" s)

#define irin_pushglobaltable(L)  \
	((void)irin_rawgeti(L, IRIN_REGISTRYINDEX, IRIN_RIDX_GLOBALS))

#define irin_tostring(L,i)	irin_tolstring(L, (i), NULL)


#define irin_insert(L,idx)	irin_rotate(L, (idx), 1)

#define irin_remove(L,idx)	(irin_rotate(L, (idx), -1), irin_pop(L, 1))

#define irin_replace(L,idx)	(irin_copy(L, -1, (idx)), irin_pop(L, 1))

/* }============================================================== */


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/
#if defined(IRIN_COMPAT_APIINTCASTS)

#define irin_pushunsigned(L,n)	irin_pushinteger(L, (irin_Integer)(n))
#define irin_tounsignedx(L,i,is)	((irin_Unsigned)irin_tointegerx(L,i,is))
#define irin_tounsigned(L,i)	irin_tounsignedx(L,(i),NULL)

#endif

#define irin_newuserdata(L,s)	irin_newuserdatauv(L,s,1)
#define irin_getuservalue(L,idx)	irin_getiuservalue(L,idx,1)
#define irin_setuservalue(L,idx)	irin_setiuservalue(L,idx,1)

#define irin_resetthread(L)	irin_closethread(L,NULL)

/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define IRIN_HOOKCALL	0
#define IRIN_HOOKRET	1
#define IRIN_HOOKLINE	2
#define IRIN_HOOKCOUNT	3
#define IRIN_HOOKTAILCALL 4


/*
** Event masks
*/
#define IRIN_MASKCALL	(1 << IRIN_HOOKCALL)
#define IRIN_MASKRET	(1 << IRIN_HOOKRET)
#define IRIN_MASKLINE	(1 << IRIN_HOOKLINE)
#define IRIN_MASKCOUNT	(1 << IRIN_HOOKCOUNT)


IRIN_API int (irin_getstack) (irin_State *L, int level, irin_Debug *ar);
IRIN_API int (irin_getinfo) (irin_State *L, const char *what, irin_Debug *ar);
IRIN_API const char *(irin_getlocal) (irin_State *L, const irin_Debug *ar, int n);
IRIN_API const char *(irin_setlocal) (irin_State *L, const irin_Debug *ar, int n);
IRIN_API const char *(irin_getupvalue) (irin_State *L, int funcindex, int n);
IRIN_API const char *(irin_setupvalue) (irin_State *L, int funcindex, int n);

IRIN_API void *(irin_upvalueid) (irin_State *L, int fidx, int n);
IRIN_API void  (irin_upvaluejoin) (irin_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

IRIN_API void (irin_sethook) (irin_State *L, irin_Hook func, int mask, int count);
IRIN_API irin_Hook (irin_gethook) (irin_State *L);
IRIN_API int (irin_gethookmask) (irin_State *L);
IRIN_API int (irin_gethookcount) (irin_State *L);


struct irin_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'locked', 'field', 'method' */
  const char *what;	/* (S) 'Irin', 'C', 'main', 'tail' */
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
  char short_src[IRIN_IDSIZE]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active fn */
};

/* }====================================================================== */


#define LUAI_TOSTRAUX(x)	#x
#define LUAI_TOSTR(x)		LUAI_TOSTRAUX(x)

#define IRIN_VERSION_MAJOR	LUAI_TOSTR(IRIN_VERSION_MAJOR_N)
#define IRIN_VERSION_MINOR	LUAI_TOSTR(IRIN_VERSION_MINOR_N)
#define IRIN_VERSION_RELEASE	LUAI_TOSTR(IRIN_VERSION_RELEASE_N)

#define IRIN_VERSION	"Irin " IRIN_VERSION_MAJOR "." IRIN_VERSION_MINOR
#define IRIN_RELEASE	IRIN_VERSION "." IRIN_VERSION_RELEASE


/******************************************************************************
* Copyright (C) 2025 github.com/nginrsw/irin, KRS-Bdg.
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
