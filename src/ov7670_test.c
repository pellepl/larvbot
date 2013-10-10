/*
 * ov7670_test.c
 *
 *  Created on: Oct 4, 2013
 *      Author: petera
 */

#include "system.h"
#include "gpio.h"
#include "miniutils.h"
#include "ov7670_test.h"

//#define OV_BITBANG

// tested alternative
// read out fifo from FSMC controller
// drive DATA0..7 to FSMC D0..7
//       RRST     to FSMC A18ish
//       OE       to gpio (or CS? wont work..)
//       RCLK     to NOE - must be hw pulled up?/down? thru ?k resistor
//                         only works when putting finger on RCLK signal
//                         will extmem work if pulling NOE?
//                         pulling via software settings wont work.
//
// might need an OR block to RCLK = FSMC NOE | OE (not tested, pull might not be necessary)
//
// enable a read from 0xnnn1fffd to 0xnnn20000 + picsize
// where first read 1fffd - 1ffff will do RRST and the rest read data

#define VSYNC_PORT    PORTB
#define VSYNC_PIN     PIN9
#define WE_PORT       PORTC
#define WE_PIN        PIN10
#define WRST_PORT     PORTC
#define WRST_PIN      PIN8
#define OE_PORT       PORTC
#define OE_PIN        PIN13
#ifdef OV_BITBANG
#define RRST_PORT     PORTC
#define RRST_PIN      PIN12
#define RCLK_PORT     PORTC
#define RCLK_PIN      PIN11
#define DATA_PORT     PORTC
#define DATA_0        PIN0
#define DATA_1        PIN1
#define DATA_2        PIN2
#define DATA_3        PIN3
#define DATA_4        PIN4
#define DATA_5        PIN5
#define DATA_6        PIN6
#define DATA_7        PIN7
#else
#define OV_BASE       0x6c000000
#define OV_RST        (volatile u8_t *)(OV_BASE+0x1fffd)
#define OV_READ       (volatile u8_t *)(OV_BASE+0x20000)
#endif

static volatile bool pic_begin = FALSE;
static volatile bool pic_end = FALSE;
static volatile bool got_pic = FALSE;

void ov7670_take_snapshot(void) {
  got_pic = FALSE;
  pic_begin = TRUE;
  gpio_interrupt_mask_enable(VSYNC_PORT, VSYNC_PIN, TRUE);
  print("ov7670 fetch pic\n");
  while (!got_pic) {
  }
  got_pic = FALSE;
  print("ov7670 got pic\n");
}

void ov7670_read_pic(u16_t *raster_buf) {
#ifdef OV_BITBANG
  // reset read pointer
  gpio_disable(RRST_PORT, RRST_PIN);
  gpio_disable(RCLK_PORT, RCLK_PIN);
  gpio_enable(RCLK_PORT, RCLK_PIN);
  gpio_disable(RCLK_PORT, RCLK_PIN);
  gpio_enable(RCLK_PORT, RCLK_PIN);
  gpio_disable(RCLK_PORT, RCLK_PIN);
  gpio_enable(RCLK_PORT, RCLK_PIN);
  gpio_enable(RRST_PORT, RRST_PIN);

  // enable output
  gpio_disable(OE_PORT, OE_PIN);

  int x,y;
  // clock out data
  u32_t pinmask =
      gpio_get_hw_pin(DATA_0) |
      gpio_get_hw_pin(DATA_1) |
      gpio_get_hw_pin(DATA_2) |
      gpio_get_hw_pin(DATA_3) |
      gpio_get_hw_pin(DATA_4) |
      gpio_get_hw_pin(DATA_5) |
      gpio_get_hw_pin(DATA_6) |
      gpio_get_hw_pin(DATA_7);
  hw_io_port hw_port = (hw_io_port)gpio_get_hw_port(DATA_PORT);
  UART_assure_tx(_UART(1), TRUE);
  UART_put_buf(_UART(1), (u8_t *)"SFRM", 4);
  for (y = 0; y < 240; y++) {
    UART_put_buf(_UART(1), (u8_t *)"SLIN", 4);
    for (x = 0; x < 320; x++) {
      gpio_disable(RCLK_PORT, RCLK_PIN);
      u8_t data_h = GPIO_read(hw_port, pinmask);
      data_h = GPIO_ReadInputData(GPIOC);
      gpio_enable(RCLK_PORT, RCLK_PIN);
      gpio_disable(RCLK_PORT, RCLK_PIN);
      u8_t data_l = GPIO_read(hw_port, pinmask);
      data_l = GPIO_ReadInputData(GPIOC);
      gpio_enable(RCLK_PORT, RCLK_PIN);
      u16_t data = (data_h << 8) | (data_l);
      //*raster_buf++ = data;
#if 0
      if ((y & 0x3) == 0 && (x == 0)) print("\n");
      if ((y & 0x3) == 0 && (x & 0x3) == 0) {

        //print("%s", data < 128 ? "." : "@");
        u8 d = (((data & 0b1111100000000000) >> (11-3)) +
                ((data & 0b0000011111100000) >> (5-2)) +
                ((data & 0b0000000000011111) << 3)) / 3;
        if (d < 32)       print(".");
        else if (d < 64)  print(":");
        else if (d < 96)  print("!");
        else if (d < 128) print("/");
        else if (d < 160) print("X");
        else if (d < 196) print("H");
        else if (d < 228) print("M");
        else              print("@");
      }
#endif
      UART_put_char(_UART(1), data_h);
      UART_put_char(_UART(1), data_l);
    }
    UART_put_buf(_UART(1), (u8_t *)"ELIN", 4);
  }
  UART_put_buf(_UART(1), (u8_t *)"EFRM", 4);

  // disable output
  gpio_enable(OE_PORT, OE_PIN);
  UART_tx_flush(_UART(1));
#else
  int x,y;

  // enable output
  gpio_disable(OE_PORT, OE_PIN);
  // reset read pointer
  (void)*(OV_RST);
  (void)*(OV_RST);
  (void)*(OV_RST);
  UART_assure_tx(_UART(1), TRUE);
  UART_put_buf(_UART(1), (u8_t *)"SFRM", 4);
  for (y = 0; y < 240; y++) {
    UART_put_buf(_UART(1), (u8_t *)"SLIN", 4);
    for (x = 0; x < 320; x++) {
      u8_t data_h = *(OV_READ);
      u8_t data_l = *(OV_READ);
#if 0
      u16_t data = (data_h << 8) | (data_l);
      //*raster_buf++ = data;
      if ((y & 0x3) == 0 && (x == 0)) print("\n");
      if ((y & 0x3) == 0 && (x & 0x3) == 0) {

        u8 d = (((data & 0b1111100000000000) >> (11-3)) +
                ((data & 0b0000011111100000) >> (5-2)) +
                ((data & 0b0000000000011111) << 3)) / 3;
        if (d < 32)       print(".");
        else if (d < 64)  print(":");
        else if (d < 96)  print("!");
        else if (d < 128) print("/");
        else if (d < 160) print("X");
        else if (d < 196) print("H");
        else if (d < 228) print("M");
        else              print("@");
      }
#endif
      UART_put_char(_UART(1), data_h);
      UART_put_char(_UART(1), data_l);
    }
    UART_put_buf(_UART(1), (u8_t *)"ELIN", 4);
  }
  UART_put_buf(_UART(1), (u8_t *)"EFRM", 4);

  // disable output
  gpio_enable(OE_PORT, OE_PIN);
  UART_tx_flush(_UART(1));
#endif
}

static void ov7670_vsync_int_fn(gpio_pin p) {
  static time tick_start;
  if (pic_begin) {
    tick_start = SYS_get_tick();
    // disable reset write address
    gpio_enable(WRST_PORT, WRST_PIN);
    // enable writing
    gpio_enable(WE_PORT, WE_PIN);
    pic_begin = FALSE;
    pic_end = TRUE;
    return;
  }
  if (pic_end) {
    time tick_end = SYS_get_tick();
    // disable writing
    gpio_disable(WE_PORT, WE_PIN);
    gpio_interrupt_mask_disable(VSYNC_PORT, VSYNC_PIN);
    pic_end = FALSE;
    got_pic = TRUE;
    // enable reset write address
    gpio_disable(WRST_PORT, WRST_PIN);
    print("vsync frame ticks:%i\n", tick_end-tick_start);
  }
}

void ov7670_hw_init(void) {
  I2C_InitTypeDef I2C_InitStructure;

  // AF4 = GPIO_AF_I2C2
  // scl
  gpio_config(PORTB, PIN10, CLK_50MHZ, AF, AF4, OPENDRAIN, PULLUP);
  // sda
  gpio_config(PORTB, PIN11, CLK_50MHZ, AF, AF4, OPENDRAIN, PULLUP);

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);

  I2C_InitStructure.I2C_ClockSpeed = 100000;
  I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
  I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
  I2C_InitStructure.I2C_OwnAddress1 = 0;
  I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
  I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;

  I2C_Cmd(I2C2, ENABLE);
  I2C_Init(I2C2, &I2C_InitStructure);

  gpio_config(VSYNC_PORT, VSYNC_PIN, CLK_100MHZ, IN, AF0, OPENDRAIN, PULLUP);
  gpio_interrupt_config(VSYNC_PORT, VSYNC_PIN, ov7670_vsync_int_fn, FLANK_UP);

  gpio_config(WE_PORT, WE_PIN, CLK_25MHZ, OUT, AF0, PUSHPULL, PULLUP);
  gpio_disable(WE_PORT, WE_PIN);
  gpio_config(WRST_PORT, WRST_PIN, CLK_25MHZ, OUT, AF0, PUSHPULL, PULLUP);
  gpio_disable(WRST_PORT, WRST_PIN);
  gpio_config(OE_PORT, OE_PIN, CLK_25MHZ, OUT, AF0, PUSHPULL, PULLUP);
  gpio_enable(OE_PORT, OE_PIN);

#ifdef OV_BITBANG
  gpio_config(RRST_PORT, RRST_PIN, CLK_25MHZ, OUT, AF0, PUSHPULL, PULLUP);
  gpio_enable(RRST_PORT, RRST_PIN);
  gpio_config(RCLK_PORT, RCLK_PIN, CLK_2MHZ, OUT, AF0, PUSHPULL, PULLUP);
  gpio_enable(RCLK_PORT, RCLK_PIN);
  gpio_config(DATA_PORT, DATA_0, CLK_50MHZ, IN, AF0, OPENDRAIN, NOPULL);
  gpio_config(DATA_PORT, DATA_1, CLK_50MHZ, IN, AF0, OPENDRAIN, NOPULL);
  gpio_config(DATA_PORT, DATA_2, CLK_50MHZ, IN, AF0, OPENDRAIN, NOPULL);
  gpio_config(DATA_PORT, DATA_3, CLK_50MHZ, IN, AF0, OPENDRAIN, NOPULL);
  gpio_config(DATA_PORT, DATA_4, CLK_50MHZ, IN, AF0, OPENDRAIN, NOPULL);
  gpio_config(DATA_PORT, DATA_5, CLK_50MHZ, IN, AF0, OPENDRAIN, NOPULL);
  gpio_config(DATA_PORT, DATA_6, CLK_50MHZ, IN, AF0, OPENDRAIN, NOPULL);
  gpio_config(DATA_PORT, DATA_7, CLK_50MHZ, IN, AF0, OPENDRAIN, NOPULL);
#else
  FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStructure;
  FSMC_NORSRAMTimingInitTypeDef  p;
  p.FSMC_AddressSetupTime = 0x0;      // 0x0..0xf
  p.FSMC_AddressHoldTime = 0x0;       // 0x0..0xf
  p.FSMC_DataSetupTime = 0x03;        // 0x00..0xff
  p.FSMC_BusTurnAroundDuration = 0x0; // 0x0..0xf

  p.FSMC_CLKDivision = 0;             // not used in sram
  p.FSMC_DataLatency = 0;             // not used in sram
  p.FSMC_AccessMode = FSMC_AccessMode_A;

  FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM4;
  FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
  FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_SRAM;
  FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_8b;
  FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
  FSMC_NORSRAMInitStructure.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable;
  FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
  FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
  FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
  FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
  FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
  FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
  FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
  FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &p;
  FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &p;

  FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);

  // Enable FSMC Bank1_SRAM4 Bank
  FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM4, ENABLE);

#endif
}

static void ov7670_i2c_start(I2C_TypeDef* I2Cx, u8_t address, u8_t direction) {
  while (I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY ))
    ;
  I2C_GenerateSTART(I2Cx, ENABLE);
  while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT ))
    ;
  I2C_Send7bitAddress(I2Cx, address, direction);
  if (direction == I2C_Direction_Transmitter ) {
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED ))
      ;
  } else if (direction == I2C_Direction_Receiver ) {
    while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED ))
      ;
  }
}

static void ov7670_i2c_write(I2C_TypeDef* I2Cx, u8_t data) {
  I2C_SendData(I2Cx, data);
  while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED ))
    ;
}

#if 0
static u8_t ov7670_i2c_read_ack(I2C_TypeDef* I2Cx) {
  u8_t data;
  I2C_AcknowledgeConfig(I2Cx, ENABLE);
  while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED));
  data = I2C_ReceiveData(I2Cx);
  return data;
}
#endif

static u8_t ov7670_i2c_read_nack(I2C_TypeDef* I2Cx) {
  u8_t data;
  I2C_AcknowledgeConfig(I2Cx, DISABLE);
  while (!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED ))
    ;
  data = I2C_ReceiveData(I2Cx);
  return data;
}

static void ov7670_i2c_stop(I2C_TypeDef* I2Cx) {
  I2C_GenerateSTOP(I2Cx, ENABLE);
}

void delayx(unsigned int ms) {
  //4694 = 1 ms
  ms *= 128;
  while (ms > 1) {
    ms--;
    asm("nop");
  }
}

u8_t ov7670_get(u8_t reg) {
  u8_t data = 0;
  ov7670_i2c_start(I2C2, 0x42, I2C_Direction_Transmitter );
  ov7670_i2c_write(I2C2, reg);
  ov7670_i2c_stop(I2C2 );
  ov7670_i2c_start(I2C2, 0x43, I2C_Direction_Receiver );
  data = ov7670_i2c_read_nack(I2C2 );
  ov7670_i2c_stop(I2C2 );
  delayx(1000);
  return data;
}

void ov7670_set(u8_t reg, u8_t data) {
  ov7670_i2c_start(I2C2, 0x42, I2C_Direction_Transmitter );
  ov7670_i2c_write(I2C2, reg);
  ov7670_i2c_write(I2C2, data);
  ov7670_i2c_stop(I2C2 );
  delayx(1000);
}

int ov7670_sw_init() {
  const char regs[][2]=
  {
  //
  // The really important stuff.
  {0x12, 0x14}, //COM7 DVE: RGB mode, color bar disabled, QVGA (= 320x240 pixels) DVE: was 0x14
  //{0x12, 0x16}, //COM7 DVE: RGB mode, color bar ENABLED, QVGA (= 320x240 pixels) DVE: was 0x14
  //{0x12, 0x0c}, //COM7 DVE: RGB mode, color bar disabled, QCIF (= 176x144 pixels)
  {0x8c, 0x00},//2 / RGB444: 0b [7-2] Reserved, [1] RGB444 [0] XR GB(0) , RG BX(1)
  {0x04, 0x00},//3 / COM1 : CCIR656 Format Disable
  {0x40, 0x10}, //COM15 DVE: RGB565 mode
  {0x14, 0x18},//5 / COM9 : 4x gain ceiling
  {0x11, /*0x01*/0x6},//6 / CLKRC : (was 3f) Internal Clock, [000000] to [111111] , 16000000 / ( [111111]+ 1 ) = 250000 DVE: deze deelt mijn MCO out van 36MHz [b0..5 + 1]
  {0x6b, 0x4a},//72/ DBLV : PPL Control, PLL clk x 4, together with CLKRC it defines the frame rate
  //
  // Hardware Window Settings
  //
  {0x0c, 0x00},//13/ COM3 : DVE: this one can enable windowing, scaling, digital cropping and since it was set to 0x00 the settings after this line were NOT effective! (
  {0x32, 0xb6},//7 / HREF : DVE; this one MUST be 0xb6 or otherwise my images get all scrambled
  {0x17, 0x13},//8 / HSTART: HREF Column start high 8-bit DVE: was 0x13
  {0x18, 0x01},//9 / HSTOP : HREF Column end high 8-bit  DVE: was 0x01
  {0x19, 0x02},//10/ VSTRT : VSYNC Start high 8-bit DVE: was 0x02
  {0x1a, 0x7a},//11/ VSTOP : VSYNC End high 8-bit, DVE: was 0x7a
  {0x03, 0x0a},//12/ VREF : Vertical Frame Control, DVE: was 0x0a
  {0x3e, 0x00},//14/ COM14 : Pixel Clock Devider DVE: was 0x00
  //
  // Mystery Scaling Numbers
  //
  {0x70, 0x3a},//15/ SCALING XSC : Horizontal Scale Factor  DVE: was 0x3a
  {0x71, 0x35},//16/ SCALING YSC : Vertical Scale Factor DVE: was 0x35
  {0x72, 0x11},//17/ SCALING DCW : DCW Control   DVE: was 0x11
  {0x73, 0xf0},//18/ SCALING PCLK: Scaling Pixel Clock Devider  DVE: was 0xf0
  {0xa2, 0x02},//19/ SCALING PCLK: Scaling Pixel Delay
  {0x15, 0x00},//20/ COM10 : VSYNC , HREF , PCLK Settings
  //
  // Gamma Curve Values
  //
  {0x7a, 0x20},//21/ SLOP : Gamma Curve Highest Segment Slope
  {0x7b, 0x10},//22/ GAM1 : Gamme Curve 1st Segment
  {0x7c, 0x1e},//23/ GAM2 : Gamme Curve 2st Segment
  {0x7d, 0x35},//24/ GAM3 : Gamme Curve 3st Segment
  {0x7e, 0x5a},//25/ GAM4 : Gamme Curve 4st Segment
  {0x7f, 0x69},//26/ GAM5 : Gamme Curve 5st Segment
  {0x80, 0x76},//27/ GAM6 : Gamme Curve 6st Segment
  {0x81, 0x80},//28/ GAM7 : Gamme Curve 7st Segment
  {0x82, 0x88},//29/ GAM8 : Gamme Curve 8st Segment
  {0x83, 0x8f},//30/ GAM9 : Gamme Curve 9st Segment
  {0x84, 0x96},//31/ GAM10: Gamme Curve 10st Segment
  {0x85, 0xa3},//32/ GAM11: Gamme Curve 11st Segment
  {0x86, 0xaf},//33/ GAM12: Gamme Curve 12st Segment
  {0x87, 0xc4},//34/ GAM13: Gamme Curve 13st Segment
  {0x88, 0xd7},//35/ GAM14: Gamme Curve 14st Segment
  {0x89, 0xe8},//36/ GAM15: Gamme Curve 15st Segment
  //
  // Automatic Gain Control and AEC Parameters
  //
  {0x13, 0x00},//37/ COM8 : Fast AGC/AEC Algorithm
  {0x00, 0x00},//38/ GAIN
  {0x10, 0x00},//39/ AECH
  {0x0d, 0x00},//40/ COM4 :
  {0x14, 0x18},//41/ COM9 : Automatic Gain Ceiling : 8x
  {0xa5, 0x05},//42/ BD50MAX: 50 Hz Banding Step Limit
  {0xab, 0x07},//43/ BD60MAX: 60 Hz Banding Step Limit
  {0x24, 0x95},//44/ AGC - Stable Operating Region Upper Limit
  {0x25, 0x33},//45/ AGC - Stable Operating Region Lower Limit
  {0x26, 0xe3},//46/ AGC - Fast Mode Operating Region
  {0x9f, 0x78},//47/ HAECC1 : Histogram based AEC Control 1
  {0xa0, 0x68},//48/ HAECC2 : Histogram based AEC Control 2
  {0xa1, 0x03},//49/ Reserved
  {0xa6, 0xd8},//50/ HAECC3 : Histogram based AEC Control 3
  {0xa7, 0xd8},//51/ HAECC4 : Histogram based AEC Control 4
  {0xa8, 0xf0},//52/ HAECC5 : Histogram based AEC Control 5
  {0xa9, 0x90},//53/ HAECC6 : Histogram based AEC Control 6
  {0xaa, 0x94},//54/ HAECC7 : AEC Algorithm Selection
  {0x13, 0xe5},//55/ COM8 : Fast AGC Algorithm, Unlimited Step Size , Banding Filter ON, AGC and AEC enable.
  //
  // Reserved Values without function specification
  //
  {0x0e, 0x61},//56/ COM5 : Reserved
  {0x0f, 0x4b},//57/ COM6 : Reserved
  {0x16, 0x02},//58/ Reserved
  {0x1e, 0x2f},//59/ MVFP : Mirror/Vflip disabled ( 0x37 enabled ) //DVE: was 0x07 is now 0x2f so mirror ENABLED. Was required for my TFT to show pictures without mirroring (probably my TFT setting is also mirrored)
  {0x21, 0x02},//60/ Reserved
  {0x22, 0x91},//61/ Reserved
  {0x29, 0x07},//62/ Reserved
  {0x33, 0x0b},//63/ Reserved
  {0x35, 0x0b},//64/ Reserved
  {0x37, 0x1d},//65/ Reserved
  {0x38, 0x71},//66/ Reserved
  {0x39, 0x2a},//67/ Reserved
  {0x3c, 0x78},//68/ COM12 : Reserved
  {0x4d, 0x40},//69/ Reserved
  {0x4e, 0x20},//70/ Reserved
  {0x69, 0x00},//71/ GFIX : Fix Gain for RGB Values
  //{0x6b, 0x80},//DEBUG DBLV: aantal frames per seconde dmv PCLK hoger of lager, staat nu op INPUT CLK x6 en INT regulator enabled
  {0x74, 0x10},//73/ Reserved
  {0x8d, 0x4f},//74/ Reserved
  {0x8e, 0x00},//75/ Reserved
  {0x8f, 0x00},//76/ Reserved
  {0x90, 0x00},//77/ Reserved
  {0x91, 0x00},//78/ Reserved
  {0x92, 0x00},//79/ Reserved
  {0x96, 0x00},//80/ Reserved
  {0x9a, 0x00},//81/ Reserved
  {0xb0, 0x84},//82/ Reserved
  {0xb1, 0x0c},//83/ Reserved
  {0xb2, 0x0e},//84/ Reserved
  {0xb3, 0x82},//85/ Reserved
  {0xb8, 0x0a},//86/ Reserved
  //
  // Reserved Values without function specification
  //
  {0x43, 0x0a},//87/ Reserved
  {0x44, 0xf0},//88/ Reserved
  {0x45, 0x34},//89/ Reserved
  {0x46, 0x58},//90/ Reserved
  {0x47, 0x28},//91/ Reserved
  {0x48, 0x3a},//92/ Reserved
  {0x59, 0x88},//93/ Reserved
  {0x5a, 0x88},//94/ Reserved
  {0x5b, 0x44},//95/ Reserved
  {0x5c, 0x67},//96/ Reserved
  {0x5d, 0x49},//97/ Reserved
  {0x5e, 0x0e},//98/ Reserved
  {0x64, 0x04},//99/ Reserved
  {0x65, 0x20},//100/ Reserved
  {0x66, 0x05},//101/ Reserved
  {0x94, 0x04},//102/ Reserved
  {0x95, 0x08},//103/ Reserved
  {0x6c, 0x0a},//104/ Reserved
  {0x6d, 0x55},//105/ Reserved
  {0x6e, 0x11},//106/ Reserved
  {0x6f, 0x9f},//107/ Reserved
  {0x6a, 0x40},//108/ Reserved
  {0x01, 0x40},//109/ REG BLUE : Reserved
  {0x02, 0x40},//110/ REG RED : Reserved
  {0x13, 0xe7},//111/ COM8 : FAST AEC, AEC unlimited STEP, Band Filter, AGC , ARC , AWB enable.
  //
  // Matrix Coefficients
  //
  {0x4f, 0x80},//112/ MTX 1 : Matrix Coefficient 1
  {0x50, 0x80},//113/ MTX 2 : Matrix Coefficient 2
  {0x51, 0x00},//114/ MTX 3 : Matrix Coefficient 3
  {0x52, 0x22},//115/ MTX 4 : Matrix Coefficient 4
  {0x53, 0x5e},//116/ MTX 5 : Matrix Coefficient 5
  {0x54, 0x80},//117/ MTX 6 : Matrix Coefficient 6
  {0x58, 0x9e},//118/ MTXS : Matrix Coefficient Sign for Coefficient 5 to 0
  {0x41, 0x08},//119/ COM16 : AWB Gain enable
  {0x3f, 0x00},//120/ EDGE : Edge Enhancement Adjustment
  {0x75, 0x05},//121/ Reserved
  {0x76, 0xe1},//122/ Reserved
  {0x4c, 0x00},//123/ Reserved
  {0x77, 0x01},//124/ Reserved
  {0x3d, 0xc0},//125/ COM13
  {0x4b, 0x09},//126/ Reserved
  {0xc9, 0x60},//127/ Reserved
  {0x41, 0x38},//128/ COM16
  {0x56, 0x40},//129/ Reserved
  {0x34, 0x11},//130/ Reserved
  {0x3b, 0x12},//131/ COM11 : Exposure and Hz Auto detect enabled.
  {0xa4, 0x88},//132/ Reserved
  {0x96, 0x00},//133/ Reserved
  {0x97, 0x30},//134/ Reserved
  {0x98, 0x20},//135/ Reserved
  {0x99, 0x30},//136/ Reserved
  {0x9a, 0x84},//137/ Reserved
  {0x9b, 0x29},//138/ Reserved
  {0x9c, 0x03},//139/ Reserved
  {0x9d, 0x4c},//140/ Reserved
  {0x9e, 0x3f},//141/ Reserved
  {0x78, 0x04},//142/ Reserved
  //
  // Mutliplexing Registers
  //
  {0x79, 0x01},//143/ Reserved
  {0xc8, 0xf0},//144/ Reserved
  {0x79, 0x0f},//145/ Reserved
  {0xc8, 0x00},//146/ Reserved
  {0x79, 0x10},//147/ Reserved
  {0xc8, 0x7e},//148/ Reserved
  {0x79, 0x0a},//149/ Reserved
  {0xc8, 0x80},//150/ Reserved
  {0x79, 0x0b},//151/ Reserved
  {0xc8, 0x01},//152/ Reserved
  {0x79, 0x0c},//153/ Reserved
  {0xc8, 0x0f},//154/ Reserved
  {0x79, 0x0d},//155/ Reserved
  {0xc8, 0x20},//156/ Reserved
  {0x79, 0x09},//157/ Reserved
  {0xc8, 0x80},//158/ Reserved
  {0x79, 0x02},//159/ Reserved
  {0xc8, 0xc0},//160/ Reserved
  {0x79, 0x03},//161/ Reserved
  {0xc8, 0x40},//162/ Reserved
  {0x79, 0x05},//163/ Reserved
  {0xc8, 0x30},//164/ Reserved
  {0x79, 0x26},//165/ Reserved
  //
  // Additional Settings
  //
  {0x09, 0x00},//166/ COM2 : Output Drive Capability
  {0x55, 0x00},//167/ Brightness Control
  //
  // End Of Constant
  //
  {0xff, 0xff},//174/ End Marker
  };

  int r = 0;
  while (regs[r][0] != 0xff && regs[r][1] != 0xff) {
    ov7670_set(regs[r][0], regs[r][1]);
    r++;
  }

  /*
   * For QQVGA the following register values worked for me

-  OV7670_WriteReg(0x0C,0x04); //enable down sampling
-  OV7670_WriteReg(0x72,0x22); //Down sample by 4
-  OV7670_WriteReg(0x73,0x02); //Clock div4
-  OV7670_WriteReg(0x3E,0x1A); //Clock div4

Note the clock div4 setup depends on what is your clock input to camera module. In my case the clock is 24MHz.
   */

  return 0;
}
