#include <cmath>
#include <cstring>

#define lapi_c
#define LUA_CORE

#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"

#define api_incr_top(L) L->top++

static TValue *index2adr(lua_State *L, int idx) {
    if (idx > 0) {
        TValue *o = L->base + (idx - 1);
        if (o >= L->top)
            return cast(TValue *, luaO_nilobject);
        else
            return o;
    } else if (idx > LUA_REGISTRYINDEX) {
        return L->top + idx;
    } else
        switch (idx) { /* pseudo-indices */
        case LUA_REGISTRYINDEX:
            return registry(L);
        case LUA_ENVIRONINDEX: {
            Closure *func = curr_func(L);
            sethvalue(L, &L->env, func->c.env);
            return &L->env;
        }
        case LUA_GLOBALSINDEX:
            return gt(L);
        default: {
            Closure *func = curr_func(L);
            idx = LUA_GLOBALSINDEX - idx;
            return (idx <= func->c.nupvalues) ? &func->c.upvalue[idx - 1]
                                              : cast(TValue *, luaO_nilobject);
        }
        }
}

static Table *getcurrenv(lua_State *L) {
    if (L->ci == L->base_ci)  /* no enclosing function? */
        return hvalue(gt(L)); /* use global table as environment */
    else {
        Closure *func = curr_func(L);
        return func->c.env;
    }
}

void luaA_pushobject(lua_State *L, const TValue *o) {
    setobj2s(L, L->top, o);
    api_incr_top(L);
}

LUA_API int lua_checkstack(lua_State *L, int size) {
    int res;

    if ((L->top - L->base + size) > LUAI_MAXCSTACK)
        res = 0; /* stack overflow */
    else {
        luaD_checkstack(L, size);
        if (L->ci->top < L->top + size)
            L->ci->top = L->top + size;
        res = 1;
    }

    return res;
}

LUA_API void lua_xmove(lua_State *from, lua_State *to, int n) {
    if (from == to)
        return;

    from->top -= n;
    for (int i = 0; i < n; i++) {
        setobj2s(to, to->top++, from->top + i);
    }
}

LUA_API lua_CFunction lua_atpanic(lua_State *L, lua_CFunction panicf) {
    lua_CFunction old = G(L)->panic;
    G(L)->panic = panicf;
    return old;
}

LUA_API lua_State *lua_newthread(lua_State *L) {
    luaC_checkGC(L);
    lua_State *L1 = luaE_newthread(L);
    setthvalue(L, L->top, L1);
    api_incr_top(L);
    return L1;
}

/*
** basic stack manipulation
*/

LUA_API int lua_gettop(lua_State *L) { return cast_int(L->top - L->base); }

LUA_API void lua_settop(lua_State *L, int idx) {
    if (idx >= 0) {
        while (L->top < L->base + idx)
            setnilvalue(L->top++);
        L->top = L->base + idx;
    } else {
        L->top += idx + 1; /* `subtract' index (index is negative) */
    }
}

LUA_API void lua_remove(lua_State *L, int idx) {
    StkId p = index2adr(L, idx);
    while (++p < L->top)
        setobjs2s(L, p - 1, p);
    L->top--;
}

LUA_API void lua_insert(lua_State *L, int idx) {
    StkId p = index2adr(L, idx);
    for (StkId q = L->top; q > p; q--)
        setobjs2s(L, q, q - 1);
    setobjs2s(L, p, L->top);
}

LUA_API void lua_replace(lua_State *L, int idx) {
    StkId o = index2adr(L, idx);
    if (idx == LUA_ENVIRONINDEX) {
        Closure *func = curr_func(L);
        func->c.env = hvalue(L->top - 1);
        luaC_barrier(L, func, L->top - 1);
    } else {
        setobj(L, o, L->top - 1);
        if (idx < LUA_GLOBALSINDEX) /* function upvalue? */
            luaC_barrier(L, curr_func(L), L->top - 1);
    }
    L->top--;
}

LUA_API void lua_pushvalue(lua_State *L, int idx) {
    setobj2s(L, L->top, index2adr(L, idx));
    api_incr_top(L);
}

/*
** access functions (stack -> C)
*/

LUA_API int lua_type(lua_State *L, int idx) {
    StkId o = index2adr(L, idx);
    return (o == luaO_nilobject) ? LUA_TNONE : ttype(o);
}

LUA_API const char *lua_typename(lua_State *L, int t) {
    UNUSED(L);
    return (t == LUA_TNONE) ? "no value" : luaT_typenames[t];
}

LUA_API int lua_iscfunction(lua_State *L, int idx) {
    StkId o = index2adr(L, idx);
    return iscfunction(o);
}

LUA_API int lua_isnumber(lua_State *L, int idx) {
    TValue n;
    const TValue *o = index2adr(L, idx);
    return tonumber(o, &n);
}

LUA_API int lua_isstring(lua_State *L, int idx) {
    int t = lua_type(L, idx);
    return (t == LUA_TSTRING || t == LUA_TNUMBER);
}

LUA_API int lua_isuserdata(lua_State *L, int idx) {
    const TValue *o = index2adr(L, idx);
    return (o->isuserdata() || o->islightuserdata());
}

LUA_API int lua_rawequal(lua_State *L, int index1, int index2) {
    StkId o1 = index2adr(L, index1);
    StkId o2 = index2adr(L, index2);
    return (o1 == luaO_nilobject || o2 == luaO_nilobject)
               ? 0
               : luaO_rawequalObj(o1, o2);
}

LUA_API int lua_equal(lua_State *L, int index1, int index2) {
    /* may call tag method */
    StkId o1 = index2adr(L, index1);
    StkId o2 = index2adr(L, index2);
    return (o1 == luaO_nilobject || o2 == luaO_nilobject) ? 0
                                                       : equalobj(L, o1, o2);
}

LUA_API int lua_lessthan(lua_State *L, int index1, int index2) {
    /* may call tag method */
    StkId o1 = index2adr(L, index1);
    StkId o2 = index2adr(L, index2);
    return (o1 == luaO_nilobject || o2 == luaO_nilobject)
            ? 0
            : luaV_lessthan(L, o1, o2);
}

LUA_API lua_Number lua_tonumber(lua_State *L, int idx) {
    TValue n;
    const TValue *o = index2adr(L, idx);
    if (tonumber(o, &n))
        return nvalue(o);
    else
        return 0;
}

LUA_API lua_Integer lua_tointeger(lua_State *L, int idx) {
    TValue n;
    const TValue *o = index2adr(L, idx);
    if (tonumber(o, &n)) {
        lua_Integer res;
        lua_Number num = nvalue(o);
        lua_number2integer(res, num);
        return res;
    } else
        return 0;
}

LUA_API int lua_toboolean(lua_State *L, int idx) {
    const TValue *o = index2adr(L, idx);
    return !o->isfalse();
}

LUA_API const char *lua_tolstring(lua_State *L, int idx, size_t *len) {
    StkId o = index2adr(L, idx);
    if (!o->isstring()) {
        /* `luaV_tostring' may create a new string */
        if (!luaV_tostring(L, o)) { /* conversion failed? */
            if (len != nullptr)
                *len = 0;
            return nullptr;
        }
        luaC_checkGC(L);
        o = index2adr(L, idx); /* previous call may reallocate the stack */
    }
    if (len != nullptr)
        *len = tsvalue(o)->len;
    return svalue(o);
}

LUA_API size_t lua_objlen(lua_State *L, int idx) {
    StkId o = index2adr(L, idx);
    switch (ttype(o)) {
    case LUA_TSTRING:
        return tsvalue(o)->len;
    case LUA_TUSERDATA:
        return uvalue(o)->len;
    case LUA_TTABLE:
        return luaH_getn(hvalue(o));
    case LUA_TNUMBER: {
        /* `luaV_tostring' may create a new string */
        return cast(size_t, (luaV_tostring(L, o) ? tsvalue(o)->len : 0));
    }
    default:
        return 0;
    }
}

LUA_API lua_CFunction lua_tocfunction(lua_State *L, int idx) {
    StkId o = index2adr(L, idx);
    return (!iscfunction(o)) ? nullptr : clvalue(o)->c.f;
}

LUA_API void *lua_touserdata(lua_State *L, int idx) {
    StkId o = index2adr(L, idx);
    switch (ttype(o)) {
    case LUA_TUSERDATA:
        return (rawuvalue(o) + 1);
    case LUA_TLIGHTUSERDATA:
        return pvalue(o);
    default:
        return nullptr;
    }
}

LUA_API lua_State *lua_tothread(lua_State *L, int idx) {
    StkId o = index2adr(L, idx);
    return (!o->isthread()) ? nullptr : thvalue(o);
}

LUA_API const void *lua_topointer(lua_State *L, int idx) {
    StkId o = index2adr(L, idx);
    switch (ttype(o)) {
    case LUA_TTABLE:
        return hvalue(o);
    case LUA_TFUNCTION:
        return clvalue(o);
    case LUA_TTHREAD:
        return thvalue(o);
    case LUA_TUSERDATA:
    case LUA_TLIGHTUSERDATA:
        return lua_touserdata(L, idx);
    default:
        return nullptr;
    }
}

/*
** push functions (C -> stack)
*/

LUA_API void lua_pushnil(lua_State *L) {
    setnilvalue(L->top);
    api_incr_top(L);
}

LUA_API void lua_pushnumber(lua_State *L, lua_Number n) {
    setnvalue(L->top, n);
    api_incr_top(L);
}

LUA_API void lua_pushinteger(lua_State *L, lua_Integer n) {
    setnvalue(L->top, cast_num(n));
    api_incr_top(L);
}

LUA_API void lua_pushlstring(lua_State *L, const char *s, size_t len) {
    luaC_checkGC(L);
    setsvalue2s(L, L->top, luaS_newlstr(L, s, len));
    api_incr_top(L);
}

LUA_API void lua_pushstring(lua_State *L, const char *s) {
    if (s == nullptr)
        lua_pushnil(L);
    else
        lua_pushlstring(L, s, strlen(s));
}

LUA_API const char *lua_pushvfstring(lua_State *L, const char *fmt,
                                     va_list argp) {
    luaC_checkGC(L);
    const char *ret = luaO_pushvfstring(L, fmt, argp);
    return ret;
}

LUA_API const char *lua_pushfstring(lua_State *L, const char *fmt, ...) {
    va_list argp;

    luaC_checkGC(L);
    va_start(argp, fmt);
    const char *ret = luaO_pushvfstring(L, fmt, argp);
    va_end(argp);

    return ret;
}

LUA_API void lua_pushcclosure(lua_State *L, lua_CFunction fn, int n) {
    luaC_checkGC(L);
    Closure *cl = luaF_newCclosure(L, n, getcurrenv(L));
    cl->c.f = fn;
    L->top -= n;
    while (n--)
        setobj2n(L, &cl->c.upvalue[n], L->top + n);
    setclvalue(L, L->top, cl);

    api_incr_top(L);
}

LUA_API void lua_pushboolean(lua_State *L, int b) {
    setbvalue(L->top, (b != 0)); /* ensure that true is 1 */
    api_incr_top(L);
}

LUA_API void lua_pushlightuserdata(lua_State *L, void *p) {
    setpvalue(L->top, p);
    api_incr_top(L);
}

LUA_API int lua_pushthread(lua_State *L) {
    setthvalue(L, L->top, L);
    api_incr_top(L);
    return (G(L)->mainthread == L);
}

/*
** get functions (Lua -> stack)
*/

LUA_API void lua_gettable(lua_State *L, int idx) {
    StkId t = index2adr(L, idx);
    luaV_gettable(L, t, L->top - 1, L->top - 1);
}

LUA_API void lua_getfield(lua_State *L, int idx, const char *k) {
    TValue key;

    StkId t = index2adr(L, idx);
    setsvalue(L, &key, luaS_new(L, k));
    luaV_gettable(L, t, &key, L->top);
    api_incr_top(L);
}

LUA_API void lua_rawget(lua_State *L, int idx) {
    StkId t = index2adr(L, idx);
    setobj2s(L, L->top - 1, luaH_get(hvalue(t), L->top - 1));
}

LUA_API void lua_rawgeti(lua_State *L, int idx, int n) {
    StkId o = index2adr(L, idx);
    setobj2s(L, L->top, luaH_getnum(hvalue(o), n));
    api_incr_top(L);
}

LUA_API void lua_createtable(lua_State *L, int narray, int nrec) {
    luaC_checkGC(L);
    sethvalue(L, L->top, luaH_new(L, narray, nrec));
    api_incr_top(L);
}

LUA_API int lua_getmetatable(lua_State *L, int objindex) {
    Table *mt = nullptr;
    int res;

    const TValue *obj = index2adr(L, objindex);
    switch (ttype(obj)) {
    case LUA_TTABLE:
        mt = hvalue(obj)->metatable;
        break;
    case LUA_TUSERDATA:
        mt = uvalue(obj)->metatable;
        break;
    default:
        mt = G(L)->mt[ttype(obj)];
        break;
    }
    if (mt == nullptr)
        res = 0;
    else {
        sethvalue(L, L->top, mt);
        api_incr_top(L);
        res = 1;
    }

    return res;
}

LUA_API void lua_getfenv(lua_State *L, int idx) {
    StkId o = index2adr(L, idx);
    switch (ttype(o)) {
    case LUA_TFUNCTION:
        sethvalue(L, L->top, clvalue(o)->c.env);
        break;
    case LUA_TUSERDATA:
        sethvalue(L, L->top, uvalue(o)->env);
        break;
    case LUA_TTHREAD:
        setobj2s(L, L->top, gt(thvalue(o)));
        break;
    default:
        setnilvalue(L->top);
        break;
    }
    api_incr_top(L);
}

/*
** set functions (stack -> Lua)
*/

LUA_API void lua_settable(lua_State *L, int idx) {
    StkId t = index2adr(L, idx);

    luaV_settable(L, t, L->top - 2, L->top - 1);
    L->top -= 2; /* pop index and value */
}

LUA_API void lua_setfield(lua_State *L, int idx, const char *k) {
    StkId t = index2adr(L, idx);
    TValue key;

    setsvalue(L, &key, luaS_new(L, k));
    luaV_settable(L, t, &key, L->top - 1);
    L->top--; /* pop value */
}

LUA_API void lua_rawset(lua_State *L, int idx) {
    StkId t = index2adr(L, idx);

    setobj2t(L, luaH_set(L, hvalue(t), L->top - 2), L->top - 1);
    luaC_barriert(L, hvalue(t), L->top - 1);
    L->top -= 2;
}

LUA_API void lua_rawseti(lua_State *L, int idx, int n) {
    StkId o = index2adr(L, idx);

    setobj2t(L, luaH_setnum(L, hvalue(o), n), L->top - 1);
    luaC_barriert(L, hvalue(o), L->top - 1);
    L->top--;
}

LUA_API int lua_setmetatable(lua_State *L, int objindex) {
    TValue *obj = index2adr(L, objindex);
    Table *mt;

    if ((L->top - 1)->isnil())
        mt = nullptr;
    else {
        mt = hvalue(L->top - 1);
    }
    switch (ttype(obj)) {
    case LUA_TTABLE: {
        hvalue(obj)->metatable = mt;
        if (mt)
            luaC_objbarriert(L, hvalue(obj), mt);
        break;
    }
    case LUA_TUSERDATA: {
        uvalue(obj)->metatable = mt;
        if (mt)
            luaC_objbarrier(L, rawuvalue(obj), mt);
        break;
    }
    default: {
        G(L)->mt[ttype(obj)] = mt;
        break;
    }
    }
    L->top--;

    return 1;
}

LUA_API int lua_setfenv(lua_State *L, int idx) {
    StkId o = index2adr(L, idx);
    int res = 1;

    switch (ttype(o)) {
    case LUA_TFUNCTION:
        clvalue(o)->c.env = hvalue(L->top - 1);
        break;
    case LUA_TUSERDATA:
        uvalue(o)->env = hvalue(L->top - 1);
        break;
    case LUA_TTHREAD:
        sethvalue(L, gt(thvalue(o)), hvalue(L->top - 1));
        break;
    default:
        res = 0;
        break;
    }
    luaC_objbarrier(L, gcvalue(o), hvalue(L->top - 1));
    L->top--;

    return res;
}

/*
** `load' and `call' functions (run Lua code)
*/

#define adjustresults(L, nres)                                                 \
    {                                                                          \
        if (nres == LUA_MULTRET && L->top >= L->ci->top)                       \
            L->ci->top = L->top;                                               \
    }

LUA_API void lua_call(lua_State *L, int nargs, int nresults) {
    StkId func = L->top - (nargs + 1);
    luaD_call(L, func, nresults);
    adjustresults(L, nresults);
}

/*
** Execute a protected call.
*/
struct CallS { /* data to `f_call' */
    StkId func;
    int nresults;
};

static void f_call(lua_State *L, void *ud) {
    CallS *c = cast(CallS *, ud);
    luaD_call(L, c->func, c->nresults);
}

LUA_API int lua_pcall(lua_State *L, int nargs, int nresults, int errfunc) {
    CallS c;
    int status;
    ptrdiff_t func;

    if (errfunc == 0)
        func = 0;
    else {
        StkId o = index2adr(L, errfunc);
        func = savestack(L, o);
    }
    c.func = L->top - (nargs + 1); /* function to be called */
    c.nresults = nresults;
    status = luaD_pcall(L, f_call, &c, savestack(L, c.func), func);
    adjustresults(L, nresults);

    return status;
}

/*
** Execute a protected C call.
*/
struct CCallS { /* data to `f_Ccall' */
    lua_CFunction func;
    void *ud;
};

static void f_Ccall(lua_State *L, void *ud) {
    CCallS *c = cast(CCallS *, ud);
    Closure *cl = luaF_newCclosure(L, 0, getcurrenv(L));
    cl->c.f = c->func;
    setclvalue(L, L->top, cl); /* push function */
    api_incr_top(L);
    setpvalue(L->top, c->ud); /* push only argument */
    api_incr_top(L);
    luaD_call(L, L->top - 2, 0);
}

LUA_API int lua_cpcall(lua_State *L, lua_CFunction func, void *ud) {
    CCallS c;
    c.func = func;
    c.ud = ud;
    return luaD_pcall(L, f_Ccall, &c, savestack(L, L->top), 0);
}

LUA_API int lua_load(lua_State *L, lua_Reader reader, void *data,
                     const char *chunkname) {
    ZIO z;
    if (!chunkname)
        chunkname = "?";
    luaZ_init(L, &z, reader, data);
    return luaD_protectedparser(L, &z, chunkname);
}

LUA_API int lua_dump(lua_State *L, lua_Writer writer, void *data) {
    int status;
    TValue *o = L->top - 1;
    if (isLfunction(o))
        status = luaU_dump(L, clvalue(o)->l.p, writer, data, 0);
    else
        status = 1;

    return status;
}

LUA_API int lua_status(lua_State *L) { return L->status; }

/*
** Garbage-collection function
*/

LUA_API int lua_gc(lua_State *L, int what, int data) {
    int res = 0;
    global_State *g = G(L);
    switch (what) {
    case LUA_GCSTOP: {
        g->GCthreshold = MAX_LUMEM;
        break;
    }
    case LUA_GCRESTART: {
        g->GCthreshold = g->totalbytes;
        break;
    }
    case LUA_GCCOLLECT: {
        luaC_fullgc(L);
        break;
    }
    case LUA_GCCOUNT: {
        /* GC values are expressed in Kbytes: #bytes/2^10 */
        res = cast_int(g->totalbytes >> 10);
        break;
    }
    case LUA_GCCOUNTB: {
        res = cast_int(g->totalbytes & 0x3ff);
        break;
    }
    case LUA_GCSTEP: {
        lu_mem a = (cast(lu_mem, data) << 10);
        if (a <= g->totalbytes)
            g->GCthreshold = g->totalbytes - a;
        else
            g->GCthreshold = 0;
        while (g->GCthreshold <= g->totalbytes)
            luaC_step(L);
        if (g->gcstate == GCSpause) /* end of cycle? */
            res = 1;                /* signal it */
        break;
    }
    case LUA_GCSETPAUSE: {
        res = g->gcpause;
        g->gcpause = data;
        break;
    }
    case LUA_GCSETSTEPMUL: {
        res = g->gcstepmul;
        g->gcstepmul = data;
        break;
    }
    default:
        res = -1; /* invalid option */
    }

    return res;
}

/*
** miscellaneous functions
*/
LUA_API int lua_error(lua_State *L) {
    luaG_errormsg(L);
    return 0; /* to avoid warnings */
}

LUA_API int lua_next(lua_State *L, int idx) {
    StkId t = index2adr(L, idx);
    int more = luaH_next(L, hvalue(t), L->top - 1);
    if (more) {
        api_incr_top(L);
    } else           /* no more elements */
        L->top -= 1; /* remove key */

    return more;
}

LUA_API void lua_concat(lua_State *L, int n) {
    if (n >= 2) {
        luaC_checkGC(L);
        luaV_concat(L, n, cast_int(L->top - L->base) - 1);
        L->top -= (n - 1);
    } else if (n == 0) { /* push empty string */
        setsvalue2s(L, L->top, luaS_newlstr(L, "", 0));
        api_incr_top(L);
    }
    /* else n == 1; nothing to do */
}

LUA_API void *lua_newuserdata(lua_State *L, size_t size) {
    luaC_checkGC(L);
    Udata *u = luaS_newudata(L, size, getcurrenv(L));
    setuvalue(L, L->top, u);
    api_incr_top(L);
    return u + 1;
}

static const char *aux_upvalue(StkId fi, int n, TValue **val) {
    if (!fi->isfunction())
        return nullptr;
    Closure *f = clvalue(fi);
    if (f->c.isC) {
        if (!(1 <= n && n <= f->c.nupvalues))
            return nullptr;
        *val = &f->c.upvalue[n - 1];
        return "";
    } else {
        Proto *p = f->l.p;
        if (!(1 <= n && n <= p->sizeupvalues))
            return nullptr;
        *val = f->l.upvals[n - 1]->v;
        return getstr(p->upvalues[n - 1]);
    }
}

LUA_API const char *lua_getupvalue(lua_State *L, int funcindex, int n) {
    TValue *val;
    const char *name = aux_upvalue(index2adr(L, funcindex), n, &val);
    if (name) {
        setobj2s(L, L->top, val);
        api_incr_top(L);
    }
    return name;
}

LUA_API const char *lua_setupvalue(lua_State *L, int funcindex, int n) {
    TValue *val;
    StkId fi = index2adr(L, funcindex);
    const char *name = aux_upvalue(fi, n, &val);
    if (name) {
        L->top--;
        setobj(L, val, L->top);
        luaC_barrier(L, clvalue(fi), L->top);
    }

    return name;
}
