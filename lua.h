#ifndef lua_h
#define lua_h

#include <climits>
#include <cstdarg>
#include <cstddef>

#define LUA_ROOT "/usr/local/"
#define LUA_LDIR LUA_ROOT "share/lua/5.1/"
#define LUA_CDIR LUA_ROOT "lib/lua/5.1/"
#define LUA_PATH_DEFAULT                                                       \
    "./?.lua;" LUA_LDIR "?.lua;" LUA_LDIR "?/init.lua;" LUA_CDIR               \
    "?.lua;" LUA_CDIR "?/init.lua"
#define LUA_CPATH_DEFAULT "./?.so;" LUA_CDIR "?.so;" LUA_CDIR "loadall.so"

#define LUA_DIRSEP "/"
#define LUA_PATHSEP ";"
#define LUA_PATH_MARK "?"
#define LUA_EXECDIR "!"
#define LUA_IGMARK "-"

#define LUA_INTEGER ptrdiff_t
#define LUA_NUMBER double

#define LUA_QL(x) "'" x "'"
#define LUA_QS LUA_QL("%s")

#define LUA_IDSIZE 60
#define LUA_MAXINPUT 512
#define LUA_MAXCAPTURES 32

#define LUA_PROMPT "> "
#define LUA_PROMPT2 ">> "
#define LUA_PROGNAME "lua"

#include <cstdio>
#include <readline/history.h>
#include <readline/readline.h>
#define lua_readline(L, b, p) ((void)L, ((b) = readline(p)) != nullptr)
#define lua_saveline(L, idx)                                                   \
    if (lua_strlen(L, idx) > 0)            /* non-empty line? */               \
        add_history(lua_tostring(L, idx)); /* add it to history */
#define lua_freeline(L, b) ((void)L, free(b))

#define LUAI_GCPAUSE 200 /* 200% (wait memory to double before next GC) */
#define LUAI_GCMUL 200   /* GC runs 'twice the speed' of memory allocation */

#define LUAI_UINT32 unsigned int
#define LUAI_INT32 int
#define LUAI_UMEM size_t
#define LUAI_MEM ptrdiff_t

#define LUAI_MAXCALLS 20000
#define LUAI_MAXCSTACK 2048
#define LUAI_MAXCCALLS 200
#define LUAI_MAXVARS 200
#define LUAI_MAXUPVALUES 60

/* minimum Lua stack available to a C function */
#define LUA_MINSTACK 20

#define LUAL_BUFFERSIZE BUFSIZ

#define LUA_NUMBER_SCAN "%lf"
#define LUA_NUMBER_FMT "%.14g"
#define lua_number2str(s, n) sprintf((s), LUA_NUMBER_FMT, (n))
#define LUAI_MAXNUMBER2STR 32 /* 16 digits, sign, point, and \0 */
#define lua_str2number(s, p) strtod((s), (p))

#if defined(LUA_CORE)
#include <cmath>
#define luai_numadd(a, b) ((a) + (b))
#define luai_numsub(a, b) ((a) - (b))
#define luai_nummul(a, b) ((a) * (b))
#define luai_numdiv(a, b) ((a) / (b))
#define luai_nummod(a, b) ((a)-floor((a) / (b)) * (b))
#define luai_numpow(a, b) (pow(a, b))
#define luai_numunm(a) (-(a))
#define luai_numeq(a, b) ((a) == (b))
#define luai_numlt(a, b) ((a) < (b))
#define luai_numle(a, b) ((a) <= (b))
#define luai_numisnan(a) (!luai_numeq((a), (a)))
#endif

#define lua_number2int(i, d) ((i) = (int)(d))
#define lua_number2integer(i, d) ((i) = (lua_Integer)(d))

#define LUAI_TRY(L, c, a)                                                      \
    try {                                                                      \
        a                                                                      \
    } catch (...) {                                                            \
        if ((c)->status == 0)                                                  \
            (c)->status = -1;                                                  \
    }
#define LUAI_THROW(L, c) throw(c)

#if defined(loslib_c)

#include <unistd.h>
#define LUA_TMPNAMBUFSIZE 32
#define lua_tmpnam(b, e)                                                       \
    {                                                                          \
        strcpy(b, "/tmp/lua_XXXXXX");                                          \
        e = mkstemp(b);                                                        \
        if (e != -1)                                                           \
            close(e);                                                          \
        e = (e == -1);                                                         \
    }

#endif

#define lua_popen(L, c, m) (UNUSED(L), popen(c, m))
#define lua_pclose(L, file) (UNUSED(L), (pclose(file) != -1))

#define LUA_INTFRMLEN "l"
#define LUA_INTFRM_T long

#define LUA_VERSION "Lua 5.1"
#define LUA_COPYRIGHT "Copyright (C) 1994-2006 Lua.org, PUC-Rio"
#define LUA_AUTHORS "R. Ierusalimschy, L. H. de Figueiredo & W. Celes"

/* mark for precompiled code (`<esc>Lua') */
#define LUA_SIGNATURE "\033Lua"

/* option for multiple returns in `lua_pcall' and `lua_call' */
#define LUA_MULTRET (-1)

/*
** pseudo-indices
*/
#define LUA_REGISTRYINDEX (-10000)
#define LUA_ENVIRONINDEX (-10001)
#define LUA_GLOBALSINDEX (-10002)
#define lua_upvalueindex(i) (LUA_GLOBALSINDEX - (i))

/* thread status; 0 is OK */
#define LUA_YIELD 1
#define LUA_ERRRUN 2
#define LUA_ERRSYNTAX 3
#define LUA_ERRMEM 4
#define LUA_ERRERR 5

struct lua_State;

using lua_CFunction = int (*)(lua_State *L);

/*
** functions that read/write blocks when loading/dumping Lua chunks
*/
using lua_Reader = const char *(*)(lua_State *L, void *ud, size_t *sz);

using lua_Writer = int (*)(lua_State *L, const void *p, size_t sz, void *ud);

/*
** prototype for memory-allocation functions
*/
using lua_Alloc = void *(*)(void *ptr, size_t nsize);

/*
** basic types
*/
#define LUA_TNONE (-1)

#define LUA_TNIL 0
#define LUA_TBOOLEAN 1
#define LUA_TLIGHTUSERDATA 2
#define LUA_TNUMBER 3
#define LUA_TSTRING 4
#define LUA_TTABLE 5
#define LUA_TFUNCTION 6
#define LUA_TUSERDATA 7
#define LUA_TTHREAD 8

/*
** generic extra include file
*/
#define LUA_API extern
#define LUALIB_API extern
#define LUAI_FUNC extern
#define LUAI_DATA extern

#include <cassert>

/* to avoid warnings, and to make sure value is really unused */
#define UNUSED(x) ((void)(x))

/* type of numbers in Lua */
using lua_Number = LUA_NUMBER;

/* type for integer functions */
using lua_Integer = LUA_INTEGER;

#define lua_open luaL_newstate

/*
** state manipulation
*/
LUA_API lua_State *(lua_newstate)(lua_Alloc f);
LUA_API void(lua_close)(lua_State *L);
LUA_API lua_State *(lua_newthread)(lua_State *L);

LUA_API lua_CFunction(lua_atpanic)(lua_State *L, lua_CFunction panicf);

/*
** basic stack manipulation
*/
LUA_API int(lua_gettop)(lua_State *L);
LUA_API void(lua_settop)(lua_State *L, int idx);
LUA_API void(lua_pushvalue)(lua_State *L, int idx);
LUA_API void(lua_remove)(lua_State *L, int idx);
LUA_API void(lua_insert)(lua_State *L, int idx);
LUA_API void(lua_replace)(lua_State *L, int idx);
LUA_API int(lua_checkstack)(lua_State *L, int sz);

LUA_API void(lua_xmove)(lua_State *from, lua_State *to, int n);

/*
** access functions (stack -> C)
*/

LUA_API int(lua_isnumber)(lua_State *L, int idx);
LUA_API int(lua_isstring)(lua_State *L, int idx);
LUA_API int(lua_iscfunction)(lua_State *L, int idx);
LUA_API int(lua_isuserdata)(lua_State *L, int idx);
LUA_API int(lua_type)(lua_State *L, int idx);
LUA_API const char *(lua_typename)(lua_State *L, int tp);

LUA_API int(lua_equal)(lua_State *L, int idx1, int idx2);
LUA_API int(lua_rawequal)(lua_State *L, int idx1, int idx2);
LUA_API int(lua_lessthan)(lua_State *L, int idx1, int idx2);

LUA_API lua_Number(lua_tonumber)(lua_State *L, int idx);
LUA_API lua_Integer(lua_tointeger)(lua_State *L, int idx);
LUA_API int(lua_toboolean)(lua_State *L, int idx);
LUA_API const char *(lua_tolstring)(lua_State *L, int idx, size_t *len);
LUA_API size_t(lua_objlen)(lua_State *L, int idx);
LUA_API lua_CFunction(lua_tocfunction)(lua_State *L, int idx);
LUA_API void *(lua_touserdata)(lua_State *L, int idx);
LUA_API lua_State *(lua_tothread)(lua_State *L, int idx);
LUA_API const void *(lua_topointer)(lua_State *L, int idx);

/*
** push functions (C -> stack)
*/
LUA_API void(lua_pushnil)(lua_State *L);
LUA_API void(lua_pushnumber)(lua_State *L, lua_Number n);
LUA_API void(lua_pushinteger)(lua_State *L, lua_Integer n);
LUA_API void(lua_pushlstring)(lua_State *L, const char *s, size_t l);
LUA_API void(lua_pushstring)(lua_State *L, const char *s);
LUA_API const char *(lua_pushvfstring)(lua_State *L, const char *fmt,
                                       va_list argp);
LUA_API const char *(lua_pushfstring)(lua_State *L, const char *fmt, ...);
LUA_API void(lua_pushcclosure)(lua_State *L, lua_CFunction fn, int n);
LUA_API void(lua_pushboolean)(lua_State *L, int b);
LUA_API void(lua_pushlightuserdata)(lua_State *L, void *p);
LUA_API int(lua_pushthread)(lua_State *L);

/*
** get functions (Lua -> stack)
*/
LUA_API void(lua_gettable)(lua_State *L, int idx);
LUA_API void(lua_getfield)(lua_State *L, int idx, const char *k);
LUA_API void(lua_rawget)(lua_State *L, int idx);
LUA_API void(lua_rawgeti)(lua_State *L, int idx, int n);
LUA_API void(lua_createtable)(lua_State *L, int narr, int nrec);
LUA_API void *(lua_newuserdata)(lua_State *L, size_t sz);
LUA_API int(lua_getmetatable)(lua_State *L, int objindex);
LUA_API void(lua_getfenv)(lua_State *L, int idx);

/*
** set functions (stack -> Lua)
*/
LUA_API void(lua_settable)(lua_State *L, int idx);
LUA_API void(lua_setfield)(lua_State *L, int idx, const char *k);
LUA_API void(lua_rawset)(lua_State *L, int idx);
LUA_API void(lua_rawseti)(lua_State *L, int idx, int n);
LUA_API int(lua_setmetatable)(lua_State *L, int objindex);
LUA_API int(lua_setfenv)(lua_State *L, int idx);

/*
** `load' and `call' functions (load and run Lua code)
*/
LUA_API void(lua_call)(lua_State *L, int nargs, int nresults);
LUA_API int(lua_pcall)(lua_State *L, int nargs, int nresults, int errfunc);
LUA_API int(lua_cpcall)(lua_State *L, lua_CFunction func, void *ud);
LUA_API int(lua_load)(lua_State *L, lua_Reader reader, void *dt,
                      const char *chunkname);

LUA_API int(lua_dump)(lua_State *L, lua_Writer writer, void *data);

/*
** coroutine functions
*/
LUA_API int(lua_yield)(lua_State *L, int nresults);
LUA_API int(lua_resume)(lua_State *L, int narg);
LUA_API int(lua_status)(lua_State *L);

/*
** garbage-collection function and options
*/

#define LUA_GCSTOP 0
#define LUA_GCRESTART 1
#define LUA_GCCOLLECT 2
#define LUA_GCCOUNT 3
#define LUA_GCCOUNTB 4
#define LUA_GCSTEP 5
#define LUA_GCSETPAUSE 6
#define LUA_GCSETSTEPMUL 7

LUA_API int(lua_gc)(lua_State *L, int what, int data);

/*
** miscellaneous functions
*/

LUA_API int(lua_error)(lua_State *L);

LUA_API int(lua_next)(lua_State *L, int idx);

LUA_API void(lua_concat)(lua_State *L, int n);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/

#define lua_pop(L, n) lua_settop(L, -(n)-1)

#define lua_newtable(L) lua_createtable(L, 0, 0)

#define lua_register(L, n, f) (lua_pushcfunction(L, (f)), lua_setglobal(L, (n)))

#define lua_pushcfunction(L, f) lua_pushcclosure(L, (f), 0)

#define lua_strlen(L, i) lua_objlen(L, (i))

#define lua_isfunction(L, n) (lua_type(L, (n)) == LUA_TFUNCTION)
#define lua_istable(L, n) (lua_type(L, (n)) == LUA_TTABLE)
#define lua_islightuserdata(L, n) (lua_type(L, (n)) == LUA_TLIGHTUSERDATA)
#define lua_isnil(L, n) (lua_type(L, (n)) == LUA_TNIL)
#define lua_isboolean(L, n) (lua_type(L, (n)) == LUA_TBOOLEAN)
#define lua_isthread(L, n) (lua_type(L, (n)) == LUA_TTHREAD)
#define lua_isnone(L, n) (lua_type(L, (n)) == LUA_TNONE)
#define lua_isnoneornil(L, n) (lua_type(L, (n)) <= 0)

#define lua_pushliteral(L, s)                                                  \
    lua_pushlstring(L, "" s, (sizeof(s) / sizeof(char)) - 1)

#define lua_setglobal(L, s) lua_setfield(L, LUA_GLOBALSINDEX, (s))
#define lua_getglobal(L, s) lua_getfield(L, LUA_GLOBALSINDEX, (s))

#define lua_tostring(L, i) lua_tolstring(L, (i), nullptr)

/*
** compatibility macros and functions
*/

#define lua_getregistry(L) lua_pushvalue(L, LUA_REGISTRYINDEX)

#define lua_getgccount(L) lua_gc(L, LUA_GCCOUNT, 0)

/*
** {======================================================================
** Debug API
** =======================================================================
*/

/*
** Event codes
*/
#define LUA_HOOKCALL 0
#define LUA_HOOKRET 1
#define LUA_HOOKLINE 2
#define LUA_HOOKCOUNT 3
#define LUA_HOOKTAILRET 4

/*
** Event masks
*/
#define LUA_MASKCALL (1 << LUA_HOOKCALL)
#define LUA_MASKRET (1 << LUA_HOOKRET)
#define LUA_MASKLINE (1 << LUA_HOOKLINE)
#define LUA_MASKCOUNT (1 << LUA_HOOKCOUNT)

struct lua_Debug {
    int event;
    const char *name;           /* (n) */
    const char *namewhat;       /* (n) `global', `local', `field', `method' */
    const char *what;           /* (S) `Lua', `C', `main', `tail' */
    const char *source;         /* (S) */
    int currentline;            /* (l) */
    int nups;                   /* (u) number of upvalues */
    int linedefined;            /* (S) */
    int lastlinedefined;        /* (S) */
    char short_src[LUA_IDSIZE]; /* (S) */
    /* private part */
    int i_ci; /* active function */
};

/* Functions to be called by the debuger in specific events */
using lua_Hook = void (*)(lua_State *L, lua_Debug *ar);

LUA_API int lua_getstack(lua_State *L, int level, lua_Debug *ar);
LUA_API int lua_getinfo(lua_State *L, const char *what, lua_Debug *ar);
LUA_API const char *lua_getlocal(lua_State *L, const lua_Debug *ar, int n);
LUA_API const char *lua_setlocal(lua_State *L, const lua_Debug *ar, int n);
LUA_API const char *lua_getupvalue(lua_State *L, int funcindex, int n);
LUA_API const char *lua_setupvalue(lua_State *L, int funcindex, int n);

LUA_API int lua_sethook(lua_State *L, lua_Hook func, int mask, int count);
LUA_API lua_Hook lua_gethook(lua_State *L);
LUA_API int lua_gethookmask(lua_State *L);
LUA_API int lua_gethookcount(lua_State *L);

#endif
