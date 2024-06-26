#include "lua.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "lauxlib.h"
#include "lualib.h"

#include <unistd.h>
#define lua_stdin_is_tty() isatty(0)

static lua_State *globalL = nullptr;

static const char *progname = LUA_PROGNAME;

static void lstop(lua_State *L, lua_Debug *ar) {
    UNUSED(ar); /* unused arg. */
    lua_sethook(L, nullptr, 0, 0);
    luaL_error(L, "interrupted!");
}

static void laction(int i) {
    signal(i, SIG_DFL); /* if another SIGINT happens before lstop,
                                terminate process (default action) */
    lua_sethook(globalL, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}

static void print_usage(void) {
    fprintf(stderr,
            R"(usage: %s [options] [script [args]].
Available options are:
  -e stat  execute string 'stat'
  -l name  require library 'name'
  -i       enter interactive mode after executing 'script'
  -v       show version information
  --       stop handling options
  -        execute stdin and stop handling options
)",
            progname);
    fflush(stderr);
}

static void l_message(const char *pname, const char *msg) {
    if (pname)
        fprintf(stderr, "%s: ", pname);
    fprintf(stderr, "%s\n", msg);
    fflush(stderr);
}

static int report(lua_State *L, int status) {
    if (status && !lua_isnil(L, -1)) {
        const char *msg = lua_tostring(L, -1);
        if (msg == nullptr)
            msg = "(error object is not a string)";
        l_message(progname, msg);
        lua_pop(L, 1);
    }
    return status;
}

static int traceback(lua_State *L) {
    lua_getfield(L, LUA_GLOBALSINDEX, "debug");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 1;
    }
    lua_getfield(L, -1, "traceback");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return 1;
    }
    lua_pushvalue(L, 1);   /* pass error message */
    lua_pushinteger(L, 2); /* skip this function and traceback */
    lua_call(L, 2, 1);     /* call debug.traceback */
    return 1;
}

static int docall(lua_State *L, int narg, int clear) {
    int status;
    int base = lua_gettop(L) - narg; /* function index */
    lua_pushcfunction(L, traceback); /* push traceback function */
    lua_insert(L, base);             /* put it under chunk and args */
    signal(SIGINT, laction);
    status = lua_pcall(L, narg, (clear ? 0 : LUA_MULTRET), base);
    signal(SIGINT, SIG_DFL);
    lua_remove(L, base); /* remove traceback function */
    /* force a complete garbage collection in case of errors */
    if (status != 0)
        lua_gc(L, LUA_GCCOLLECT, 0);
    return status;
}

static void print_version(void) {
    l_message(nullptr, LUA_VERSION "  " LUA_COPYRIGHT);
}

static int getargs(lua_State *L, char **argv, int n) {
    int argc = 0;
    while (argv[argc])
        argc++;            /* count total number of arguments */
    int narg = argc - (n + 1); /* number of arguments to the script */
    luaL_checkstack(L, narg + 3, "too many arguments to script");
    for (int i = n + 1; i < argc; i++)
        lua_pushstring(L, argv[i]);
    lua_createtable(L, narg, n + 1);
    for (int i = 0; i < argc; i++) {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, i - n);
    }
    return narg;
}

static int dofile(lua_State *L, const char *name) {
    int status = luaL_loadfile(L, name) || docall(L, 0, 1);
    return report(L, status);
}

static int dostring(lua_State *L, const char *s, const char *name) {
    int status = luaL_loadbuffer(L, s, strlen(s), name) || docall(L, 0, 1);
    return report(L, status);
}

static int dolibrary(lua_State *L, const char *name) {
    lua_getglobal(L, "require");
    lua_pushstring(L, name);
    return report(L, lua_pcall(L, 1, 0, 0));
}

static const char *get_prompt(lua_State *L, int firstline) {
    const char *p;
    lua_getfield(L, LUA_GLOBALSINDEX, firstline ? "_PROMPT" : "_PROMPT2");
    p = lua_tostring(L, -1);
    if (p == nullptr)
        p = (firstline ? LUA_PROMPT : LUA_PROMPT2);
    lua_pop(L, 1); /* remove global */
    return p;
}

static int incomplete(lua_State *L, int status) {
    if (status == LUA_ERRSYNTAX) {
        size_t lmsg;
        const char *msg = lua_tolstring(L, -1, &lmsg);
        const char *tp = msg + lmsg - (sizeof(LUA_QL("<eof>")) - 1);
        if (strstr(msg, LUA_QL("<eof>")) == tp) {
            lua_pop(L, 1);
            return 1;
        }
    }
    return 0; /* else... */
}

static int pushline(lua_State *L, int firstline) {
    char buffer[LUA_MAXINPUT];
    char *b = buffer;
    size_t l;
    const char *prmt = get_prompt(L, firstline);
    if (lua_readline(L, b, prmt) == 0)
        return 0; /* no input */
    l = strlen(b);
    if (l > 0 && b[l - 1] == '\n') /* line ends with newline? */
        b[l - 1] = '\0';           /* remove it */
    if (firstline && b[0] == '=')  /* first line starts with `=' ? */
        lua_pushfstring(L, "return %s", b + 1); /* change it to `return' */
    else
        lua_pushstring(L, b);
    lua_freeline(L, b);
    return 1;
}

static int loadline(lua_State *L) {
    int status;
    lua_settop(L, 0);
    if (!pushline(L, 1))
        return -1; /* no input */
    for (;;) {     /* repeat until gets a complete line */
        status =
            luaL_loadbuffer(L, lua_tostring(L, 1), lua_strlen(L, 1), "=stdin");
        if (!incomplete(L, status))
            break;           /* cannot try to add lines? */
        if (!pushline(L, 0)) /* no more input? */
            return -1;
        lua_pushliteral(L, "\n"); /* add a new line... */
        lua_insert(L, -2);        /* ...between the two lines */
        lua_concat(L, 3);         /* join them */
    }
    lua_saveline(L, 1);
    lua_remove(L, 1); /* remove line */
    return status;
}

static void dotty(lua_State *L) {
    int status;
    const char *oldprogname = progname;
    progname = nullptr;
    while ((status = loadline(L)) != -1) {
        if (status == 0)
            status = docall(L, 0, 0);
        report(L, status);
        if (status == 0 && lua_gettop(L) > 0) { /* any result to print? */
            lua_getglobal(L, "print");
            lua_insert(L, 1);
            if (lua_pcall(L, lua_gettop(L) - 1, 0, 0) != 0)
                l_message(
                    progname,
                    lua_pushfstring(L, "error calling " LUA_QL("print") " (%s)",
                                    lua_tostring(L, -1)));
        }
    }
    lua_settop(L, 0); /* clear stack */
    fputs("\n", stdout);
    fflush(stdout);
    progname = oldprogname;
}

static int handle_script(lua_State *L, char **argv, int n) {
    int narg = getargs(L, argv, n); /* collect arguments */
    lua_setglobal(L, "arg");
    const char *fname = argv[n];
    if (strcmp(fname, "-") == 0 && strcmp(argv[n - 1], "--") != 0)
        fname = nullptr; /* stdin */
    int status = luaL_loadfile(L, fname);
    lua_insert(L, -(narg + 1));
    if (status == 0)
        status = docall(L, narg, 0);
    else
        lua_pop(L, narg);
    return report(L, status);
}

static int collectargs(char **argv, int *pi, int *pv, int *pe) {
    for (int i = 1; argv[i] != nullptr; i++) {
        if (argv[i][0] != '-') /* not an option? */
            return i;
        switch (argv[i][1]) { /* option */
        case '-':
            return (argv[i + 1] != nullptr ? i + 1 : 0);
        case '\0':
            return i;
        case 'i':
            *pi = 1; /* go through */
        case 'v':
            *pv = 1;
            break;
        case 'e':
            *pe = 1; /* go through */
        case 'l':
            if (argv[i][2] == '\0') {
                i++;
                if (argv[i] == nullptr)
                    return -1;
            }
            break;
        default:
            return -1; /* invalid option */
        }
    }
    return 0;
}

static int runargs(lua_State *L, char **argv, int n) {
    for (int i = 1; i < n; i++) {
        if (argv[i] == nullptr)
            continue;
        switch (argv[i][1]) { /* option */
        case 'e': {
            const char *chunk = argv[i] + 2;
            if (*chunk == '\0')
                chunk = argv[++i];
            if (dostring(L, chunk, "=(command line)") != 0)
                return 1;
            break;
        }
        case 'l': {
            const char *filename = argv[i] + 2;
            if (*filename == '\0')
                filename = argv[++i];
            if (dolibrary(L, filename))
                return 1; /* stop if file fails */
            break;
        }
        default:
            break;
        }
    }
    return 0;
}

static int handle_luainit(lua_State *L) {
    const char *init = getenv("LUA_INIT");
    if (init == nullptr)
        return 0; /* status OK */
    if (init[0] == '@') {
        return dofile(L, init + 1);
    }
    return dostring(L, init, "=LUA_INIT");
}

struct Smain {
    int argc;
    char **argv;
    int status;
};

static int pmain(lua_State *L) {
    Smain *s = (Smain *)lua_touserdata(L, 1);
    char **argv = s->argv;

    int has_i = 0, has_v = 0, has_e = 0;
    globalL = L;
    if (argv[0] && argv[0][0])
        progname = argv[0];
    lua_gc(L, LUA_GCSTOP, 0); /* stop collector during initialization */
    luaL_openlibs(L);         /* open libraries */
    lua_gc(L, LUA_GCRESTART, 0);
    s->status = handle_luainit(L);
    if (s->status != 0)
        return 0;
    int script = collectargs(argv, &has_i, &has_v, &has_e);
    if (script < 0) { /* invalid args? */
        print_usage();
        s->status = 1;
        return 0;
    }
    if (has_v)
        print_version();
    s->status = runargs(L, argv, (script > 0) ? script : s->argc);
    if (s->status != 0)
        return 0;
    if (script)
        s->status = handle_script(L, argv, script);
    if (s->status != 0)
        return 0;
    if (has_i)
        dotty(L);
    else if (script == 0 && !has_e && !has_v) {
        if (lua_stdin_is_tty()) {
            print_version();
            dotty(L);
        } else
            dofile(L, nullptr); /* executes stdin as a file */
    }
    return 0;
}

int main(int argc, char **argv) {
    Smain s;
    lua_State *L = lua_open(); /* create state */
    if (L == nullptr) {
        l_message(argv[0], "cannot create state: not enough memory");
        return EXIT_FAILURE;
    }
    s.argc = argc;
    s.argv = argv;
    int status = lua_cpcall(L, &pmain, &s);
    report(L, status);
    lua_close(L);
    return (status || s.status) ? EXIT_FAILURE : EXIT_SUCCESS;
}
