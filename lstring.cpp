#include <cstring>

#define lstring_c
#define LUA_CORE

#include "lua.h"

#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"

void luaS_resize(lua_State *L, int newsize) {
    GCObject **newhash;
    stringtable *tb;

    if (G(L)->gcstate == GCSsweepstring)
        return; /* cannot resize during GC traverse */
    newhash = luaM_newvector<GCObject *>(L, newsize);
    tb = &G(L)->strt;
    for (int i = 0; i < newsize; i++)
        newhash[i] = nullptr;
    /* rehash */
    for (int i = 0; i < tb->size; i++) {
        GCObject *p = tb->hash[i];
        while (p) {                       /* for each node in the list */
            GCObject *next = p->gch.next; /* save next */
            unsigned int h = gco2ts(p)->hash;
            int h1 = lmod(h, newsize); /* new position */
            p->gch.next = newhash[h1]; /* chain it */
            newhash[h1] = p;
            p = next;
        }
    }
    luaM_freearray<TString *>(L, tb->hash, tb->size);
    tb->size = newsize;
    tb->hash = newhash;
}

static TString *newlstr(lua_State *L, const char *str, size_t l,
                        unsigned int h) {
    if (l + 1 > (MAX_SIZET - sizeof(TString)) / sizeof(char))
        luaM_toobig(L);
    TString *ts = cast(
        TString *, luaM_malloc(L, (l + 1) * sizeof(char) + sizeof(TString)));
    ts->len = l;
    ts->hash = h;
    ts->marked = luaC_white(G(L));
    ts->tt = LUA_TSTRING;
    ts->reserved = 0;
    memcpy(ts + 1, str, l * sizeof(char));
    ((char *)(ts + 1))[l] = '\0'; /* ending 0 */
    stringtable *tb = &G(L)->strt;
    h = lmod(h, tb->size);
    ts->next = tb->hash[h]; /* chain new entry */
    tb->hash[h] = obj2gco(ts);
    tb->nuse++;
    if (tb->nuse > cast(lu_int32, tb->size) && tb->size <= MAX_INT / 2)
        luaS_resize(L, tb->size * 2); /* too crowded */
    return ts;
}

TString *luaS_newlstr(lua_State *L, const char *str, size_t l) {
    unsigned int h = cast(unsigned int, l); /* seed */
    size_t step =
        (l >> 5) + 1; /* if string is too long, don't hash all its chars */

    for (size_t l1 = l; l1 >= step; l1 -= step) /* compute hash */
        h = h ^ ((h << 5) + (h >> 2) + cast(unsigned char, str[l1 - 1]));
    for (GCObject *o = G(L)->strt.hash[lmod(h, G(L)->strt.size)]; o != nullptr;
         o = o->gch.next) {
        TString *ts = rawgco2ts(o);
        if (ts->len == l && (memcmp(str, getstr(ts), l) == 0)) {
            /* string may be dead */
            if (isdead(G(L), o))
                changewhite(o);
            return ts;
        }
    }
    return newlstr(L, str, l, h); /* not found */
}

Udata *luaS_newudata(lua_State *L, size_t s, Table *e) {
    Udata *u;
    if (s > MAX_SIZET - sizeof(Udata))
        luaM_toobig(L);
    u = cast(Udata *, luaM_malloc(L, s + sizeof(Udata)));
    u->marked = luaC_white(G(L)); /* is not finalized */
    u->tt = LUA_TUSERDATA;
    u->len = s;
    u->metatable = nullptr;
    u->env = e;
    /* chain it on udata list (after main thread) */
    u->next = G(L)->mainthread->next;
    G(L)->mainthread->next = obj2gco(u);
    return u;
}
