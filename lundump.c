/*
** $Id: lundump.c $
** load precompiled Ilya chunks
** See Copyright Notice in ilya.h
*/

#define lundump_c
#define ILYA_CORE

#include "lprefix.h"


#include <limits.h>
#include <string.h>

#include "ilya.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstring.h"
#include "ltable.h"
#include "lundump.h"
#include "lzio.h"


#if !defined(ilyai_verifycode)
#define ilyai_verifycode(L,f)  /* empty */
#endif


typedef struct {
  ilya_State *L;
  ZIO *Z;
  const char *name;
  Table *h;  /* list for string reuse */
  size_t offset;  /* current position relative to beginning of dump */
  ilya_Integer nstr;  /* number of strings in the list */
  lu_byte fixed;  /* dump is fixed in memory */
} LoadState;


static l_noret error (LoadState *S, const char *why) {
  ilyaO_pushfstring(S->L, "%s: bad binary format (%s)", S->name, why);
  ilyaD_throw(S->L, ILYA_ERRSYNTAX);
}


/*
** All high-level loads go through loadVector; you can change it to
** adapt to the endianness of the input
*/
#define loadVector(S,b,n)	loadBlock(S,b,(n)*sizeof((b)[0]))

static void loadBlock (LoadState *S, void *b, size_t size) {
  if (ilyaZ_read(S->Z, b, size) != 0)
    error(S, "truncated chunk");
  S->offset += size;
}


static void loadAlign (LoadState *S, unsigned align) {
  unsigned padding = align - cast_uint(S->offset % align);
  if (padding < align) {  /* (padding == align) means no padding */
    ilya_Integer paddingContent;
    loadBlock(S, &paddingContent, padding);
    ilya_assert(S->offset % align == 0);
  }
}


#define getaddr(S,n,t)	cast(t *, getaddr_(S,(n) * sizeof(t)))

static const void *getaddr_ (LoadState *S, size_t size) {
  const void *block = ilyaZ_getaddr(S->Z, size);
  S->offset += size;
  if (block == NULL)
    error(S, "truncated fixed buffer");
  return block;
}


#define loadVar(S,x)		loadVector(S,&x,1)


static lu_byte loadByte (LoadState *S) {
  int b = zgetc(S->Z);
  if (b == EOZ)
    error(S, "truncated chunk");
  S->offset++;
  return cast_byte(b);
}


static size_t loadVarint (LoadState *S, size_t limit) {
  size_t x = 0;
  int b;
  limit >>= 7;
  do {
    b = loadByte(S);
    if (x > limit)
      error(S, "integer overflow");
    x = (x << 7) | (b & 0x7f);
  } while ((b & 0x80) != 0);
  return x;
}


static size_t loadSize (LoadState *S) {
  return loadVarint(S, MAX_SIZE);
}


/*
** Read an non-negative int */
static unsigned loadUint (LoadState *S) {
  return cast_uint(loadVarint(S, cast_sizet(INT_MAX)));
}


static int loadInt (LoadState *S) {
  return cast_int(loadVarint(S, cast_sizet(INT_MAX)));
}



static ilya_Number loadNumber (LoadState *S) {
  ilya_Number x;
  loadVar(S, x);
  return x;
}


static ilya_Integer loadInteger (LoadState *S) {
  ilya_Integer x;
  loadVar(S, x);
  return x;
}


/*
** Load a nullable string into slot 'sl' from prototype 'p'. The
** assignment to the slot and the barrier must be performed before any
** possible GC activity, to anchor the string. (Both 'loadVector' and
** 'ilyaH_setint' can call the GC.)
*/
static void loadString (LoadState *S, Proto *p, TString **sl) {
  ilya_State *L = S->L;
  TString *ts;
  TValue sv;
  size_t size = loadSize(S);
  if (size == 0) {  /* no string? */
    ilya_assert(*sl == NULL);  /* must be prefilled */
    return;
  }
  else if (size == 1) {  /* previously saved string? */
    ilya_Integer idx = cast(ilya_Integer, loadSize(S));  /* get its index */
    TValue stv;
    ilyaH_getint(S->h, idx, &stv);  /* get its value */
    *sl = ts = tsvalue(&stv);
    ilyaC_objbarrier(L, p, ts);
    return;  /* do not save it again */
  }
  else if ((size -= 2) <= ILYAI_MAXSHORTLEN) {  /* short string? */
    char buff[ILYAI_MAXSHORTLEN + 1];  /* extra space for '\0' */
    loadVector(S, buff, size + 1);  /* load string into buffer */
    *sl = ts = ilyaS_newlstr(L, buff, size);  /* create string */
    ilyaC_objbarrier(L, p, ts);
  }
  else if (S->fixed) {  /* for a fixed buffer, use a fixed string */
    const char *s = getaddr(S, size + 1, char);  /* get content address */
    *sl = ts = ilyaS_newextlstr(L, s, size, NULL, NULL);
    ilyaC_objbarrier(L, p, ts);
  }
  else {  /* create internal copy */
    *sl = ts = ilyaS_createlngstrobj(L, size);  /* create string */
    ilyaC_objbarrier(L, p, ts);
    loadVector(S, getlngstr(ts), size + 1);  /* load directly in final place */
  }
  /* add string to list of saved strings */
  S->nstr++;
  setsvalue(L, &sv, ts);
  ilyaH_setint(L, S->h, S->nstr, &sv);
  ilyaC_objbarrierback(L, obj2gco(S->h), ts);
}


static void loadCode (LoadState *S, Proto *f) {
  unsigned n = loadUint(S);
  loadAlign(S, sizeof(f->code[0]));
  if (S->fixed) {
    f->code = getaddr(S, n, Instruction);
    f->sizecode = cast_int(n);
  }
  else {
    f->code = ilyaM_newvectorchecked(S->L, n, Instruction);
    f->sizecode = cast_int(n);
    loadVector(S, f->code, n);
  }
}


static void loadFunction(LoadState *S, Proto *f);


static void loadConstants (LoadState *S, Proto *f) {
  unsigned i;
  unsigned n = loadUint(S);
  f->k = ilyaM_newvectorchecked(S->L, n, TValue);
  f->sizek = cast_int(n);
  for (i = 0; i < n; i++)
    setnilvalue(&f->k[i]);
  for (i = 0; i < n; i++) {
    TValue *o = &f->k[i];
    int t = loadByte(S);
    switch (t) {
      case ILYA_VNIL:
        setnilvalue(o);
        break;
      case ILYA_VFALSE:
        setbfvalue(o);
        break;
      case ILYA_VTRUE:
        setbtvalue(o);
        break;
      case ILYA_VNUMFLT:
        setfltvalue(o, loadNumber(S));
        break;
      case ILYA_VNUMINT:
        setivalue(o, loadInteger(S));
        break;
      case ILYA_VSHRSTR:
      case ILYA_VLNGSTR: {
        ilya_assert(f->source == NULL);
        loadString(S, f, &f->source);  /* use 'source' to anchor string */
        if (f->source == NULL)
          error(S, "bad format for constant string");
        setsvalue2n(S->L, o, f->source);  /* save it in the right place */
        f->source = NULL;
        break;
      }
      default: ilya_assert(0);
    }
  }
}


static void loadProtos (LoadState *S, Proto *f) {
  unsigned i;
  unsigned n = loadUint(S);
  f->p = ilyaM_newvectorchecked(S->L, n, Proto *);
  f->sizep = cast_int(n);
  for (i = 0; i < n; i++)
    f->p[i] = NULL;
  for (i = 0; i < n; i++) {
    f->p[i] = ilyaF_newproto(S->L);
    ilyaC_objbarrier(S->L, f, f->p[i]);
    loadFunction(S, f->p[i]);
  }
}


/*
** Load the upvalues for a fn. The names must be filled first,
** because the filling of the other fields can raise read errors and
** the creation of the error message can call an emergency collection;
** in that case all prototypes must be consistent for the GC.
*/
static void loadUpvalues (LoadState *S, Proto *f) {
  unsigned i;
  unsigned n = loadUint(S);
  f->upvalues = ilyaM_newvectorchecked(S->L, n, Upvaldesc);
  f->sizeupvalues = cast_int(n);
  for (i = 0; i < n; i++)  /* make array valid for GC */
    f->upvalues[i].name = NULL;
  for (i = 0; i < n; i++) {  /* following calls can raise errors */
    f->upvalues[i].instack = loadByte(S);
    f->upvalues[i].idx = loadByte(S);
    f->upvalues[i].kind = loadByte(S);
  }
}


static void loadDebug (LoadState *S, Proto *f) {
  unsigned i;
  unsigned n = loadUint(S);
  if (S->fixed) {
    f->lineinfo = getaddr(S, n, ls_byte);
    f->sizelineinfo = cast_int(n);
  }
  else {
    f->lineinfo = ilyaM_newvectorchecked(S->L, n, ls_byte);
    f->sizelineinfo = cast_int(n);
    loadVector(S, f->lineinfo, n);
  }
  n = loadUint(S);
  if (n > 0) {
    loadAlign(S, sizeof(int));
    if (S->fixed) {
      f->abslineinfo = getaddr(S, n, AbsLineInfo);
      f->sizeabslineinfo = cast_int(n);
    }
    else {
      f->abslineinfo = ilyaM_newvectorchecked(S->L, n, AbsLineInfo);
      f->sizeabslineinfo = cast_int(n);
      loadVector(S, f->abslineinfo, n);
    }
  }
  n = loadUint(S);
  f->locvars = ilyaM_newvectorchecked(S->L, n, LocVar);
  f->sizelocvars = cast_int(n);
  for (i = 0; i < n; i++)
    f->locvars[i].varname = NULL;
  for (i = 0; i < n; i++) {
    loadString(S, f, &f->locvars[i].varname);
    f->locvars[i].startpc = loadInt(S);
    f->locvars[i].endpc = loadInt(S);
  }
  n = loadUint(S);
  if (n != 0)  /* does it have debug information? */
    n = cast_uint(f->sizeupvalues);  /* must be this many */
  for (i = 0; i < n; i++)
    loadString(S, f, &f->upvalues[i].name);
}


static void loadFunction (LoadState *S, Proto *f) {
  f->linedefined = loadInt(S);
  f->lastlinedefined = loadInt(S);
  f->numparams = loadByte(S);
  f->flag = loadByte(S) & PF_ISVARARG;  /* get only the meaningful flags */
  if (S->fixed)
    f->flag |= PF_FIXED;  /* signal that code is fixed */
  f->maxstacksize = loadByte(S);
  loadCode(S, f);
  loadConstants(S, f);
  loadUpvalues(S, f);
  loadProtos(S, f);
  loadString(S, f, &f->source);
  loadDebug(S, f);
}


static void checkliteral (LoadState *S, const char *s, const char *msg) {
  char buff[sizeof(ILYA_SIGNATURE) + sizeof(ILYAC_DATA)]; /* larger than both */
  size_t len = strlen(s);
  loadVector(S, buff, len);
  if (memcmp(s, buff, len) != 0)
    error(S, msg);
}


static void fchecksize (LoadState *S, size_t size, const char *tname) {
  if (loadByte(S) != size)
    error(S, ilyaO_pushfstring(S->L, "%s size mismatch", tname));
}


#define checksize(S,t)	fchecksize(S,sizeof(t),#t)

static void checkHeader (LoadState *S) {
  /* skip 1st char (already read and checked) */
  checkliteral(S, &ILYA_SIGNATURE[1], "not a binary chunk");
  if (loadByte(S) != ILYAC_VERSION)
    error(S, "version mismatch");
  if (loadByte(S) != ILYAC_FORMAT)
    error(S, "format mismatch");
  checkliteral(S, ILYAC_DATA, "corrupted chunk");
  checksize(S, Instruction);
  checksize(S, ilya_Integer);
  checksize(S, ilya_Number);
  if (loadInteger(S) != ILYAC_INT)
    error(S, "integer format mismatch");
  if (loadNumber(S) != ILYAC_NUM)
    error(S, "float format mismatch");
}


/*
** Load precompiled chunk.
*/
LClosure *ilyaU_undump (ilya_State *L, ZIO *Z, const char *name, int fixed) {
  LoadState S;
  LClosure *cl;
  if (*name == '@' || *name == '=')
    S.name = name + 1;
  else if (*name == ILYA_SIGNATURE[0])
    S.name = "binary string";
  else
    S.name = name;
  S.L = L;
  S.Z = Z;
  S.fixed = cast_byte(fixed);
  S.offset = 1;  /* fist byte was already read */
  checkHeader(&S);
  cl = ilyaF_newLclosure(L, loadByte(&S));
  setclLvalue2s(L, L->top.p, cl);
  ilyaD_inctop(L);
  S.h = ilyaH_new(L);  /* create list of saved strings */
  S.nstr = 0;
  sethvalue2s(L, L->top.p, S.h);  /* anchor it */
  ilyaD_inctop(L);
  cl->p = ilyaF_newproto(L);
  ilyaC_objbarrier(L, cl, cl->p);
  loadFunction(&S, cl->p);
  ilya_assert(cl->nupvalues == cl->p->sizeupvalues);
  ilyai_verifycode(L, cl->p);
  L->top.p--;  /* pop table */
  return cl;
}

