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
#include "bl_exec.h"
#include "os.h"
#include "heap.h"
#include "wifi_impl.h"
#include "adc_driver.h"
#include "spi_dev.h"
#include "gpio.h"

#define CLI_PROMPT "> "
#define IS_STRING(s) ((u8_t*)(s) >= (u8_t*)cli_pars_buf && (u8_t*)(s) < (u8_t*)cli_pars_buf + sizeof(cli_pars_buf))


#define TEST_THREAD


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
static bool pipe = FALSE;
static u8_t pipe_io;
static io_rx_cb pipe_cb_func;
static void *pipe_cb_arg;
static u8_t pipe_ends = 0;


static void cli_pipe(u8_t io, void *arg, u16_t available);

// --------------------------------------------
// COMMAND LINE INTERFACE FUNCTIONS DEFINITIONS
// --------------------------------------------


#ifdef CONFIG_I2C
static int f_i2c_scan(int speed);
static int f_i2c_read(int slave, int reg);
#ifdef CONFIG_LSM303
static int f_lsm_open(void);
static int f_lsm_read(int);
#endif
#endif

#ifdef CONFIG_WIFI232
static int f_wifi_reset(int);
static int f_wifi_state(void);
static int f_wifi_scan(void);
static int f_wifi_wip(void);
static int f_wifi_ssid(void);
static int f_wifi_config(char *, char *, char *);
static int f_wifi_open(void);
#endif

#ifdef CONFIG_ADC
static int f_adc_one(int channel);
static int f_adc_two(int ch1, int ch2);
static int f_adc_two_cont(int ch1, int ch2, int freq);
static int f_adc_cont_stop(void);
static int f_adc_get_timer(void);
#endif

#ifdef CONFIG_SPI
static int f_spi_setup(int port, int pin, int speed, int cpol, int cpha, int fbit);
static int f_spi(int cmd, int rx_len);
static int f_spi_pic(void);
static int f_spi_odo(int);
static int f_spi_mot(void);
static int f_spi_squal(void);
#endif

static int f_spam(int io, char *s, int delta, int times);
static int f_pipe(int io);
static int f_assert(void);
static int f_dump(void);
static int f_trace(void);
static int f_reset(void);
#ifdef CONFIG_BOOTLOADER
static int f_reset_boot();
static int f_reset_fw_upgrade();
#endif
static int f_memrd(void *addr, u32_t size, u8_t io);
static int f_dbg(void);
static int f_help(char *s);

#ifdef TEST_THREAD
static int f_test_thread(void);
#endif

static int f_ov7670_pic(void);
static int f_ov7670_send(void);
static int f_ov7670_read(int);
static int f_ov7670_write(int,int);
static int f_ov7670_init(void);

static int f_ear_gain_init(void);
static int f_ear_gain_max(void);
static int f_ear_gain_min(void);

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
    {.name = "i2c_r",     .fn = (func)f_i2c_read,
        .help = "Reads one byte register value from i2c device\n"\
        "i2c_r <slave> <register>\n"
        "ex. i2c_r 0x42 0x1+\n"
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

#ifdef CONFIG_WIFI232
    {.name = "wifi_reset",     .fn = (func)f_wifi_reset,
        .help = "Resets wifi block\n"
    },
    {.name = "wifi_state",     .fn = (func)f_wifi_state,
        .help = "Returns state of wifi block\n"
    },
    {.name = "wifi_scan",     .fn = (func)f_wifi_scan,
        .help = "Wifi scan of surrounding APs\n"
    },
    {.name = "wifi_wip",     .fn = (func)f_wifi_wip,
        .help = "Returns wifi ip settings\n"
    },
    {.name = "wifi_ssid",     .fn = (func)f_wifi_ssid,
        .help = "Returns SSID connected to wifi block\n"
    },
    {.name = "wifi_config",     .fn = (func)f_wifi_config,
        .help = "Configs WIFI232\n"
            "wifi_config <ssid> <enc> <passw>\n"
    },
    {.name = "wifi_open",     .fn = (func)f_wifi_open,
        .help = "Enter wifi config mode and opens pipe\n"
    },
#endif


#ifdef CONFIG_ADC
    {.name = "adc_one",     .fn = (func)f_adc_one,
        .help = "Samples one channel\n" \
            "adc_one <channel> (<num>)\n"
    },
    {.name = "adc_two",     .fn = (func)f_adc_two,
        .help = "Samples two channels simultaneously\n" \
            "adc_two <channel1> <channel2> (<num>)\n"
    },
    {.name = "adc_two_cont",     .fn = (func)f_adc_two_cont,
        .help = "Samples two channels continuously\n" \
            "adc_two_cont <channel1> <channel2> <freq>\n"
    },
    {.name = "adc_stop",     .fn = (func)f_adc_cont_stop,
        .help = "Stops continuous sampling\n"
    },
    {.name = "adc_tim",     .fn = (func)f_adc_get_timer,
        .help = "Returns adc timer value\n"
    },
#endif

#ifdef CONFIG_SPI
    {.name = "spi_setup",     .fn = (func)f_spi_setup,
        .help = "Sets up a spi device against port 0 with given configuration\n"\
        "i2c_scan <port> <pin> <speed> <cpol> <cpha> <fbit>\n"
        "<port> and <pin>: nCS signal, hw specific\n"
        "<speed>: 0=highest, 1=18Mbps, 2=9Mbps, 3=4.5Mbps, 4=2.3MBps, 5=1.1Mbps, 6=0.5Mbps, 7=lowest\n"
        "<cpol>: polarity 0=high, 1=low\n"
        "<cpha>: phase 0=1st edge, 1=2nd edge\n"
        "<fbit>: firstbit 0=msb, 1=lsb\n"
        "ex: spi_setup 1 12 0 0 0 0 - setups a spi device against port2, pin12, highest speed\n"
    },
    {.name = "spi",         .fn = (func)f_spi,
        .help = "TX/RX against spi device\n"
        "spi <txbyte> <rx_len>\n"
        "ex: spi 0x55 4\n"
    },
    {.name = "spi_pic",         .fn = (func)f_spi_pic,
        .help = "ADNS3000 spi pic grab\n"
    },
    {.name = "spi_odo",         .fn = (func)f_spi_odo,
        .help = "ADNS3000 irq motion cfg\n"
    },
    {.name = "spi_mot",         .fn = (func)f_spi_mot,
        .help = "ADNS3000 motion\n"
    },
    {.name = "spi_squal",       .fn = (func)f_spi_squal,
        .help = "ADNS3000 surface qualtiy, until keypress\n"
    },

#endif

    {.name = "spam",     .fn = (func)f_spam,
        .help = "Spams an io port with text\n"\
        "spam <io> <text> <delta_ms> <times>\n"
        "Spams port <io> with <text> every <delta_ms> millisecond, <times> times\n"
    },
    {.name = "pipe",     .fn = (func)f_pipe,
        .help = "Connects CLI to another io\n"\
        "pipe <io>\n"
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


    {.name = "memrd",   .fn = (func)f_memrd,
        .help = "Read out memory\n"\
        "memrd (addr) (size) (io)\n"\
        "ex: memrd 0x20000000 16\n"
    },

    {.name = "dbg",   .fn = (func)f_dbg,
        .help = "Set debug filter and level\n"\
        "dbg (level <dbg|info|warn|fatal>) (enable [x]*) (disable [x]*)\n"\
        "x - <task|heap|comm|cnc|cli|nvs|spi|all>\n"\
        "ex: dbg level info disable all enable cnc comm\n"
    },


    {.name = "help",     .fn = (func)f_help,
        .help = "Prints help\n"\
        "help or help <command>\n"
    },
    {.name = "?",     .fn = (func)f_help,
        .help = "Prints help\n"\
        "help or help <command>\n"
    },

#ifdef TEST_THREAD
    {.name = "test_thread", .fn = (func)f_test_thread,
        .help = "Starts a test thread\n"
    },
#endif

    {.name = "ov_p", .fn = (func)f_ov7670_pic,
        .help = "Takes a snapshot\n"
    },
    {.name = "ov_s", .fn = (func)f_ov7670_send,
        .help = "Sends latest snapshot\n"
    },
    {.name = "ov_r", .fn = (func)f_ov7670_read,
        .help = "Reads ov7670 register\n"
    },
    {.name = "ov_w", .fn = (func)f_ov7670_write,
        .help = "writes ov7670 register\n"
    },
    {.name = "ov_init", .fn = (func)f_ov7670_init,
        .help = "Inits ov7670\n"
    },

    {.name = "ear_init", .fn = (func)f_ear_gain_init,
        .help = "Init ear gain\n"
    },
    {.name = "ear_max", .fn = (func)f_ear_gain_max,
        .help = "Sets ear max gain\n"
    },
    {.name = "ear_min", .fn = (func)f_ear_gain_min,
        .help = "Sets ear min gain\n"
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
  I2C_reset(_I2C_BUS(0));
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

static i2c_dev i2c_d;
static i2c_dev_sequence cli_i2c_seq[2];
static u8_t cli_i2c_tx[8];
static u8_t cli_i2c_rx[8];

static void cli_i2c_d_cb(i2c_dev *d, int res) {
  if (res < 0) {
    print("i2c_res: %i phy:%i\n", res, I2C_phy_err(_I2C_BUS(0)));
    I2C_DEV_close(&i2c_d);
    return;
  }
  print("i2c_cb: %02x\n", cli_i2c_rx[0]);
  I2C_DEV_close(&i2c_d);
}

static int f_i2c_read(int slave, int reg) {
  if (_argc != 2) return -1;
  I2C_DEV_init(&i2c_d, 100000, _I2C_BUS(0), slave);
  s32_t res = I2C_DEV_set_callback(&i2c_d, cli_i2c_d_cb);
  I2C_DEV_open(&i2c_d);
  if (res < 0) {
    print("error:%i\n", res);
    return 0;
  }
  cli_i2c_tx[0] = reg;
  cli_i2c_seq[0] = I2C_SEQ_TX(cli_i2c_tx, 1);
  cli_i2c_seq[1] = I2C_SEQ_RX_STOP(cli_i2c_rx, 1);
  res = I2C_DEV_sequence(&i2c_d, cli_i2c_seq, 2);
  if (res < 0) {
    print("error:%i\n", res);
    return 0;
  }
  return 0;
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


#ifdef CONFIG_WIFI232
static int f_wifi_reset(int hw) {
  if (_argc == 0) hw = TRUE;
  WIFI_IMPL_reset(hw);
  return 0;
}
static int f_wifi_state(void) {
  WIFI_IMPL_state();
  return 0;
}
static int f_wifi_scan(void) {
  int res = WIFI_IMPL_scan();
  if (res != 0) {
    print("err: %i\n", res);
  }
  return 0;
}
static int f_wifi_wip(void) {
  int res = WIFI_IMPL_get_wan();
  if (res != 0) {
    print("err: %i\n", res);
  }
  return 0;
}
static int f_wifi_ssid(void) {
  int res = WIFI_IMPL_get_ssid();
  if (res != 0) {
    print("err: %i\n", res);
  }
  return 0;
}
wifi_config wc;
static int f_wifi_config(char *ssid, char *enc, char *pass) {
  if (_argc != 3) return -1;
  strcpy((char*)&wc.encryption, enc);
  wc.gateway[0] = 192;
  wc.gateway[1] = 168;
  wc.gateway[2] = 0;
  wc.gateway[3] = 1;
  strcpy((char*)&wc.password, pass);
  wc.port=0xcafe;
  wc.protocol = WIFI_COMM_PROTO_UDP;
  wc.server[0] = 192;
  wc.server[1] = 168;
  wc.server[2] = 0;
  wc.server[3] = 102;
  strcpy((char*)&wc.ssid, ssid);
  wc.type = WIFI_TYPE_CLIENT;
  wc.wan.method = WIFI_WAN_DHCP;
  wc.wan.ip[0] = 0;
  wc.wan.ip[1] = 0;
  wc.wan.ip[2] = 0;
  wc.wan.ip[3] = 0;
  wc.wan.gateway[0] = 192;
  wc.wan.gateway[1] = 168;
  wc.wan.gateway[2] = 0;
  wc.wan.gateway[3] = 1;
  wc.wan.netmask[0] = 255;
  wc.wan.netmask[1] = 255;
  wc.wan.netmask[2] = 255;
  wc.wan.netmask[3] = 0;

  int res = WIFI_set_config(&wc);
  if (res != 0) {
    print("err: %i\n", res);
  }
  return 0;
}
static int f_wifi_open(void) {
  WIFI_IMPL_set_idle(TRUE);
  IO_put_buf(IOWIFI, (u8_t*)"+++", 3);
  //SYS_hardsleep_ms(10);
  u32_t spoon_guard = 0x100000;
  while ((UART_rx_available(_UART(WIFI_UART)) == 0) && --spoon_guard);
  if (spoon_guard == 0) {
    print("No answer\n");
    WIFI_IMPL_set_idle(FALSE);
    return 0;
  }
  UART_get_char(_UART(WIFI_UART));
  UART_put_char(_UART(WIFI_UART), 'a');
  UART_tx_flush(_UART(WIFI_UART));
  _argc = 1;
  f_pipe(IOWIFI);
  WIFI_IMPL_set_idle(FALSE);
  return 0;
}

#endif // CONFIG_WIFI232

#ifdef CONFIG_ADC
static time adc_start;

u8_t adc_buf[256*2];
volatile u16_t adc_ix = 0;

static void cli_adc_cb(adc_channel ch1, u32_t val1, adc_channel ch2, u32_t val2) {
  //time diff = SYS_get_tick() - adc_start;
  //print("ADC result: ch%i %08x  ch%i %08x  (dt:%i ticks)\n", ch1, val1, ch2, val2, diff);
  //UART_tx_force_char(_UART(1), val1>>4);
  adc_buf[adc_ix++] = (u8_t)(val1>>4);
  adc_buf[adc_ix++] = (u8_t)(val2>>4);
  adc_ix %= sizeof(adc_buf);
}

static int f_adc_one(int channel) {
  if (_argc != 1) {
    return -1;
  }
  if (channel + 1 >= _ADC_CHANNELS) {
    print("bad channel\n");
    return 0;
  }
  ADC_set_callback(cli_adc_cb);
  adc_start = SYS_get_tick();
  int res = ADC_sample_mono_single(channel+1);
  if (res != ADC_OK) {
    print("err: %i\n", res);
  }

  return 0;
}
static int f_adc_two(int ch1, int ch2) {
  if (_argc != 2) {
    return -1;
  }
  if (ch1 + 1 >= _ADC_CHANNELS) {
    print("bad channel1\n");
    return 0;
  }
  if (ch2 + 1 >= _ADC_CHANNELS) {
    print("bad channel2\n");
    return 0;
  }
  ADC_set_callback(cli_adc_cb);
  adc_start = SYS_get_tick();
  int res = ADC_sample_stereo_single(ch1+1, ch2+1);
  if (res != ADC_OK) {
    print("err: %i\n", res);
  }

  return 0;
}

static int f_adc_two_cont(int ch1, int ch2, int freq) {
  if (_argc != 3) return -1;
  if (ch1 + 1 >= _ADC_CHANNELS) {
    print("bad channel1\n");
    return 0;
  }
  if (ch2 + 1 >= _ADC_CHANNELS) {
    print("bad channel2\n");
    return 0;
  }
  ADC_set_callback(cli_adc_cb);
  int res = ADC_sample_stereo_continuous(ch1+1, ch2+1, freq);
  if (res != ADC_OK) {
    print("err: %i\n", res);
  }
  return 0;
}

static int f_adc_cont_stop(void) {
  int res = ADC_sample_continuous_stop();
  if (res != ADC_OK) {
    print("err: %i\n", res);
  }
  return 0;
}

static int f_adc_get_timer(void) {
  print("TIM3:%i\n", TIM_GetCounter(TIM3));
  return 0;
}

#endif // CONFIG_ADC

#ifdef CONFIG_SPI
static spi_dev spid;
static spi_dev_sequence spi_seq[2];
static u8_t cli_spi_rx_buf[256];
static volatile u16_t cli_spi_rx_len;
static u8_t cli_spi_tx[2];
static volatile u8_t cli_spi_st;
static s8_t cli_spi_dx;
static s8_t cli_spi_dy;
static bool cli_spi_irq = FALSE;
static s32_t cli_spi_x = 0;
static s32_t cli_spi_y = 0;

void cli_spi_dev_cb(spi_dev *dev, int res) {
  if (res != SPI_OK) {
    cli_spi_irq = FALSE;
    print("spi cb err:%i\n", res);
    return;
  }
  if (cli_spi_rx_len == 0 && cli_spi_st == 0) {
    print("spi cb ok\n");
    return;
  }
  if (cli_spi_st == 0) {
    // output register value
    print("spi cb, rx buf:\n");
    printbuf(IOSTD, cli_spi_rx_buf, cli_spi_rx_len);
  } else if (cli_spi_st == 1) {
    // output / read pixel
    bool valid_pix = (cli_spi_rx_buf[0] & 0x80) != 0;
    if (valid_pix) {
      if (cli_spi_rx_len > 0) {
        //print("%02x", (cli_spi_rx_buf[0] & 0x7f));
        int d = (cli_spi_rx_buf[0] & 0x7f);
        if (d < 16)       print(".");
        else if (d < 32)  print(":");
        else if (d < 48)  print("!");
        else if (d < 64) print("/");
        else if (d < 80) print("X");
        else if (d < 96) print("H");
        else if (d < 112) print("M");
        else              print("@");
      }
      if (((cli_spi_rx_len) % 22) == 0) print("\n");
    }

    if (!valid_pix ||  cli_spi_rx_len < 484) {
      cli_spi_tx[0] = 0x0b;
      SPI_DEV_SEQ_TX(spi_seq[0], cli_spi_tx, 1);
      SPI_DEV_SEQ_RX_STOP(spi_seq[1], cli_spi_rx_buf, 1);
      res = SPI_DEV_sequence(&spid, spi_seq, 2);
      if (res != SPI_OK) {
        cli_spi_st = 0;
        print("spi cb st 1 err: %i\n", res);
        return;
      }
    }

    if (valid_pix) {
      cli_spi_rx_len++;
    }
  } else if (cli_spi_st == 2) {
    // read status 0x02
    cli_spi_tx[0] = 0x02;
    SPI_DEV_SEQ_TX(spi_seq[0], cli_spi_tx, 1);
    SPI_DEV_SEQ_RX_STOP(spi_seq[1], cli_spi_rx_buf, 1);
    res = SPI_DEV_sequence(&spid, spi_seq, 2);
    if (res != SPI_OK) {
      cli_spi_irq = FALSE;
      print("spi cb st 2 err: %i\n", res);
      return;
    }
    cli_spi_st = 3;
  } else if (cli_spi_st == 3) {
    if ((cli_spi_rx_buf[0] & 0x80) == 0) {
      cli_spi_irq = FALSE;
      print(" - end\n");
      return;
    }
    // read delta x 0x03
    cli_spi_tx[0] = 0x03;

    SPI_DEV_SEQ_TX(spi_seq[0], cli_spi_tx, 1);
    SPI_DEV_SEQ_RX_STOP(spi_seq[1], cli_spi_rx_buf, 1);
    res = SPI_DEV_sequence(&spid, spi_seq, 2);
    if (res != SPI_OK) {
      cli_spi_irq = FALSE;
      print("spi cb st 3 err: %i\n", res);
      return;
    }
    cli_spi_st = 4;
  } else if (cli_spi_st == 4) {
    // read delta y 0x04
    cli_spi_dx = cli_spi_rx_buf[0];

    cli_spi_tx[0] = 0x04;

    SPI_DEV_SEQ_TX(spi_seq[0], cli_spi_tx, 1);
    SPI_DEV_SEQ_RX_STOP(spi_seq[1], cli_spi_rx_buf, 1);
    res = SPI_DEV_sequence(&spid, spi_seq, 2);
    if (res != SPI_OK) {
      cli_spi_irq = FALSE;
      print("spi cb st 4 err: %i\n", res);
      return;
    }
    cli_spi_st = 5;
  } else if (cli_spi_st == 5) {
    cli_spi_dy = cli_spi_rx_buf[0];
    cli_spi_x += cli_spi_dx;
    cli_spi_y += cli_spi_dy;
    print("\ndx:%i dy:%i [x:%i y:%i]", cli_spi_dx, cli_spi_dy, cli_spi_x, cli_spi_y);
    cli_spi_st = 3;

    cli_spi_tx[0] = 0x02;
    SPI_DEV_SEQ_TX(spi_seq[0], cli_spi_tx, 1);
    SPI_DEV_SEQ_RX_STOP(spi_seq[1], cli_spi_rx_buf, 1);
    res = SPI_DEV_sequence(&spid, spi_seq, 2);
    if (res != SPI_OK) {
      cli_spi_irq = FALSE;
      print("spi cb st 2 err: %i\n", res);
      return;
    }
  }
}

static void cli_spi_io_int_fn(gpio_pin pin) {
  if (cli_spi_irq) return;
  cli_spi_irq = TRUE;
  cli_spi_st = 2;
  cli_spi_dev_cb(NULL, 0);
}

static int f_spi_setup(int port, int pin, int speed, int cpol, int cpha, int fbit) {
  if (_argc < 6) {
    fbit = SPIDEV_CONFIG_FBIT_MSB;
  } else {
    fbit = fbit ? SPIDEV_CONFIG_FBIT_LSB : SPIDEV_CONFIG_FBIT_MSB;
  }
  if (_argc < 5) {
    cpha = SPIDEV_CONFIG_CPHA_1E;
  } else {
    cpha = cpha ? SPIDEV_CONFIG_CPHA_2E : SPIDEV_CONFIG_CPHA_1E;
  }
  if (_argc < 4) {
    cpol = SPIDEV_CONFIG_CPOL_LO;
  } else {
    cpol = cpol ? SPIDEV_CONFIG_CPOL_LO : SPIDEV_CONFIG_CPOL_HI;
  }
  if (_argc < 3) {
    speed = SPIDEV_CONFIG_SPEED_1_1M;
  }
  if (_argc < 2) {
    pin = PIN12;
  }
  if (_argc < 1) {
    port = PORTB;
  }

  print("spi dev config\n");
  print("  nCS:   port %c, pin %i\n", 'A' + port, pin);
  print("  speed: %i\n", speed);
  print("  cpol: %s\n", cpol == SPIDEV_CONFIG_CPOL_LO ? "LO" : "HI");
  print("  cpha: %s\n", cpha == SPIDEV_CONFIG_CPHA_2E ? "2ND" : "1ST");
  print("  fbit: %s\n", fbit == SPIDEV_CONFIG_FBIT_LSB ? "LSB" : "MSB");

  gpio_config(port, pin, CLK_100MHZ, OUT, AF0, PUSHPULL, NOPULL);
  gpio_enable(port, pin);

  SPI_DEV_init(&spid, speed | cpol | cpha | fbit, _SPI_BUS(0), gpio_get_hw_port(port),
      gpio_get_hw_pin(pin), SPI_CONF_IRQ_DRIVEN | SPI_CONF_IRQ_CALLBACK);
  SPI_DEV_set_callback(&spid, cli_spi_dev_cb);
  SPI_DEV_open(&spid);

  return 0;
}

static int f_spi(int cmd, int rx_len) {
  if (_argc == 1) {
    rx_len = 1;
  } else if (_argc == 0 || _argc > 2) {
    return -1;
  }
  if (rx_len > sizeof(cli_spi_rx_buf)) {
    print("rx max size: %i\n", sizeof(cli_spi_rx_buf));
    return 0;
  }
  cli_spi_tx[0] = cmd;
  cli_spi_rx_len = rx_len;
  cli_spi_st = 0;
  SPI_DEV_SEQ_TX(spi_seq[0], cli_spi_tx, 1);
  SPI_DEV_SEQ_RX_STOP(spi_seq[1], cli_spi_rx_buf, rx_len);
  int res = SPI_DEV_sequence(&spid, spi_seq, rx_len > 0 ? 2 : 1);
  if (res != SPI_OK) {
    print("err: %i\n", res);
  }
  return 0;
}

static int f_spi_mot(void) {
  if (SPI_DEV_is_busy(&spid)) return 0;

  cli_spi_st = 2;
  cli_spi_tx[0] = 0x41 | 0x80;
  cli_spi_tx[1] = 0x00;
  SPI_DEV_SEQ_TX_STOP(spi_seq[0], cli_spi_tx, 2);
  int res = SPI_DEV_sequence(&spid, spi_seq, 1);
  if (res != SPI_OK) {
    print("err: %i\n", res);
  }
  return 0;
}

static int f_spi_pic(void) {
  cli_spi_st = 1;
  cli_spi_rx_len = 0;
  cli_spi_tx[0] = 0x0b | 0x80;
  cli_spi_tx[1] = 0;
  SPI_DEV_SEQ_TX_STOP(spi_seq[0], cli_spi_tx, 2);
  int res = SPI_DEV_sequence(&spid, spi_seq, 1);
  if (res != SPI_OK) {
    print("err: %i\n", res);
  }
  return 0;
}

static int f_spi_odo(int enable) {
  if (_argc == 0) enable = TRUE;
  if (enable) {
    print("odo irq enabled\n");
    gpio_config(PORTC, PIN4, CLK_100MHZ, IN, AF0, OPENDRAIN, PULLUP);
    gpio_interrupt_config(PORTC, PIN4, cli_spi_io_int_fn, FLANK_DOWN);
    gpio_interrupt_mask_enable(PORTC, PIN4, TRUE);
  } else {
    print("odo irq disabled\n");
    gpio_interrupt_deconfig(PORTC, PIN4);
  }
  return 0;
}

static int f_spi_squal(void) {
  if (SPI_DEV_is_busy(&spid)) return 0;

  cli_spi_st = 0;

  cli_spi_tx[0] = 0x05;
  cli_spi_rx_len = 1;

  print("send cli data to exit\n");

  while (IO_rx_available(IOSTD) == 0) {
    SPI_DEV_SEQ_TX(spi_seq[0], cli_spi_tx, 1);
    SPI_DEV_SEQ_RX_STOP(spi_seq[1], cli_spi_rx_buf, 1);
    int res = SPI_DEV_sequence(&spid, spi_seq, 2);
    if (res != SPI_OK) {
      print("err: %i\n", res);
      cli_spi_st = 0;
      break;
    } else {
      SYS_hardsleep_ms(300);
    }
  }
  return 0;
}


#endif // CONFIG_SPI

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

static int f_pipe(int io) {
  if (_argc != 1) {
    return -1;
  }
  if (io == IOSTD) {
    print("Cannot pipe STD\n");
    return 0;
  }

  print("\nEntering io pipe %i, type *** to exit\n", pipe_io);

  enter_critical();
  pipe_io = io;
  IO_get_callback(io, &pipe_cb_func, &pipe_cb_arg);
  IO_set_callback(io, cli_pipe, NULL);
  pipe = TRUE;
  exit_critical();

  return 0;
}

static int f_assert(void) {
  ASSERT(FALSE);
  return 0;
}

static int f_dump(void) {
  HEAP_dump();
  print("\n");
#ifdef CONFIG_TASK_QUEUE
  TASK_dump(IOSTD);
  print("\n");
#endif
#ifdef CONFIG_OS
  OS_DBG_dump(IOSTD);
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

static int f_memrd(void *addr, u32_t size, u8_t io) {
  if (_argc < 3) {
    io = IOSTD;
  }
  if (_argc < 2) {
    size = 1;
  }
  if (_argc == 0) {
    return -1;
  }
  bool old_blocking_tx = IO_assure_tx(io, TRUE);
  IO_tx_flush(io);

  printbuf(io, (u8_t*)addr, size);

  IO_tx_flush(io);
  IO_assure_tx(io, old_blocking_tx);

  return 0;
}

#ifdef DBG_OFF
static int f_dbg() {
  print("Debug disabled compile-time\n");
  return 0;
}
#else
const char* DBG_BIT_NAME[] = _DBG_BIT_NAMES;

static void print_debug_setting() {
  print("DBG level: %i\n", SYS_dbg_get_level());
  int d;
  for (d = 0; d < sizeof(DBG_BIT_NAME)/sizeof(const char*); d++) {
    print("DBG mask %s: %s\n", DBG_BIT_NAME[d], SYS_dbg_get_mask() & (1<<d) ? "ON":"OFF");
  }
}


static int f_dbg() {
  enum state {DNONE, DLEVEL, DENABLE, DDISABLE} st = DNONE;
  int a;
  if (_argc == 0) {
    print_debug_setting();
    return 0;
  }
  for (a = 0; a < _argc; a++) {
    u32_t f = 0;
    char *s = (char*)_args[a];
    if (!IS_STRING(s)) {
      return -1;
    }
    if (strcmp("level", s) == 0 || strcmp("l", s) == 0) {
      st = DLEVEL;
    } else if (strcmp("enable", s) == 0 || strcmp("e", s) == 0) {
      st = DENABLE;
    } else if (strcmp("disable", s) == 0 || strcmp("d", s) == 0) {
      st = DDISABLE;
    } else {
      switch (st) {
      case DLEVEL:
        if (strcmp("dbg", s) == 0) {
          SYS_dbg_level(D_DEBUG);
        }
        else if (strcmp("info", s) == 0) {
          SYS_dbg_level(D_INFO);
        }
        else if (strcmp("warn", s) == 0) {
          SYS_dbg_level(D_WARN);
        }
        else if (strcmp("fatal", s) == 0) {
          SYS_dbg_level(D_FATAL);
        } else {
          return -1;
        }
        break;
      case DENABLE:
      case DDISABLE: {
        int d;
        for (d = 0; f == 0 && d < sizeof(DBG_BIT_NAME)/sizeof(const char*); d++) {
          if (strcmp(DBG_BIT_NAME[d], s) == 0) {
            f = (1<<d);
          }
        }
        if (strcmp("all", s) == 0 || strcmp("a", s) == 0) {
          f = D_ANY;
        }
        if (f == 0) {
          return -1;
        }
        if (st == DENABLE) {
          SYS_dbg_mask_enable(f);
        } else {
          SYS_dbg_mask_disable(f);
        }
        break;
      }
      default:
        return -1;
      }
    }
  }
  //CONFIG_store();
  print_debug_setting();
  return 0;
}
#endif

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

#ifdef TEST_THREAD
os_thread t_thread;
static void *tt_func(void *stack) {
  int t = 10;
  while (--t) {
    print("test thread %i\n", t);
    OS_thread_sleep(1000);
  }
  HEAP_xfree(stack);
  return NULL;
}
static int f_test_thread(void) {
  void *stack = HEAP_xmalloc(256);
  OS_thread_create(
      &t_thread,
      OS_THREAD_FLAG_PRIVILEGED,
      tt_func,
      stack,
      stack,
      256,
      "test_thread");
  return 0;
}
#endif

#include "ov7670_test.h"
static int f_ov7670_init(void) {
  ov7670_hw_init();
  ov7670_sw_init();
  return 0;
}
static int f_ov7670_read(int reg) {
  print("ov7670_reg %02x : %02x\n", reg, ov7670_get(reg));
  return 0;
}
static int f_ov7670_write(int r, int v) {
  ov7670_set(r,v);
  print("ov7670_reg %02x : %02x\n", r, ov7670_get(r));
  return 0;
}
static int f_ov7670_pic(void) {
  ov7670_take_snapshot();
  ov7670_read_pic(NULL);
  return 0;
}
static int f_ov7670_send(void) {
  ov7670_read_pic(NULL);
  return 0;
}

static int f_ear_gain_init(void) {
  // CS
  gpio_config(PORTC, PIN0, CLK_25MHZ, OUT, AF0, PUSHPULL, NOPULL);
  // U/nD
  gpio_config(PORTC, PIN1, CLK_25MHZ, OUT, AF0, PUSHPULL, NOPULL);

  gpio_disable(PORTC, PIN0);
  gpio_disable(PORTC, PIN1);

  return 0;
}

static int f_ear_gain_max(void) {
  gpio_enable(PORTC, PIN1);

  gpio_enable(PORTC, PIN0);
  SYS_hardsleep_ms(1);
  int i;
  for (i = 0; i < 32; i++) {
    gpio_enable(PORTC, PIN1);
    SYS_hardsleep_ms(1);
    gpio_disable(PORTC, PIN1);
    SYS_hardsleep_ms(1);
  }
  gpio_disable(PORTC, PIN0);
  return 0;
}
static int f_ear_gain_min(void) {
  gpio_disable(PORTC, PIN1);

  gpio_enable(PORTC, PIN0);
  SYS_hardsleep_ms(1);
  int i;
  for (i = 0; i < 32; i++) {
    gpio_enable(PORTC, PIN1);
    SYS_hardsleep_ms(1);
    gpio_disable(PORTC, PIN1);
    SYS_hardsleep_ms(1);
  }
  gpio_disable(PORTC, PIN0);
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

static void cli_pipe_leave(void) {
  print("\nleaving io pipe %i\n", pipe_io);
  enter_critical();
  IO_set_callback(pipe_io, pipe_cb_func, pipe_cb_arg);
  pipe = FALSE;
  exit_critical();
  cli_buf_ix = 0;
}

static void cli_on_line(u8_t *buf, u16_t len) {
  if (len == 0) return;
  memcpy(cli_pars_buf, cli_buf, len);
  TASK_run(cli_task, len, cli_pars_buf);
}

static void cli_io_cb(u8_t io, void *arg, u16_t len) {
  u16_t i;
  if (pipe) {
    IO_get_buf(io, cli_buf, len);
    for (i = 0; i < len; i++) {
      ioprint(pipe_io, "%c", cli_buf[i]);
      if (cli_buf[i] == '*') {
        pipe_ends++;
        if (pipe_ends >= 3) {
          cli_pipe_leave();
        }
      } else {
        pipe_ends = 0;
      }
    }
  } else {
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
}

static void cli_pipe(u8_t io, void *arg, u16_t len) {
  u16_t i;
  IO_get_buf(io, &cli_buf[cli_buf_ix], len);
  for (i = cli_buf_ix; i < cli_buf_ix + len; i++) {
    print("%c", cli_buf[i]);
  }
}

void CLI_init(void) {
  IO_set_callback(IOSTD, cli_io_cb, NULL);
  cli_task = TASK_create(cli_task_f, TASK_STATIC);
  // TODO remove
  ADC_set_callback(cli_adc_cb);
}
