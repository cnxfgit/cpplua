/*
** $Id: llimits.h,v 1.68 2005/12/22 16:19:56 roberto Exp roberto $
** Limits, basic types, and some other `installation-dependent' definitions
** See Copyright Notice in lua.h
*/

#ifndef llimits_h
#define llimits_h


#include <climits>
#include <cstddef>


#include "lua.h"


typedef LUAI_UINT32 lu_int32;

typedef LUAI_UMEM lu_mem;

typedef LUAI_MEM l_mem;



/* chars used as small naturals (so that `char' is reserved for characters) */
typedef unsigned char lu_byte;


#define MAX_SIZET	((size_t)(~(size_t)0)-2)

#define MAX_LUMEM	((lu_mem)(~(lu_mem)0)-2)


#define MAX_INT (INT_MAX-2)  /* maximum value of an int (-2 for safety) */

/*
** conversion of pointer to integer
** this is for hashing only; there is no problem if the integer
** cannot hold the whole pointer value
*/
#define IntPoint(p)  ((unsigned int)(lu_mem)(p))



/* type to ensure maximum alignment */
typedef LUAI_USER_ALIGNMENT_T L_Umaxalign;


/* result of a `usual argument conversion' over lua_Number */
typedef LUAI_UACNUMBER l_uacNumber;


/* internal assertions for in-house debugging */
#define check_exp(c,e)		(lua_assert(c), (e))
#define api_check(l,e)		lua_assert(e)


#define cast(t, exp)	((t)(exp))
#define cast_byte(i)	cast(lu_byte, (i))
#define cast_num(i)	cast(lua_Number, (i))
#define cast_int(i)	cast(int, (i))



/*
** type for virtual-machine instructions
** must be an unsigned with (at least) 4 bytes (see details in lopcodes.h)
*/
typedef lu_int32 Instruction;



/* maximum stack for a Lua function */
#define MAXSTACK	250



/* minimum size for the string table (must be power of 2) */
#define MINSTRTABSIZE	32


/* minimum size for string buffer */
#define LUA_MINBUFFER	32

#define luai_threadyield(L)     { }

#endif
