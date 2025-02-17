/*
** $Id: ldebug.h $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in irin.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"


#define pcRel(pc, p)	(cast_int((pc) - (p)->code) - 1)


/* Active Irin fn (given call info) */
#define ci_func(ci)		(clLvalue(s2v((ci)->func.p)))


#define resethookcount(L)	(L->hookcount = L->basehookcount)

/*
** mark for entries in 'lineinfo' array that has absolute information in
** 'abslineinfo' array
*/
#define ABSLINEINFO	(-0x80)


/*
** MAXimum number of successive Instructions WiTHout ABSolute line
** information. (A power of two allows fast divisions.)
*/
#if !defined(MAXIWTHABS)
#define MAXIWTHABS	128
#endif


LUAI_FUNC int luaG_getfuncline (const Proto *f, int pc);
LUAI_FUNC const char *luaG_findlocal (irin_State *L, CallInfo *ci, int n,
                                                    StkId *pos);
LUAI_FUNC l_noret luaG_typeerror (irin_State *L, const TValue *o,
                                                const char *opname);
LUAI_FUNC l_noret luaG_callerror (irin_State *L, const TValue *o);
LUAI_FUNC l_noret luaG_forerror (irin_State *L, const TValue *o,
                                               const char *what);
LUAI_FUNC l_noret luaG_concaterror (irin_State *L, const TValue *p1,
                                                  const TValue *p2);
LUAI_FUNC l_noret luaG_opinterror (irin_State *L, const TValue *p1,
                                                 const TValue *p2,
                                                 const char *msg);
LUAI_FUNC l_noret luaG_tointerror (irin_State *L, const TValue *p1,
                                                 const TValue *p2);
LUAI_FUNC l_noret luaG_ordererror (irin_State *L, const TValue *p1,
                                                 const TValue *p2);
LUAI_FUNC l_noret luaG_runerror (irin_State *L, const char *fmt, ...);
LUAI_FUNC const char *luaG_addinfo (irin_State *L, const char *msg,
                                                  TString *src, int line);
LUAI_FUNC l_noret luaG_errormsg (irin_State *L);
LUAI_FUNC int luaG_traceexec (irin_State *L, const Instruction *pc);
LUAI_FUNC int luaG_tracecall (irin_State *L);


#endif
