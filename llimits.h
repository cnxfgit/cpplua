#ifndef llimits_h
#define llimits_h

#include <climits>
#include <cstddef>

#include "lua.h"

using lu_int32 = LUAI_UINT32;

using lu_mem = LUAI_UMEM;

using l_mem = LUAI_MEM;

/* chars used as small naturals (so that `char' is reserved for characters) */
using lu_byte = unsigned char;

#define MAX_SIZET ((size_t)(~(size_t)0) - 2)

#define MAX_LUMEM ((lu_mem)(~(lu_mem)0) - 2)

#define MAX_INT (INT_MAX - 2) /* maximum value of an int (-2 for safety) */

/*
** conversion of pointer to integer
** this is for hashing only; there is no problem if the integer
** cannot hold the whole pointer value
*/
#define IntPoint(p) ((unsigned int)(lu_mem)(p))

/* type to ensure maximum alignment */
using L_Umaxalign = union Umaxalign {
    double u;
    void *s;
    long l;
};

/* result of a `usual argument conversion' over lua_Number */
using l_uacNumber = LUA_NUMBER;

#define cast(t, exp) ((t)(exp))
#define cast_byte(i) cast(lu_byte, (i))
#define cast_num(i) cast(lua_Number, (i))
#define cast_int(i) cast(int, (i))

/*
** type for virtual-machine instructions
** must be an unsigned with (at least) 4 bytes (see details in lopcodes.h)
*/
using Instruction = lu_int32;

/* maximum stack for a Lua function */
#define MAXSTACK 250

/* minimum size for the string table (must be power of 2) */
#define MINSTRTABSIZE 32

/* minimum size for string buffer */
#define LUA_MINBUFFER 32

#define luai_threadyield(L)                                                    \
    {}

#endif
