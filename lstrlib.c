/*
** $Id: lstrlib.c $
** Standard library for string operations and pattern-matching
** See Copyright Notice in ilya.h
*/

#define lstrlib_c
#define ILYA_LIB

#include "lprefix.h"


#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ilya.h"

#include "lauxlib.h"
#include "ilyalib.h"
#include "llimits.h"


/*
** maximum number of captures that a pattern can do during
** pattern-matching. This limit is arbitrary, but must fit in
** an unsigned char.
*/
#if !defined(ILYA_MAXCAPTURES)
#define ILYA_MAXCAPTURES		32
#endif


static int str_len (ilya_State *L) {
  size_t l;
  ilyaL_checklstring(L, 1, &l);
  ilya_pushinteger(L, (ilya_Integer)l);
  return 1;
}


/*
** translate a relative initial string position
** (negative means back from end): clip result to [1, inf).
** The length of any string in Ilya must fit in a ilya_Integer,
** so there are no overflows in the casts.
** The inverted comparison avoids a possible overflow
** computing '-pos'.
*/
static size_t posrelatI (ilya_Integer pos, size_t len) {
  if (pos > 0)
    return (size_t)pos;
  else if (pos == 0)
    return 1;
  else if (pos < -(ilya_Integer)len)  /* inverted comparison */
    return 1;  /* clip to 1 */
  else return len + (size_t)pos + 1;
}


/*
** Gets an optional ending string position from argument 'arg',
** with default value 'def'.
** Negative means back from end: clip result to [0, len]
*/
static size_t getendpos (ilya_State *L, int arg, ilya_Integer def,
                         size_t len) {
  ilya_Integer pos = ilyaL_optinteger(L, arg, def);
  if (pos > (ilya_Integer)len)
    return len;
  else if (pos >= 0)
    return (size_t)pos;
  else if (pos < -(ilya_Integer)len)
    return 0;
  else return len + (size_t)pos + 1;
}


static int str_sub (ilya_State *L) {
  size_t l;
  const char *s = ilyaL_checklstring(L, 1, &l);
  size_t start = posrelatI(ilyaL_checkinteger(L, 2), l);
  size_t end = getendpos(L, 3, -1, l);
  if (start <= end)
    ilya_pushlstring(L, s + start - 1, (end - start) + 1);
  else ilya_pushliteral(L, "");
  return 1;
}


static int str_reverse (ilya_State *L) {
  size_t l, i;
  ilyaL_Buffer b;
  const char *s = ilyaL_checklstring(L, 1, &l);
  char *p = ilyaL_buffinitsize(L, &b, l);
  for (i = 0; i < l; i++)
    p[i] = s[l - i - 1];
  ilyaL_pushresultsize(&b, l);
  return 1;
}


static int str_lower (ilya_State *L) {
  size_t l;
  size_t i;
  ilyaL_Buffer b;
  const char *s = ilyaL_checklstring(L, 1, &l);
  char *p = ilyaL_buffinitsize(L, &b, l);
  for (i=0; i<l; i++)
    p[i] = cast_char(tolower(cast_uchar(s[i])));
  ilyaL_pushresultsize(&b, l);
  return 1;
}


static int str_upper (ilya_State *L) {
  size_t l;
  size_t i;
  ilyaL_Buffer b;
  const char *s = ilyaL_checklstring(L, 1, &l);
  char *p = ilyaL_buffinitsize(L, &b, l);
  for (i=0; i<l; i++)
    p[i] = cast_char(toupper(cast_uchar(s[i])));
  ilyaL_pushresultsize(&b, l);
  return 1;
}


static int str_rep (ilya_State *L) {
  size_t l, lsep;
  const char *s = ilyaL_checklstring(L, 1, &l);
  ilya_Integer n = ilyaL_checkinteger(L, 2);
  const char *sep = ilyaL_optlstring(L, 3, "", &lsep);
  if (n <= 0)
    ilya_pushliteral(L, "");
  else if (l_unlikely(l + lsep < l || l + lsep > MAX_SIZE / cast_sizet(n)))
    return ilyaL_error(L, "resulting string too large");
  else {
    size_t totallen = ((size_t)n * (l + lsep)) - lsep;
    ilyaL_Buffer b;
    char *p = ilyaL_buffinitsize(L, &b, totallen);
    while (n-- > 1) {  /* first n-1 copies (followed by separator) */
      memcpy(p, s, l * sizeof(char)); p += l;
      if (lsep > 0) {  /* empty 'memcpy' is not that cheap */
        memcpy(p, sep, lsep * sizeof(char));
        p += lsep;
      }
    }
    memcpy(p, s, l * sizeof(char));  /* last copy (not followed by separator) */
    ilyaL_pushresultsize(&b, totallen);
  }
  return 1;
}


static int str_byte (ilya_State *L) {
  size_t l;
  const char *s = ilyaL_checklstring(L, 1, &l);
  ilya_Integer pi = ilyaL_optinteger(L, 2, 1);
  size_t posi = posrelatI(pi, l);
  size_t pose = getendpos(L, 3, pi, l);
  int n, i;
  if (posi > pose) return 0;  /* empty interval; return no values */
  if (l_unlikely(pose - posi >= (size_t)INT_MAX))  /* arithmetic overflow? */
    return ilyaL_error(L, "string slice too long");
  n = (int)(pose -  posi) + 1;
  ilyaL_checkstack(L, n, "string slice too long");
  for (i=0; i<n; i++)
    ilya_pushinteger(L, cast_uchar(s[posi + cast_uint(i) - 1]));
  return n;
}


static int str_char (ilya_State *L) {
  int n = ilya_gettop(L);  /* number of arguments */
  int i;
  ilyaL_Buffer b;
  char *p = ilyaL_buffinitsize(L, &b, cast_uint(n));
  for (i=1; i<=n; i++) {
    ilya_Unsigned c = (ilya_Unsigned)ilyaL_checkinteger(L, i);
    ilyaL_argcheck(L, c <= (ilya_Unsigned)UCHAR_MAX, i, "value out of range");
    p[i - 1] = cast_char(cast_uchar(c));
  }
  ilyaL_pushresultsize(&b, cast_uint(n));
  return 1;
}


/*
** Buffer to store the result of 'string.dump'. It must be initialized
** after the call to 'ilya_dump', to ensure that the fn is on the
** top of the stack when 'ilya_dump' is called. ('ilyaL_buffinit' might
** push stuff.)
*/
struct str_Writer {
  int init;  /* true iff buffer has been initialized */
  ilyaL_Buffer B;
};


static int writer (ilya_State *L, const void *b, size_t size, void *ud) {
  struct str_Writer *state = (struct str_Writer *)ud;
  if (!state->init) {
    state->init = 1;
    ilyaL_buffinit(L, &state->B);
  }
  if (b == NULL) {  /* finishing dump? */
    ilyaL_pushresult(&state->B);  /* push result */
    ilya_replace(L, 1);  /* move it to reserved slot */
  }
  else
    ilyaL_addlstring(&state->B, (const char *)b, size);
  return 0;
}


static int str_dump (ilya_State *L) {
  struct str_Writer state;
  int strip = ilya_toboolean(L, 2);
  ilyaL_argcheck(L, ilya_type(L, 1) == ILYA_TFUNCTION && !ilya_iscfunction(L, 1),
                   1, "Ilya fn expected");
  /* ensure fn is on the top of the stack and vacate slot 1 */
  ilya_pushvalue(L, 1);
  state.init = 0;
  ilya_dump(L, writer, &state, strip);
  ilya_settop(L, 1);  /* leave final result on top */
  return 1;
}



/*
** {======================================================
** METAMETHODS
** =======================================================
*/

#if defined(ILYA_NOCVTS2N)	/* { */

/* no coercion from strings to numbers */

static const ilyaL_Reg stringmetamethods[] = {
  {"__index", NULL},  /* placeholder */
  {NULL, NULL}
};

#else		/* }{ */

static int tonum (ilya_State *L, int arg) {
  if (ilya_type(L, arg) == ILYA_TNUMBER) {  /* already a number? */
    ilya_pushvalue(L, arg);
    return 1;
  }
  else {  /* check whether it is a numerical string */
    size_t len;
    const char *s = ilya_tolstring(L, arg, &len);
    return (s != NULL && ilya_stringtonumber(L, s) == len + 1);
  }
}


static void trymt (ilya_State *L, const char *mtname) {
  ilya_settop(L, 2);  /* back to the original arguments */
  if (l_unlikely(ilya_type(L, 2) == ILYA_TSTRING ||
                 !ilyaL_getmetafield(L, 2, mtname)))
    ilyaL_error(L, "attempt to %s a '%s' with a '%s'", mtname + 2,
                  ilyaL_typename(L, -2), ilyaL_typename(L, -1));
  ilya_insert(L, -3);  /* put metamethod before arguments */
  ilya_call(L, 2, 1);  /* call metamethod */
}


static int arith (ilya_State *L, int op, const char *mtname) {
  if (tonum(L, 1) && tonum(L, 2))
    ilya_arith(L, op);  /* result will be on the top */
  else
    trymt(L, mtname);
  return 1;
}


static int arith_add (ilya_State *L) {
  return arith(L, ILYA_OPADD, "__add");
}

static int arith_sub (ilya_State *L) {
  return arith(L, ILYA_OPSUB, "__sub");
}

static int arith_mul (ilya_State *L) {
  return arith(L, ILYA_OPMUL, "__mul");
}

static int arith_mod (ilya_State *L) {
  return arith(L, ILYA_OPMOD, "__mod");
}

static int arith_pow (ilya_State *L) {
  return arith(L, ILYA_OPPOW, "__pow");
}

static int arith_div (ilya_State *L) {
  return arith(L, ILYA_OPDIV, "__div");
}

static int arith_idiv (ilya_State *L) {
  return arith(L, ILYA_OPIDIV, "__idiv");
}

static int arith_unm (ilya_State *L) {
  return arith(L, ILYA_OPUNM, "__unm");
}


static const ilyaL_Reg stringmetamethods[] = {
  {"__add", arith_add},
  {"__sub", arith_sub},
  {"__mul", arith_mul},
  {"__mod", arith_mod},
  {"__pow", arith_pow},
  {"__div", arith_div},
  {"__idiv", arith_idiv},
  {"__unm", arith_unm},
  {"__index", NULL},  /* placeholder */
  {NULL, NULL}
};

#endif		/* } */

/* }====================================================== */

/*
** {======================================================
** PATTERN MATCHING
** =======================================================
*/


#define CAP_UNFINISHED	(-1)
#define CAP_POSITION	(-2)


typedef struct MatchState {
  const char *src_init;  /* init of source string */
  const char *src_end;  /* end ('\0') of source string */
  const char *p_end;  /* end ('\0') of pattern */
  ilya_State *L;
  int matchdepth;  /* control for recursive depth (to avoid C stack overflow) */
  int level;  /* total number of captures (finished or unfinished) */
  struct {
    const char *init;
    ptrdiff_t len;  /* length or special value (CAP_*) */
  } capture[ILYA_MAXCAPTURES];
} MatchState;


/* recursive fn */
static const char *match (MatchState *ms, const char *s, const char *p);


/* maximum recursion depth for 'match' */
#if !defined(MAXCCALLS)
#define MAXCCALLS	200
#endif


#define L_ESC		'%'
#define SPECIALS	"^$*+?.([%-"


static int check_capture (MatchState *ms, int l) {
  l -= '1';
  if (l_unlikely(l < 0 || l >= ms->level ||
                 ms->capture[l].len == CAP_UNFINISHED))
    return ilyaL_error(ms->L, "invalid capture index %%%d", l + 1);
  return l;
}


static int capture_to_close (MatchState *ms) {
  int level = ms->level;
  for (level--; level>=0; level--)
    if (ms->capture[level].len == CAP_UNFINISHED) return level;
  return ilyaL_error(ms->L, "invalid pattern capture");
}


static const char *classend (MatchState *ms, const char *p) {
  switch (*p++) {
    case L_ESC: {
      if (l_unlikely(p == ms->p_end))
        ilyaL_error(ms->L, "malformed pattern (ends with '%%')");
      return p+1;
    }
    case '[': {
      if (*p == '^') p++;
      do {  /* look for a ']' */
        if (l_unlikely(p == ms->p_end))
          ilyaL_error(ms->L, "malformed pattern (missing ']')");
        if (*(p++) == L_ESC && p < ms->p_end)
          p++;  /* skip escapes (e.g. '%]') */
      } while (*p != ']');
      return p+1;
    }
    default: {
      return p;
    }
  }
}


static int match_class (int c, int cl) {
  int res;
  switch (tolower(cl)) {
    case 'a' : res = isalpha(c); break;
    case 'c' : res = iscntrl(c); break;
    case 'd' : res = isdigit(c); break;
    case 'g' : res = isgraph(c); break;
    case 'l' : res = islower(c); break;
    case 'p' : res = ispunct(c); break;
    case 's' : res = isspace(c); break;
    case 'u' : res = isupper(c); break;
    case 'w' : res = isalnum(c); break;
    case 'x' : res = isxdigit(c); break;
    case 'z' : res = (c == 0); break;  /* deprecated option */
    default: return (cl == c);
  }
  return (islower(cl) ? res : !res);
}


static int matchbracketclass (int c, const char *p, const char *ec) {
  int sig = 1;
  if (*(p+1) == '^') {
    sig = 0;
    p++;  /* skip the '^' */
  }
  while (++p < ec) {
    if (*p == L_ESC) {
      p++;
      if (match_class(c, cast_uchar(*p)))
        return sig;
    }
    else if ((*(p+1) == '-') && (p+2 < ec)) {
      p+=2;
      if (cast_uchar(*(p-2)) <= c && c <= cast_uchar(*p))
        return sig;
    }
    else if (cast_uchar(*p) == c) return sig;
  }
  return !sig;
}


static int singlematch (MatchState *ms, const char *s, const char *p,
                        const char *ep) {
  if (s >= ms->src_end)
    return 0;
  else {
    int c = cast_uchar(*s);
    switch (*p) {
      case '.': return 1;  /* matches any char */
      case L_ESC: return match_class(c, cast_uchar(*(p+1)));
      case '[': return matchbracketclass(c, p, ep-1);
      default:  return (cast_uchar(*p) == c);
    }
  }
}


static const char *matchbalance (MatchState *ms, const char *s,
                                   const char *p) {
  if (l_unlikely(p >= ms->p_end - 1))
    ilyaL_error(ms->L, "malformed pattern (missing arguments to '%%b')");
  if (*s != *p) return NULL;
  else {
    int b = *p;
    int e = *(p+1);
    int cont = 1;
    while (++s < ms->src_end) {
      if (*s == e) {
        if (--cont == 0) return s+1;
      }
      else if (*s == b) cont++;
    }
  }
  return NULL;  /* string ends out of balance */
}


static const char *max_expand (MatchState *ms, const char *s,
                                 const char *p, const char *ep) {
  ptrdiff_t i = 0;  /* counts maximum expand for item */
  while (singlematch(ms, s + i, p, ep))
    i++;
  /* keeps trying to match with the maximum repetitions */
  while (i>=0) {
    const char *res = match(ms, (s+i), ep+1);
    if (res) return res;
    i--;  /* else didn't match; reduce 1 repetition to try again */
  }
  return NULL;
}


static const char *min_expand (MatchState *ms, const char *s,
                                 const char *p, const char *ep) {
  for (;;) {
    const char *res = match(ms, s, ep+1);
    if (res != NULL)
      return res;
    else if (singlematch(ms, s, p, ep))
      s++;  /* try with one more repetition */
    else return NULL;
  }
}


static const char *start_capture (MatchState *ms, const char *s,
                                    const char *p, int what) {
  const char *res;
  int level = ms->level;
  if (level >= ILYA_MAXCAPTURES) ilyaL_error(ms->L, "too many captures");
  ms->capture[level].init = s;
  ms->capture[level].len = what;
  ms->level = level+1;
  if ((res=match(ms, s, p)) == NULL)  /* match failed? */
    ms->level--;  /* undo capture */
  return res;
}


static const char *end_capture (MatchState *ms, const char *s,
                                  const char *p) {
  int l = capture_to_close(ms);
  const char *res;
  ms->capture[l].len = s - ms->capture[l].init;  /* close capture */
  if ((res = match(ms, s, p)) == NULL)  /* match failed? */
    ms->capture[l].len = CAP_UNFINISHED;  /* undo capture */
  return res;
}


static const char *match_capture (MatchState *ms, const char *s, int l) {
  size_t len;
  l = check_capture(ms, l);
  len = cast_sizet(ms->capture[l].len);
  if ((size_t)(ms->src_end-s) >= len &&
      memcmp(ms->capture[l].init, s, len) == 0)
    return s+len;
  else return NULL;
}


static const char *match (MatchState *ms, const char *s, const char *p) {
  if (l_unlikely(ms->matchdepth-- == 0))
    ilyaL_error(ms->L, "pattern too complex");
  init: /* using goto to optimize tail recursion */
  if (p != ms->p_end) {  /* end of pattern? */
    switch (*p) {
      case '(': {  /* start capture */
        if (*(p + 1) == ')')  /* position capture? */
          s = start_capture(ms, s, p + 2, CAP_POSITION);
        else
          s = start_capture(ms, s, p + 1, CAP_UNFINISHED);
        break;
      }
      case ')': {  /* end capture */
        s = end_capture(ms, s, p + 1);
        break;
      }
      case '$': {
        if ((p + 1) != ms->p_end)  /* is the '$' the last char in pattern? */
          goto dflt;  /* no; go to default */
        s = (s == ms->src_end) ? s : NULL;  /* check end of string */
        break;
      }
      case L_ESC: {  /* escaped sequences not in the format class[*+?-]? */
        switch (*(p + 1)) {
          case 'b': {  /* balanced string? */
            s = matchbalance(ms, s, p + 2);
            if (s != NULL) {
              p += 4; goto init;  /* return match(ms, s, p + 4); */
            }  /* else fail (s == NULL) */
            break;
          }
          case 'f': {  /* frontier? */
            const char *ep; char previous;
            p += 2;
            if (l_unlikely(*p != '['))
              ilyaL_error(ms->L, "missing '[' after '%%f' in pattern");
            ep = classend(ms, p);  /* points to what is next */
            previous = (s == ms->src_init) ? '\0' : *(s - 1);
            if (!matchbracketclass(cast_uchar(previous), p, ep - 1) &&
               matchbracketclass(cast_uchar(*s), p, ep - 1)) {
              p = ep; goto init;  /* return match(ms, s, ep); */
            }
            s = NULL;  /* match failed */
            break;
          }
          case '0': case '1': case '2': case '3':
          case '4': case '5': case '6': case '7':
          case '8': case '9': {  /* capture results (%0-%9)? */
            s = match_capture(ms, s, cast_uchar(*(p + 1)));
            if (s != NULL) {
              p += 2; goto init;  /* return match(ms, s, p + 2) */
            }
            break;
          }
          default: goto dflt;
        }
        break;
      }
      default: dflt: {  /* pattern class plus optional suffix */
        const char *ep = classend(ms, p);  /* points to optional suffix */
        /* does not match at least once? */
        if (!singlematch(ms, s, p, ep)) {
          if (*ep == '*' || *ep == '?' || *ep == '-') {  /* accept empty? */
            p = ep + 1; goto init;  /* return match(ms, s, ep + 1); */
          }
          else  /* '+' or no suffix */
            s = NULL;  /* fail */
        }
        else {  /* matched once */
          switch (*ep) {  /* handle optional suffix */
            case '?': {  /* optional */
              const char *res;
              if ((res = match(ms, s + 1, ep + 1)) != NULL)
                s = res;
              else {
                p = ep + 1; goto init;  /* else return match(ms, s, ep + 1); */
              }
              break;
            }
            case '+':  /* 1 or more repetitions */
              s++;  /* 1 match already done */
              /* FALLTHROUGH */
            case '*':  /* 0 or more repetitions */
              s = max_expand(ms, s, p, ep);
              break;
            case '-':  /* 0 or more repetitions (minimum) */
              s = min_expand(ms, s, p, ep);
              break;
            default:  /* no suffix */
              s++; p = ep; goto init;  /* return match(ms, s + 1, ep); */
          }
        }
        break;
      }
    }
  }
  ms->matchdepth++;
  return s;
}



static const char *lmemfind (const char *s1, size_t l1,
                               const char *s2, size_t l2) {
  if (l2 == 0) return s1;  /* empty strings are everywhere */
  else if (l2 > l1) return NULL;  /* avoids a negative 'l1' */
  else {
    const char *init;  /* to search for a '*s2' inside 's1' */
    l2--;  /* 1st char will be checked by 'memchr' */
    l1 = l1-l2;  /* 's2' cannot be found after that */
    while (l1 > 0 && (init = (const char *)memchr(s1, *s2, l1)) != NULL) {
      init++;   /* 1st char is already checked */
      if (memcmp(init, s2+1, l2) == 0)
        return init-1;
      else {  /* correct 'l1' and 's1' to try again */
        l1 -= ct_diff2sz(init - s1);
        s1 = init;
      }
    }
    return NULL;  /* not found */
  }
}


/*
** get information about the i-th capture. If there are no captures
** and 'i==0', return information about the whole match, which
** is the range 's'..'e'. If the capture is a string, return
** its length and put its address in '*cap'. If it is an integer
** (a position), push it on the stack and return CAP_POSITION.
*/
static ptrdiff_t get_onecapture (MatchState *ms, int i, const char *s,
                              const char *e, const char **cap) {
  if (i >= ms->level) {
    if (l_unlikely(i != 0))
      ilyaL_error(ms->L, "invalid capture index %%%d", i + 1);
    *cap = s;
    return (e - s);
  }
  else {
    ptrdiff_t capl = ms->capture[i].len;
    *cap = ms->capture[i].init;
    if (l_unlikely(capl == CAP_UNFINISHED))
      ilyaL_error(ms->L, "unfinished capture");
    else if (capl == CAP_POSITION)
      ilya_pushinteger(ms->L,
          ct_diff2S(ms->capture[i].init - ms->src_init) + 1);
    return capl;
  }
}


/*
** Push the i-th capture on the stack.
*/
static void push_onecapture (MatchState *ms, int i, const char *s,
                                                    const char *e) {
  const char *cap;
  ptrdiff_t l = get_onecapture(ms, i, s, e, &cap);
  if (l != CAP_POSITION)
    ilya_pushlstring(ms->L, cap, cast_sizet(l));
  /* else position was already pushed */
}


static int push_captures (MatchState *ms, const char *s, const char *e) {
  int i;
  int nlevels = (ms->level == 0 && s) ? 1 : ms->level;
  ilyaL_checkstack(ms->L, nlevels, "too many captures");
  for (i = 0; i < nlevels; i++)
    push_onecapture(ms, i, s, e);
  return nlevels;  /* number of strings pushed */
}


/* check whether pattern has no special characters */
static int nospecials (const char *p, size_t l) {
  size_t upto = 0;
  do {
    if (strpbrk(p + upto, SPECIALS))
      return 0;  /* pattern has a special character */
    upto += strlen(p + upto) + 1;  /* may have more after \0 */
  } while (upto <= l);
  return 1;  /* no special chars found */
}


static void prepstate (MatchState *ms, ilya_State *L,
                       const char *s, size_t ls, const char *p, size_t lp) {
  ms->L = L;
  ms->matchdepth = MAXCCALLS;
  ms->src_init = s;
  ms->src_end = s + ls;
  ms->p_end = p + lp;
}


static void reprepstate (MatchState *ms) {
  ms->level = 0;
  ilya_assert(ms->matchdepth == MAXCCALLS);
}


static int str_find_aux (ilya_State *L, int find) {
  size_t ls, lp;
  const char *s = ilyaL_checklstring(L, 1, &ls);
  const char *p = ilyaL_checklstring(L, 2, &lp);
  size_t init = posrelatI(ilyaL_optinteger(L, 3, 1), ls) - 1;
  if (init > ls) {  /* start after string's end? */
    ilyaL_pushfail(L);  /* cannot find anything */
    return 1;
  }
  /* explicit request or no special characters? */
  if (find && (ilya_toboolean(L, 4) || nospecials(p, lp))) {
    /* do a plain search */
    const char *s2 = lmemfind(s + init, ls - init, p, lp);
    if (s2) {
      ilya_pushinteger(L, ct_diff2S(s2 - s) + 1);
      ilya_pushinteger(L, cast_st2S(ct_diff2sz(s2 - s) + lp));
      return 2;
    }
  }
  else {
    MatchState ms;
    const char *s1 = s + init;
    int anchor = (*p == '^');
    if (anchor) {
      p++; lp--;  /* skip anchor character */
    }
    prepstate(&ms, L, s, ls, p, lp);
    do {
      const char *res;
      reprepstate(&ms);
      if ((res=match(&ms, s1, p)) != NULL) {
        if (find) {
          ilya_pushinteger(L, ct_diff2S(s1 - s) + 1);  /* start */
          ilya_pushinteger(L, ct_diff2S(res - s));   /* end */
          return push_captures(&ms, NULL, 0) + 2;
        }
        else
          return push_captures(&ms, s1, res);
      }
    } while (s1++ < ms.src_end && !anchor);
  }
  ilyaL_pushfail(L);  /* not found */
  return 1;
}


static int str_find (ilya_State *L) {
  return str_find_aux(L, 1);
}


static int str_match (ilya_State *L) {
  return str_find_aux(L, 0);
}


/* state for 'gmatch' */
typedef struct GMatchState {
  const char *src;  /* current position */
  const char *p;  /* pattern */
  const char *lastmatch;  /* end of last match */
  MatchState ms;  /* match state */
} GMatchState;


static int gmatch_aux (ilya_State *L) {
  GMatchState *gm = (GMatchState *)ilya_touserdata(L, ilya_upvalueindex(3));
  const char *src;
  gm->ms.L = L;
  for (src = gm->src; src <= gm->ms.src_end; src++) {
    const char *e;
    reprepstate(&gm->ms);
    if ((e = match(&gm->ms, src, gm->p)) != NULL && e != gm->lastmatch) {
      gm->src = gm->lastmatch = e;
      return push_captures(&gm->ms, src, e);
    }
  }
  return 0;  /* not found */
}


static int gmatch (ilya_State *L) {
  size_t ls, lp;
  const char *s = ilyaL_checklstring(L, 1, &ls);
  const char *p = ilyaL_checklstring(L, 2, &lp);
  size_t init = posrelatI(ilyaL_optinteger(L, 3, 1), ls) - 1;
  GMatchState *gm;
  ilya_settop(L, 2);  /* keep strings on closure to avoid being collected */
  gm = (GMatchState *)ilya_newuserdatauv(L, sizeof(GMatchState), 0);
  if (init > ls)  /* start after string's end? */
    init = ls + 1;  /* avoid overflows in 's + init' */
  prepstate(&gm->ms, L, s, ls, p, lp);
  gm->src = s + init; gm->p = p; gm->lastmatch = NULL;
  ilya_pushcclosure(L, gmatch_aux, 3);
  return 1;
}


static void add_s (MatchState *ms, ilyaL_Buffer *b, const char *s,
                                                   const char *e) {
  size_t l;
  ilya_State *L = ms->L;
  const char *news = ilya_tolstring(L, 3, &l);
  const char *p;
  while ((p = (char *)memchr(news, L_ESC, l)) != NULL) {
    ilyaL_addlstring(b, news, ct_diff2sz(p - news));
    p++;  /* skip ESC */
    if (*p == L_ESC)  /* '%%' */
      ilyaL_addchar(b, *p);
    else if (*p == '0')  /* '%0' */
        ilyaL_addlstring(b, s, ct_diff2sz(e - s));
    else if (isdigit(cast_uchar(*p))) {  /* '%n' */
      const char *cap;
      ptrdiff_t resl = get_onecapture(ms, *p - '1', s, e, &cap);
      if (resl == CAP_POSITION)
        ilyaL_addvalue(b);  /* add position to accumulated result */
      else
        ilyaL_addlstring(b, cap, cast_sizet(resl));
    }
    else
      ilyaL_error(L, "invalid use of '%c' in replacement string", L_ESC);
    l -= ct_diff2sz(p + 1 - news);
    news = p + 1;
  }
  ilyaL_addlstring(b, news, l);
}


/*
** Add the replacement value to the string buffer 'b'.
** Return true if the original string was changed. (Function calls and
** table indexing resulting in nil or false do not change the subject.)
*/
static int add_value (MatchState *ms, ilyaL_Buffer *b, const char *s,
                                      const char *e, int tr) {
  ilya_State *L = ms->L;
  switch (tr) {
    case ILYA_TFUNCTION: {  /* call the fn */
      int n;
      ilya_pushvalue(L, 3);  /* push the fn */
      n = push_captures(ms, s, e);  /* all captures as arguments */
      ilya_call(L, n, 1);  /* call it */
      break;
    }
    case ILYA_TTABLE: {  /* index the table */
      push_onecapture(ms, 0, s, e);  /* first capture is the index */
      ilya_gettable(L, 3);
      break;
    }
    default: {  /* ILYA_TNUMBER or ILYA_TSTRING */
      add_s(ms, b, s, e);  /* add value to the buffer */
      return 1;  /* something changed */
    }
  }
  if (!ilya_toboolean(L, -1)) {  /* nil or false? */
    ilya_pop(L, 1);  /* remove value */
    ilyaL_addlstring(b, s, ct_diff2sz(e - s));  /* keep original text */
    return 0;  /* no changes */
  }
  else if (l_unlikely(!ilya_isstring(L, -1)))
    return ilyaL_error(L, "invalid replacement value (a %s)",
                         ilyaL_typename(L, -1));
  else {
    ilyaL_addvalue(b);  /* add result to accumulator */
    return 1;  /* something changed */
  }
}


static int str_gsub (ilya_State *L) {
  size_t srcl, lp;
  const char *src = ilyaL_checklstring(L, 1, &srcl);  /* subject */
  const char *p = ilyaL_checklstring(L, 2, &lp);  /* pattern */
  const char *lastmatch = NULL;  /* end of last match */
  int tr = ilya_type(L, 3);  /* replacement type */
  /* max replacements */
  ilya_Integer max_s = ilyaL_optinteger(L, 4, cast_st2S(srcl) + 1);
  int anchor = (*p == '^');
  ilya_Integer n = 0;  /* replacement count */
  int changed = 0;  /* change flag */
  MatchState ms;
  ilyaL_Buffer b;
  ilyaL_argexpected(L, tr == ILYA_TNUMBER || tr == ILYA_TSTRING ||
                   tr == ILYA_TFUNCTION || tr == ILYA_TTABLE, 3,
                      "string/fn/table");
  ilyaL_buffinit(L, &b);
  if (anchor) {
    p++; lp--;  /* skip anchor character */
  }
  prepstate(&ms, L, src, srcl, p, lp);
  while (n < max_s) {
    const char *e;
    reprepstate(&ms);  /* (re)prepare state for new match */
    if ((e = match(&ms, src, p)) != NULL && e != lastmatch) {  /* match? */
      n++;
      changed = add_value(&ms, &b, src, e, tr) | changed;
      src = lastmatch = e;
    }
    else if (src < ms.src_end)  /* otherwise, skip one character */
      ilyaL_addchar(&b, *src++);
    else break;  /* end of subject */
    if (anchor) break;
  }
  if (!changed)  /* no changes? */
    ilya_pushvalue(L, 1);  /* return original string */
  else {  /* something changed */
    ilyaL_addlstring(&b, src, ct_diff2sz(ms.src_end - src));
    ilyaL_pushresult(&b);  /* create and return new string */
  }
  ilya_pushinteger(L, n);  /* number of substitutions */
  return 2;
}

/* }====================================================== */



/*
** {======================================================
** STRING FORMAT
** =======================================================
*/

#if !defined(ilya_number2strx)	/* { */

/*
** Hexadecimal floating-point formatter
*/

#define SIZELENMOD	(sizeof(ILYA_NUMBER_FRMLEN)/sizeof(char))


/*
** Number of bits that goes into the first digit. It can be any value
** between 1 and 4; the following definition tries to align the number
** to nibble boundaries by making what is left after that first digit a
** multiple of 4.
*/
#define L_NBFD		((l_floatatt(MANT_DIG) - 1)%4 + 1)


/*
** Add integer part of 'x' to buffer and return new 'x'
*/
static ilya_Number adddigit (char *buff, unsigned n, ilya_Number x) {
  ilya_Number dd = l_mathop(floor)(x);  /* get integer part from 'x' */
  int d = (int)dd;
  buff[n] = cast_char(d < 10 ? d + '0' : d - 10 + 'a');  /* add to buffer */
  return x - dd;  /* return what is left */
}


static int num2straux (char *buff, unsigned sz, ilya_Number x) {
  /* if 'inf' or 'NaN', format it like '%g' */
  if (x != x || x == (ilya_Number)HUGE_VAL || x == -(ilya_Number)HUGE_VAL)
    return l_sprintf(buff, sz, ILYA_NUMBER_FMT, (ILYAI_UACNUMBER)x);
  else if (x == 0) {  /* can be -0... */
    /* create "0" or "-0" followed by exponent */
    return l_sprintf(buff, sz, ILYA_NUMBER_FMT "x0p+0", (ILYAI_UACNUMBER)x);
  }
  else {
    int e;
    ilya_Number m = l_mathop(frexp)(x, &e);  /* 'x' fraction and exponent */
    unsigned n = 0;  /* character count */
    if (m < 0) {  /* is number negative? */
      buff[n++] = '-';  /* add sign */
      m = -m;  /* make it positive */
    }
    buff[n++] = '0'; buff[n++] = 'x';  /* add "0x" */
    m = adddigit(buff, n++, m * (1 << L_NBFD));  /* add first digit */
    e -= L_NBFD;  /* this digit goes before the radix point */
    if (m > 0) {  /* more digits? */
      buff[n++] = ilya_getlocaledecpoint();  /* add radix point */
      do {  /* add as many digits as needed */
        m = adddigit(buff, n++, m * 16);
      } while (m > 0);
    }
    n += cast_uint(l_sprintf(buff + n, sz - n, "p%+d", e));  /* add exponent */
    ilya_assert(n < sz);
    return cast_int(n);
  }
}


static int ilya_number2strx (ilya_State *L, char *buff, unsigned sz,
                            const char *fmt, ilya_Number x) {
  int n = num2straux(buff, sz, x);
  if (fmt[SIZELENMOD] == 'A') {
    int i;
    for (i = 0; i < n; i++)
      buff[i] = cast_char(toupper(cast_uchar(buff[i])));
  }
  else if (l_unlikely(fmt[SIZELENMOD] != 'a'))
    return ilyaL_error(L, "modifiers for format '%%a'/'%%A' not implemented");
  return n;
}

#endif				/* } */


/*
** Maximum size for items formatted with '%f'. This size is produced
** by format('%.99f', -maxfloat), and is equal to 99 + 3 ('-', '.',
** and '\0') + number of decimal digits to represent maxfloat (which
** is maximum exponent + 1). (99+3+1, adding some extra, 110)
*/
#define MAX_ITEMF	(110 + l_floatatt(MAX_10_EXP))


/*
** All formats except '%f' do not need that large limit.  The other
** float formats use exponents, so that they fit in the 99 limit for
** significant digits; 's' for large strings and 'q' add items directly
** to the buffer; all integer formats also fit in the 99 limit.  The
** worst case are floats: they may need 99 significant digits, plus
** '0x', '-', '.', 'e+XXXX', and '\0'. Adding some extra, 120.
*/
#define MAX_ITEM	120


/* valid flags in a format specification */
#if !defined(L_FMTFLAGSF)

/* valid flags for a, A, e, E, f, F, g, and G conversions */
#define L_FMTFLAGSF	"-+#0 "

/* valid flags for o, x, and X conversions */
#define L_FMTFLAGSX	"-#0"

/* valid flags for d and i conversions */
#define L_FMTFLAGSI	"-+0 "

/* valid flags for u conversions */
#define L_FMTFLAGSU	"-0"

/* valid flags for c, p, and s conversions */
#define L_FMTFLAGSC	"-"

#endif


/*
** Maximum size of each format specification (such as "%-099.99d"):
** Initial '%', flags (up to 5), width (2), period, precision (2),
** length modifier (8), conversion specifier, and final '\0', plus some
** extra.
*/
#define MAX_FORMAT	32


static void addquoted (ilyaL_Buffer *b, const char *s, size_t len) {
  ilyaL_addchar(b, '"');
  while (len--) {
    if (*s == '"' || *s == '\\' || *s == '\n') {
      ilyaL_addchar(b, '\\');
      ilyaL_addchar(b, *s);
    }
    else if (iscntrl(cast_uchar(*s))) {
      char buff[10];
      if (!isdigit(cast_uchar(*(s+1))))
        l_sprintf(buff, sizeof(buff), "\\%d", (int)cast_uchar(*s));
      else
        l_sprintf(buff, sizeof(buff), "\\%03d", (int)cast_uchar(*s));
      ilyaL_addstring(b, buff);
    }
    else
      ilyaL_addchar(b, *s);
    s++;
  }
  ilyaL_addchar(b, '"');
}


/*
** Serialize a floating-point number in such a way that it can be
** scanned back by Ilya. Use hexadecimal format for "common" numbers
** (to preserve precision); inf, -inf, and NaN are handled separately.
** (NaN cannot be expressed as a numeral, so we write '(0/0)' for it.)
*/
static int quotefloat (ilya_State *L, char *buff, ilya_Number n) {
  const char *s;  /* for the fixed representations */
  if (n == (ilya_Number)HUGE_VAL)  /* inf? */
    s = "1e9999";
  else if (n == -(ilya_Number)HUGE_VAL)  /* -inf? */
    s = "-1e9999";
  else if (n != n)  /* NaN? */
    s = "(0/0)";
  else {  /* format number as hexadecimal */
    int  nb = ilya_number2strx(L, buff, MAX_ITEM,
                                 "%" ILYA_NUMBER_FRMLEN "a", n);
    /* ensures that 'buff' string uses a dot as the radix character */
    if (memchr(buff, '.', cast_uint(nb)) == NULL) {  /* no dot? */
      char point = ilya_getlocaledecpoint();  /* try locale point */
      char *ppoint = (char *)memchr(buff, point, cast_uint(nb));
      if (ppoint) *ppoint = '.';  /* change it to a dot */
    }
    return nb;
  }
  /* for the fixed representations */
  return l_sprintf(buff, MAX_ITEM, "%s", s);
}


static void addliteral (ilya_State *L, ilyaL_Buffer *b, int arg) {
  switch (ilya_type(L, arg)) {
    case ILYA_TSTRING: {
      size_t len;
      const char *s = ilya_tolstring(L, arg, &len);
      addquoted(b, s, len);
      break;
    }
    case ILYA_TNUMBER: {
      char *buff = ilyaL_prepbuffsize(b, MAX_ITEM);
      int nb;
      if (!ilya_isinteger(L, arg))  /* float? */
        nb = quotefloat(L, buff, ilya_tonumber(L, arg));
      else {  /* integers */
        ilya_Integer n = ilya_tointeger(L, arg);
        const char *format = (n == ILYA_MININTEGER)  /* corner case? */
                           ? "0x%" ILYA_INTEGER_FRMLEN "x"  /* use hex */
                           : ILYA_INTEGER_FMT;  /* else use default format */
        nb = l_sprintf(buff, MAX_ITEM, format, (ILYAI_UACINT)n);
      }
      ilyaL_addsize(b, cast_uint(nb));
      break;
    }
    case ILYA_TNIL: case ILYA_TBOOLEAN: {
      ilyaL_tolstring(L, arg, NULL);
      ilyaL_addvalue(b);
      break;
    }
    default: {
      ilyaL_argerror(L, arg, "value has no literal form");
    }
  }
}


static const char *get2digits (const char *s) {
  if (isdigit(cast_uchar(*s))) {
    s++;
    if (isdigit(cast_uchar(*s))) s++;  /* (2 digits at most) */
  }
  return s;
}


/*
** Check whether a conversion specification is valid. When called,
** first character in 'form' must be '%' and last character must
** be a valid conversion specifier. 'flags' are the accepted flags;
** 'precision' signals whether to accept a precision.
*/
static void checkformat (ilya_State *L, const char *form, const char *flags,
                                       int precision) {
  const char *spec = form + 1;  /* skip '%' */
  spec += strspn(spec, flags);  /* skip flags */
  if (*spec != '0') {  /* a width cannot start with '0' */
    spec = get2digits(spec);  /* skip width */
    if (*spec == '.' && precision) {
      spec++;
      spec = get2digits(spec);  /* skip precision */
    }
  }
  if (!isalpha(cast_uchar(*spec)))  /* did not go to the end? */
    ilyaL_error(L, "invalid conversion specification: '%s'", form);
}


/*
** Get a conversion specification and copy it to 'form'.
** Return the address of its last character.
*/
static const char *getformat (ilya_State *L, const char *strfrmt,
                                            char *form) {
  /* spans flags, width, and precision ('0' is included as a flag) */
  size_t len = strspn(strfrmt, L_FMTFLAGSF "123456789.");
  len++;  /* adds following character (should be the specifier) */
  /* still needs space for '%', '\0', plus a length modifier */
  if (len >= MAX_FORMAT - 10)
    ilyaL_error(L, "invalid format (too long)");
  *(form++) = '%';
  memcpy(form, strfrmt, len * sizeof(char));
  *(form + len) = '\0';
  return strfrmt + len - 1;
}


/*
** add length modifier into formats
*/
static void addlenmod (char *form, const char *lenmod) {
  size_t l = strlen(form);
  size_t lm = strlen(lenmod);
  char spec = form[l - 1];
  strcpy(form + l - 1, lenmod);
  form[l + lm - 1] = spec;
  form[l + lm] = '\0';
}


static int str_format (ilya_State *L) {
  int top = ilya_gettop(L);
  int arg = 1;
  size_t sfl;
  const char *strfrmt = ilyaL_checklstring(L, arg, &sfl);
  const char *strfrmt_end = strfrmt+sfl;
  const char *flags;
  ilyaL_Buffer b;
  ilyaL_buffinit(L, &b);
  while (strfrmt < strfrmt_end) {
    if (*strfrmt != L_ESC)
      ilyaL_addchar(&b, *strfrmt++);
    else if (*++strfrmt == L_ESC)
      ilyaL_addchar(&b, *strfrmt++);  /* %% */
    else { /* format item */
      char form[MAX_FORMAT];  /* to store the format ('%...') */
      unsigned maxitem = MAX_ITEM;  /* maximum length for the result */
      char *buff = ilyaL_prepbuffsize(&b, maxitem);  /* to put result */
      int nb = 0;  /* number of bytes in result */
      if (++arg > top)
        return ilyaL_argerror(L, arg, "no value");
      strfrmt = getformat(L, strfrmt, form);
      switch (*strfrmt++) {
        case 'c': {
          checkformat(L, form, L_FMTFLAGSC, 0);
          nb = l_sprintf(buff, maxitem, form, (int)ilyaL_checkinteger(L, arg));
          break;
        }
        case 'd': case 'i':
          flags = L_FMTFLAGSI;
          goto intcase;
        case 'u':
          flags = L_FMTFLAGSU;
          goto intcase;
        case 'o': case 'x': case 'X':
          flags = L_FMTFLAGSX;
         intcase: {
          ilya_Integer n = ilyaL_checkinteger(L, arg);
          checkformat(L, form, flags, 1);
          addlenmod(form, ILYA_INTEGER_FRMLEN);
          nb = l_sprintf(buff, maxitem, form, (ILYAI_UACINT)n);
          break;
        }
        case 'a': case 'A':
          checkformat(L, form, L_FMTFLAGSF, 1);
          addlenmod(form, ILYA_NUMBER_FRMLEN);
          nb = ilya_number2strx(L, buff, maxitem, form,
                                  ilyaL_checknumber(L, arg));
          break;
        case 'f':
          maxitem = MAX_ITEMF;  /* extra space for '%f' */
          buff = ilyaL_prepbuffsize(&b, maxitem);
          /* FALLTHROUGH */
        case 'e': case 'E': case 'g': case 'G': {
          ilya_Number n = ilyaL_checknumber(L, arg);
          checkformat(L, form, L_FMTFLAGSF, 1);
          addlenmod(form, ILYA_NUMBER_FRMLEN);
          nb = l_sprintf(buff, maxitem, form, (ILYAI_UACNUMBER)n);
          break;
        }
        case 'p': {
          const void *p = ilya_topointer(L, arg);
          checkformat(L, form, L_FMTFLAGSC, 0);
          if (p == NULL) {  /* avoid calling 'printf' with argument NULL */
            p = "(null)";  /* result */
            form[strlen(form) - 1] = 's';  /* format it as a string */
          }
          nb = l_sprintf(buff, maxitem, form, p);
          break;
        }
        case 'q': {
          if (form[2] != '\0')  /* modifiers? */
            return ilyaL_error(L, "specifier '%%q' cannot have modifiers");
          addliteral(L, &b, arg);
          break;
        }
        case 's': {
          size_t l;
          const char *s = ilyaL_tolstring(L, arg, &l);
          if (form[2] == '\0')  /* no modifiers? */
            ilyaL_addvalue(&b);  /* keep entire string */
          else {
            ilyaL_argcheck(L, l == strlen(s), arg, "string contains zeros");
            checkformat(L, form, L_FMTFLAGSC, 1);
            if (strchr(form, '.') == NULL && l >= 100) {
              /* no precision and string is too long to be formatted */
              ilyaL_addvalue(&b);  /* keep entire string */
            }
            else {  /* format the string into 'buff' */
              nb = l_sprintf(buff, maxitem, form, s);
              ilya_pop(L, 1);  /* remove result from 'ilyaL_tolstring' */
            }
          }
          break;
        }
        default: {  /* also treat cases 'pnLlh' */
          return ilyaL_error(L, "invalid conversion '%s' to 'format'", form);
        }
      }
      ilya_assert(cast_uint(nb) < maxitem);
      ilyaL_addsize(&b, cast_uint(nb));
    }
  }
  ilyaL_pushresult(&b);
  return 1;
}

/* }====================================================== */


/*
** {======================================================
** PACK/UNPACK
** =======================================================
*/


/* value used for padding */
#if !defined(ILYAL_PACKPADBYTE)
#define ILYAL_PACKPADBYTE		0x00
#endif

/* maximum size for the binary representation of an integer */
#define MAXINTSIZE	16

/* number of bits in a character */
#define NB	CHAR_BIT

/* mask for one character (NB 1's) */
#define MC	((1 << NB) - 1)

/* size of a ilya_Integer */
#define SZINT	((int)sizeof(ilya_Integer))


/* dummy union to get native endianness */
static const union {
  int dummy;
  char little;  /* true iff machine is little endian */
} nativeendian = {1};


/*
** information to pack/unpack stuff
*/
typedef struct Header {
  ilya_State *L;
  int islittle;
  unsigned maxalign;
} Header;


/*
** options for pack/unpack
*/
typedef enum KOption {
  Kint,		/* signed integers */
  Kuint,	/* unsigned integers */
  Kfloat,	/* single-precision floating-point numbers */
  Knumber,	/* Ilya "native" floating-point numbers */
  Kdouble,	/* double-precision floating-point numbers */
  Kchar,	/* fixed-length strings */
  Kstring,	/* strings with prefixed length */
  Kzstr,	/* zero-terminated strings */
  Kpadding,	/* padding */
  Kpaddalign,	/* padding for alignment */
  Knop		/* no-op (configuration or spaces) */
} KOption;


/*
** Read an integer numeral from string 'fmt' or return 'df' if
** there is no numeral
*/
static int digit (int c) { return '0' <= c && c <= '9'; }

static size_t getnum (const char **fmt, size_t df) {
  if (!digit(**fmt))  /* no number? */
    return df;  /* return default value */
  else {
    size_t a = 0;
    do {
      a = a*10 + cast_uint(*((*fmt)++) - '0');
    } while (digit(**fmt) && a <= (MAX_SIZE - 9)/10);
    return a;
  }
}


/*
** Read an integer numeral and raises an error if it is larger
** than the maximum size of integers.
*/
static unsigned getnumlimit (Header *h, const char **fmt, size_t df) {
  size_t sz = getnum(fmt, df);
  if (l_unlikely((sz - 1u) >= MAXINTSIZE))
    return cast_uint(ilyaL_error(h->L,
               "integral size (%d) out of limits [1,%d]", sz, MAXINTSIZE));
  return cast_uint(sz);
}


/*
** Initialize Header
*/
static void initheader (ilya_State *L, Header *h) {
  h->L = L;
  h->islittle = nativeendian.little;
  h->maxalign = 1;
}


/*
** Read and classify next option. 'size' is filled with option's size.
*/
static KOption getoption (Header *h, const char **fmt, size_t *size) {
  /* dummy structure to get native alignment requirements */
  struct cD { char c; union { ILYAI_MAXALIGN; } u; };
  int opt = *((*fmt)++);
  *size = 0;  /* default */
  switch (opt) {
    case 'b': *size = sizeof(char); return Kint;
    case 'B': *size = sizeof(char); return Kuint;
    case 'h': *size = sizeof(short); return Kint;
    case 'H': *size = sizeof(short); return Kuint;
    case 'l': *size = sizeof(long); return Kint;
    case 'L': *size = sizeof(long); return Kuint;
    case 'j': *size = sizeof(ilya_Integer); return Kint;
    case 'J': *size = sizeof(ilya_Integer); return Kuint;
    case 'T': *size = sizeof(size_t); return Kuint;
    case 'f': *size = sizeof(float); return Kfloat;
    case 'n': *size = sizeof(ilya_Number); return Knumber;
    case 'd': *size = sizeof(double); return Kdouble;
    case 'i': *size = getnumlimit(h, fmt, sizeof(int)); return Kint;
    case 'I': *size = getnumlimit(h, fmt, sizeof(int)); return Kuint;
    case 's': *size = getnumlimit(h, fmt, sizeof(size_t)); return Kstring;
    case 'c':
      *size = getnum(fmt, cast_sizet(-1));
      if (l_unlikely(*size == cast_sizet(-1)))
        ilyaL_error(h->L, "missing size for format option 'c'");
      return Kchar;
    case 'z': return Kzstr;
    case 'x': *size = 1; return Kpadding;
    case 'X': return Kpaddalign;
    case ' ': break;
    case '<': h->islittle = 1; break;
    case '>': h->islittle = 0; break;
    case '=': h->islittle = nativeendian.little; break;
    case '!': {
      const size_t maxalign = offsetof(struct cD, u);
      h->maxalign = getnumlimit(h, fmt, maxalign);
      break;
    }
    default: ilyaL_error(h->L, "invalid format option '%c'", opt);
  }
  return Knop;
}


/*
** Read, classify, and fill other details about the next option.
** 'psize' is filled with option's size, 'notoalign' with its
** alignment requirements.
** Local variable 'size' gets the size to be aligned. (Kpadal option
** always gets its full alignment, other options are limited by
** the maximum alignment ('maxalign'). Kchar option needs no alignment
** despite its size.
*/
static KOption getdetails (Header *h, size_t totalsize, const char **fmt,
                           size_t *psize, unsigned *ntoalign) {
  KOption opt = getoption(h, fmt, psize);
  size_t align = *psize;  /* usually, alignment follows size */
  if (opt == Kpaddalign) {  /* 'X' gets alignment from following option */
    if (**fmt == '\0' || getoption(h, fmt, &align) == Kchar || align == 0)
      ilyaL_argerror(h->L, 1, "invalid next option for option 'X'");
  }
  if (align <= 1 || opt == Kchar)  /* need no alignment? */
    *ntoalign = 0;
  else {
    if (align > h->maxalign)  /* enforce maximum alignment */
      align = h->maxalign;
    if (l_unlikely(!ispow2(align)))  /* not a power of 2? */
      ilyaL_argerror(h->L, 1, "format asks for alignment not power of 2");
    else {
      /* 'szmoda' = totalsize % align */
      unsigned szmoda = cast_uint(totalsize & (align - 1));
      *ntoalign = cast_uint((align - szmoda) & (align - 1));
    }
  }
  return opt;
}


/*
** Pack integer 'n' with 'size' bytes and 'islittle' endianness.
** The final 'if' handles the case when 'size' is larger than
** the size of a Ilya integer, correcting the extra sign-extension
** bytes if necessary (by default they would be zeros).
*/
static void packint (ilyaL_Buffer *b, ilya_Unsigned n,
                     int islittle, unsigned size, int neg) {
  char *buff = ilyaL_prepbuffsize(b, size);
  unsigned i;
  buff[islittle ? 0 : size - 1] = (char)(n & MC);  /* first byte */
  for (i = 1; i < size; i++) {
    n >>= NB;
    buff[islittle ? i : size - 1 - i] = (char)(n & MC);
  }
  if (neg && size > SZINT) {  /* negative number need sign extension? */
    for (i = SZINT; i < size; i++)  /* correct extra bytes */
      buff[islittle ? i : size - 1 - i] = (char)MC;
  }
  ilyaL_addsize(b, size);  /* add result to buffer */
}


/*
** Copy 'size' bytes from 'src' to 'dest', correcting endianness if
** given 'islittle' is different from native endianness.
*/
static void copywithendian (char *dest, const char *src,
                            unsigned size, int islittle) {
  if (islittle == nativeendian.little)
    memcpy(dest, src, size);
  else {
    dest += size - 1;
    while (size-- != 0)
      *(dest--) = *(src++);
  }
}


static int str_pack (ilya_State *L) {
  ilyaL_Buffer b;
  Header h;
  const char *fmt = ilyaL_checkstring(L, 1);  /* format string */
  int arg = 1;  /* current argument to pack */
  size_t totalsize = 0;  /* accumulate total size of result */
  initheader(L, &h);
  ilya_pushnil(L);  /* mark to separate arguments from string buffer */
  ilyaL_buffinit(L, &b);
  while (*fmt != '\0') {
    unsigned ntoalign;
    size_t size;
    KOption opt = getdetails(&h, totalsize, &fmt, &size, &ntoalign);
    ilyaL_argcheck(L, size + ntoalign <= MAX_SIZE - totalsize, arg,
                     "result too long");
    totalsize += ntoalign + size;
    while (ntoalign-- > 0)
     ilyaL_addchar(&b, ILYAL_PACKPADBYTE);  /* fill alignment */
    arg++;
    switch (opt) {
      case Kint: {  /* signed integers */
        ilya_Integer n = ilyaL_checkinteger(L, arg);
        if (size < SZINT) {  /* need overflow check? */
          ilya_Integer lim = (ilya_Integer)1 << ((size * NB) - 1);
          ilyaL_argcheck(L, -lim <= n && n < lim, arg, "integer overflow");
        }
        packint(&b, (ilya_Unsigned)n, h.islittle, cast_uint(size), (n < 0));
        break;
      }
      case Kuint: {  /* unsigned integers */
        ilya_Integer n = ilyaL_checkinteger(L, arg);
        if (size < SZINT)  /* need overflow check? */
          ilyaL_argcheck(L, (ilya_Unsigned)n < ((ilya_Unsigned)1 << (size * NB)),
                           arg, "unsigned overflow");
        packint(&b, (ilya_Unsigned)n, h.islittle, cast_uint(size), 0);
        break;
      }
      case Kfloat: {  /* C float */
        float f = (float)ilyaL_checknumber(L, arg);  /* get argument */
        char *buff = ilyaL_prepbuffsize(&b, sizeof(f));
        /* move 'f' to final result, correcting endianness if needed */
        copywithendian(buff, (char *)&f, sizeof(f), h.islittle);
        ilyaL_addsize(&b, size);
        break;
      }
      case Knumber: {  /* Ilya float */
        ilya_Number f = ilyaL_checknumber(L, arg);  /* get argument */
        char *buff = ilyaL_prepbuffsize(&b, sizeof(f));
        /* move 'f' to final result, correcting endianness if needed */
        copywithendian(buff, (char *)&f, sizeof(f), h.islittle);
        ilyaL_addsize(&b, size);
        break;
      }
      case Kdouble: {  /* C double */
        double f = (double)ilyaL_checknumber(L, arg);  /* get argument */
        char *buff = ilyaL_prepbuffsize(&b, sizeof(f));
        /* move 'f' to final result, correcting endianness if needed */
        copywithendian(buff, (char *)&f, sizeof(f), h.islittle);
        ilyaL_addsize(&b, size);
        break;
      }
      case Kchar: {  /* fixed-size string */
        size_t len;
        const char *s = ilyaL_checklstring(L, arg, &len);
        ilyaL_argcheck(L, len <= size, arg, "string longer than given size");
        ilyaL_addlstring(&b, s, len);  /* add string */
        if (len < size) {  /* does it need padding? */
          size_t psize = size - len;  /* pad size */
          char *buff = ilyaL_prepbuffsize(&b, psize);
          memset(buff, ILYAL_PACKPADBYTE, psize);
          ilyaL_addsize(&b, psize);
        }
        break;
      }
      case Kstring: {  /* strings with length count */
        size_t len;
        const char *s = ilyaL_checklstring(L, arg, &len);
        ilyaL_argcheck(L, size >= sizeof(ilya_Unsigned) ||
                         len < ((ilya_Unsigned)1 << (size * NB)),
                         arg, "string length does not fit in given size");
        /* pack length */
        packint(&b, (ilya_Unsigned)len, h.islittle, cast_uint(size), 0);
        ilyaL_addlstring(&b, s, len);
        totalsize += len;
        break;
      }
      case Kzstr: {  /* zero-terminated string */
        size_t len;
        const char *s = ilyaL_checklstring(L, arg, &len);
        ilyaL_argcheck(L, strlen(s) == len, arg, "string contains zeros");
        ilyaL_addlstring(&b, s, len);
        ilyaL_addchar(&b, '\0');  /* add zero at the end */
        totalsize += len + 1;
        break;
      }
      case Kpadding: ilyaL_addchar(&b, ILYAL_PACKPADBYTE);  /* FALLTHROUGH */
      case Kpaddalign: case Knop:
        arg--;  /* undo increment */
        break;
    }
  }
  ilyaL_pushresult(&b);
  return 1;
}


static int str_packsize (ilya_State *L) {
  Header h;
  const char *fmt = ilyaL_checkstring(L, 1);  /* format string */
  size_t totalsize = 0;  /* accumulate total size of result */
  initheader(L, &h);
  while (*fmt != '\0') {
    unsigned ntoalign;
    size_t size;
    KOption opt = getdetails(&h, totalsize, &fmt, &size, &ntoalign);
    ilyaL_argcheck(L, opt != Kstring && opt != Kzstr, 1,
                     "variable-length format");
    size += ntoalign;  /* total space used by option */
    ilyaL_argcheck(L, totalsize <= ILYA_MAXINTEGER - size,
                     1, "format result too large");
    totalsize += size;
  }
  ilya_pushinteger(L, cast_st2S(totalsize));
  return 1;
}


/*
** Unpack an integer with 'size' bytes and 'islittle' endianness.
** If size is smaller than the size of a Ilya integer and integer
** is signed, must do sign extension (propagating the sign to the
** higher bits); if size is larger than the size of a Ilya integer,
** it must check the unread bytes to see whether they do not cause an
** overflow.
*/
static ilya_Integer unpackint (ilya_State *L, const char *str,
                              int islittle, int size, int issigned) {
  ilya_Unsigned res = 0;
  int i;
  int limit = (size  <= SZINT) ? size : SZINT;
  for (i = limit - 1; i >= 0; i--) {
    res <<= NB;
    res |= (ilya_Unsigned)(unsigned char)str[islittle ? i : size - 1 - i];
  }
  if (size < SZINT) {  /* real size smaller than ilya_Integer? */
    if (issigned) {  /* needs sign extension? */
      ilya_Unsigned mask = (ilya_Unsigned)1 << (size*NB - 1);
      res = ((res ^ mask) - mask);  /* do sign extension */
    }
  }
  else if (size > SZINT) {  /* must check unread bytes */
    int mask = (!issigned || (ilya_Integer)res >= 0) ? 0 : MC;
    for (i = limit; i < size; i++) {
      if (l_unlikely((unsigned char)str[islittle ? i : size - 1 - i] != mask))
        ilyaL_error(L, "%d-byte integer does not fit into Ilya Integer", size);
    }
  }
  return (ilya_Integer)res;
}


static int str_unpack (ilya_State *L) {
  Header h;
  const char *fmt = ilyaL_checkstring(L, 1);
  size_t ld;
  const char *data = ilyaL_checklstring(L, 2, &ld);
  size_t pos = posrelatI(ilyaL_optinteger(L, 3, 1), ld) - 1;
  int n = 0;  /* number of results */
  ilyaL_argcheck(L, pos <= ld, 3, "initial position out of string");
  initheader(L, &h);
  while (*fmt != '\0') {
    unsigned ntoalign;
    size_t size;
    KOption opt = getdetails(&h, pos, &fmt, &size, &ntoalign);
    ilyaL_argcheck(L, ntoalign + size <= ld - pos, 2,
                    "data string too short");
    pos += ntoalign;  /* skip alignment */
    /* stack space for item + next position */
    ilyaL_checkstack(L, 2, "too many results");
    n++;
    switch (opt) {
      case Kint:
      case Kuint: {
        ilya_Integer res = unpackint(L, data + pos, h.islittle,
                                       cast_int(size), (opt == Kint));
        ilya_pushinteger(L, res);
        break;
      }
      case Kfloat: {
        float f;
        copywithendian((char *)&f, data + pos, sizeof(f), h.islittle);
        ilya_pushnumber(L, (ilya_Number)f);
        break;
      }
      case Knumber: {
        ilya_Number f;
        copywithendian((char *)&f, data + pos, sizeof(f), h.islittle);
        ilya_pushnumber(L, f);
        break;
      }
      case Kdouble: {
        double f;
        copywithendian((char *)&f, data + pos, sizeof(f), h.islittle);
        ilya_pushnumber(L, (ilya_Number)f);
        break;
      }
      case Kchar: {
        ilya_pushlstring(L, data + pos, size);
        break;
      }
      case Kstring: {
        ilya_Unsigned len = (ilya_Unsigned)unpackint(L, data + pos,
                                          h.islittle, cast_int(size), 0);
        ilyaL_argcheck(L, len <= ld - pos - size, 2, "data string too short");
        ilya_pushlstring(L, data + pos + size, len);
        pos += len;  /* skip string */
        break;
      }
      case Kzstr: {
        size_t len = strlen(data + pos);
        ilyaL_argcheck(L, pos + len < ld, 2,
                         "unfinished string for format 'z'");
        ilya_pushlstring(L, data + pos, len);
        pos += len + 1;  /* skip string plus final '\0' */
        break;
      }
      case Kpaddalign: case Kpadding: case Knop:
        n--;  /* undo increment */
        break;
    }
    pos += size;
  }
  ilya_pushinteger(L, cast_st2S(pos) + 1);  /* next position */
  return n + 1;
}

/* }====================================================== */


static const ilyaL_Reg strlib[] = {
  {"byte", str_byte},
  {"char", str_char},
  {"dump", str_dump},
  {"find", str_find},
  {"format", str_format},
  {"gmatch", gmatch},
  {"gsub", str_gsub},
  {"len", str_len},
  {"lower", str_lower},
  {"match", str_match},
  {"rep", str_rep},
  {"reverse", str_reverse},
  {"sub", str_sub},
  {"upper", str_upper},
  {"pack", str_pack},
  {"packsize", str_packsize},
  {"unpack", str_unpack},
  {NULL, NULL}
};


static void createmetatable (ilya_State *L) {
  /* table to be metatable for strings */
  ilyaL_newlibtable(L, stringmetamethods);
  ilyaL_setfuncs(L, stringmetamethods, 0);
  ilya_pushliteral(L, "");  /* dummy string */
  ilya_pushvalue(L, -2);  /* copy table */
  ilya_setmetatable(L, -2);  /* set table as metatable for strings */
  ilya_pop(L, 1);  /* pop dummy string */
  ilya_pushvalue(L, -2);  /* get string library */
  ilya_setfield(L, -2, "__index");  /* metatable.__index = string */
  ilya_pop(L, 1);  /* pop metatable */
}


/*
** Open string library
*/
ILYAMOD_API int ilyaopen_string (ilya_State *L) {
  ilyaL_newlib(L, strlib);
  createmetatable(L);
  return 1;
}

