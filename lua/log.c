#include <stdint.h>
#include <lua.h>
#include <lauxlib.h>
#define CLOG_MAIN
#include "../clog.h"

#define MT_NAME "log_t"

typedef struct uv_buf_t {
  char* base;    // 指向缓冲区的起始地址
  size_t len;    // 缓冲区的长度（字节数）
} uv_buf_t;

static const char *const lvl_options[] = {
    "DEBUG", "INFO", "WARN", "ERROR", NULL,
};

int log_check_level(lua_State *L, int idx) {
  int lvl = 0;

  if (lua_isnumber(L, idx)) {
    lvl = lua_tointeger(L, idx);
  } else if (lua_isstring(L, idx)) {
    lvl = luaL_checkoption(L, idx, NULL, lvl_options);
  } else
    luaL_argerror(L, idx, "invalid log level");

  luaL_argcheck(L, lvl >= CLOG_DEBUG && lvl <= CLOG_ERROR, idx,
                "Out of log level range, accept DEBUG, INFO, WARN, ERROR");
  return lvl;
};

inline static int log_id(lua_State *L, int i) {
  if (lua_type(L, i) == LUA_TNUMBER)
    return lua_tointeger(L, i);
  int id = *(int *)luaL_checkudata(L, i, MT_NAME);
  luaL_argcheck(L, id >= 0 && id < CLOG_MAX_LOGGERS, i, "Out of range");
  return id;
}

static int log_tostring(lua_State *L) {
  int id = log_id(L, 1);
  lua_pushfstring(L, "%s: %02x", MT_NAME, id);
  return 1;
}

static int log_init(lua_State *L) {
  int id = log_id(L, 1);
  int ret = -1;
  if (_clog_loggers[id] != NULL) {
    *(int *)lua_newuserdata(L, sizeof(int)) = id;
    luaL_setmetatable(L, MT_NAME);
    return 1;
  };

  if (lua_type(L, 2) == LUA_TSTRING) {
    ret = clog_init_path(id, lua_tostring(L, 2));
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    ret = clog_init_fd(id, lua_tointeger(L, 2));
  } else
    ret = clog_init_fd(id, STDOUT_FILENO);
  if (ret == 0) {
    *(int *)lua_newuserdata(L, sizeof(int)) = id;
    luaL_setmetatable(L, MT_NAME);
    return 1;
  }
  lua_pushnil(L);
  lua_pushinteger(L, ret);
  return 2;
}

static int log_rotate(lua_State *L) {
  int id = log_id(L, 1);
  const char *path = luaL_checkstring(L, 2);

  int ret = clog_rotate(id, path);
  lua_pushboolean(L, ret == 0);
  return ret;
}

static int log_close(lua_State *L) {
  int id = log_id(L, 1);
  clog_free(id);
  return 0;
}

static int log_fd(lua_State *L) {
  int id = log_id(L, 1);
  struct clog *log = _clog_loggers[id];
  lua_pushinteger(L, log->fd);
  return 1;
}

static int log_isatty(lua_State *L) {
  int id = log_id(L, 1);
  struct clog *log = _clog_loggers[id];
  lua_pushboolean(L, log->isatty);
  return 1;
}

static int log_level(lua_State *L) {
  int id = log_id(L, 1);
  struct clog *log = _clog_loggers[id];
  int level = -1;
  if (lua_type(L, 2) == LUA_TNONE) {
    lua_pushstring(L, CLOG_LEVEL_NAMES[log->level]);
    return 1;
  }

  level = log_check_level(L, 2);
  level = clog_set_level(id, level);
  if (level == 0) {
    lua_pushvalue(L, 1);
    return 1;
  }
  lua_pushnil(L);
  lua_pushinteger(L, level);
  return 2;
}

static int log_date_fmt(lua_State *L) {
  int id = log_id(L, 1);
  struct clog *log = _clog_loggers[id];
  const char *fmt;
  int ret;
  if (lua_type(L, 2) == LUA_TNONE) {
    lua_pushstring(L, log->date_fmt);
    return 1;
  }
  fmt = luaL_checkstring(L, 2);
  ret = clog_set_date_fmt(id, fmt);
  if (ret == 0) {
    lua_pushvalue(L, 1);
    return 1;
  }
  lua_pushnil(L);
  lua_pushinteger(L, ret);
  return 2;
}

static int log_time_fmt(lua_State *L) {
  int id = log_id(L, 1);
  struct clog *log = _clog_loggers[id];
  const char *fmt;
  int ret;
  if (lua_type(L, 2) == LUA_TNONE) {
    lua_pushstring(L, log->time_fmt);
    return 1;
  }
  fmt = luaL_checkstring(L, 2);
  ret = clog_set_time_fmt(id, fmt);
  if (ret == 0) {
    lua_pushvalue(L, 1);
    return 1;
  }
  lua_pushnil(L);
  lua_pushinteger(L, ret);
  return 2;
}

static int log_fmt(lua_State *L) {
  int id = log_id(L, 1);
  struct clog *log = _clog_loggers[id];
  const char *fmt;
  int ret;
  if (lua_type(L, 2) == LUA_TNONE) {
    lua_pushstring(L, log->fmt);
    return 1;
  }
  fmt = luaL_checkstring(L, 2);
  ret = clog_set_fmt(id, fmt);
  if (ret == 0) {
    lua_pushvalue(L, 1);
    return 1;
  }
  lua_pushnil(L);
  lua_pushinteger(L, ret);
  return 2;
}

static const char *log_getinfo(int id, lua_State *L, int rtlvl, lua_Debug *ar) {
  int ret;
  char *fname = "?";

  struct clog *log = _clog_loggers[id];
  if (log == NULL) {
    luaL_error(L, "invalid logger, maybe closed");
    return fname;
  }
  if (strstr(log->fmt, "%f") == NULL) {
    return fname;
  }

  ret = lua_getstack(L, rtlvl, ar);
  if (ret == 0) {
    luaL_error(L, "invalid stack level");
    return fname;
  }
  ret = lua_getinfo(L, "Sl", ar);
  if (ret == 0) {
    luaL_error(L, "invalid option to getinfo");
    return fname;
  }

  if (ar->short_src[0]) {
    fname = ar->short_src;
    if (strncmp(fname, "[string \"", 9) == 0) {
      fname += 9;
      ret = strlen(fname);
      if (fname[ret - 1] == ']')
        fname[ret - 2] = '\0';
      if (fname[ret - 2] == '"')
        fname[ret - 2] = '\0';
    }
  }

  return fname;
}

static int log_debug(lua_State *L) {
  lua_Debug ar = {0};
  int id = log_id(L, 1);
  const char *str = luaL_checkstring(L, 2);
  const char *fname =
      lua_isnone(L, 3) ? log_getinfo(id, L, 1, &ar) : luaL_optstring(L, 3, "");
  ar.currentline = luaL_optinteger(L, 4, ar.currentline);

  clog_debug(fname, ar.currentline, id, "%s", str);
  lua_pushvalue(L, 1);
  return 1;
}

static int log_info(lua_State *L) {
  lua_Debug ar = {0};
  int id = log_id(L, 1);
  const char *str = luaL_checkstring(L, 2);
  const char *fname =
      lua_isnone(L, 3) ? log_getinfo(id, L, 1, &ar) : luaL_optstring(L, 3, "");
  ar.currentline = luaL_optinteger(L, 4, ar.currentline);

  clog_info(fname, ar.currentline, id, "%s", str);
  lua_pushvalue(L, 1);
  return 1;
}

static int log_warn(lua_State *L) {
  lua_Debug ar = {0};
  int id = log_id(L, 1);
  const char *str = luaL_checkstring(L, 2);
  const char *fname =
      lua_isnone(L, 3) ? log_getinfo(id, L, 1, &ar) : luaL_optstring(L, 3, "");
  ar.currentline = luaL_optinteger(L, 4, ar.currentline);

  clog_warn(fname, ar.currentline, id, "%s", str);
  lua_pushvalue(L, 1);
  return 1;
}

static int log_error(lua_State *L) {
  lua_Debug ar = {0};
  int id = log_id(L, 1);
  const char *str = luaL_checkstring(L, 2);
  const char *fname =
      lua_isnone(L, 3) ? log_getinfo(id, L, 1, &ar) : luaL_optstring(L, 3, "");
  ar.currentline = luaL_optinteger(L, 4, ar.currentline);

  clog_error(fname, ar.currentline, id, "%s", str);
  lua_pushvalue(L, 1);
  return 1;
}

void clog_buffer(const char *sfile, int sline, int id, int level,
                 const char *title, const uv_buf_t buf) {
  char str[4096];
  char txt[17];
  size_t i, idx = 0;
  const uint8_t *base = (uint8_t *)buf.base;
  int len = buf.len;
  struct clog *log = _clog_loggers[id];

  /* lvl under log->level, do nothing */
  if (log->level > level)
    return;

  if (title)
    snprintf(str + idx, sizeof(str) - idx, "dumping `%s` %p (%u bytes)", title,
             base, (unsigned int)len);
  else
    snprintf(str + idx, sizeof(str) - idx, "dumping %p (%u bytes)", base,
             (unsigned int)len);

  clog_debug(sfile, sline, id, "%s", str + idx);

  idx = 0;
  memset(txt, 0, sizeof(txt));
  for (i = 0; i < len; i++) {
    if (i >= 4096)
      break;

    if (i % 16 == 0) {
      if (i > 0) {
        idx += snprintf(str + idx, sizeof(str) - idx, "  %s\n", txt);
        clog_log(log, str, idx);

        idx = 0;
        memset(txt, 0, sizeof(txt));
      }

      idx += snprintf(str + idx, sizeof(str) - idx, "%04x: ", (unsigned int)i);
    }

    idx +=
        snprintf(str + idx, sizeof(str) - idx, " %02x", (unsigned int)base[i]);
    txt[i % 16] = (base[i] > 31 && base[i] < 127) ? base[i] : '.';
  }

  if (len > 0) {
    for (/* i = i */; i % 16 != 0; i++)
      idx += snprintf(str + idx, sizeof(str) - idx, "   ");

    idx += snprintf(str + idx, sizeof(str) - idx, "  %s\n", txt);
    clog_log(log, str, idx);
  }
}

static int log_buffer(lua_State *L) {
  int id = log_id(L, 1);
  size_t sz;
  const char *str = luaL_checklstring(L, 2, &sz);
  const char *title = luaL_optstring(L, 3, "");
  int lvl = luaL_optinteger(L, 4, 0);

  uv_buf_t buf = {0};
  lua_Debug ar = {0};
  const char *fname = log_getinfo(id, L, 1, &ar);

  buf.base = (char *)str;
  buf.len = sz;
  clog_buffer(fname, ar.currentline, id, lvl, title, buf);
  lua_pushvalue(L, 1);
  return 1;
}

static const luaL_Reg log_funcs[] = {{"close", log_close},
                                     {"rotate", log_rotate},

                                     {"fd", log_fd},
                                     {"isatty", log_isatty},
                                     {"level", log_level},
                                     {"date_fmt", log_date_fmt},
                                     {"time_fmt", log_time_fmt},
                                     {"fmt", log_fmt},

                                     {"debug", log_debug},
                                     {"info", log_info},
                                     {"warn", log_warn},
                                     {"error", log_error},
                                     {"buffer", log_buffer},

                                     {NULL, NULL}};

static int log_status(lua_State *L) {
  int i;
  lua_createtable(L, CLOG_MAX_LOGGERS, 0);
  for (i = 0; i < CLOG_MAX_LOGGERS; i++) {
    lua_pushboolean(L, _clog_loggers[i] != NULL);
    lua_rawseti(L, -2, i);
  }
  return 1;
}

static int log_hex(lua_State *L) {
  char str[224];
  char txt[32];
  size_t i, len, idx = 0;
  const uint8_t *base = (uint8_t *)luaL_checklstring(L, 1, &len);
  lua_settop(L, 1);

  // grow stack
  idx = len / 16 + 1;
  i = lua_checkstack(L, idx);
  if (!i)
    luaL_error(L,"Input data too large");

  idx = 0;
  memset(txt, 0, sizeof(txt));
  for (i = 0; i < len; i++) {
    if (i > 0xFFFF)
      break;

    if (i % 16 == 0) {
      if (i > 0) {
        idx += snprintf(str + idx, sizeof(str) - idx, "  %s\n", txt);
        lua_pushlstring(L, str, idx);

        idx = 0;
        memset(txt, 0, sizeof(txt));
      }

      idx += snprintf(str + idx, sizeof(str) - idx, "%04x: ", (unsigned int)i);
    }

    idx +=
        snprintf(str + idx, sizeof(str) - idx, " %02x", (unsigned int)base[i]);
    txt[i % 16] = (base[i] > 31 && base[i] < 127) ? base[i] : '.';
  }

  if (len > 0) {
    for (/* i = i */; i % 16 != 0; i++)
      idx += snprintf(str + idx, sizeof(str) - idx, "   ");

    idx += snprintf(str + idx, sizeof(str) - idx, "  %s\n", txt);
    lua_pushlstring(L, str, idx);
  }

  i = lua_gettop(L);
  lua_concat(L,  i - 1);
  return 1;
}

LUALIB_API
int luaopen_log(lua_State *L) {
  luaL_newmetatable(L, MT_NAME);
  lua_pushliteral(L, MT_NAME);
  lua_setfield(L, -2, "__name");
  lua_pushcfunction(L, log_tostring);
  lua_setfield(L, -2, "__tostring");
  lua_newtable(L);
  luaL_setfuncs(L, log_funcs, 0);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);

  lua_newtable(L);
  lua_pushliteral(L, "MAX_LOGGERS");
  lua_pushinteger(L, CLOG_MAX_LOGGERS);
  lua_rawset(L, -3);

  lua_pushliteral(L, "FORMAT_LENGTH");
  lua_pushinteger(L, CLOG_FORMAT_LENGTH);
  lua_rawset(L, -3);

  lua_pushliteral(L, "DATETIME_LENGTH");
  lua_pushinteger(L, CLOG_DATETIME_LENGTH);
  lua_rawset(L, -3);

  lua_pushliteral(L, "DEFAULT_FORMAT");
  lua_pushliteral(L, CLOG_DEFAULT_FORMAT);
  lua_rawset(L, -3);

  lua_pushliteral(L, "DEFAULT_DATE_FORMAT");
  lua_pushliteral(L, CLOG_DEFAULT_DATE_FORMAT);
  lua_rawset(L, -3);

  lua_pushliteral(L, "DEFAULT_TIME_FORMAT");
  lua_pushliteral(L, CLOG_DEFAULT_TIME_FORMAT);
  lua_rawset(L, -3);

  lua_pushliteral(L, "DEBUG");
  lua_pushinteger(L, CLOG_DEBUG);
  lua_rawset(L, -3);

  lua_pushliteral(L, "INFO");
  lua_pushinteger(L, CLOG_INFO);
  lua_rawset(L, -3);

  lua_pushliteral(L, "WARN");
  lua_pushinteger(L, CLOG_WARN);
  lua_rawset(L, -3);

  lua_pushliteral(L, "ERROR");
  lua_pushinteger(L, CLOG_ERROR);
  lua_rawset(L, -3);

  lua_pushliteral(L, "STDOUT");
  lua_pushinteger(L, STDOUT_FILENO);
  lua_rawset(L, -3);

  lua_pushliteral(L, "STDOERR");
  lua_pushinteger(L, STDERR_FILENO);
  lua_rawset(L, -3);

  lua_pushliteral(L, "init");
  lua_pushcfunction(L, log_init);
  lua_rawset(L, -3);

  lua_pushliteral(L, "status");
  lua_pushcfunction(L, log_status);
  lua_rawset(L, -3);

  lua_pushliteral(L, "hex");
  lua_pushcfunction(L, log_hex);
  lua_rawset(L, -3);

  return 1;
}
