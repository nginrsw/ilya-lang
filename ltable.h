/*
** $Id: ltable.h $
** Ilya tables (hash)
** See Copyright Notice in ilya.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define gnode(t,i)	(&(t)->node[i])
#define gval(n)		(&(n)->i_val)
#define gnext(n)	((n)->u.next)


/*
** Clear all bits of fast-access metamethods, which means that the table
** may have any of these metamethods. (First access that fails after the
** clearing will set the bit again.)
*/
#define invalidateTMcache(t)	((t)->flags &= cast_byte(~maskflags))


/*
** Bit BITDUMMY set in 'flags' means the table is using the dummy node
** for its hash part.
*/

#define BITDUMMY		(1 << 6)
#define NOTBITDUMMY		cast_byte(~BITDUMMY)
#define isdummy(t)		((t)->flags & BITDUMMY)

#define setnodummy(t)		((t)->flags &= NOTBITDUMMY)
#define setdummy(t)		((t)->flags |= BITDUMMY)



/* allocated size for hash nodes */
#define allocsizenode(t)	(isdummy(t) ? 0 : sizenode(t))


/* returns the Node, given the value of a table entry */
#define nodefromval(v)	cast(Node *, (v))



#define ilyaH_fastgeti(t,k,res,tag) \
  { Table *h = t; ilya_Unsigned u = l_castS2U(k) - 1u; \
    if ((u < h->asize)) { \
      tag = *getArrTag(h, u); \
      if (!tagisempty(tag)) { farr2val(h, u, tag, res); }} \
    else { tag = ilyaH_getint(h, (k), res); }}


#define ilyaH_fastseti(t,k,val,hres) \
  { Table *h = t; ilya_Unsigned u = l_castS2U(k) - 1u; \
    if ((u < h->asize)) { \
      lu_byte *tag = getArrTag(h, u); \
      if (checknoTM(h->metatable, TM_NEWINDEX) || !tagisempty(*tag)) \
        { fval2arr(h, u, tag, val); hres = HOK; } \
      else hres = ~cast_int(u); } \
    else { hres = ilyaH_psetint(h, k, val); }}


/* results from pset */
#define HOK		0
#define HNOTFOUND	1
#define HNOTATABLE	2
#define HFIRSTNODE	3

/*
** 'ilyaH_get*' operations set 'res', unless the value is absent, and
** return the tag of the result.
** The 'ilyaH_pset*' (pre-set) operations set the given value and return
** HOK, unless the original value is absent. In that case, if the key
** is really absent, they return HNOTFOUND. Otherwise, if there is a
** slot with that key but with no value, 'ilyaH_pset*' return an encoding
** of where the key is (usually called 'hres'). (pset cannot set that
** value because there might be a metamethod.) If the slot is in the
** hash part, the encoding is (HFIRSTNODE + hash index); if the slot is
** in the array part, the encoding is (~array index), a negative value.
** The value HNOTATABLE is used by the fast macros to signal that the
** value being indexed is not a table.
** (The size for the array part is limited by the maximum power of two
** that fits in an unsigned integer; that is INT_MAX+1. So, the C-index
** ranges from 0, which encodes to -1, to INT_MAX, which encodes to
** INT_MIN. The size of the hash part is limited by the maximum power of
** two that fits in a signed integer; that is (INT_MAX+1)/2. So, it is
** safe to add HFIRSTNODE to any index there.)
*/


/*
** The array part of a table is represented by an inverted array of
** values followed by an array of tags, to avoid wasting space with
** padding. In between them there is an unsigned int, explained later.
** The 'array' pointer points between the two arrays, so that values are
** indexed with negative indices and tags with non-negative indices.

             Values                              Tags
  --------------------------------------------------------
  ...  |   Value 1     |   Value 0     |unsigned|0|1|...
  --------------------------------------------------------
                                       ^ t->array

** All accesses to 't->array' should be through the macros 'getArrTag'
** and 'getArrVal'.
*/

/* Computes the address of the tag for the abstract C-index 'k' */
#define getArrTag(t,k)	(cast(lu_byte*, (t)->array) + sizeof(unsigned) + (k))

/* Computes the address of the value for the abstract C-index 'k' */
#define getArrVal(t,k)	((t)->array - 1 - (k))


/*
** The unsigned between the two arrays is used as a hint for #t;
** see ilyaH_getn. It is stored there to avoid wasting space in
** the structure Table for tables with no array part.
*/
#define lenhint(t)	cast(unsigned*, (t)->array)


/*
** Move TValues to/from arrays, using C indices
*/
#define arr2obj(h,k,val)  \
  ((val)->tt_ = *getArrTag(h,(k)), (val)->value_ = *getArrVal(h,(k)))

#define obj2arr(h,k,val)  \
  (*getArrTag(h,(k)) = (val)->tt_, *getArrVal(h,(k)) = (val)->value_)


/*
** Often, we need to check the tag of a value before moving it. The
** following macros also move TValues to/from arrays, but receive the
** precomputed tag value or address as an extra argument.
*/
#define farr2val(h,k,tag,res)  \
  ((res)->tt_ = tag, (res)->value_ = *getArrVal(h,(k)))

#define fval2arr(h,k,tag,val)  \
  (*tag = (val)->tt_, *getArrVal(h,(k)) = (val)->value_)


ILYAI_FUNC lu_byte ilyaH_get (Table *t, const TValue *key, TValue *res);
ILYAI_FUNC lu_byte ilyaH_getshortstr (Table *t, TString *key, TValue *res);
ILYAI_FUNC lu_byte ilyaH_getstr (Table *t, TString *key, TValue *res);
ILYAI_FUNC lu_byte ilyaH_getint (Table *t, ilya_Integer key, TValue *res);

/* Special get for metamethods */
ILYAI_FUNC const TValue *ilyaH_Hgetshortstr (Table *t, TString *key);

ILYAI_FUNC int ilyaH_psetint (Table *t, ilya_Integer key, TValue *val);
ILYAI_FUNC int ilyaH_psetshortstr (Table *t, TString *key, TValue *val);
ILYAI_FUNC int ilyaH_psetstr (Table *t, TString *key, TValue *val);
ILYAI_FUNC int ilyaH_pset (Table *t, const TValue *key, TValue *val);

ILYAI_FUNC void ilyaH_setint (ilya_State *L, Table *t, ilya_Integer key,
                                                    TValue *value);
ILYAI_FUNC void ilyaH_set (ilya_State *L, Table *t, const TValue *key,
                                                 TValue *value);

ILYAI_FUNC void ilyaH_finishset (ilya_State *L, Table *t, const TValue *key,
                                              TValue *value, int hres);
ILYAI_FUNC Table *ilyaH_new (ilya_State *L);
ILYAI_FUNC void ilyaH_resize (ilya_State *L, Table *t, unsigned nasize,
                                                    unsigned nhsize);
ILYAI_FUNC void ilyaH_resizearray (ilya_State *L, Table *t, unsigned nasize);
ILYAI_FUNC lu_mem ilyaH_size (Table *t);
ILYAI_FUNC void ilyaH_free (ilya_State *L, Table *t);
ILYAI_FUNC int ilyaH_next (ilya_State *L, Table *t, StkId key);
ILYAI_FUNC ilya_Unsigned ilyaH_getn (Table *t);


#if defined(ILYA_DEBUG)
ILYAI_FUNC Node *ilyaH_mainposition (const Table *t, const TValue *key);
#endif


#endif
