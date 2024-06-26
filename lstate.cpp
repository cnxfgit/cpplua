#define lstate_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"

/*
** Main thread combines a thread state and the global state
*/
struct LG {
    lua_State l;
    global_State g;
};

static void stack_init(lua_State *L1, lua_State *L) {
    /* initialize CallInfo array */
    L1->base_ci = luaM_newvector<CallInfo>(L, BASIC_CI_SIZE);
    L1->ci = L1->base_ci;
    L1->size_ci = BASIC_CI_SIZE;
    L1->end_ci = L1->base_ci + L1->size_ci - 1;
    /* initialize stack array */
    L1->stack = luaM_newvector<TValue>(L, BASIC_STACK_SIZE + EXTRA_STACK);
    L1->stacksize = BASIC_STACK_SIZE + EXTRA_STACK;
    L1->top = L1->stack;
    L1->stack_last = L1->stack + (L1->stacksize - EXTRA_STACK) - 1;
    /* initialize first ci */
    L1->ci->func = L1->top;
    setnilvalue(L1->top++); /* `function' entry for this `ci' */
    L1->base = L1->ci->base = L1->top;
    L1->ci->top = L1->top + LUA_MINSTACK;
}

static void freestack(lua_State *L, lua_State *L1) {
    luaM_freearray<CallInfo>(L, L1->base_ci, L1->size_ci);
    luaM_freearray<TValue>(L, L1->stack, L1->stacksize);
}

/*
** open parts that may cause memory-allocation errors
*/
static void f_luaopen(lua_State *L, void *ud) {
    global_State *g = G(L);
    UNUSED(ud);
    stack_init(L, L);                             /* init stack */
    sethvalue(L, gt(L), luaH_new(L, 0, 2));       /* table of globals */
    sethvalue(L, registry(L), luaH_new(L, 0, 2)); /* registry */
    luaS_resize(L, MINSTRTABSIZE); /* initial size of string table */
    luaT_init(L);
    luaX_init(L);
    luaS_fix(luaS_newliteral(L, MEMERRMSG));
    g->GCthreshold = 4 * g->totalbytes;
}

static void preinit_state(lua_State *L, global_State *g) {
    G(L) = g;
    L->stack = nullptr;
    L->stacksize = 0;
    L->errorJmp = nullptr;
    L->hook = nullptr;
    L->hookmask = 0;
    L->basehookcount = 0;
    L->allowhook = 1;
    resethookcount(L);
    L->openupval = nullptr;
    L->size_ci = 0;
    L->nCcalls = 0;
    L->status = 0;
    L->base_ci = L->ci = nullptr;
    L->savedpc = nullptr;
    L->errfunc = 0;
    setnilvalue(gt(L));
}

static void close_state(lua_State *L) {
    global_State *g = G(L);
    luaF_close(L, L->stack); /* close all upvalues for this thread */
    luaC_freeall(L);         /* collect all objects */
    luaM_freearray<TString *>(L, G(L)->strt.hash, G(L)->strt.size);
    luaZ_resizebuffer(L, &g->buff, 0);
    freestack(L, L);
    (*g->frealloc)(L, 0);
}

lua_State *luaE_newthread(lua_State *L) {
    lua_State *L1 = cast(lua_State*, luaM_malloc(L, sizeof(lua_State)));
    luaC_link(L, obj2gco(L1), LUA_TTHREAD);
    preinit_state(L1, G(L));
    stack_init(L1, L);          /* init stack */
    setobj2n(L, gt(L1), gt(L)); /* share table of globals */
    L1->hookmask = L->hookmask;
    L1->basehookcount = L->basehookcount;
    L1->hook = L->hook;
    resethookcount(L1);
    return L1;
}

void luaE_freethread(lua_State *L, lua_State *L1) {
    luaF_close(L1, L1->stack); /* close all upvalues for this thread */

    freestack(L, L1);
    luaM_freemem(L, L1, sizeof(lua_State));
}

LUA_API lua_State *lua_newstate(lua_Alloc f) {
    lua_State *L;
    global_State *g;
    void *l = (*f)(nullptr, sizeof(LG));
    if (l == nullptr)
        return nullptr;
    L = cast(lua_State*,l);
    g = &((LG *)L)->g;
    L->next = nullptr;
    L->tt = LUA_TTHREAD;
    g->currentwhite = bit2mask(WHITE0BIT, FIXEDBIT);
    L->marked = luaC_white(g);
    set2bits(L->marked, FIXEDBIT, SFIXEDBIT);
    preinit_state(L, g);
    g->frealloc = f;
    g->mainthread = L;
    g->uvhead.u.l.prev = &g->uvhead;
    g->uvhead.u.l.next = &g->uvhead;
    g->GCthreshold = 0; /* mark it as unfinished state */
    g->strt.size = 0;
    g->strt.nuse = 0;
    g->strt.hash = nullptr;
    setnilvalue(registry(L));
    g->buff = Mbuffer();
    g->panic = nullptr;
    g->gcstate = GCSpause;
    g->rootgc = obj2gco(L);
    g->sweepstrgc = 0;
    g->sweepgc = &g->rootgc;
    g->gray = nullptr;
    g->grayagain = nullptr;
    g->weak = nullptr;
    g->tmudata = nullptr;
    g->totalbytes = sizeof(LG);
    g->gcpause = LUAI_GCPAUSE;
    g->gcstepmul = LUAI_GCMUL;
    g->gcdept = 0;
    for (int i = 0; i < NUM_TAGS; i++)
        g->mt[i] = nullptr;
    if (luaD_rawrunprotected(L, f_luaopen, nullptr) != 0) {
        /* memory allocation error: free partial state */
        close_state(L);
        L = nullptr;
    }
    return L;
}

static void callallgcTM(lua_State *L, void *ud) {
    UNUSED(ud);
    luaC_callGCTM(L); /* call GC metamethods for all udata */
}

LUA_API void lua_close(lua_State *L) {
    L = G(L)->mainthread; /* only the main thread can be closed */

    luaF_close(L, L->stack);  /* close all upvalues for this thread */
    luaC_separateudata(L, 1); /* separate udata that have GC metamethods */
    L->errfunc = 0;           /* no error function during GC metamethods */
    do {                      /* repeat until no more errors */
        L->ci = L->base_ci;
        L->base = L->top = L->ci->base;
        L->nCcalls = 0;
    } while (luaD_rawrunprotected(L, callallgcTM, nullptr) != 0);
    close_state(L);
}
