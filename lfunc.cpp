#define lfunc_c
#define LUA_CORE

#include "lua.h"

#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"

Closure *luaF_newCclosure(lua_State *L, int nelems, Table *e) {
    Closure *c = cast(Closure *, luaM_malloc(L, sizeCclosure(nelems)));
    luaC_link(L, obj2gco(c), LUA_TFUNCTION);
    c->c.isC = 1;
    c->c.env = e;
    c->c.nupvalues = cast_byte(nelems);
    return c;
}

Closure *luaF_newLclosure(lua_State *L, int nelems, Table *e) {
    Closure *c = cast(Closure *, luaM_malloc(L, sizeLclosure(nelems)));
    luaC_link(L, obj2gco(c), LUA_TFUNCTION);
    c->l.isC = 0;
    c->l.env = e;
    c->l.nupvalues = cast_byte(nelems);
    while (nelems--)
        c->l.upvals[nelems] = nullptr;
    return c;
}

UpVal *luaF_newupval(lua_State *L) {
    UpVal *uv = luaM_new<UpVal>(L);
    luaC_link(L, obj2gco(uv), LUA_TUPVAL);
    uv->v = &uv->u.value;
    setnilvalue(uv->v);
    return uv;
}

UpVal *luaF_findupval(lua_State *L, StkId level) {
    global_State *g = G(L);
    GCObject **pp = &L->openupval;
    UpVal *p;
    UpVal *uv;
    while ((p = ngcotouv(*pp)) != nullptr && p->v >= level) {
        if (p->v == level) {             /* found a corresponding upvalue? */
            if (isdead(g, obj2gco(p)))   /* is it dead? */
                changewhite(obj2gco(p)); /* ressurect it */
            return p;
        }
        pp = &p->next;
    }
    uv = luaM_new<UpVal>(L); /* not found: create a new one */
    uv->tt = LUA_TUPVAL;
    uv->marked = luaC_white(g);
    uv->v = level;  /* current value lives in the stack */
    uv->next = *pp; /* chain it in the proper position */
    *pp = obj2gco(uv);
    uv->u.l.prev = &g->uvhead; /* double link it in `uvhead' list */
    uv->u.l.next = g->uvhead.u.l.next;
    uv->u.l.next->u.l.prev = uv;
    g->uvhead.u.l.next = uv;
    return uv;
}

static void unlinkupval(UpVal *uv) {
    uv->u.l.next->u.l.prev = uv->u.l.prev; /* remove from `uvhead' list */
    uv->u.l.prev->u.l.next = uv->u.l.next;
}

void luaF_freeupval(lua_State *L, UpVal *uv) {
    if (uv->v != &uv->u.value) /* is it open? */
        unlinkupval(uv);       /* remove from open list */
    luaM_free(L, uv);          /* free upvalue */
}

void luaF_close(lua_State *L, StkId level) {
    UpVal *uv;
    global_State *g = G(L);
    while ((uv = ngcotouv(L->openupval)) != nullptr && uv->v >= level) {
        GCObject *o = obj2gco(uv);
        L->openupval = uv->next; /* remove from `open' list */
        if (isdead(g, o))
            luaF_freeupval(L, uv); /* free upvalue */
        else {
            unlinkupval(uv);
            setobj(L, &uv->u.value, uv->v);
            uv->v = &uv->u.value;  /* now current value lives here */
            luaC_linkupval(L, uv); /* link upvalue into `gcroot' list */
        }
    }
}

Proto *luaF_newproto(lua_State *L) {
    Proto *f = luaM_new<Proto>(L);
    luaC_link(L, obj2gco(f), LUA_TPROTO);
    f->k = nullptr;
    f->sizek = 0;
    f->p = nullptr;
    f->sizep = 0;
    f->code = nullptr;
    f->sizecode = 0;
    f->sizelineinfo = 0;
    f->sizeupvalues = 0;
    f->nups = 0;
    f->upvalues = nullptr;
    f->numparams = 0;
    f->is_vararg = 0;
    f->maxstacksize = 0;
    f->lineinfo = nullptr;
    f->sizelocvars = 0;
    f->locvars = nullptr;
    f->linedefined = 0;
    f->lastlinedefined = 0;
    f->source = nullptr;
    return f;
}

void luaF_freeproto(lua_State *L, Proto *f) {
    luaM_freearray<Instruction>(L, f->code, f->sizecode);
    luaM_freearray<Proto *>(L, f->p, f->sizep);
    luaM_freearray<TValue>(L, f->k, f->sizek);
    luaM_freearray<int>(L, f->lineinfo, f->sizelineinfo);
    luaM_freearray<LocVar>(L, f->locvars, f->sizelocvars);
    luaM_freearray<TString *>(L, f->upvalues, f->sizeupvalues);
    luaM_free(L, f);
}

void luaF_freeclosure(lua_State *L, Closure *c) {
    int size = (c->c.isC) ? sizeCclosure(c->c.nupvalues)
                          : sizeLclosure(c->l.nupvalues);
    luaM_freemem(L, c, size);
}

/*
** Look for n-th local variable at line `line' in function `func'.
** Returns nullptr if not found.
*/
const char *luaF_getlocalname(const Proto *f, int local_number, int pc) {
    for (int i = 0; i < f->sizelocvars && f->locvars[i].startpc <= pc; i++) {
        if (pc < f->locvars[i].endpc) { /* is variable active? */
            local_number--;
            if (local_number == 0)
                return getstr(f->locvars[i].varname);
        }
    }
    return nullptr; /* not found */
}
