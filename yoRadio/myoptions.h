#ifndef myoptions_h
#define myoptions_h

/* - - - = = = - - - Choose the Radio (defined by platformio.ini env) - - - = = = - - - */

//#define ESP32_S3_N16R8_TEST_BOARD // bare board for testing purposes
//#define DSP_SH1106_PCM_I2S_REMOTE // Self-contained with OLED and Remote
//#define DSP_SH1106_VS1053_3_BUTTONS // Ali Speaker with OLED, VS1053, Remote
//#define DSP_ILI9488_PCM_I2S // Big Screen with PCM, 1 button
//#define DSP_ST7735_PCM_I2S // Color TFT (red board) with PCM I2S


/* - - - = = = - - - Auto-update Firmware File Name - - - = = = - - - */

#if defined(DSP_SH1106_PCM_I2S_REMOTE)
#undef FIRMWARE
#define FIRMWARE "firmware-dsp_sh1106-pcm_i2s-remote.bin"
#elif defined(DSP_SH1106_VS1053_3_BUTTONS)
#undef FIRMWARE
#define FIRMWARE "firmware-dsp_sh1006-vs1053-3_buttons.bin"
#elif defined(DSP_ILI9488_PCM_I2S)
#undef FIRMWARE
#define FIRMWARE "firmware-dsp_ili9488-pcm_i2s.bin"
#elif defined(DSP_ST7735_PCM_I2S)
#undef FIRMWARE
#define FIRMWARE "firmware-dsp_st7735-pcm_i2s.bin"
#else //ESP32_S3_N16R8_TEST_BOARD
#undef FIRMWARE
#define FIRMWARE "firmware-esp32_s3_n16r8-test_board.bin"
#endif


/* --- BOARD --- */

/* Everything so far uses */
#define ARDUINO_ESP32S3_DEV


/* --- LED --- */

/* S3 RGB LED (all) */
#define USE_BUILTIN_LED   false /* usually...! Unless you actually want to use the builtin as defined by the board's .h file */

#if defined(DSP_SH1106_PCM_I2S_REMOTE)
/* LED config for Self-contained - on remote */
#define LED_BUILTIN_S3 8
#define LED_INVERT	true
#else
/* LED config for all others */
#define LED_BUILTIN_S3 255 /* 255 keeps it unlit */
/* LED Actual... Probably don't use... */
//#define LED_BUILTIN 48 /* actual PIN is 48 on S3 Antennas (and never use this line?) */
#endif


/* --- DISPLAY --- */

#if defined(DSP_SH1106_PCM_I2S_REMOTE) || defined(DSP_SH1106_VS1053_3_BUTTONS)
/* Display config for I2C OLED displays (pick one) */
//#define DSP_MODEL			DSP_SSD1306x32 /* Tiny OLED (didn't build any yet) */
//#define DSP_MODEL			DSP_SSD1306 /* none but maybe useful to note */
#define DSP_MODEL			DSP_SH1106 /* Regular OLED - platformio.ini */
#define YO_FIX
#define PRINT_FIX

/* Pins for I2C Displays above */
#define I2C_SDA			42
#define I2C_SCL			41
#endif

/* Display config for SPI displays (pick one) */
/* When using SPI Displays, trying to use same SPI MOSI, SCK, MISO as VS1053 doesn't work */
#if defined(DSP_ILI9488_PCM_I2S)
#define DSP_MODEL			DSP_ILI9488 /* Big Display */
#endif
#if defined(DSP_ST7735_PCM_I2S)
#define DSP_MODEL			DSP_ST7735 /* Red board / 1.8" Black Tab, if problems try one of DTYPE */
#define YO_FIX
#define PRINT_FIX
/* DSP_ST7735 DTYPES BELOW (add if needed but so far, not needed)*/
//#define DTYPE			INITR_GREENTAB /* Add this for Green Tab */
//#define DTYPE			INITR_REDTAB /* Add this for Red Tab */
//#define DTYPE			INITR_144GREENTAB /* Add this for 1.44" Green Tab */
//#define DTYPE			INITR_MINI160x80 /* Add this for 0.96" Mini 160x80 */
#endif
#if defined(DSP_ST7735_PCM_I2S)
/* Pins for SPI Display: Red Board */
#define TFT_DC			10
#define TFT_CS			9
#define BRIGHTNESS_PIN	4 /* Red Smaller TFT doesn't have brightness control so leave commented? use unused pin? or 255? */
#define TFT_RST			-1 /* set to -1 if connected to ESP EN pin */
#endif
#if defined(DSP_ILI9488_PCM_I2S)
/* Pins for SPI Display: Big Display */
#define YO_FIX
#define PRINT_FIX
#define TFT_DC			10
#define TFT_CS			9
#define BRIGHTNESS_PIN	4
#define TFT_RST			-1 /* set to -1 if connected to ESP EN pin */
#define DOWN_LEVEL 63     /* Maleksm's mod: brightness level 0 to 255, default 2 */
#define DOWN_INTERVAL 60   /* Maleksm's mod: seconds to dim, default 60 = 60 seconds */
/* modify src\displays\displayILI9488.cpp -- in section DspCore::initDisplay and add setRotation(3); to do 180 degree rotation */
#endif


/* --- AUDIO DECODER --- */

#if defined(DSP_SH1106_VS1053_3_BUTTONS)
/* SPI VS1053 config for OLEDs */
#define VS_HSPI       false
#define VS1053_CS		9
#define VS1053_DCS	14
#define VS1053_DREQ	10
#define VS1053_RST    -1  /* set to -1 if connected to ESP EN pin */
#define I2S_DOUT      255 /* set to 255 to disable PCM */
#define VS_PATCH_ENABLE false /* For the 2.5V boards with wrong voltage regulator.  See here: https://github.com/e2002/yoradio/issues/108 */
                                /* Probably works on all...? */
#endif
#if defined(DSP_ST7735_PCM_I2S) || defined(DSP_ILI9488_PCM_I2S)
/* PCM I2S config for TFT Red & Big Display */
#define I2S_DOUT		15
#define I2S_BCLK		7
#define I2S_LRC			6
#define VS1053_CS     255 // set to 255 to disable VS1053
#endif
#if defined(DSP_SH1106_PCM_I2S_REMOTE)
/* PCM I2S config for Self-contained */
#define I2S_DOUT		12
#define I2S_BCLK		11
#define I2S_LRC			10
#define VS1053_CS     255 // set to 255 to disable VS1053
#endif


/* --- BUTTONS --- */

#if defined(DSP_SH1106_VS1053_3_BUTTONS)
/* Button config for OLED with 3 Buttons */
#define BTN_UP                17           /*  Prev, Move Up */
#define BTN_DOWN              18           /*  Next, Move Down */
#define BTN_MODE              16           /*  MODE switcher  */
#define WAKE_PIN              16           /*  Wake from Deepsleep (actually using existing pins kind of disables sleep) */
#endif
#if defined(DSP_SH1106_PCM_I2S_REMOTE)
/*Button config for Self-contained - buttons on remote */
#define BTN_UP                17          /*  Prev, Move Up */
#define BTN_DOWN              16          /*  Next, Move Down */
//#define BTN_MODE              18          /*  MODE switcher  */
#define BTN_CENTER            18          /*  ENTER, Play/pause  */
#define BTN_LEFT              7           /*  VolDown, Prev */
#define BTN_RIGHT             15          /*  VolUp, Next */
#define WAKE_PIN              18          /*  Wake from Deepsleep (actually using existing pins kind of disables sleep) */
#endif
#if defined(DSP_ST7735_PCM_I2S) || defined(DSP_ILI9488_PCM_I2S)
/* TFT Red & Big Screen Version - just one button */
#define BTN_DOWN              42           /*  Next, Move Down */
#endif

/* Extras: unused in all */
//#define BTN_INTERNALPULLUP    false         /*  Enable the weak pull up resistors */
//#define BTN_LONGPRESS_LOOP_DELAY    200     /*  Delay between calling DuringLongPress event */
//#define BTN_CLICK_TICKS    300              /*  Event Timing https://github.com/mathertel/OneButton#event-timing */
//#define BTN_PRESS_TICKS    500              /*  Event Timing https://github.com/mathertel/OneButton#event-timing */


/* --- ROTARY ENCODER(S) --- */

#if defined(DSP_SH1106_VS1053_3_BUTTONS) || defined(DSP_ST7735_PCM_I2S) || defined(DSP_ILI9488_PCM_I2S) ||\
    defined(DSP_SH1106_PCM_I2S_REMOTE)
/* Rotary Encoder: OLED, TFT Red, Big Screen, Self-contained */
#define ENC_BTNR			40
#define ENC_BTNL			39
#define ENC_BTNB			38
#endif

/* Extras: unused in all */
//#define ENC_INTERNALPULLUP    true
//#define ENC_HALFQUARD         false

/* 2nd Rotary Encoder: ?? None yet */
//#define ENC2_BTNR			40
//#define ENC2_BTNL			39
//#define ENC2_BTNB			38
/* Extras: unused */
//#define ENC2_INTERNALPULLUP    true
//#define ENC2_HALFQUARD         false


/* --- SD CARD --- */

#if defined(DSP_ST7735_PCM_I2S) || defined(DSP_SH1106_PCM_I2S_REMOTE)
/* SD Card config for TFT Red, Self-contained */
#define SD_SPIPINS	21, 13, 14			/* SCK, MISO, MOSI */
#define SDC_CS			47
#endif
#if defined(DSP_SH1106_VS1053_3_BUTTONS ) || defined(DSP_ILI9488_PCM_I2S)
/* SD Card config for OLED, Big Screen */
#define SD_SPIPINS	21, 2, 1			/* SCK, MISO, MOSI */
#define SDC_CS			47
#endif

/* Extras: unused in all */
//#define SD_HSPI /* false (not needed when using custom pins) */
//#define SDSPISPEED 8000000 /* Default speed 4000000 but try 8000000? */
// actually default is 20000000 ?? so whoops means 40000000 is okay?
//#define SDSPISPEED 8000000


/* --- REGIONAL DEFAULTS --- */

/* Make sure Timezone options conform to TZDB standards */
/* https://github.com/trip5/timezones.json/blob/master/timezones.json */
#define TIMEZONES_JSON_URL "https://raw.githubusercontent.com/trip5/timezones.json/master/timezones.json.gz"
#define TIMEZONE_NAME "Asia/Seoul"
#define TIMEZONE_POSIX "KST-9"
#define SNTP1 "pool.ntp.org"
#define SNTP2 "north-america.pool.ntp.org"

/* Weather Co-ordinates */
#define WEATHERLAT "55.7512" /* latitude */
#define WEATHERLON "37.6184" /* longitude */

/* Use https://www.radio-browser.info/ API to get JSON of radio streams */
//#define RADIO_BROWSER_SERVERS_URL "https://all.api.radio-browser.info/json/servers"


/* --- MALEKSM'S BATTERY WIDGET --- */

//#define BATTERY_WIDGET
//#define ADC_PIN x
//#define R1
//#define R2
//#define DELTA_BAT
//#define R1 50 // resistor from battery positive to ADC_PIN, default 50 = 50k ohm
//#define R2 100  // resistor from ADC_PIN to GND, default 100 = 100k ohm
//#define DELTA_BAT // simple addition or subtraction to correct the calculation, default 0


/* --- MORE, UNUSED, UNKNOWN --- */

#define ESPFILEUPDATER_DEBUG

/* Extras: unused in all */
//#define L10N_LANGUAGE EN
// #define IR_PIN 4

/* Memory? */
#define XTASK_MEM_SIZE 4096 /* default 4096*/

/* Does this get carried to SD Lib and allow Exfat? */
//#define FF_FS_EXFAT 1

#endif // myoptions_h
