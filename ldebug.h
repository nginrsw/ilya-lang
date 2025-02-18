/*
** $Id: ldebug.h $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in ilya.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"


#define pcRel(pc, p)	(cast_int((pc) - (p)->code) - 1)


/* Active Ilya fn (given call info) */
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


ILYAI_FUNC int ilyaG_getfuncline (const Proto *f, int pc);
ILYAI_FUNC const char *ilyaG_findlocal (ilya_State *L, CallInfo *ci, int n,
                                                    StkId *pos);
ILYAI_FUNC l_noret ilyaG_typeerror (ilya_State *L, const TValue *o,
                                                const char *opname);
ILYAI_FUNC l_noret ilyaG_callerror (ilya_State *L, const TValue *o);
ILYAI_FUNC l_noret ilyaG_forerror (ilya_State *L, const TValue *o,
                                               const char *what);
ILYAI_FUNC l_noret ilyaG_concaterror (ilya_State *L, const TValue *p1,
                                                  const TValue *p2);
ILYAI_FUNC l_noret ilyaG_opinterror (ilya_State *L, const TValue *p1,
                                                 const TValue *p2,
                                                 const char *msg);
ILYAI_FUNC l_noret ilyaG_tointerror (ilya_State *L, const TValue *p1,
                                                 const TValue *p2);
ILYAI_FUNC l_noret ilyaG_ordererror (ilya_State *L, const TValue *p1,
                                                 const TValue *p2);
ILYAI_FUNC l_noret ilyaG_runerror (ilya_State *L, const char *fmt, ...);
ILYAI_FUNC const char *ilyaG_addinfo (ilya_State *L, const char *msg,
                                                  TString *src, int line);
ILYAI_FUNC l_noret ilyaG_errormsg (ilya_State *L);
ILYAI_FUNC int ilyaG_traceexec (ilya_State *L, const Instruction *pc);
ILYAI_FUNC int ilyaG_tracecall (ilya_State *L);


#endif
