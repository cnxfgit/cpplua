#ifndef lobject_h
#define lobject_h

#include <cstdarg>

#include "llimits.h"
#include "lua.h"

/* tags for values visible from Lua */
#define LAST_TAG LUA_TTHREAD

#define NUM_TAGS (LAST_TAG + 1)

/*
** Extra tags for non-values
*/
#define LUA_TPROTO (LAST_TAG + 1)
#define LUA_TUPVAL (LAST_TAG + 2)
#define LUA_TDEADKEY (LAST_TAG + 3)

/*
** Union of all collectable objects
*/
union GCObject;

struct Table;

/*
** Common Header for all collectable objects (in macro form, to be
** included in other objects)
*/
#define CommonHeader                                                           \
    GCObject *next;                                                            \
    lu_byte tt;                                                                \
    lu_byte marked

/*
** Common header in struct form
*/
struct GCheader {
    CommonHeader;
};

/*
** Union of all Lua values
*/
union Value {
    GCObject *gc;
    void *p;
    lua_Number n;
    int b;
};

/*
** Tagged Values
*/
struct TValue {
    Value value;
    int tt;

    inline bool isnil() const { return this->tt == LUA_TNIL; }
    inline bool isnumber() const { return this->tt == LUA_TNUMBER; }
    inline bool isstring() const { return this->tt == LUA_TSTRING; }
    inline bool istable() const { return this->tt == LUA_TTABLE; }
    inline bool isfunction() const { return this->tt == LUA_TFUNCTION; }
    inline bool isboolean() const { return this->tt == LUA_TBOOLEAN; }
    inline bool isuserdata() const { return this->tt == LUA_TUSERDATA; }
    inline bool isthread() const { return this->tt == LUA_TTHREAD; }
    inline bool islightuserdata() const {
        return this->tt == LUA_TLIGHTUSERDATA;
    }

    inline bool isfalse() const {
        return this->isnil() || (this->isboolean() && this->value.b == 0);
    }
};

/* Macros to access values */
#define ttype(o) ((o)->tt)
#define gcvalue(o) ((o)->value.gc)
#define pvalue(o) ((o)->value.p)
#define nvalue(o) ((o)->value.n)
#define rawtsvalue(o) (&(o)->value.gc->ts)
#define tsvalue(o) (rawtsvalue(o))
#define rawuvalue(o) (&(o)->value.gc->u)
#define uvalue(o) (rawuvalue(o))
#define clvalue(o) (&(o)->value.gc->cl)
#define hvalue(o) (&(o)->value.gc->h)
#define bvalue(o) ((o)->value.b)
#define thvalue(o) (&(o)->value.gc->th)

/* Macros to set values */
#define setnilvalue(obj) ((obj)->tt = LUA_TNIL)

#define setnvalue(obj, x)                                                      \
    {                                                                          \
        TValue *i_o = (obj);                                                   \
        i_o->value.n = (x);                                                    \
        i_o->tt = LUA_TNUMBER;                                                 \
    }

#define setpvalue(obj, x)                                                      \
    {                                                                          \
        TValue *i_o = (obj);                                                   \
        i_o->value.p = (x);                                                    \
        i_o->tt = LUA_TLIGHTUSERDATA;                                          \
    }

#define setbvalue(obj, x)                                                      \
    {                                                                          \
        TValue *i_o = (obj);                                                   \
        i_o->value.b = (x);                                                    \
        i_o->tt = LUA_TBOOLEAN;                                                \
    }

#define setsvalue(L, obj, x)                                                   \
    {                                                                          \
        TValue *i_o = (obj);                                                   \
        i_o->value.gc = cast(GCObject *, (x));                                 \
        i_o->tt = LUA_TSTRING;                                                 \
    }

#define setuvalue(L, obj, x)                                                   \
    {                                                                          \
        TValue *i_o = (obj);                                                   \
        i_o->value.gc = cast(GCObject *, (x));                                 \
        i_o->tt = LUA_TUSERDATA;                                               \
    }

#define setthvalue(L, obj, x)                                                  \
    {                                                                          \
        TValue *i_o = (obj);                                                   \
        i_o->value.gc = cast(GCObject *, (x));                                 \
        i_o->tt = LUA_TTHREAD;                                                 \
    }

#define setclvalue(L, obj, x)                                                  \
    {                                                                          \
        TValue *i_o = (obj);                                                   \
        i_o->value.gc = cast(GCObject *, (x));                                 \
        i_o->tt = LUA_TFUNCTION;                                               \
    }

#define sethvalue(L, obj, x)                                                   \
    {                                                                          \
        TValue *i_o = (obj);                                                   \
        i_o->value.gc = cast(GCObject *, (x));                                 \
        i_o->tt = LUA_TTABLE;                                                  \
    }

#define setptvalue(L, obj, x)                                                  \
    {                                                                          \
        TValue *i_o = (obj);                                                   \
        i_o->value.gc = cast(GCObject *, (x));                                 \
        i_o->tt = LUA_TPROTO;                                                  \
    }

#define setobj(L, obj1, obj2)                                                  \
    {                                                                          \
        const TValue *o2 = (obj2);                                             \
        TValue *o1 = (obj1);                                                   \
        o1->value = o2->value;                                                 \
        o1->tt = o2->tt;                                                       \
    }

/*
** different types of sets, according to destination
*/

/* from stack to (same) stack */
#define setobjs2s setobj
/* to stack (not from same stack) */
#define setobj2s setobj
#define setsvalue2s setsvalue
#define sethvalue2s sethvalue
#define setptvalue2s setptvalue
/* from table to same table */
#define setobjt2t setobj
/* to table */
#define setobj2t setobj
/* to new object */
#define setobj2n setobj
#define setsvalue2n setsvalue

#define setttype(obj, tt) (ttype(obj) = (tt))

#define iscollectable(o) (ttype(o) >= LUA_TSTRING)

using StkId = TValue *; /* index to stack elements */

/*
** String headers for string table
*/
struct TString {
    CommonHeader;
    lu_byte reserved;
    unsigned int hash;
    size_t len;
};

#define getstr(ts) cast(const char *, (ts) + 1)
#define svalue(o) getstr(tsvalue(o))

struct Udata {
    CommonHeader;
    Table *metatable;
    Table *env;
    size_t len;
};

/*
** Function Prototypes
*/
struct Proto {
    CommonHeader;
    TValue *k; /* constants used by the function */
    Instruction *code;
    struct Proto **p;       /* functions defined inside the function */
    int *lineinfo;          /* map from opcodes to source lines */
    struct LocVar *locvars; /* information about local variables */
    TString **upvalues;     /* upvalue names */
    TString *source;
    int sizeupvalues;
    int sizek; /* size of `k' */
    int sizecode;
    int sizelineinfo;
    int sizep; /* size of `p' */
    int sizelocvars;
    int linedefined;
    int lastlinedefined;
    GCObject *gclist;
    lu_byte nups; /* number of upvalues */
    lu_byte numparams;
    lu_byte is_vararg;
    lu_byte maxstacksize;
};

/* masks for new-style vararg */
#define VARARG_HASARG 1
#define VARARG_ISVARARG 2
#define VARARG_NEEDSARG 4

struct LocVar {
    TString *varname;
    int startpc; /* first point where variable is active */
    int endpc;   /* first point where variable is dead */
};

/*
** Upvalues
*/
struct UpVal {
    CommonHeader;
    TValue *v; /* points to stack or to its own value */
    union {
        TValue value; /* the value (when closed) */
        struct {      /* double linked list (when open) */
            UpVal *prev;
            UpVal *next;
        } l;
    } u;
};

/*
** Closures
*/
#define ClosureHeader                                                          \
    CommonHeader;                                                              \
    lu_byte isC;                                                               \
    lu_byte nupvalues;                                                         \
    GCObject *gclist;                                                          \
    Table *env

struct CClosure {
    ClosureHeader;
    lua_CFunction f;
    TValue upvalue[1];
};

struct LClosure {
    ClosureHeader;
    struct Proto *p;
    UpVal *upvals[1];
};

union Closure {
    CClosure c;
    LClosure l;
};

#define iscfunction(o) (ttype(o) == LUA_TFUNCTION && clvalue(o)->c.isC)
#define isLfunction(o) (ttype(o) == LUA_TFUNCTION && !clvalue(o)->c.isC)

/*
** Tables
*/
struct TKey : public TValue{
    struct Node *next; /* for chaining */
};

struct Node {
    TValue i_val;
    TKey i_key;
};

struct Table {
    CommonHeader;
    lu_byte flags;     /* 1<<p means tagmethod(p) is not present */
    lu_byte lsizenode; /* log2 of size of `node' array */
    Table *metatable;
    TValue *array; /* array part */
    Node *node;
    Node *lastfree; /* any free position is before this position */
    GCObject *gclist;
    int sizearray; /* size of `array' array */
};

/*
** `module' operation for hashing (size is always a power of 2)
*/
#define lmod(s, size) (cast(int, (s) & ((size)-1)))

#define twoto(x) (1 << (x))
#define sizenode(t) (twoto((t)->lsizenode))

#define luaO_nilobject (&luaO_nilobject_)

LUAI_DATA const TValue luaO_nilobject_;

#define ceillog2(x) (luaO_log2((x)-1) + 1)

LUAI_FUNC int luaO_log2(unsigned int x);
LUAI_FUNC int luaO_int2fb(unsigned int x);
LUAI_FUNC int luaO_fb2int(int x);
LUAI_FUNC int luaO_rawequalObj(const TValue *t1, const TValue *t2);
LUAI_FUNC int luaO_str2d(const char *s, lua_Number *result);
LUAI_FUNC const char *luaO_pushvfstring(lua_State *L, const char *fmt,
                                        va_list argp);
LUAI_FUNC const char *luaO_pushfstring(lua_State *L, const char *fmt, ...);
LUAI_FUNC void luaO_chunkid(char *out, const char *source, size_t len);

#endif
