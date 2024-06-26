/*
** Implementation of tables (aka arrays, objects, or hash tables).
** Tables keep its elements in two parts: an array part and a hash part.
** Non-negative integer keys are all candidates to be kept in the array
** part. The actual size of the array is the largest `n' such that at
** least half the slots between 0 and n are in use.
** Hash uses a mix of chained scatter table with Brent's variation.
** A main invariant of these tables is that, if an element is not
** in its main position (i.e. the `original' position that its hash gives
** to it), then the colliding element is in its own main position.
** Hence even when the load factor reaches 100%, performance remains good.
*/
#include <cmath>
#include <cstring>

#define ltable_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "ltable.h"

/*
** max size of array part is 2^MAXBITS
*/
#define MAXBITS 26

#define MAXASIZE (1 << MAXBITS)

#define hashpow2(t, n) (gnode(t, lmod((n), sizenode(t))))

#define hashstr(t, str) hashpow2(t, (str)->hash)
#define hashboolean(t, p) hashpow2(t, p)

/*
** for some types, it is better to avoid modulus by power of 2, as
** they tend to have many 2 factors.
*/
#define hashmod(t, n) (gnode(t, ((n) % ((sizenode(t) - 1) | 1))))

#define hashpointer(t, p) hashmod(t, IntPoint(p))

/*
** number of ints inside a lua_Number
*/
#define numints cast_int(sizeof(lua_Number) / sizeof(int))

#define dummynode (&dummynode_)

static const Node dummynode_ = {{}, /* value */ {} /* key */};

/*
** hash for lua_Numbers
*/
static Node *hashnum(const Table *t, lua_Number n) {
    unsigned int a[numints];
    n += 1; /* normalize number (avoid -0) */
    memcpy(a, &n, sizeof(a));
    for (int i = 1; i < numints; i++)
        a[0] += a[i];
    return hashmod(t, a[0]);
}

/*
** returns the `main' position of an element in a table (that is, the index
** of its hash value)
*/
static Node *mainposition(const Table *t, const TValue *key) {
    switch (ttype(key)) {
    case LUA_TNUMBER:
        return hashnum(t, nvalue(key));
    case LUA_TSTRING:
        return hashstr(t, rawtsvalue(key));
    case LUA_TBOOLEAN:
        return hashboolean(t, bvalue(key));
    case LUA_TLIGHTUSERDATA:
        return hashpointer(t, pvalue(key));
    default:
        return hashpointer(t, gcvalue(key));
    }
}

/*
** returns the index for `key' if `key' is an appropriate key to live in
** the array part of the table, -1 otherwise.
*/
static int arrayindex(const TValue *key) {
    if (key->isnumber()) {
        lua_Number n = nvalue(key);
        int k;
        lua_number2int(k, n);
        if (luai_numeq(cast_num(k), n))
            return k;
    }
    return -1; /* `key' did not match some condition */
}

/*
** returns the index of a `key' for table traversals. First goes all
** elements in the array part, then elements in the hash part. The
** beginning of a traversal is signalled by -1.
*/
static int findindex(lua_State *L, Table *t, StkId key) {
    if (key->isnil())
        return -1; /* first iteration */
    int i = arrayindex(key);
    if (0 < i && i <= t->sizearray) /* is `key' inside array part? */
        return i - 1;               /* yes; that's the index (corrected to C) */
    else {
        Node *n = mainposition(t, key);
        do { /* check whether `key' is somewhere in the chain */
            /* key may be dead already, but it is ok to use it in `next' */
            if (luaO_rawequalObj(key2tval(n), key) ||
                (ttype(gkey(n)) == LUA_TDEADKEY && iscollectable(key) &&
                 gcvalue(gkey(n)) == gcvalue(key))) {
                i = cast_int(n - gnode(t, 0)); /* key index in hash table */
                /* hash elements are numbered after array ones */
                return i + t->sizearray;
            } else
                n = gnext(n);
        } while (n);
        luaG_runerror(L, "invalid key to " LUA_QL("next")); /* key not found */
        return 0; /* to avoid warnings */
    }
}

int luaH_next(lua_State *L, Table *t, StkId key) {
    int i = findindex(L, t, key);       /* find original element */
    for (i++; i < t->sizearray; i++) {  /* try first array part */
        if (!(&t->array[i])->isnil()) { /* a non-nil value? */
            setnvalue(key, cast_num(i + 1));
            setobj2s(L, key + 1, &t->array[i]);
            return 1;
        }
    }
    for (i -= t->sizearray; i < sizenode(t); i++) { /* then hash part */
        if (!(gval(gnode(t, i)))->isnil()) {        /* a non-nil value? */
            setobj2s(L, key, key2tval(gnode(t, i)));
            setobj2s(L, key + 1, gval(gnode(t, i)));
            return 1;
        }
    }
    return 0; /* no more elements */
}

/* Rehash */
static int computesizes(int nums[], int *narray) {
    int a = 0;  /* number of elements smaller than 2^i */
    int na = 0; /* number of elements to go to array part */
    int n = 0;  /* optimal size for array part */
    for (int i = 0, twotoi = 1 /* 2^i */; twotoi / 2 < *narray;
         i++, twotoi *= 2) {
        if (nums[i] > 0) {
            a += nums[i];
            if (a > twotoi / 2) { /* more than half elements present? */
                n = twotoi;       /* optimal size (till now) */
                na = a; /* all elements smaller than n will go to array part */
            }
        }
        if (a == *narray)
            break; /* all elements already counted */
    }
    *narray = n;
    return na;
}

static int countint(const TValue *key, int *nums) {
    int k = arrayindex(key);
    if (0 < k && k <= MAXASIZE) { /* is `key' an appropriate array index? */
        nums[ceillog2(k)]++;      /* count as such */
        return 1;
    } else
        return 0;
}

static int numusearray(const Table *t, int *nums) {
    int lg;
    int ttlg;     /* 2^lg */
    int ause = 0; /* summation of `nums' */
    int i = 1;    /* count to traverse all array keys */
    for (lg = 0, ttlg = 1; lg <= MAXBITS;
         lg++, ttlg *= 2) { /* for each slice */
        int lc = 0;         /* counter */
        int lim = ttlg;
        if (lim > t->sizearray) {
            lim = t->sizearray; /* adjust upper limit */
            if (i > lim)
                break; /* no more elements to count */
        }
        /* count elements in range (2^(lg-1), 2^lg] */
        for (; i <= lim; i++) {
            if (!(&t->array[i - 1])->isnil())
                lc++;
        }
        nums[lg] += lc;
        ause += lc;
    }
    return ause;
}

static int numusehash(const Table *t, int *nums, int *pnasize) {
    int totaluse = 0; /* total number of elements */
    int ause = 0;     /* summation of `nums' */
    int i = sizenode(t);
    while (i--) {
        Node *n = &t->node[i];
        if (!gval(n)->isnil()) {
            ause += countint(key2tval(n), nums);
            totaluse++;
        }
    }
    *pnasize += ause;
    return totaluse;
}

static void setarrayvector(lua_State *L, Table *t, int size) {
    luaM_reallocvector<TValue>(L, &t->array, t->sizearray, size);
    for (int i = t->sizearray; i < size; i++)
        setnilvalue(&t->array[i]);
    t->sizearray = size;
}

static void setnodevector(lua_State *L, Table *t, int size) {
    int lsize;
    if (size == 0) {                       /* no elements to hash part? */
        t->node = cast(Node *, dummynode); /* use common `dummynode' */
        lsize = 0;
    } else {
        lsize = ceillog2(size);
        if (lsize > MAXBITS)
            luaG_runerror(L, "table overflow");
        size = twoto(lsize);
        t->node = luaM_newvector<Node>(L, size);
        for (int i = 0; i < size; i++) {
            Node *n = gnode(t, i);
            gnext(n) = nullptr;
            setnilvalue(gkey(n));
            setnilvalue(gval(n));
        }
    }
    t->lsizenode = cast_byte(lsize);
    t->lastfree = gnode(t, size); /* all positions are free */
}

static void resize(lua_State *L, Table *t, int nasize, int nhsize) {
    int oldasize = t->sizearray;
    int oldhsize = t->lsizenode;
    Node *nold = t->node;  /* save old hash ... */
    if (nasize > oldasize) /* array part must grow? */
        setarrayvector(L, t, nasize);
    /* create new hash part with appropriate size */
    setnodevector(L, t, nhsize);
    if (nasize < oldasize) { /* array part must shrink? */
        t->sizearray = nasize;
        /* re-insert elements from vanishing slice */
        for (int i = nasize; i < oldasize; i++) {
            if (!(&t->array[i])->isnil())
                setobjt2t(L, luaH_setnum(L, t, i + 1), &t->array[i]);
        }
        /* shrink array */
        luaM_reallocvector<TValue>(L, &t->array, oldasize, nasize);
    }
    /* re-insert elements from hash part */
    for (int i = twoto(oldhsize) - 1; i >= 0; i--) {
        Node *old = nold + i;
        if (!gval(old)->isnil())
            setobjt2t(L, luaH_set(L, t, key2tval(old)), gval(old));
    }
    if (nold != dummynode)
        luaM_freearray<Node>(L, nold, twoto(oldhsize)); /* free old array */
}

void luaH_resizearray(lua_State *L, Table *t, int nasize) {
    int nsize = (t->node == dummynode) ? 0 : sizenode(t);
    resize(L, t, nasize, nsize);
}

static void rehash(lua_State *L, Table *t, const TValue *ek) {
    int nasize, na;
    int nums[MAXBITS +
             1]; /* nums[i] = number of keys between 2^(i-1) and 2^i */
    int totaluse;
    for (int i = 0; i <= MAXBITS; i++)
        nums[i] = 0;               /* reset counts */
    nasize = numusearray(t, nums); /* count keys in array part */
    totaluse = nasize;             /* all those keys are integer keys */
    totaluse += numusehash(t, nums, &nasize); /* count keys in hash part */
    /* count extra key */
    nasize += countint(ek, nums);
    totaluse++;
    /* compute new size for array part */
    na = computesizes(nums, &nasize);
    /* resize the table to new computed sizes */
    resize(L, t, nasize, totaluse - na);
}

Table *luaH_new(lua_State *L, int narray, int nhash) {
    Table *t = luaM_new<Table>(L);
    luaC_link(L, obj2gco(t), LUA_TTABLE);
    t->metatable = nullptr;
    t->flags = cast_byte(~0);
    /* temporary values (kept only if some malloc fails) */
    t->array = nullptr;
    t->sizearray = 0;
    t->lsizenode = 0;
    t->node = cast(Node *, dummynode);
    setarrayvector(L, t, narray);
    setnodevector(L, t, nhash);
    return t;
}

void luaH_free(lua_State *L, Table *t) {
    if (t->node != dummynode)
        luaM_freearray<Node>(L, t->node, sizenode(t));
    luaM_freearray<TValue>(L, t->array, t->sizearray);
    luaM_free(L, t);
}

static Node *getfreepos(Table *t) {
    while (t->lastfree-- > t->node) {
        if ((gkey(t->lastfree))->isnil())
            return t->lastfree;
    }
    return nullptr; /* could not find a free place */
}

/*
** inserts a new key into a hash table; first, check whether key's main
** position is free. If not, check whether colliding node is in its main
** position or not: if it is not, move colliding node to an empty place and
** put new key in its main position; otherwise (colliding node is in its main
** position), new key goes to an empty position.
*/
static TValue *newkey(lua_State *L, Table *t, const TValue *key) {
    Node *mp = mainposition(t, key);
    if (!gval(mp)->isnil() || mp == dummynode) {
        Node *othern;
        Node *n = getfreepos(t);        /* get a free place */
        if (n == nullptr) {             /* cannot find a free place? */
            rehash(L, t, key);          /* grow table */
            return luaH_set(L, t, key); /* re-insert key into grown table */
        }
        othern = mainposition(t, key2tval(mp));
        if (othern != mp) { /* is colliding node out of its main position? */
            /* yes; move colliding node into free position */
            while (gnext(othern) != mp)
                othern = gnext(othern); /* find previous */
            gnext(othern) = n; /* redo the chain with `n' in place of `mp' */
            *n = *mp; /* copy colliding node into free pos. (mp->next also goes)
                       */
            gnext(mp) = nullptr; /* now `mp' is free */
            setnilvalue(gval(mp));
        } else { /* colliding node is in its own main position */
            /* new node will go into free position */
            gnext(n) = gnext(mp); /* chain new position */
            gnext(mp) = n;
            mp = n;
        }
    }
    gkey(mp)->value = key->value;
    gkey(mp)->tt = key->tt;
    luaC_barriert(L, t, key);
    return gval(mp);
}

/*
** search function for integers
*/
const TValue *luaH_getnum(Table *t, int key) {
    /* (1 <= key && key <= t->sizearray) */
    if (cast(unsigned int, key - 1) < cast(unsigned int, t->sizearray))
        return &t->array[key - 1];
    else {
        lua_Number nk = cast_num(key);
        Node *n = hashnum(t, nk);
        do { /* check whether `key' is somewhere in the chain */
            if ((gkey(n))->isnumber() && luai_numeq(nvalue(gkey(n)), nk))
                return gval(n); /* that's it */
            else
                n = gnext(n);
        } while (n);
        return luaO_nilobject;
    }
}

/*
** search function for strings
*/
const TValue *luaH_getstr(Table *t, TString *key) {
    Node *n = hashstr(t, key);
    do { /* check whether `key' is somewhere in the chain */
        if ((gkey(n)->isstring()) && rawtsvalue(gkey(n)) == key)
            return gval(n); /* that's it */
        else
            n = gnext(n);
    } while (n);
    return luaO_nilobject;
}

/*
** main search function
*/
const TValue *luaH_get(Table *t, const TValue *key) {
    switch (ttype(key)) {
    case LUA_TNIL:
        return luaO_nilobject;
    case LUA_TSTRING:
        return luaH_getstr(t, rawtsvalue(key));
    case LUA_TNUMBER: {
        int k;
        lua_Number n = nvalue(key);
        lua_number2int(k, n);
        if (luai_numeq(cast_num(k), nvalue(key))) /* index is int? */
            return luaH_getnum(t, k);             /* use specialized version */
                                                  /* else go through */
    }
    default: {
        Node *n = mainposition(t, key);
        do { /* check whether `key' is somewhere in the chain */
            if (luaO_rawequalObj(key2tval(n), key))
                return gval(n); /* that's it */
            else
                n = gnext(n);
        } while (n);
        return luaO_nilobject;
    }
    }
}

TValue *luaH_set(lua_State *L, Table *t, const TValue *key) {
    const TValue *p = luaH_get(t, key);
    t->flags = 0;
    if (p != luaO_nilobject)
        return cast(TValue *, p);
    else {
        if (key->isnil())
            luaG_runerror(L, "table index is nil");
        else if (key->isnumber() && luai_numisnan(nvalue(key)))
            luaG_runerror(L, "table index is NaN");
        return newkey(L, t, key);
    }
}

TValue *luaH_setnum(lua_State *L, Table *t, int key) {
    const TValue *p = luaH_getnum(t, key);
    if (p != luaO_nilobject)
        return cast(TValue *, p);
    else {
        TValue k;
        setnvalue(&k, cast_num(key));
        return newkey(L, t, &k);
    }
}

TValue *luaH_setstr(lua_State *L, Table *t, TString *key) {
    const TValue *p = luaH_getstr(t, key);
    if (p != luaO_nilobject)
        return cast(TValue *, p);
    else {
        TValue k;
        setsvalue(L, &k, key);
        return newkey(L, t, &k);
    }
}

static int unbound_search(Table *t, unsigned int j) {
    unsigned int i = j; /* i is zero or a present index */
    j++;
    /* find `i' and `j' such that i is present and j is not */
    while (!luaH_getnum(t, j)->isnil()) {
        i = j;
        j *= 2;
        if (j > cast(unsigned int, MAX_INT)) { /* overflow? */
            /* table was built with bad purposes: resort to linear search */
            i = 1;
            while (!luaH_getnum(t, i)->isnil())
                i++;
            return i - 1;
        }
    }
    /* now do a binary search between them */
    while (j - i > 1) {
        unsigned int m = (i + j) / 2;
        if (luaH_getnum(t, m)->isnil())
            j = m;
        else
            i = m;
    }
    return i;
}

/*
** Try to find a boundary in table `t'. A `boundary' is an integer index
** such that t[i] is non-nil and t[i+1] is nil (and 0 if t[1] is nil).
*/
int luaH_getn(Table *t) {
    unsigned int j = t->sizearray;
    if (j > 0 && (&t->array[j - 1])->isnil()) {
        /* there is a boundary in the array part: (binary) search for it */
        unsigned int i = 0;
        while (j - i > 1) {
            unsigned int m = (i + j) / 2;
            if ((&t->array[m - 1])->isnil())
                j = m;
            else
                i = m;
        }
        return i;
    }
    /* else must find a boundary in hash part */
    else if (t->node == dummynode) /* hash part is empty? */
        return j;                  /* that is easy... */
    else
        return unbound_search(t, j);
}
