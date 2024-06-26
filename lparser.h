#ifndef lparser_h
#define lparser_h

#include "llimits.h"
#include "lobject.h"
#include "ltable.h"
#include "lzio.h"

/*
** Expression descriptor
*/

enum expkind{
    VVOID, /* no value */
    VNIL,
    VTRUE,
    VFALSE,
    VK,         /* info = index of constant in `k' */
    VKNUM,      /* nval = numerical value */
    VLOCAL,     /* info = local register */
    VUPVAL,     /* info = index of upvalue in `upvalues' */
    VGLOBAL,    /* info = index of table; aux = index of global name in `k' */
    VINDEXED,   /* info = table register; aux = index register (or `k') */
    VJMP,       /* info = instruction pc */
    VRELOCABLE, /* info = instruction pc */
    VNONRELOC,  /* info = result register */
    VCALL,      /* info = instruction pc */
    VVARARG     /* info = instruction pc */
};

struct expdesc {
    expkind k;
    union {
        struct {
            int info, aux;
        } s;
        lua_Number nval;
    } u;
    int t; /* patch list of `exit when true' */
    int f; /* patch list of `exit when false' */
};

struct upvaldesc {
    lu_byte k;
    lu_byte info;
};

struct BlockCnt; /* defined in lparser.c */

/* state needed to generate code for a given function */
struct FuncState {
    Proto *f;               /* current function header */
    Table *h;               /* table to find (and reuse) elements in `k' */
    FuncState *prev; /* enclosing function */
    struct LexState *ls;    /* lexical state */
    lua_State *L;    /* copy of the Lua state */
    BlockCnt *bl;    /* chain of current blocks */
    int pc;                 /* next position to code (equivalent to `ncode') */
    int lasttarget;         /* `pc' of last `jump target' */
    int jpc;                /* list of pending jumps to `pc' */
    int freereg;            /* first free register */
    int nk;                 /* number of elements in `k' */
    int np;                 /* number of elements in `p' */
    short nlocvars;         /* number of elements in `locvars' */
    lu_byte nactvar;        /* number of active local variables */
    upvaldesc upvalues[LUAI_MAXUPVALUES]; /* upvalues */
    unsigned short actvar[LUAI_MAXVARS];  /* declared-variable stack */
};

LUAI_FUNC Proto *luaY_parser(lua_State *L, ZIO *z, Mbuffer *buff,
                             const char *name);

#endif
