/*
 * cli.c
 *
 *  Created on: Sep 11, 2013
 *      Author: petera
 */

#include "cli.h"
#include "system.h"
#include "io.h"
#include "taskq.h"
#include "miniutils.h"

#define CLI_PROMPT "> "
#define IS_STRING(s) ((u8_t*)(s) >= (u8_t*)cli_pars_buf && (u8_t*)(s) < (u8_t*)cli_pars_buf + sizeof(cli_pars_buf))

typedef int(*func)(int a, ...);

typedef struct cmd_s {
  const char* name;
  const func fn;
  const char* help;
} cmd;

static u8_t cli_buf[CLI_BUF_LEN];
static u8_t cli_pars_buf[CLI_BUF_LEN];
static u16_t cli_buf_ix = 0;
static task *cli_task;
static u32_t _argc;
static void *_args[16];


// --------------------------------------------
// COMMAND LINE INTERFACE FUNCTIONS DEFINITIONS
// --------------------------------------------


static int f_spam(int io, char *s, int delta, int times);
static int f_help(char *s);


// ----------------------------
// COMMAND LINE INTERFACE TABLE
// ----------------------------


static cmd c_tbl[] = {
    {.name = "spam",     .fn = (func)f_spam,
        .help = "Spams an io port with text\n"\
        "spam <io> <text> <delta_ms> <times>\n"
        "Spams port <io> with <text> every <delta_ms> millisecond, <times> times\n"
    },
    {.name = "help",     .fn = (func)f_help,
        .help = "Prints help\n"\
        "help or help <command>\n"
    },
    {.name = "?",     .fn = (func)f_help,
        .help = "Prints help\n"\
        "help or help <command>\n"
    },

    // menu end marker
    {.name = NULL,    .fn = (func)0,        .help = NULL},
};


// ------------------------------------------------
// COMMAND LINE INTERFACE FUNCTIONS IMPLEMENTATIONS
// ------------------------------------------------

static int f_spam(int io, char *s, int delta, int times) {
  if (_argc < 4 || _argc > 4 || !IS_STRING(s)) {
    return -1;
  }

  while (times--) {
    ioprint(io, s);
    SYS_hardsleep_ms(delta);
  }

  return 0;
}

static int f_help(char *s) {
  if (IS_STRING(s)) {
    int i = 0;
    while (c_tbl[i].name != NULL) {
      if (strcmp(s, c_tbl[i].name) == 0) {
        print("%s\t%s", c_tbl[i].name, c_tbl[i].help);
        return 0;
      }
      i++;
    }
    print("%s\tno such command\n",s);
  } else {
    print("\n"APP_NAME" command line interface\n\n");
    int i = 0;
    while (c_tbl[i].name != NULL) {
      int len = strpbrk(c_tbl[i].help, "\n") - c_tbl[i].help;
      char tmp[64];
      strncpy(tmp, c_tbl[i].help, len+1);
      tmp[len+1] = 0;
      char fill[24];
      int fill_len = sizeof(fill)-strlen(c_tbl[i].name);
      memset(fill, ' ', sizeof(fill));
      fill[fill_len] = 0;
      print("  %s%s%s", c_tbl[i].name, fill, tmp);
      i++;
    }
  }
  return 0;
}


// -------------------------------
// command line interface handlers
// -------------------------------

static void cli_task_f(u32_t rlen, void *buf_p) {
  char *in = (char *)buf_p;

  cursor cursor;
  strarg_init(&cursor, (char*)in, rlen);
  strarg arg;
  _argc = 0;
  func fn = NULL;
  int ix = 0;

  // parse command and args
  while (strarg_next(&cursor, &arg)) {
    if (arg.type == INT) {
      //DBG(D_CLI, D_DEBUG, "CONS arg %i:\tlen:%i\tint:%i\n",arg_c, arg.len, arg.val);
    } else if (arg.type == STR) {
      //DBG(D_CLI, D_DEBUG, "CONS arg %i:\tlen:%i\tstr:\"%s\"\n", arg_c, arg.len, arg.str);
    }
    if (_argc == 0) {
      // first argument, look for command function
      if (arg.type != STR) {
        break;
      } else {
        while (c_tbl[ix].name != NULL) {
          if (strcmp(arg.str, c_tbl[ix].name) == 0) {
            fn = c_tbl[ix].fn;
            break;
          }
          ix++;
        }
        if (fn == NULL) {
          break;
        }
      }
    } else {
      // succeeding arguments¸ store them in global vector
      if (_argc-1 >= 16) {
        DBG(D_CLI, D_WARN, "CONS too many args\n");
        fn = NULL;
        break;
      }
      _args[_argc-1] = (void*)arg.val;
    }
    _argc++;
  }

  // execute command
  if (fn) {
    _argc--;
    //DBG(D_CLI, D_DEBUG, "CONS calling [%p] with %i args\n", fn, _argc);
    int res = (int)_variadic_call(fn, _argc, _args);
    if (res == -1) {
      print("%s", c_tbl[ix].help);
    } else {
      print("OK\n");
    }
  } else {
    print("unknown command - try help\n");
  }
  print(CLI_PROMPT);


}

static void cli_on_line(u8_t *buf, u16_t len) {
  if (len == 0) return;
  memcpy(cli_pars_buf, cli_buf, len);
  TASK_run(cli_task, len, cli_pars_buf);
}

static void cli_io_cb(u8_t io, void *arg, u16_t len) {
  u16_t i;
  IO_get_buf(io, &cli_buf[cli_buf_ix], len);
  for (i = cli_buf_ix; i < cli_buf_ix + len; i++) {
    if (cli_buf[i] == '\n') {
      cli_buf[i] = 0;
      cli_on_line(&cli_buf[0], i);
      cli_buf_ix = 0;
      return;
    }
  }
  cli_buf_ix = i;
}

void CLI_init(void) {
  IO_set_callback(IOSTD, cli_io_cb, NULL);
  cli_task = TASK_create(cli_task_f, TASK_STATIC);
}
