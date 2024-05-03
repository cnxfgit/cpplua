#ifndef lmem_h
#define lmem_h

#include <cstddef>

#include "llimits.h"
#include "lua.h"

#define MEMERRMSG "not enough memory"

#define luaM_reallocv(L, b, on, n, e)                                          \
    ((cast(size_t, (n) + 1) <= MAX_SIZET / (e)) ? /* +1 to avoid warnings */   \
         luaM_realloc_(L, (b), (on) * (e), (n) * (e))                          \
                                                : luaM_toobig(L))

LUAI_FUNC void *luaM_realloc_(lua_State *L, void *block, size_t oldsize,
                              size_t size);
LUAI_FUNC void *luaM_toobig(lua_State *L);
LUAI_FUNC void *luaM_growaux_(lua_State *L, void *block, int *size,
                              size_t size_elem, int limit,
                              const char *errormsg);

template <typename T>
inline void luaM_growvector(lua_State *L, T **v, size_t nelems, int *size,
                            int limit, const char *e) {
    if (nelems + 1 > (size_t)(*size))
        *v = cast(T *, luaM_growaux_(L, *v, size, sizeof(T), limit, e));
}

#define luaM_freemem(L, b, s) luaM_realloc_(L, (b), (s), 0)
#define luaM_free(L, b) luaM_realloc_(L, (b), sizeof(*(b)), 0)
template <typename T>
inline void luaM_freearray(lua_State *L, void *block, size_t oldSize) {
    luaM_reallocv(L, block, oldSize, 0, sizeof(T));
}

#define luaM_malloc(L, size) luaM_realloc_(L, nullptr, 0, size)
template <typename T> inline T *luaM_new(lua_State *L) {
    return cast(T *, luaM_malloc(L, sizeof(T)));
}
template <typename T> inline T *luaM_newvector(lua_State *L, size_t newSize) {
    return cast(T *, luaM_reallocv(L, nullptr, 0, newSize, sizeof(T)));
}

template <typename T>
inline void luaM_reallocvector(lua_State *L, T **v, size_t oldSize,
                               size_t newSize) {
    *v = cast(T *, luaM_reallocv(L, *v, oldSize, newSize, sizeof(T)));
}

#endif
