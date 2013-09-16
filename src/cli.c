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
#include "i2c_driver.h"
#include "lsm303_driver.h"
#ifdef CONFIG_BOOTLOADER
#include "bl_exec.h"
#endif

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


#ifdef CONFIG_I2C
static int f_i2c_scan(int speed);
#ifdef CONFIG_LSM303
static int f_lsm_open(void);
static int f_lsm_read(int);
#endif
#endif

static int f_spam(int io, char *s, int delta, int times);
static int f_assert(void);
static int f_dump(void);
static int f_trace(void);
static int f_reset(void);
#ifdef CONFIG_BOOTLOADER
static int f_reset_boot();
static int f_reset_fw_upgrade();
#endif
static int f_help(char *s);


// ----------------------------
// COMMAND LINE INTERFACE TABLE
// ----------------------------


static cmd c_tbl[] = {
#ifdef CONFIG_I2C
    {.name = "i2c_scan",     .fn = (func)f_i2c_scan,
        .help = "Scans i2c bus for acknowledged addresses\n"\
        "i2c_scan <speed>\n"
        "ex. i2c_scan 400000\n"
    },
#ifdef CONFIG_LSM303
    {.name = "lsm_open",     .fn = (func)f_lsm_open,
        .help = "Opens LSM303 device\n"
    },
    {.name = "lsm_read",     .fn = (func)f_lsm_read,
        .help = "Reads LSM303 device\n"
    },
#endif
#endif
    {.name = "spam",     .fn = (func)f_spam,
        .help = "Spams an io port with text\n"\
        "spam <io> <text> <delta_ms> <times>\n"
        "Spams port <io> with <text> every <delta_ms> millisecond, <times> times\n"
    },
    {.name = "assert",     .fn = (func)f_assert,
        .help = "Asserts the system\n"\
        "Asserts the system and stops everything\n"
    },
    {.name = "dump",     .fn = (func)f_dump,
        .help = "Dumps system state to stdout\n"\
        "Dumps state of system components to stdout\n"
    },
    {.name = "trace",     .fn = (func)f_trace,
        .help = "Dumps system trace stdout\n"\
        "Dumps system trace in chronological order to stdout\n"
    },
    {.name = "reset",     .fn = (func)f_reset,
        .help = "Resets system\n"
    },
#ifdef CONFIG_BOOTLOADER
    {.name = "reset_boot",  .fn = (func)f_reset_boot,
        .help = "Resets system into bootloader\n"
    },
    {.name = "reset_fwupgrade",  .fn = (func)f_reset_fw_upgrade,
        .help = "Resets system into bootloader for upgrade\n"
    },
#endif // CONFIG_BOOTLOADER
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

#ifdef CONFIG_I2C
static int cli_i2c_scan_addr;
static void i2c_scan_report_task(u32_t ures, void *vbus) {
  i2c_bus *bus = (i2c_bus *)vbus;
  s32_t res = (s32_t)ures;
  if (cli_i2c_scan_addr < 0xff) {
    if (cli_i2c_scan_addr == 0) {
      print("     0 2 4 6 8 a c e\n"
            "    ----------------");
    }
    if ((cli_i2c_scan_addr & 0x0f) == 0) {
      print("\n %1x | ", (cli_i2c_scan_addr>>4));
    }
    switch(res) {
    case I2C_OK:
      print("A "); break;
    case I2C_ERR_BUS_BUSY:
      print("_ "); break;
    case I2C_ERR_PHY:
      if (I2C_phy_err(bus) & (1<<I2C_ERR_PHY_ACK_FAIL)) {
        print(". ");
      } else {
        print("X ");
      }
        break;
    case I2C_ERR_UNKNOWN_STATE:
      print("? "); break;
    default:
      print("NA"); break;
    }
    cli_i2c_scan_addr += 2;
    I2C_query(bus, cli_i2c_scan_addr);
  } else {
    print("\n   END\n");
    I2C_close(bus);
  }
}

static void cli_i2c_scan_cb(i2c_bus *bus, int res) {
  task *report_scan_task = TASK_create(i2c_scan_report_task, 0);
  TASK_run(report_scan_task, res, bus);
}

static int f_i2c_scan(int speed) {
  if (_argc != 1) return -1;
  print("scanning i2c bus at %i Hz\n", speed);
  s32_t res = I2C_config(_I2C_BUS(0), speed);
  if (res < 0) {
    print("error:%i\n", res);
    return 0;
  }
  res = I2C_set_callback(_I2C_BUS(0), cli_i2c_scan_cb);
  if (res < 0) {
    print("error:%i\n", res);
    return 0;
  }
  cli_i2c_scan_addr = 0;
  I2C_query(_I2C_BUS(0), cli_i2c_scan_addr);
  return 1;
}

#ifdef CONFIG_LSM303
static lsm303_dev lsm_dev;
static int lsm_state = 0;
static void cli_lsm_cb(lsm303_dev *dev, int res) {
  print("lsm_res: %i\n", res);
  if (lsm_state == 0) {
    print("lsm configured\n");
  } else if (lsm_state == 1) {
    s16_t *acc = lsm_get_acc_reading(&lsm_dev);
    s16_t *mag = lsm_get_mag_reading(&lsm_dev);
    print("acc:%04x %04x %04x   mag:%04x %04x %04x\n", acc[0], acc[1], acc[2], mag[0], mag[1], mag[2]);
    print("heading:%04x / %i\n", lsm_get_heading(&lsm_dev), (lsm_get_heading(&lsm_dev)*360)>>16);
  }
}

static int f_lsm_open(void) {
  lsm_state = 0;
  lsm_open(&lsm_dev, _I2C_BUS(0), FALSE, cli_lsm_cb);
  lsm_config_default(&lsm_dev);
  return 0;
}

static int f_lsm_read(int count) {
  if (_argc == 0) {
    count = 0;
  }
  do {
    lsm_state = 1;
    lsm_read_both(&lsm_dev);
    if (count > 0) {
      SYS_hardsleep_ms(100);
    }
  } while (count--);
  return 0;
}
#endif // CONFIG_LSM303


#endif // CONFIG_I2C

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

static int f_assert(void) {
  ASSERT(FALSE);
  return 0;
}

static int f_dump(void) {
#ifdef CONFIG_TASK_QUEUE
  TASK_dump(IOSTD);
  print("\n");
#endif
  return 0;
}

static int f_trace(void) {
#ifdef DBG_TRACE_MON
  SYS_dump_trace(IOSTD);
#else
  print("Trace disabled, DBG_TRACE_MON not defined\n");
#endif
  return 0;
}

#ifdef CONFIG_BOOTLOADER
static int f_reset_boot() {
  SYS_reboot(REBOOT_EXEC_BOOTLOADER);
  return 0;
}

static int f_reset_fw_upgrade() {
  bootloader_update_fw();
  SYS_reboot(REBOOT_EXEC_BOOTLOADER);
  return 0;
}
#endif

static int f_reset(void) {
  arch_reset();
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
  bool prompt = TRUE;
  if (fn) {
    _argc--;
    //DBG(D_CLI, D_DEBUG, "CONS calling [%p] with %i args\n", fn, _argc);
    int res = (int)_variadic_call(fn, _argc, _args);
    if (res == -1) {
      print("%s", c_tbl[ix].help);
    } else if (res == 0) {
      print("OK\n");
    } else {
      prompt = FALSE;
    }
  } else {
    print("unknown command - try help\n");
  }
  if (prompt) print(CLI_PROMPT);


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
