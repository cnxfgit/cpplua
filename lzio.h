#ifndef lzio_h
#define lzio_h

#include "lua.h"

#include "lmem.h"

#define EOZ (-1) /* end of stream */

struct Zio {
    size_t n;      /* bytes still unread */
    const char *p; /* current position in buffer */
    lua_Reader reader;
    void *data;   /* additional data */
    lua_State *L; /* Lua state (for reader) */
};

using ZIO = Zio;

#define char2int(c) cast(int, cast(unsigned char, (c)))

#define zgetc(z) (((z)->n--) > 0 ? char2int(*(z)->p++) : luaZ_fill(z))

struct Mbuffer {
    char *buffer;
    size_t n;
    size_t buffsize;
    Mbuffer() : buffer(nullptr), buffsize(0) {}
    ~Mbuffer() {}
};

#define luaZ_buffer(buff) ((buff)->buffer)
#define luaZ_sizebuffer(buff) ((buff)->buffsize)
#define luaZ_bufflen(buff) ((buff)->n)

#define luaZ_resetbuffer(buff) ((buff)->n = 0)

#define luaZ_resizebuffer(L, buff, size)                                       \
    {                                                                          \
        luaM_reallocvector<char>(L, &(buff)->buffer, (buff)->buffsize, size);   \
        (buff)->buffsize = size;                                               \
    }

LUAI_FUNC char *luaZ_openspace(lua_State *L, Mbuffer *buff, size_t n);
LUAI_FUNC void luaZ_init(lua_State *L, ZIO *z, lua_Reader reader, void *data);
LUAI_FUNC size_t luaZ_read(ZIO *z, void *b, size_t n); /* read next n bytes */
LUAI_FUNC int luaZ_lookahead(ZIO *z);

/* --------- Private Part ------------------ */

LUAI_FUNC int luaZ_fill(ZIO *z);

#endif
