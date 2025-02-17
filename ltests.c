/*
** $Id: ltests.c $
** Internal Module for Debugging of the Ilya Implementation
** See Copyright Notice in ilya.h
*/

#define ltests_c
#define ILYA_CORE

#include "lprefix.h"


#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ilya.h"

#include "lapi.h"
#include "lauxlib.h"
#include "lcode.h"
#include "lctype.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lopcodes.h"
#include "lopnames.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ilyalib.h"



/*
** The whole module only makes sense with ILYA_DEBUG on
*/
#if defined(ILYA_DEBUG)


void *l_Trick = 0;


#define obj_at(L,k)	s2v(L->ci->func.p + (k))


static int runC (ilya_State *L, ilya_State *L1, const char *pc);


static void setnameval (ilya_State *L, const char *name, int val) {
  ilya_pushinteger(L, val);
  ilya_setfield(L, -2, name);
}


static void pushobject (ilya_State *L, const TValue *o) {
  setobj2s(L, L->top.p, o);
  api_incr_top(L);
}


static void badexit (const char *fmt, const char *s1, const char *s2) {
  fprintf(stderr, fmt, s1);
  if (s2)
    fprintf(stderr, "extra info: %s\n", s2);
  /* avoid assertion failures when exiting */
  l_memcontrol.numblocks = l_memcontrol.total = 0;
  exit(EXIT_FAILURE);
}


static int tpanic (ilya_State *L) {
  const char *msg = (ilya_type(L, -1) == ILYA_TSTRING)
                  ? ilya_tostring(L, -1)
                  : "error object is not a string";
  return (badexit("PANIC: unprotected error in call to Ilya API (%s)\n",
                   msg, NULL),
          0);  /* do not return to Ilya */
}


/*
** Warning fn for tests. First, it concatenates all parts of
** a warning in buffer 'buff'. Then, it has three modes:
** - 0.normal: messages starting with '#' are shown on standard output;
** - other messages abort the tests (they represent real warning
** conditions; the standard tests should not generate these conditions
** unexpectedly);
** - 1.allow: all messages are shown;
** - 2.store: all warnings go to the global '_WARN';
*/
static void warnf (void *ud, const char *msg, int tocont) {
  ilya_State *L = cast(ilya_State *, ud);
  static char buff[200] = "";  /* should be enough for tests... */
  static int onoff = 0;
  static int mode = 0;  /* start in normal mode */
  static int lasttocont = 0;
  if (!lasttocont && !tocont && *msg == '@') {  /* control message? */
    if (buff[0] != '\0')
      badexit("Control warning during warning: %s\naborting...\n", msg, buff);
    if (strcmp(msg, "@off") == 0)
      onoff = 0;
    else if (strcmp(msg, "@on") == 0)
      onoff = 1;
    else if (strcmp(msg, "@normal") == 0)
      mode = 0;
    else if (strcmp(msg, "@allow") == 0)
      mode = 1;
    else if (strcmp(msg, "@store") == 0)
      mode = 2;
    else
      badexit("Invalid control warning in test mode: %s\naborting...\n",
              msg, NULL);
    return;
  }
  lasttocont = tocont;
  if (strlen(msg) >= sizeof(buff) - strlen(buff))
    badexit("warnf-buffer overflow (%s)\n", msg, buff);
  strcat(buff, msg);  /* add new message to current warning */
  if (!tocont) {  /* message finished? */
    ilya_unlock(L);
    luaL_checkstack(L, 1, "warn stack space");
    ilya_getglobal(L, "_WARN");
    if (!ilya_toboolean(L, -1))
      ilya_pop(L, 1);  /* ok, no previous unexpected warning */
    else {
      badexit("Unhandled warning in store mode: %s\naborting...\n",
              ilya_tostring(L, -1), buff);
    }
    ilya_lock(L);
    switch (mode) {
      case 0: {  /* normal */
        if (buff[0] != '#' && onoff)  /* unexpected warning? */
          badexit("Unexpected warning in test mode: %s\naborting...\n",
                  buff, NULL);
      }  /* FALLTHROUGH */
      case 1: {  /* allow */
        if (onoff)
          fprintf(stderr, "Ilya warning: %s\n", buff);  /* print warning */
        break;
      }
      case 2: {  /* store */
        ilya_unlock(L);
        luaL_checkstack(L, 1, "warn stack space");
        ilya_pushstring(L, buff);
        ilya_setglobal(L, "_WARN");  /* assign message to global '_WARN' */
        ilya_lock(L);
        break;
      }
    }
    buff[0] = '\0';  /* prepare buffer for next warning */
  }
}


/*
** {======================================================================
** Controlled version for realloc.
** =======================================================================
*/

#define MARK		0x55  /* 01010101 (a nice pattern) */

typedef union Header {
  LUAI_MAXALIGN;
  struct {
    size_t size;
    int type;
  } d;
} Header;


#if !defined(EXTERNMEMCHECK)

/* full memory check */
#define MARKSIZE	16  /* size of marks after each block */
#define fillmem(mem,size)	memset(mem, -MARK, size)

#else

/* external memory check: don't do it twice */
#define MARKSIZE	0
#define fillmem(mem,size)	/* empty */

#endif


Memcontrol l_memcontrol =
  {0, 0UL, 0UL, 0UL, 0UL, (~0UL),
   {0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL}};


static void freeblock (Memcontrol *mc, Header *block) {
  if (block) {
    size_t size = block->d.size;
    int i;
    for (i = 0; i < MARKSIZE; i++)  /* check marks after block */
      ilya_assert(*(cast_charp(block + 1) + size + i) == MARK);
    mc->objcount[block->d.type]--;
    fillmem(block, sizeof(Header) + size + MARKSIZE);  /* erase block */
    free(block);  /* actually free block */
    mc->numblocks--;  /* update counts */
    mc->total -= size;
  }
}


void *debug_realloc (void *ud, void *b, size_t oldsize, size_t size) {
  Memcontrol *mc = cast(Memcontrol *, ud);
  Header *block = cast(Header *, b);
  int type;
  if (mc->memlimit == 0) {  /* first time? */
    char *limit = getenv("MEMLIMIT");  /* initialize memory limit */
    mc->memlimit = limit ? strtoul(limit, NULL, 10) : ULONG_MAX;
  }
  if (block == NULL) {
    type = (oldsize < ILYA_NUMTYPES) ? cast_int(oldsize) : 0;
    oldsize = 0;
  }
  else {
    block--;  /* go to real header */
    type = block->d.type;
    ilya_assert(oldsize == block->d.size);
  }
  if (size == 0) {
    freeblock(mc, block);
    return NULL;
  }
  if (mc->failnext) {
    mc->failnext = 0;
    return NULL;  /* fake a single memory allocation error */
  }
  if (mc->countlimit != ~0UL && size != oldsize) {  /* count limit in use? */
    if (mc->countlimit == 0)
      return NULL;  /* fake a memory allocation error */
    mc->countlimit--;
  }
  if (size > oldsize && mc->total+size-oldsize > mc->memlimit)
    return NULL;  /* fake a memory allocation error */
  else {
    Header *newblock;
    int i;
    size_t commonsize = (oldsize < size) ? oldsize : size;
    size_t realsize = sizeof(Header) + size + MARKSIZE;
    if (realsize < size) return NULL;  /* arithmetic overflow! */
    newblock = cast(Header *, malloc(realsize));  /* alloc a new block */
    if (newblock == NULL)
      return NULL;  /* really out of memory? */
    if (block) {
      memcpy(newblock + 1, block + 1, commonsize);  /* copy old contents */
      freeblock(mc, block);  /* erase (and check) old copy */
    }
    /* initialize new part of the block with something weird */
    fillmem(cast_charp(newblock + 1) + commonsize, size - commonsize);
    /* initialize marks after block */
    for (i = 0; i < MARKSIZE; i++)
      *(cast_charp(newblock + 1) + size + i) = MARK;
    newblock->d.size = size;
    newblock->d.type = type;
    mc->total += size;
    if (mc->total > mc->maxmem)
      mc->maxmem = mc->total;
    mc->numblocks++;
    mc->objcount[type]++;
    return newblock + 1;
  }
}


/* }====================================================================== */



/*
** {=====================================================================
** Functions to check memory consistency.
** Most of these checks are done through asserts, so this code does
** not make sense with asserts off. For this reason, it uses 'assert'
** directly, instead of 'ilya_assert'.
** ======================================================================
*/

#include <assert.h>

/*
** Check GC invariants. For incremental mode, a black object cannot
** point to a white one. For generational mode, really old objects
** cannot point to young objects. Both old1 and touched2 objects
** cannot point to new objects (but can point to survivals).
** (Threads and open upvalues, despite being marked "really old",
** continue to be visited in all collections, and therefore can point to
** new objects. They, and only they, are old but gray.)
*/
static int testobjref1 (global_State *g, GCObject *f, GCObject *t) {
  if (isdead(g,t)) return 0;
  if (issweepphase(g))
    return 1;  /* no invariants */
  else if (g->gckind != KGC_GENMINOR)
    return !(isblack(f) && iswhite(t));  /* basic incremental invariant */
  else {  /* generational mode */
    if ((getage(f) == G_OLD && isblack(f)) && !isold(t))
      return 0;
    if ((getage(f) == G_OLD1 || getage(f) == G_TOUCHED2) &&
         getage(t) == G_NEW)
      return 0;
    return 1;
  }
}


static void printobj (global_State *g, GCObject *o) {
  printf("||%s(%p)-%c%c(%02X)||",
           ttypename(novariant(o->tt)), (void *)o,
           isdead(g,o) ? 'd' : isblack(o) ? 'b' : iswhite(o) ? 'w' : 'g',
           "ns01oTt"[getage(o)], o->marked);
  if (o->tt == ILYA_VSHRSTR || o->tt == ILYA_VLNGSTR)
    printf(" '%s'", getstr(gco2ts(o)));
}


void ilya_printobj (ilya_State *L, struct GCObject *o) {
  printobj(G(L), o);
}


void ilya_printvalue (TValue *v) {
  switch (ttype(v)) {
    case ILYA_TNUMBER: {
      char buff[ILYA_N2SBUFFSZ];
      unsigned len = luaO_tostringbuff(v, buff);
      buff[len] = '\0';
      printf("%s", buff);
      break;
    }
    case ILYA_TSTRING: {
      printf("'%s'", getstr(tsvalue(v)));
      break;
    }
    case ILYA_TBOOLEAN: {
      printf("%s", (!l_isfalse(v) ? "true" : "false"));
      break;
    }
    case ILYA_TLIGHTUSERDATA: {
      printf("light udata: %p", pvalue(v));
      break;
    }
    case ILYA_TNIL: {
      printf("nil");
      break;
    }
    default: {
      if (ttislcf(v))
        printf("light C fn: %p", fvalue(v));
      else  /* must be collectable */
        printf("%s: %p", ttypename(ttype(v)), gcvalue(v));
      break;
    }
  }
}


static int testobjref (global_State *g, GCObject *f, GCObject *t) {
  int r1 = testobjref1(g, f, t);
  if (!r1) {
    printf("%d(%02X) - ", g->gcstate, g->currentwhite);
    printobj(g, f);
    printf("  ->  ");
    printobj(g, t);
    printf("\n");
  }
  return r1;
}


static void checkobjref (global_State *g, GCObject *f, GCObject *t) {
    assert(testobjref(g, f, t));
}


/*
** Version where 't' can be NULL. In that case, it should not apply the
** macro 'obj2gco' over the object. ('t' may have several types, so this
** definition must be a macro.)  Most checks need this version, because
** the check may run while an object is still being created.
*/
#define checkobjrefN(g,f,t)	{ if (t) checkobjref(g,f,obj2gco(t)); }


static void checkvalref (global_State *g, GCObject *f, const TValue *t) {
  assert(!iscollectable(t) || (righttt(t) && testobjref(g, f, gcvalue(t))));
}


static void checktable (global_State *g, Table *h) {
  unsigned int i;
  unsigned int asize = h->asize;
  Node *n, *limit = gnode(h, sizenode(h));
  GCObject *hgc = obj2gco(h);
  checkobjrefN(g, hgc, h->metatable);
  for (i = 0; i < asize; i++) {
    TValue aux;
    arr2obj(h, i, &aux);
    checkvalref(g, hgc, &aux);
  }
  for (n = gnode(h, 0); n < limit; n++) {
    if (!isempty(gval(n))) {
      TValue k;
      getnodekey(g->mainthread, &k, n);
      assert(!keyisnil(n));
      checkvalref(g, hgc, &k);
      checkvalref(g, hgc, gval(n));
    }
  }
}


static void checkudata (global_State *g, Udata *u) {
  int i;
  GCObject *hgc = obj2gco(u);
  checkobjrefN(g, hgc, u->metatable);
  for (i = 0; i < u->nuvalue; i++)
    checkvalref(g, hgc, &u->uv[i].uv);
}


static void checkproto (global_State *g, Proto *f) {
  int i;
  GCObject *fgc = obj2gco(f);
  checkobjrefN(g, fgc, f->source);
  for (i=0; i<f->sizek; i++) {
    if (iscollectable(f->k + i))
      checkobjref(g, fgc, gcvalue(f->k + i));
  }
  for (i=0; i<f->sizeupvalues; i++)
    checkobjrefN(g, fgc, f->upvalues[i].name);
  for (i=0; i<f->sizep; i++)
    checkobjrefN(g, fgc, f->p[i]);
  for (i=0; i<f->sizelocvars; i++)
    checkobjrefN(g, fgc, f->locvars[i].varname);
}


static void checkCclosure (global_State *g, CClosure *cl) {
  GCObject *clgc = obj2gco(cl);
  int i;
  for (i = 0; i < cl->nupvalues; i++)
    checkvalref(g, clgc, &cl->upvalue[i]);
}


static void checkLclosure (global_State *g, LClosure *cl) {
  GCObject *clgc = obj2gco(cl);
  int i;
  checkobjrefN(g, clgc, cl->p);
  for (i=0; i<cl->nupvalues; i++) {
    UpVal *uv = cl->upvals[i];
    if (uv) {
      checkobjrefN(g, clgc, uv);
      if (!upisopen(uv))
        checkvalref(g, obj2gco(uv), uv->v.p);
    }
  }
}


static int ilya_checkpc (CallInfo *ci) {
  if (!isLua(ci)) return 1;
  else {
    StkId f = ci->func.p;
    Proto *p = clLvalue(s2v(f))->p;
    return p->code <= ci->u.l.savedpc &&
           ci->u.l.savedpc <= p->code + p->sizecode;
  }
}


static void checkstack (global_State *g, ilya_State *L1) {
  StkId o;
  CallInfo *ci;
  UpVal *uv;
  assert(!isdead(g, L1));
  if (L1->stack.p == NULL) {  /* incomplete thread? */
    assert(L1->openupval == NULL && L1->ci == NULL);
    return;
  }
  for (uv = L1->openupval; uv != NULL; uv = uv->u.open.next)
    assert(upisopen(uv));  /* must be open */
  assert(L1->top.p <= L1->stack_last.p);
  assert(L1->tbclist.p <= L1->top.p);
  for (ci = L1->ci; ci != NULL; ci = ci->previous) {
    assert(ci->top.p <= L1->stack_last.p);
    assert(ilya_checkpc(ci));
  }
  for (o = L1->stack.p; o < L1->stack_last.p; o++)
    checkliveness(L1, s2v(o));  /* entire stack must have valid values */
}


static void checkrefs (global_State *g, GCObject *o) {
  switch (o->tt) {
    case ILYA_VUSERDATA: {
      checkudata(g, gco2u(o));
      break;
    }
    case ILYA_VUPVAL: {
      checkvalref(g, o, gco2upv(o)->v.p);
      break;
    }
    case ILYA_VTABLE: {
      checktable(g, gco2t(o));
      break;
    }
    case ILYA_VTHREAD: {
      checkstack(g, gco2th(o));
      break;
    }
    case ILYA_VLCL: {
      checkLclosure(g, gco2lcl(o));
      break;
    }
    case ILYA_VCCL: {
      checkCclosure(g, gco2ccl(o));
      break;
    }
    case ILYA_VPROTO: {
      checkproto(g, gco2p(o));
      break;
    }
    case ILYA_VSHRSTR:
    case ILYA_VLNGSTR: {
      assert(!isgray(o));  /* strings are never gray */
      break;
    }
    default: assert(0);
  }
}


/*
** Check consistency of an object:
** - Dead objects can only happen in the 'allgc' list during a sweep
** phase (controlled by the caller through 'maybedead').
** - During pause, all objects must be white.
** - In generational mode:
**   * objects must be old enough for their lists ('listage').
**   * old objects cannot be white.
**   * old objects must be black, except for 'touched1', 'old0',
**     threads, and open upvalues.
**   * 'touched1' objects must be gray.
*/
static void checkobject (global_State *g, GCObject *o, int maybedead,
                         int listage) {
  if (isdead(g, o))
    assert(maybedead);
  else {
    assert(g->gcstate != GCSpause || iswhite(o));
    if (g->gckind == KGC_GENMINOR) {  /* generational mode? */
      assert(getage(o) >= listage);
      if (isold(o)) {
        assert(!iswhite(o));
        assert(isblack(o) ||
        getage(o) == G_TOUCHED1 ||
        getage(o) == G_OLD0 ||
        o->tt == ILYA_VTHREAD ||
        (o->tt == ILYA_VUPVAL && upisopen(gco2upv(o))));
      }
      assert(getage(o) != G_TOUCHED1 || isgray(o));
    }
    checkrefs(g, o);
  }
}


static l_mem checkgraylist (global_State *g, GCObject *o) {
  int total = 0;  /* count number of elements in the list */
  cast_void(g);  /* better to keep it if we need to print an object */
  while (o) {
    assert(!!isgray(o) ^ (getage(o) == G_TOUCHED2));
    assert(!testbit(o->marked, TESTBIT));
    if (keepinvariant(g))
      l_setbit(o->marked, TESTBIT);  /* mark that object is in a gray list */
    total++;
    switch (o->tt) {
      case ILYA_VTABLE: o = gco2t(o)->gclist; break;
      case ILYA_VLCL: o = gco2lcl(o)->gclist; break;
      case ILYA_VCCL: o = gco2ccl(o)->gclist; break;
      case ILYA_VTHREAD: o = gco2th(o)->gclist; break;
      case ILYA_VPROTO: o = gco2p(o)->gclist; break;
      case ILYA_VUSERDATA:
        assert(gco2u(o)->nuvalue > 0);
        o = gco2u(o)->gclist;
        break;
      default: assert(0);  /* other objects cannot be in a gray list */
    }
  }
  return total;
}


/*
** Check objects in gray lists.
*/
static l_mem checkgrays (global_State *g) {
  l_mem total = 0;  /* count number of elements in all lists */
  if (!keepinvariant(g)) return total;
  total += checkgraylist(g, g->gray);
  total += checkgraylist(g, g->grayagain);
  total += checkgraylist(g, g->weak);
  total += checkgraylist(g, g->allweak);
  total += checkgraylist(g, g->ephemeron);
  return total;
}


/*
** Check whether 'o' should be in a gray list. If so, increment
** 'count' and check its TESTBIT. (It must have been previously set by
** 'checkgraylist'.)
*/
static void incifingray (global_State *g, GCObject *o, l_mem *count) {
  if (!keepinvariant(g))
    return;  /* gray lists not being kept in these phases */
  if (o->tt == ILYA_VUPVAL) {
    /* only open upvalues can be gray */
    assert(!isgray(o) || upisopen(gco2upv(o)));
    return;  /* upvalues are never in gray lists */
  }
  /* these are the ones that must be in gray lists */
  if (isgray(o) || getage(o) == G_TOUCHED2) {
    (*count)++;
    assert(testbit(o->marked, TESTBIT));
    resetbit(o->marked, TESTBIT);  /* prepare for next cycle */
  }
}


static l_mem checklist (global_State *g, int maybedead, int tof,
  GCObject *newl, GCObject *survival, GCObject *old, GCObject *reallyold) {
  GCObject *o;
  l_mem total = 0;  /* number of object that should be in  gray lists */
  for (o = newl; o != survival; o = o->next) {
    checkobject(g, o, maybedead, G_NEW);
    incifingray(g, o, &total);
    assert(!tof == !tofinalize(o));
  }
  for (o = survival; o != old; o = o->next) {
    checkobject(g, o, 0, G_SURVIVAL);
    incifingray(g, o, &total);
    assert(!tof == !tofinalize(o));
  }
  for (o = old; o != reallyold; o = o->next) {
    checkobject(g, o, 0, G_OLD1);
    incifingray(g, o, &total);
    assert(!tof == !tofinalize(o));
  }
  for (o = reallyold; o != NULL; o = o->next) {
    checkobject(g, o, 0, G_OLD);
    incifingray(g, o, &total);
    assert(!tof == !tofinalize(o));
  }
  return total;
}


int ilya_checkmemory (ilya_State *L) {
  global_State *g = G(L);
  GCObject *o;
  int maybedead;
  l_mem totalin;  /* total of objects that are in gray lists */
  l_mem totalshould;  /* total of objects that should be in gray lists */
  if (keepinvariant(g)) {
    assert(!iswhite(g->mainthread));
    assert(!iswhite(gcvalue(&g->l_registry)));
  }
  assert(!isdead(g, gcvalue(&g->l_registry)));
  assert(g->sweepgc == NULL || issweepphase(g));
  totalin = checkgrays(g);

  /* check 'fixedgc' list */
  for (o = g->fixedgc; o != NULL; o = o->next) {
    assert(o->tt == ILYA_VSHRSTR && isgray(o) && getage(o) == G_OLD);
  }

  /* check 'allgc' list */
  maybedead = (GCSatomic < g->gcstate && g->gcstate <= GCSswpallgc);
  totalshould = checklist(g, maybedead, 0, g->allgc,
                             g->survival, g->old1, g->reallyold);

  /* check 'finobj' list */
  totalshould += checklist(g, 0, 1, g->finobj,
                              g->finobjsur, g->finobjold1, g->finobjrold);

  /* check 'tobefnz' list */
  for (o = g->tobefnz; o != NULL; o = o->next) {
    checkobject(g, o, 0, G_NEW);
    incifingray(g, o, &totalshould);
    assert(tofinalize(o));
    assert(o->tt == ILYA_VUSERDATA || o->tt == ILYA_VTABLE);
  }
  if (keepinvariant(g))
    assert(totalin == totalshould);
  return 0;
}

/* }====================================================== */



/*
** {======================================================
** Disassembler
** =======================================================
*/


static char *buildop (Proto *p, int pc, char *buff) {
  char *obuff = buff;
  Instruction i = p->code[pc];
  OpCode o = GET_OPCODE(i);
  const char *name = opnames[o];
  int line = luaG_getfuncline(p, pc);
  int lineinfo = (p->lineinfo != NULL) ? p->lineinfo[pc] : 0;
  if (lineinfo == ABSLINEINFO)
    buff += sprintf(buff, "(__");
  else
    buff += sprintf(buff, "(%2d", lineinfo);
  buff += sprintf(buff, " - %4d) %4d - ", line, pc);
  switch (getOpMode(o)) {
    case iABC:
      sprintf(buff, "%-12s%4d %4d %4d%s", name,
              GETARG_A(i), GETARG_B(i), GETARG_C(i),
              GETARG_k(i) ? " (k)" : "");
      break;
    case ivABC:
      sprintf(buff, "%-12s%4d %4d %4d%s", name,
              GETARG_A(i), GETARG_vB(i), GETARG_vC(i),
              GETARG_k(i) ? " (k)" : "");
      break;
    case iABx:
      sprintf(buff, "%-12s%4d %4d", name, GETARG_A(i), GETARG_Bx(i));
      break;
    case iAsBx:
      sprintf(buff, "%-12s%4d %4d", name, GETARG_A(i), GETARG_sBx(i));
      break;
    case iAx:
      sprintf(buff, "%-12s%4d", name, GETARG_Ax(i));
      break;
    case isJ:
      sprintf(buff, "%-12s%4d", name, GETARG_sJ(i));
      break;
  }
  return obuff;
}


#if 0
void luaI_printcode (Proto *pt, int size) {
  int pc;
  for (pc=0; pc<size; pc++) {
    char buff[100];
    printf("%s\n", buildop(pt, pc, buff));
  }
  printf("-------\n");
}


void luaI_printinst (Proto *pt, int pc) {
  char buff[100];
  printf("%s\n", buildop(pt, pc, buff));
}
#endif


static int listcode (ilya_State *L) {
  int pc;
  Proto *p;
  luaL_argcheck(L, ilya_isfunction(L, 1) && !ilya_iscfunction(L, 1),
                 1, "Ilya fn expected");
  p = getproto(obj_at(L, 1));
  ilya_newtable(L);
  setnameval(L, "maxstack", p->maxstacksize);
  setnameval(L, "numparams", p->numparams);
  for (pc=0; pc<p->sizecode; pc++) {
    char buff[100];
    ilya_pushinteger(L, pc+1);
    ilya_pushstring(L, buildop(p, pc, buff));
    ilya_settable(L, -3);
  }
  return 1;
}


static int printcode (ilya_State *L) {
  int pc;
  Proto *p;
  luaL_argcheck(L, ilya_isfunction(L, 1) && !ilya_iscfunction(L, 1),
                 1, "Ilya fn expected");
  p = getproto(obj_at(L, 1));
  printf("maxstack: %d\n", p->maxstacksize);
  printf("numparams: %d\n", p->numparams);
  for (pc=0; pc<p->sizecode; pc++) {
    char buff[100];
    printf("%s\n", buildop(p, pc, buff));
  }
  return 0;
}


static int listk (ilya_State *L) {
  Proto *p;
  int i;
  luaL_argcheck(L, ilya_isfunction(L, 1) && !ilya_iscfunction(L, 1),
                 1, "Ilya fn expected");
  p = getproto(obj_at(L, 1));
  ilya_createtable(L, p->sizek, 0);
  for (i=0; i<p->sizek; i++) {
    pushobject(L, p->k+i);
    ilya_rawseti(L, -2, i+1);
  }
  return 1;
}


static int listabslineinfo (ilya_State *L) {
  Proto *p;
  int i;
  luaL_argcheck(L, ilya_isfunction(L, 1) && !ilya_iscfunction(L, 1),
                 1, "Ilya fn expected");
  p = getproto(obj_at(L, 1));
  luaL_argcheck(L, p->abslineinfo != NULL, 1, "fn has no debug info");
  ilya_createtable(L, 2 * p->sizeabslineinfo, 0);
  for (i=0; i < p->sizeabslineinfo; i++) {
    ilya_pushinteger(L, p->abslineinfo[i].pc);
    ilya_rawseti(L, -2, 2 * i + 1);
    ilya_pushinteger(L, p->abslineinfo[i].line);
    ilya_rawseti(L, -2, 2 * i + 2);
  }
  return 1;
}


static int listlocals (ilya_State *L) {
  Proto *p;
  int pc = cast_int(luaL_checkinteger(L, 2)) - 1;
  int i = 0;
  const char *name;
  luaL_argcheck(L, ilya_isfunction(L, 1) && !ilya_iscfunction(L, 1),
                 1, "Ilya fn expected");
  p = getproto(obj_at(L, 1));
  while ((name = luaF_getlocalname(p, ++i, pc)) != NULL)
    ilya_pushstring(L, name);
  return i-1;
}

/* }====================================================== */



void ilya_printstack (ilya_State *L) {
  int i;
  int n = ilya_gettop(L);
  printf("stack: >>\n");
  for (i = 1; i <= n; i++) {
    printf("%3d: ", i);
    ilya_printvalue(s2v(L->ci->func.p + i));
    printf("\n");
  }
  printf("<<\n");
}


static int get_limits (ilya_State *L) {
  ilya_createtable(L, 0, 5);
  setnameval(L, "IS32INT", LUAI_IS32INT);
  setnameval(L, "MAXARG_Ax", MAXARG_Ax);
  setnameval(L, "MAXARG_Bx", MAXARG_Bx);
  setnameval(L, "OFFSET_sBx", OFFSET_sBx);
  setnameval(L, "NUM_OPCODES", NUM_OPCODES);
  return 1;
}


static int mem_query (ilya_State *L) {
  if (ilya_isnone(L, 1)) {
    ilya_pushinteger(L, cast(ilya_Integer, l_memcontrol.total));
    ilya_pushinteger(L, cast(ilya_Integer, l_memcontrol.numblocks));
    ilya_pushinteger(L, cast(ilya_Integer, l_memcontrol.maxmem));
    return 3;
  }
  else if (ilya_isnumber(L, 1)) {
    unsigned long limit = cast(unsigned long, luaL_checkinteger(L, 1));
    if (limit == 0) limit = ULONG_MAX;
    l_memcontrol.memlimit = limit;
    return 0;
  }
  else {
    const char *t = luaL_checkstring(L, 1);
    int i;
    for (i = ILYA_NUMTYPES - 1; i >= 0; i--) {
      if (strcmp(t, ttypename(i)) == 0) {
        ilya_pushinteger(L, cast(ilya_Integer, l_memcontrol.objcount[i]));
        return 1;
      }
    }
    return luaL_error(L, "unknown type '%s'", t);
  }
}


static int alloc_count (ilya_State *L) {
  if (ilya_isnone(L, 1))
    l_memcontrol.countlimit = cast(unsigned long, ~0L);
  else
    l_memcontrol.countlimit = cast(unsigned long, luaL_checkinteger(L, 1));
  return 0;
}


static int alloc_failnext (ilya_State *L) {
  UNUSED(L);
  l_memcontrol.failnext = 1;
  return 0;
}


static int settrick (ilya_State *L) {
  if (ttisnil(obj_at(L, 1)))
    l_Trick = NULL;
  else
    l_Trick = gcvalue(obj_at(L, 1));
  return 0;
}


static int gc_color (ilya_State *L) {
  TValue *o;
  luaL_checkany(L, 1);
  o = obj_at(L, 1);
  if (!iscollectable(o))
    ilya_pushstring(L, "no collectable");
  else {
    GCObject *obj = gcvalue(o);
    ilya_pushstring(L, isdead(G(L), obj) ? "dead" :
                      iswhite(obj) ? "white" :
                      isblack(obj) ? "black" : "gray");
  }
  return 1;
}


static int gc_age (ilya_State *L) {
  TValue *o;
  luaL_checkany(L, 1);
  o = obj_at(L, 1);
  if (!iscollectable(o))
    ilya_pushstring(L, "no collectable");
  else {
    static const char *gennames[] = {"new", "survival", "old0", "old1",
                                     "old", "touched1", "touched2"};
    GCObject *obj = gcvalue(o);
    ilya_pushstring(L, gennames[getage(obj)]);
  }
  return 1;
}


static int gc_printobj (ilya_State *L) {
  TValue *o;
  luaL_checkany(L, 1);
  o = obj_at(L, 1);
  if (!iscollectable(o))
    printf("no collectable\n");
  else {
    GCObject *obj = gcvalue(o);
    printobj(G(L), obj);
    printf("\n");
  }
  return 0;
}


static const char *statenames[] = {
  "propagate", "enteratomic", "atomic", "sweepallgc", "sweepfinobj",
  "sweeptobefnz", "sweepend", "callfin", "pause", ""};

static int gc_state (ilya_State *L) {
  static const int states[] = {
    GCSpropagate, GCSenteratomic, GCSatomic, GCSswpallgc, GCSswpfinobj,
    GCSswptobefnz, GCSswpend, GCScallfin, GCSpause, -1};
  int option = states[luaL_checkoption(L, 1, "", statenames)];
  global_State *g = G(L);
  if (option == -1) {
    ilya_pushstring(L, statenames[g->gcstate]);
    return 1;
  }
  else {
    if (g->gckind != KGC_INC)
      luaL_error(L, "cannot change states in generational mode");
    ilya_lock(L);
    if (option < g->gcstate) {  /* must cross 'pause'? */
      luaC_runtilstate(L, GCSpause, 1);  /* run until pause */
    }
    luaC_runtilstate(L, option, 0);  /* do not skip propagation state */
    ilya_assert(g->gcstate == option);
    ilya_unlock(L);
    return 0;
  }
}


static int tracinggc = 0;
void luai_tracegctest (ilya_State *L, int first) {
  if (!tracinggc) return;
  else {
    global_State *g = G(L);
    ilya_unlock(L);
    g->gcstp = GCSTPGC;
    ilya_checkstack(L, 10);
    ilya_getfield(L, ILYA_REGISTRYINDEX, "tracegc");
    ilya_pushboolean(L, first);
    ilya_call(L, 1, 0);
    g->gcstp = 0;
    ilya_lock(L);
  }
}


static int tracegc (ilya_State *L) {
  if (ilya_isnil(L, 1))
    tracinggc = 0;
  else {
    tracinggc = 1;
    ilya_setfield(L, ILYA_REGISTRYINDEX, "tracegc");
  }
  return 0;
}


static int hash_query (ilya_State *L) {
  if (ilya_isnone(L, 2)) {
    luaL_argcheck(L, ilya_type(L, 1) == ILYA_TSTRING, 1, "string expected");
    ilya_pushinteger(L, cast_int(tsvalue(obj_at(L, 1))->hash));
  }
  else {
    TValue *o = obj_at(L, 1);
    Table *t;
    luaL_checktype(L, 2, ILYA_TTABLE);
    t = hvalue(obj_at(L, 2));
    ilya_pushinteger(L, cast(ilya_Integer, luaH_mainposition(t, o) - t->node));
  }
  return 1;
}


static int stacklevel (ilya_State *L) {
  int a = 0;
  ilya_pushinteger(L, cast(ilya_Integer, L->top.p - L->stack.p));
  ilya_pushinteger(L, stacksize(L));
  ilya_pushinteger(L, cast(ilya_Integer, L->nCcalls));
  ilya_pushinteger(L, L->nci);
  ilya_pushinteger(L, (ilya_Integer)(size_t)&a);
  return 5;
}


static int table_query (ilya_State *L) {
  const Table *t;
  int i = cast_int(luaL_optinteger(L, 2, -1));
  unsigned int asize;
  luaL_checktype(L, 1, ILYA_TTABLE);
  t = hvalue(obj_at(L, 1));
  asize = t->asize;
  if (i == -1) {
    ilya_pushinteger(L, cast(ilya_Integer, asize));
    ilya_pushinteger(L, cast(ilya_Integer, allocsizenode(t)));
    ilya_pushinteger(L, cast(ilya_Integer, asize > 0 ? *lenhint(t) : 0));
    return 3;
  }
  else if (cast_uint(i) < asize) {
    ilya_pushinteger(L, i);
    if (!tagisempty(*getArrTag(t, i)))
      arr2obj(t, cast_uint(i), s2v(L->top.p));
    else
      setnilvalue(s2v(L->top.p));
    api_incr_top(L);
    ilya_pushnil(L);
  }
  else if (cast_uint(i -= cast_int(asize)) < sizenode(t)) {
    TValue k;
    getnodekey(L, &k, gnode(t, i));
    if (!isempty(gval(gnode(t, i))) ||
        ttisnil(&k) ||
        ttisnumber(&k)) {
      pushobject(L, &k);
    }
    else
      ilya_pushliteral(L, "<undef>");
    if (!isempty(gval(gnode(t, i))))
      pushobject(L, gval(gnode(t, i)));
    else
      ilya_pushnil(L);
    ilya_pushinteger(L, gnext(&t->node[i]));
  }
  return 3;
}


static int gc_query (ilya_State *L) {
  global_State *g = G(L);
  ilya_pushstring(L, g->gckind == KGC_INC ? "inc"
                  : g->gckind == KGC_GENMAJOR ? "genmajor"
                  : "genminor");
  ilya_pushstring(L, statenames[g->gcstate]);
  ilya_pushinteger(L, cast_st2S(gettotalbytes(g)));
  ilya_pushinteger(L, cast_st2S(g->GCdebt));
  ilya_pushinteger(L, cast_st2S(g->GCmarked));
  ilya_pushinteger(L, cast_st2S(g->GCmajorminor));
  return 6;
}


static int test_codeparam (ilya_State *L) {
  ilya_Integer p = luaL_checkinteger(L, 1);
  ilya_pushinteger(L, luaO_codeparam(cast_uint(p)));
  return 1;
}


static int test_applyparam (ilya_State *L) {
  ilya_Integer p = luaL_checkinteger(L, 1);
  ilya_Integer x = luaL_checkinteger(L, 2);
  ilya_pushinteger(L, cast(ilya_Integer, luaO_applyparam(cast_byte(p), x)));
  return 1;
}


static int string_query (ilya_State *L) {
  stringtable *tb = &G(L)->strt;
  int s = cast_int(luaL_optinteger(L, 1, 0)) - 1;
  if (s == -1) {
    ilya_pushinteger(L ,tb->size);
    ilya_pushinteger(L ,tb->nuse);
    return 2;
  }
  else if (s < tb->size) {
    TString *ts;
    int n = 0;
    for (ts = tb->hash[s]; ts != NULL; ts = ts->u.hnext) {
      setsvalue2s(L, L->top.p, ts);
      api_incr_top(L);
      n++;
    }
    return n;
  }
  else return 0;
}


static int getreftable (ilya_State *L) {
  if (ilya_istable(L, 2))  /* is there a table as second argument? */
    return 2;  /* use it as the table */
  else
    return ILYA_REGISTRYINDEX;  /* default is to use the register */
}


static int tref (ilya_State *L) {
  int t = getreftable(L);
  int level = ilya_gettop(L);
  luaL_checkany(L, 1);
  ilya_pushvalue(L, 1);
  ilya_pushinteger(L, luaL_ref(L, t));
  cast_void(level);  /* to avoid warnings */
  ilya_assert(ilya_gettop(L) == level+1);  /* +1 for result */
  return 1;
}


static int getref (ilya_State *L) {
  int t = getreftable(L);
  int level = ilya_gettop(L);
  ilya_rawgeti(L, t, luaL_checkinteger(L, 1));
  cast_void(level);  /* to avoid warnings */
  ilya_assert(ilya_gettop(L) == level+1);
  return 1;
}

static int unref (ilya_State *L) {
  int t = getreftable(L);
  int level = ilya_gettop(L);
  luaL_unref(L, t, cast_int(luaL_checkinteger(L, 1)));
  cast_void(level);  /* to avoid warnings */
  ilya_assert(ilya_gettop(L) == level);
  return 0;
}


static int upvalue (ilya_State *L) {
  int n = cast_int(luaL_checkinteger(L, 2));
  luaL_checktype(L, 1, ILYA_TFUNCTION);
  if (ilya_isnone(L, 3)) {
    const char *name = ilya_getupvalue(L, 1, n);
    if (name == NULL) return 0;
    ilya_pushstring(L, name);
    return 2;
  }
  else {
    const char *name = ilya_setupvalue(L, 1, n);
    ilya_pushstring(L, name);
    return 1;
  }
}


static int newuserdata (ilya_State *L) {
  size_t size = cast_sizet(luaL_optinteger(L, 1, 0));
  int nuv = cast_int(luaL_optinteger(L, 2, 0));
  char *p = cast_charp(ilya_newuserdatauv(L, size, nuv));
  while (size--) *p++ = '\0';
  return 1;
}


static int pushuserdata (ilya_State *L) {
  ilya_Integer u = luaL_checkinteger(L, 1);
  ilya_pushlightuserdata(L, cast_voidp(cast_sizet(u)));
  return 1;
}


static int udataval (ilya_State *L) {
  ilya_pushinteger(L, cast(ilya_Integer, cast(size_t, ilya_touserdata(L, 1))));
  return 1;
}


static int doonnewstack (ilya_State *L) {
  ilya_State *L1 = ilya_newthread(L);
  size_t l;
  const char *s = luaL_checklstring(L, 1, &l);
  int status = luaL_loadbuffer(L1, s, l, s);
  if (status == ILYA_OK)
    status = ilya_pcall(L1, 0, 0, 0);
  ilya_pushinteger(L, status);
  return 1;
}


static int s2d (ilya_State *L) {
  ilya_pushnumber(L, cast_num(*cast(const double *, luaL_checkstring(L, 1))));
  return 1;
}


static int d2s (ilya_State *L) {
  double d = cast(double, luaL_checknumber(L, 1));
  ilya_pushlstring(L, cast_charp(&d), sizeof(d));
  return 1;
}


static int num2int (ilya_State *L) {
  ilya_pushinteger(L, ilya_tointeger(L, 1));
  return 1;
}


static int makeseed (ilya_State *L) {
  ilya_pushinteger(L, cast(ilya_Integer, luaL_makeseed(L)));
  return 1;
}


static int newstate (ilya_State *L) {
  void *ud;
  ilya_Alloc f = ilya_getallocf(L, &ud);
  ilya_State *L1 = ilya_newstate(f, ud, 0);
  if (L1) {
    ilya_atpanic(L1, tpanic);
    ilya_pushlightuserdata(L, L1);
  }
  else
    ilya_pushnil(L);
  return 1;
}


static ilya_State *getstate (ilya_State *L) {
  ilya_State *L1 = cast(ilya_State *, ilya_touserdata(L, 1));
  luaL_argcheck(L, L1 != NULL, 1, "state expected");
  return L1;
}


static int loadlib (ilya_State *L) {
  ilya_State *L1 = getstate(L);
  int load = cast_int(luaL_checkinteger(L, 2));
  int preload = cast_int(luaL_checkinteger(L, 3));
  luaL_openselectedlibs(L1, load, preload);
  luaL_requiref(L1, "T", luaB_opentests, 0);
  ilya_assert(ilya_type(L1, -1) == ILYA_TTABLE);
  /* 'requiref' should not reload module already loaded... */
  luaL_requiref(L1, "T", NULL, 1);  /* seg. fault if it reloads */
  /* ...but should return the same module */
  ilya_assert(ilya_compare(L1, -1, -2, ILYA_OPEQ));
  return 0;
}

static int closestate (ilya_State *L) {
  ilya_State *L1 = getstate(L);
  ilya_close(L1);
  return 0;
}

static int doremote (ilya_State *L) {
  ilya_State *L1 = getstate(L);
  size_t lcode;
  const char *code = luaL_checklstring(L, 2, &lcode);
  int status;
  ilya_settop(L1, 0);
  status = luaL_loadbuffer(L1, code, lcode, code);
  if (status == ILYA_OK)
    status = ilya_pcall(L1, 0, ILYA_MULTRET, 0);
  if (status != ILYA_OK) {
    ilya_pushnil(L);
    ilya_pushstring(L, ilya_tostring(L1, -1));
    ilya_pushinteger(L, status);
    return 3;
  }
  else {
    int i = 0;
    while (!ilya_isnone(L1, ++i))
      ilya_pushstring(L, ilya_tostring(L1, i));
    ilya_pop(L1, i-1);
    return i-1;
  }
}


static int log2_aux (ilya_State *L) {
  unsigned int x = (unsigned int)luaL_checkinteger(L, 1);
  ilya_pushinteger(L, luaO_ceillog2(x));
  return 1;
}


struct Aux { jmp_buf jb; const char *paniccode; ilya_State *L; };

/*
** does a long-jump back to "main program".
*/
static int panicback (ilya_State *L) {
  struct Aux *b;
  ilya_checkstack(L, 1);  /* open space for 'Aux' struct */
  ilya_getfield(L, ILYA_REGISTRYINDEX, "_jmpbuf");  /* get 'Aux' struct */
  b = (struct Aux *)ilya_touserdata(L, -1);
  ilya_pop(L, 1);  /* remove 'Aux' struct */
  runC(b->L, L, b->paniccode);  /* run optional panic code */
  longjmp(b->jb, 1);
  return 1;  /* to avoid warnings */
}

static int checkpanic (ilya_State *L) {
  struct Aux b;
  void *ud;
  ilya_State *L1;
  const char *code = luaL_checkstring(L, 1);
  ilya_Alloc f = ilya_getallocf(L, &ud);
  b.paniccode = luaL_optstring(L, 2, "");
  b.L = L;
  L1 = ilya_newstate(f, ud, 0);  /* create new state */
  if (L1 == NULL) {  /* error? */
    ilya_pushstring(L, MEMERRMSG);
    return 1;
  }
  ilya_atpanic(L1, panicback);  /* set its panic fn */
  ilya_pushlightuserdata(L1, &b);
  ilya_setfield(L1, ILYA_REGISTRYINDEX, "_jmpbuf");  /* store 'Aux' struct */
  if (setjmp(b.jb) == 0) {  /* set jump buffer */
    runC(L, L1, code);  /* run code unprotected */
    ilya_pushliteral(L, "no errors");
  }
  else {  /* error handling */
    /* move error message to original state */
    ilya_pushstring(L, ilya_tostring(L1, -1));
  }
  ilya_close(L1);
  return 1;
}


static int externKstr (ilya_State *L) {
  size_t len;
  const char *s = luaL_checklstring(L, 1, &len);
  ilya_pushexternalstring(L, s, len, NULL, NULL);
  return 1;
}


/*
** Create a buffer with the content of a given string and then
** create an external string using that buffer. Use the allocation
** fn from Ilya to create and free the buffer.
*/
static int externstr (ilya_State *L) {
  size_t len;
  const char *s = luaL_checklstring(L, 1, &len);
  void *ud;
  ilya_Alloc allocf = ilya_getallocf(L, &ud);  /* get allocation fn */
  /* create the buffer */
  char *buff = cast_charp((*allocf)(ud, NULL, 0, len + 1));
  if (buff == NULL) {  /* memory error? */
    ilya_pushliteral(L, "not enough memory");
    ilya_error(L);  /* raise a memory error */
  }
  /* copy string content to buffer, including ending 0 */
  memcpy(buff, s, (len + 1) * sizeof(char));
  /* create external string */
  ilya_pushexternalstring(L, buff, len, allocf, ud);
  return 1;
}


/*
** {====================================================================
** fn to test the API with C. It interprets a kind of assembler
** language with calls to the API, so the test can be driven by Ilya code
** =====================================================================
*/


static void sethookaux (ilya_State *L, int mask, int count, const char *code);

static const char *const delimits = " \t\n,;";

static void skip (const char **pc) {
  for (;;) {
    if (**pc != '\0' && strchr(delimits, **pc)) (*pc)++;
    else if (**pc == '#') {  /* comment? */
      while (**pc != '\n' && **pc != '\0') (*pc)++;  /* until end-of-line */
    }
    else break;
  }
}

static int getnum_aux (ilya_State *L, ilya_State *L1, const char **pc) {
  int res = 0;
  int sig = 1;
  skip(pc);
  if (**pc == '.') {
    res = cast_int(ilya_tointeger(L1, -1));
    ilya_pop(L1, 1);
    (*pc)++;
    return res;
  }
  else if (**pc == '*') {
    res = ilya_gettop(L1);
    (*pc)++;
    return res;
  }
  else if (**pc == '!') {
    (*pc)++;
    if (**pc == 'G')
      res = ILYA_RIDX_GLOBALS;
    else if (**pc == 'M')
      res = ILYA_RIDX_MAINTHREAD;
    else ilya_assert(0);
    (*pc)++;
    return res;
  }
  else if (**pc == '-') {
    sig = -1;
    (*pc)++;
  }
  if (!lisdigit(cast_uchar(**pc)))
    luaL_error(L, "number expected (%s)", *pc);
  while (lisdigit(cast_uchar(**pc))) res = res*10 + (*(*pc)++) - '0';
  return sig*res;
}

static const char *getstring_aux (ilya_State *L, char *buff, const char **pc) {
  int i = 0;
  skip(pc);
  if (**pc == '"' || **pc == '\'') {  /* quoted string? */
    int quote = *(*pc)++;
    while (**pc != quote) {
      if (**pc == '\0') luaL_error(L, "unfinished string in C script");
      buff[i++] = *(*pc)++;
    }
    (*pc)++;
  }
  else {
    while (**pc != '\0' && !strchr(delimits, **pc))
      buff[i++] = *(*pc)++;
  }
  buff[i] = '\0';
  return buff;
}


static int getindex_aux (ilya_State *L, ilya_State *L1, const char **pc) {
  skip(pc);
  switch (*(*pc)++) {
    case 'R': return ILYA_REGISTRYINDEX;
    case 'U': return ilya_upvalueindex(getnum_aux(L, L1, pc));
    default: {
      int n;
      (*pc)--;  /* to read again */
      n = getnum_aux(L, L1, pc);
      if (n == 0) return 0;
      else return ilya_absindex(L1, n);
    }
  }
}


static const char *const statcodes[] = {"OK", "YIELD", "ERRRUN",
    "ERRSYNTAX", MEMERRMSG, "ERRERR"};

/*
** Avoid these stat codes from being collected, to avoid possible
** memory error when pushing them.
*/
static void regcodes (ilya_State *L) {
  unsigned int i;
  for (i = 0; i < sizeof(statcodes) / sizeof(statcodes[0]); i++) {
    ilya_pushboolean(L, 1);
    ilya_setfield(L, ILYA_REGISTRYINDEX, statcodes[i]);
  }
}


#define EQ(s1)	(strcmp(s1, inst) == 0)

#define getnum		(getnum_aux(L, L1, &pc))
#define getstring	(getstring_aux(L, buff, &pc))
#define getindex	(getindex_aux(L, L1, &pc))


static int testC (ilya_State *L);
static int Cfunck (ilya_State *L, int status, ilya_KContext ctx);

/*
** arithmetic operation encoding for 'arith' instruction
** ILYA_OPIDIV  -> \
** ILYA_OPSHL   -> <
** ILYA_OPSHR   -> >
** ILYA_OPUNM   -> _
** ILYA_OPBNOT  -> !
*/
static const char ops[] = "+-*%^/\\&|~<>_!";

static int runC (ilya_State *L, ilya_State *L1, const char *pc) {
  char buff[300];
  int status = 0;
  if (pc == NULL) return luaL_error(L, "attempt to runC null script");
  for (;;) {
    const char *inst = getstring;
    if EQ("") return 0;
    else if EQ("absindex") {
      ilya_pushinteger(L1, getindex);
    }
    else if EQ("append") {
      int t = getindex;
      int i = cast_int(ilya_rawlen(L1, t));
      ilya_rawseti(L1, t, i + 1);
    }
    else if EQ("arith") {
      int op;
      skip(&pc);
      op = cast_int(strchr(ops, *pc++) - ops);
      ilya_arith(L1, op);
    }
    else if EQ("call") {
      int narg = getnum;
      int nres = getnum;
      ilya_call(L1, narg, nres);
    }
    else if EQ("callk") {
      int narg = getnum;
      int nres = getnum;
      int i = getindex;
      ilya_callk(L1, narg, nres, i, Cfunck);
    }
    else if EQ("checkstack") {
      int sz = getnum;
      const char *msg = getstring;
      if (*msg == '\0')
        msg = NULL;  /* to test 'luaL_checkstack' with no message */
      luaL_checkstack(L1, sz, msg);
    }
    else if EQ("rawcheckstack") {
      int sz = getnum;
      ilya_pushboolean(L1, ilya_checkstack(L1, sz));
    }
    else if EQ("compare") {
      const char *opt = getstring;  /* EQ, LT, or LE */
      int op = (opt[0] == 'E') ? ILYA_OPEQ
                               : (opt[1] == 'T') ? ILYA_OPLT : ILYA_OPLE;
      int a = getindex;
      int b = getindex;
      ilya_pushboolean(L1, ilya_compare(L1, a, b, op));
    }
    else if EQ("concat") {
      ilya_concat(L1, getnum);
    }
    else if EQ("copy") {
      int f = getindex;
      ilya_copy(L1, f, getindex);
    }
    else if EQ("func2num") {
      ilya_CFunction func = ilya_tocfunction(L1, getindex);
      ilya_pushinteger(L1, cast(ilya_Integer, cast(size_t, func)));
    }
    else if EQ("getfield") {
      int t = getindex;
      int tp = ilya_getfield(L1, t, getstring);
      ilya_assert(tp == ilya_type(L1, -1));
    }
    else if EQ("getglobal") {
      ilya_getglobal(L1, getstring);
    }
    else if EQ("getmetatable") {
      if (ilya_getmetatable(L1, getindex) == 0)
        ilya_pushnil(L1);
    }
    else if EQ("gettable") {
      int tp = ilya_gettable(L1, getindex);
      ilya_assert(tp == ilya_type(L1, -1));
    }
    else if EQ("gettop") {
      ilya_pushinteger(L1, ilya_gettop(L1));
    }
    else if EQ("gsub") {
      int a = getnum; int b = getnum; int c = getnum;
      luaL_gsub(L1, ilya_tostring(L1, a),
                    ilya_tostring(L1, b),
                    ilya_tostring(L1, c));
    }
    else if EQ("insert") {
      ilya_insert(L1, getnum);
    }
    else if EQ("iscfunction") {
      ilya_pushboolean(L1, ilya_iscfunction(L1, getindex));
    }
    else if EQ("isfunction") {
      ilya_pushboolean(L1, ilya_isfunction(L1, getindex));
    }
    else if EQ("isnil") {
      ilya_pushboolean(L1, ilya_isnil(L1, getindex));
    }
    else if EQ("isnull") {
      ilya_pushboolean(L1, ilya_isnone(L1, getindex));
    }
    else if EQ("isnumber") {
      ilya_pushboolean(L1, ilya_isnumber(L1, getindex));
    }
    else if EQ("isstring") {
      ilya_pushboolean(L1, ilya_isstring(L1, getindex));
    }
    else if EQ("istable") {
      ilya_pushboolean(L1, ilya_istable(L1, getindex));
    }
    else if EQ("isudataval") {
      ilya_pushboolean(L1, ilya_islightuserdata(L1, getindex));
    }
    else if EQ("isuserdata") {
      ilya_pushboolean(L1, ilya_isuserdata(L1, getindex));
    }
    else if EQ("len") {
      ilya_len(L1, getindex);
    }
    else if EQ("Llen") {
      ilya_pushinteger(L1, luaL_len(L1, getindex));
    }
    else if EQ("loadfile") {
      luaL_loadfile(L1, luaL_checkstring(L1, getnum));
    }
    else if EQ("loadstring") {
      size_t slen;
      const char *s = luaL_checklstring(L1, getnum, &slen);
      const char *name = getstring;
      const char *mode = getstring;
      luaL_loadbufferx(L1, s, slen, name, mode);
    }
    else if EQ("newmetatable") {
      ilya_pushboolean(L1, luaL_newmetatable(L1, getstring));
    }
    else if EQ("newtable") {
      ilya_newtable(L1);
    }
    else if EQ("newthread") {
      ilya_newthread(L1);
    }
    else if EQ("resetthread") {
      ilya_pushinteger(L1, ilya_resetthread(L1));  /* deprecated */
    }
    else if EQ("newuserdata") {
      ilya_newuserdata(L1, cast_sizet(getnum));
    }
    else if EQ("next") {
      ilya_next(L1, -2);
    }
    else if EQ("objsize") {
      ilya_pushinteger(L1, l_castU2S(ilya_rawlen(L1, getindex)));
    }
    else if EQ("pcall") {
      int narg = getnum;
      int nres = getnum;
      status = ilya_pcall(L1, narg, nres, getnum);
    }
    else if EQ("pcallk") {
      int narg = getnum;
      int nres = getnum;
      int i = getindex;
      status = ilya_pcallk(L1, narg, nres, 0, i, Cfunck);
    }
    else if EQ("pop") {
      ilya_pop(L1, getnum);
    }
    else if EQ("printstack") {
      int n = getnum;
      if (n != 0) {
        ilya_printvalue(s2v(L->ci->func.p + n));
        printf("\n");
      }
      else ilya_printstack(L1);
    }
    else if EQ("print") {
      const char *msg = getstring;
      printf("%s\n", msg);
    }
    else if EQ("warningC") {
      const char *msg = getstring;
      ilya_warning(L1, msg, 1);
    }
    else if EQ("warning") {
      const char *msg = getstring;
      ilya_warning(L1, msg, 0);
    }
    else if EQ("pushbool") {
      ilya_pushboolean(L1, getnum);
    }
    else if EQ("pushcclosure") {
      ilya_pushcclosure(L1, testC, getnum);
    }
    else if EQ("pushint") {
      ilya_pushinteger(L1, getnum);
    }
    else if EQ("pushnil") {
      ilya_pushnil(L1);
    }
    else if EQ("pushnum") {
      ilya_pushnumber(L1, (ilya_Number)getnum);
    }
    else if EQ("pushstatus") {
      ilya_pushstring(L1, statcodes[status]);
    }
    else if EQ("pushstring") {
      ilya_pushstring(L1, getstring);
    }
    else if EQ("pushupvalueindex") {
      ilya_pushinteger(L1, ilya_upvalueindex(getnum));
    }
    else if EQ("pushvalue") {
      ilya_pushvalue(L1, getindex);
    }
    else if EQ("pushfstringI") {
      ilya_pushfstring(L1, ilya_tostring(L, -2), (int)ilya_tointeger(L, -1));
    }
    else if EQ("pushfstringS") {
      ilya_pushfstring(L1, ilya_tostring(L, -2), ilya_tostring(L, -1));
    }
    else if EQ("pushfstringP") {
      ilya_pushfstring(L1, ilya_tostring(L, -2), ilya_topointer(L, -1));
    }
    else if EQ("rawget") {
      int t = getindex;
      ilya_rawget(L1, t);
    }
    else if EQ("rawgeti") {
      int t = getindex;
      ilya_rawgeti(L1, t, getnum);
    }
    else if EQ("rawgetp") {
      int t = getindex;
      ilya_rawgetp(L1, t, cast_voidp(cast_sizet(getnum)));
    }
    else if EQ("rawset") {
      int t = getindex;
      ilya_rawset(L1, t);
    }
    else if EQ("rawseti") {
      int t = getindex;
      ilya_rawseti(L1, t, getnum);
    }
    else if EQ("rawsetp") {
      int t = getindex;
      ilya_rawsetp(L1, t, cast_voidp(cast_sizet(getnum)));
    }
    else if EQ("remove") {
      ilya_remove(L1, getnum);
    }
    else if EQ("replace") {
      ilya_replace(L1, getindex);
    }
    else if EQ("resume") {
      int i = getindex;
      int nres;
      status = ilya_resume(ilya_tothread(L1, i), L, getnum, &nres);
    }
    else if EQ("traceback") {
      const char *msg = getstring;
      int level = getnum;
      luaL_traceback(L1, L1, msg, level);
    }
    else if EQ("threadstatus") {
      ilya_pushstring(L1, statcodes[ilya_status(L1)]);
    }
    else if EQ("alloccount") {
      l_memcontrol.countlimit = cast_uint(getnum);
    }
    else if EQ("return") {
      int n = getnum;
      if (L1 != L) {
        int i;
        for (i = 0; i < n; i++) {
          int idx = -(n - i);
          switch (ilya_type(L1, idx)) {
            case ILYA_TBOOLEAN:
              ilya_pushboolean(L, ilya_toboolean(L1, idx));
              break;
            default:
              ilya_pushstring(L, ilya_tostring(L1, idx));
              break;
          }
        }
      }
      return n;
    }
    else if EQ("rotate") {
      int i = getindex;
      ilya_rotate(L1, i, getnum);
    }
    else if EQ("setfield") {
      int t = getindex;
      const char *s = getstring;
      ilya_setfield(L1, t, s);
    }
    else if EQ("seti") {
      int t = getindex;
      ilya_seti(L1, t, getnum);
    }
    else if EQ("setglobal") {
      const char *s = getstring;
      ilya_setglobal(L1, s);
    }
    else if EQ("sethook") {
      int mask = getnum;
      int count = getnum;
      const char *s = getstring;
      sethookaux(L1, mask, count, s);
    }
    else if EQ("setmetatable") {
      int idx = getindex;
      ilya_setmetatable(L1, idx);
    }
    else if EQ("settable") {
      ilya_settable(L1, getindex);
    }
    else if EQ("settop") {
      ilya_settop(L1, getnum);
    }
    else if EQ("testudata") {
      int i = getindex;
      ilya_pushboolean(L1, luaL_testudata(L1, i, getstring) != NULL);
    }
    else if EQ("error") {
      ilya_error(L1);
    }
    else if EQ("abort") {
      abort();
    }
    else if EQ("throw") {
#if defined(__cplusplus)
static struct X { int x; } x;
      throw x;
#else
      luaL_error(L1, "C++");
#endif
      break;
    }
    else if EQ("tobool") {
      ilya_pushboolean(L1, ilya_toboolean(L1, getindex));
    }
    else if EQ("tocfunction") {
      ilya_pushcfunction(L1, ilya_tocfunction(L1, getindex));
    }
    else if EQ("tointeger") {
      ilya_pushinteger(L1, ilya_tointeger(L1, getindex));
    }
    else if EQ("tonumber") {
      ilya_pushnumber(L1, ilya_tonumber(L1, getindex));
    }
    else if EQ("topointer") {
      ilya_pushlightuserdata(L1, cast_voidp(ilya_topointer(L1, getindex)));
    }
    else if EQ("touserdata") {
      ilya_pushlightuserdata(L1, ilya_touserdata(L1, getindex));
    }
    else if EQ("tostring") {
      const char *s = ilya_tostring(L1, getindex);
      const char *s1 = ilya_pushstring(L1, s);
      cast_void(s1);  /* to avoid warnings */
      ilya_longassert((s == NULL && s1 == NULL) || strcmp(s, s1) == 0);
    }
    else if EQ("Ltolstring") {
      luaL_tolstring(L1, getindex, NULL);
    }
    else if EQ("type") {
      ilya_pushstring(L1, luaL_typename(L1, getnum));
    }
    else if EQ("xmove") {
      int f = getindex;
      int t = getindex;
      ilya_State *fs = (f == 0) ? L1 : ilya_tothread(L1, f);
      ilya_State *ts = (t == 0) ? L1 : ilya_tothread(L1, t);
      int n = getnum;
      if (n == 0) n = ilya_gettop(fs);
      ilya_xmove(fs, ts, n);
    }
    else if EQ("isyieldable") {
      ilya_pushboolean(L1, ilya_isyieldable(ilya_tothread(L1, getindex)));
    }
    else if EQ("yield") {
      return ilya_yield(L1, getnum);
    }
    else if EQ("yieldk") {
      int nres = getnum;
      int i = getindex;
      return ilya_yieldk(L1, nres, i, Cfunck);
    }
    else if EQ("toclose") {
      ilya_toclose(L1, getnum);
    }
    else if EQ("closeslot") {
      ilya_closeslot(L1, getnum);
    }
    else if EQ("argerror") {
      int arg = getnum;
      luaL_argerror(L1, arg, getstring);
    }
    else luaL_error(L, "unknown instruction %s", buff);
  }
  return 0;
}


static int testC (ilya_State *L) {
  ilya_State *L1;
  const char *pc;
  if (ilya_isuserdata(L, 1)) {
    L1 = getstate(L);
    pc = luaL_checkstring(L, 2);
  }
  else if (ilya_isthread(L, 1)) {
    L1 = ilya_tothread(L, 1);
    pc = luaL_checkstring(L, 2);
  }
  else {
    L1 = L;
    pc = luaL_checkstring(L, 1);
  }
  return runC(L, L1, pc);
}


static int Cfunc (ilya_State *L) {
  return runC(L, L, ilya_tostring(L, ilya_upvalueindex(1)));
}


static int Cfunck (ilya_State *L, int status, ilya_KContext ctx) {
  ilya_pushstring(L, statcodes[status]);
  ilya_setglobal(L, "status");
  ilya_pushinteger(L, cast(ilya_Integer, ctx));
  ilya_setglobal(L, "ctx");
  return runC(L, L, ilya_tostring(L, cast_int(ctx)));
}


static int makeCfunc (ilya_State *L) {
  luaL_checkstring(L, 1);
  ilya_pushcclosure(L, Cfunc, ilya_gettop(L));
  return 1;
}


/* }====================================================== */


/*
** {======================================================
** tests for C hooks
** =======================================================
*/

/*
** C hook that runs the C script stored in registry.C_HOOK[L]
*/
static void Chook (ilya_State *L, ilya_Debug *ar) {
  const char *scpt;
  const char *const events [] = {"call", "ret", "line", "count", "tailcall"};
  ilya_getfield(L, ILYA_REGISTRYINDEX, "C_HOOK");
  ilya_pushlightuserdata(L, L);
  ilya_gettable(L, -2);  /* get C_HOOK[L] (script saved by sethookaux) */
  scpt = ilya_tostring(L, -1);  /* not very religious (string will be popped) */
  ilya_pop(L, 2);  /* remove C_HOOK and script */
  ilya_pushstring(L, events[ar->event]);  /* may be used by script */
  ilya_pushinteger(L, ar->currentline);  /* may be used by script */
  runC(L, L, scpt);  /* run script from C_HOOK[L] */
}


/*
** sets 'registry.C_HOOK[L] = scpt' and sets 'Chook' as a hook
*/
static void sethookaux (ilya_State *L, int mask, int count, const char *scpt) {
  if (*scpt == '\0') {  /* no script? */
    ilya_sethook(L, NULL, 0, 0);  /* turn off hooks */
    return;
  }
  ilya_getfield(L, ILYA_REGISTRYINDEX, "C_HOOK");  /* get C_HOOK table */
  if (!ilya_istable(L, -1)) {  /* no hook table? */
    ilya_pop(L, 1);  /* remove previous value */
    ilya_newtable(L);  /* create new C_HOOK table */
    ilya_pushvalue(L, -1);
    ilya_setfield(L, ILYA_REGISTRYINDEX, "C_HOOK");  /* register it */
  }
  ilya_pushlightuserdata(L, L);
  ilya_pushstring(L, scpt);
  ilya_settable(L, -3);  /* C_HOOK[L] = script */
  ilya_sethook(L, Chook, mask, count);
}


static int sethook (ilya_State *L) {
  if (ilya_isnoneornil(L, 1))
    ilya_sethook(L, NULL, 0, 0);  /* turn off hooks */
  else {
    const char *scpt = luaL_checkstring(L, 1);
    const char *smask = luaL_checkstring(L, 2);
    int count = cast_int(luaL_optinteger(L, 3, 0));
    int mask = 0;
    if (strchr(smask, 'c')) mask |= ILYA_MASKCALL;
    if (strchr(smask, 'r')) mask |= ILYA_MASKRET;
    if (strchr(smask, 'l')) mask |= ILYA_MASKLINE;
    if (count > 0) mask |= ILYA_MASKCOUNT;
    sethookaux(L, mask, count, scpt);
  }
  return 0;
}


static int coresume (ilya_State *L) {
  int status, nres;
  ilya_State *co = ilya_tothread(L, 1);
  luaL_argcheck(L, co, 1, "coroutine expected");
  status = ilya_resume(co, L, 0, &nres);
  if (status != ILYA_OK && status != ILYA_YIELD) {
    ilya_pushboolean(L, 0);
    ilya_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    ilya_pushboolean(L, 1);
    return 1;
  }
}

/* }====================================================== */



static const struct luaL_Reg tests_funcs[] = {
  {"checkmemory", ilya_checkmemory},
  {"closestate", closestate},
  {"d2s", d2s},
  {"doonnewstack", doonnewstack},
  {"doremote", doremote},
  {"gccolor", gc_color},
  {"gcage", gc_age},
  {"gcstate", gc_state},
  {"tracegc", tracegc},
  {"pobj", gc_printobj},
  {"getref", getref},
  {"hash", hash_query},
  {"log2", log2_aux},
  {"limits", get_limits},
  {"listcode", listcode},
  {"printcode", printcode},
  {"listk", listk},
  {"listabslineinfo", listabslineinfo},
  {"listlocals", listlocals},
  {"loadlib", loadlib},
  {"checkpanic", checkpanic},
  {"newstate", newstate},
  {"newuserdata", newuserdata},
  {"num2int", num2int},
  {"makeseed", makeseed},
  {"pushuserdata", pushuserdata},
  {"gcquery", gc_query},
  {"querystr", string_query},
  {"querytab", table_query},
  {"codeparam", test_codeparam},
  {"applyparam", test_applyparam},
  {"ref", tref},
  {"resume", coresume},
  {"s2d", s2d},
  {"sethook", sethook},
  {"stacklevel", stacklevel},
  {"testC", testC},
  {"makeCfunc", makeCfunc},
  {"totalmem", mem_query},
  {"alloccount", alloc_count},
  {"allocfailnext", alloc_failnext},
  {"trick", settrick},
  {"udataval", udataval},
  {"unref", unref},
  {"upvalue", upvalue},
  {"externKstr", externKstr},
  {"externstr", externstr},
  {NULL, NULL}
};


static void checkfinalmem (void) {
  ilya_assert(l_memcontrol.numblocks == 0);
  ilya_assert(l_memcontrol.total == 0);
}


int luaB_opentests (ilya_State *L) {
  void *ud;
  ilya_Alloc f = ilya_getallocf(L, &ud);
  ilya_atpanic(L, &tpanic);
  ilya_setwarnf(L, &warnf, L);
  ilya_pushboolean(L, 0);
  ilya_setglobal(L, "_WARN");  /* _WARN = false */
  regcodes(L);
  atexit(checkfinalmem);
  ilya_assert(f == debug_realloc && ud == cast_voidp(&l_memcontrol));
  ilya_setallocf(L, f, ud);  /* exercise this fn */
  luaL_newlib(L, tests_funcs);
  return 1;
}

#endif

