#ifndef myoptions_h
#define myoptions_h


/* - - - = = = - - - Choose the Radio (defined by platformio.ini env) - - - = = = - - - */
/* automatic builds define the board - - - be sure to comment all lines after debugging */

//#define DEBUG_MYOPTIONS                              // uncomment to debug myoptions.h
//#define ESP32_S3_TRIP5_SH1106_PCM_REMOTE             // Self-contained OLED with PCM, Remote
//#define ESP32_S3_TRIP5_SH1106_PCM_1BUTTON            // Mini OLED with PCM, 1 Button, Speakers built-in
//#define ESP32_S3_TRIP5_SSD1306X32_PCM_1BUTTON        // Mini Tiny OLED with PCM, 1 Button, Speakers built-in
//#define ESP32_S3_TRIP5_SH1106_VS1053_3BUTTONS        // Ali Speaker with OLED, VS1053, 3 Buttons
//#define ESP32_S3_TRIP5_ST7735_PCM_1BUTTON            // Color TFT (red board) with PCM I2S, 1 Button
//#define ESP32_S3_TRIP5_ILI9488_PCM_1BUTTON           // Big Screen with PCM, 1 button


/* --- UPDATE FILES --- */

#define UPDATEURL "https://github.com/trip5/yoradio/releases/latest/download/" // + FIRMWARE for the file
#define FILESURL "https://github.com/trip5/yoradio/releases/download/2025.08.07/" // + FILE for SPIFFS files (this version)
#define CHECKUPDATEURL "https://github.com/trip5/yoradio/releases/latest/download/version.txt" // automatically extracted from options.h during Github workflow
#define VERSIONSTRING "#define YOVERSION" // the file above should have a line that contains this followed by a version number


/* --- FIRMWARE FILENAME & BOARD --- */

#if defined(BOARD_ESP32) & not defined(DEBUG_MYOPTIONS)
#undef FIRMWARE
#define FIRMWARE "board_esp32.bin"
//#undef UPDATEURL // if an ESP does not have the memory to do online updates from https sources (this will disable it)
#elif defined(BOARD_ESP32_S3_N16R8)
#undef FIRMWARE
#define FIRMWARE "board_esp32_s3_n16r8.bin"
#define ARDUINO_ESP32S3_DEV
#elif defined(ESP32_S3_TRIP5_SH1106_PCM_REMOTE)
#undef FIRMWARE
#define FIRMWARE "esp32_s3_trip5_sh1106_pcm_remote.bin"
#define ARDUINO_ESP32S3_DEV
#elif defined(ESP32_S3_TRIP5_SH1106_PCM_1BUTTON)
#undef FIRMWARE
#define FIRMWARE "esp32_s3_trip5_sh1106_pcm_1button.bin"
#define ARDUINO_ESP32S3_DEV
#elif defined(ESP32_S3_TRIP5_SSD1306X32_PCM_1BUTTON)
#undef FIRMWARE
#define FIRMWARE "esp32_s3_trip5_ssd1306x32_pcm_1button.bin"
#define ARDUINO_ESP32S3_DEV
#elif defined(ESP32_S3_TRIP5_SH1106_VS1053_3BUTTONS)
#undef FIRMWARE
#define FIRMWARE "esp32_s3_trip5_sh1106_vs1053_3buttons.bin"
#define ARDUINO_ESP32S3_DEV
#elif defined(ESP32_S3_TRIP5_ST7735_PCM_1BUTTON)
#undef FIRMWARE
#define FIRMWARE "esp32_s3_trip5_st7735_pcm_1button.bin"
#define ARDUINO_ESP32S3_DEV
#elif defined(ESP32_S3_TRIP5_ILI9488_PCM_1BUTTON)
#undef FIRMWARE
#define FIRMWARE "esp32_s3_trip5_ili9488_pcm_1button.bin"
#define ARDUINO_ESP32S3_DEV
#endif


/* --- LED --- */

#if defined(BOARD_ESP32)
#define USE_BUILTIN_LED   true
#else
#define USE_BUILTIN_LED   false /* usually...! Unless you actually want to use the builtin as defined by the board's .h file */
#endif

#if defined(ESP32_S3_TRIP5_SH1106_PCM_REMOTE)
#define LED_BUILTIN_S3 8
#define LED_INVERT	true
#elif defined(BOARD_ESP32_S3_N16R8)
//#define LED_BUILTIN 48 /* S3 RGB LED */
#else
/* LED config for all others - keep it off */
#define LED_BUILTIN_S3 255
#endif


/* --- DISPLAY --- */

#if defined(ESP32_S3_TRIP5_SH1106_PCM_REMOTE) || defined(ESP32_S3_TRIP5_SH1106_PCM_1BUTTON) ||\
    defined(ESP32_S3_TRIP5_SH1106_VS1053_3BUTTONS)
#define DSP_MODEL			DSP_SH1106 /* Regular OLED - platformio.ini */
#define YO_FIX
#define PRINT_FIX
#define I2C_SDA			42
#define I2C_SCL			41
#endif

#if defined(ESP32_S3_TRIP5_SSD1306X32_PCM_1BUTTON)
#define DSP_MODEL			DSP_SSD1306x32 /* Tiny OLED */
#define YO_FIX
#define PRINT_FIX
#define I2C_SDA			42
#define I2C_SCL			41
#endif

/* Display config for SPI displays (pick one) */
/* When using SPI Displays, trying to use same SPI MOSI, SCK, MISO as VS1053 doesn't work */
#if defined(ESP32_S3_TRIP5_ILI9488_PCM_1BUTTON)
#define DSP_MODEL			DSP_ILI9488 /* Big Display */
#endif
#if defined(ESP32_S3_TRIP5_ST7735_PCM_1BUTTON)
#define DSP_MODEL			DSP_ST7735 /* Red board / 1.8" Black Tab, if problems try one of DTYPE */
#define YO_FIX
#define PRINT_FIX
/* DSP_ST7735 DTYPES BELOW (add if needed but so far, not needed)*/
//#define DTYPE			INITR_GREENTAB /* Add this for Green Tab */
//#define DTYPE			INITR_REDTAB /* Add this for Red Tab */
//#define DTYPE			INITR_144GREENTAB /* Add this for 1.44" Green Tab */
//#define DTYPE			INITR_MINI160x80 /* Add this for 0.96" Mini 160x80 */
#endif
#if defined(ESP32_S3_TRIP5_ST7735_PCM_1BUTTON)
#define TFT_DC			10
#define TFT_CS			9
#define BRIGHTNESS_PIN	4 /* Red Smaller TFT doesn't have brightness control so leave commented? use unused pin? or 255? */
#define TFT_RST			-1 /* set to -1 if connected to ESP EN pin */
#endif
#if defined(ESP32_S3_TRIP5_ILI9488_PCM_1BUTTON)
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

#if defined(ESP32_S3_TRIP5_SH1106_VS1053_3BUTTONS)
#define VS_HSPI       false
#define VS1053_CS		9
#define VS1053_DCS	14
#define VS1053_DREQ	10
#define VS1053_RST    -1  /* set to -1 if connected to ESP EN pin */
#define I2S_DOUT      255 /* set to 255 to disable PCM */
#define VS_PATCH_ENABLE false /* For the 2.5V boards with wrong voltage regulator.  See here: https://github.com/e2002/yoradio/issues/108 */
                                /* Probably works on all...? */
#endif
#if defined(ESP32_S3_TRIP5_ST7735_PCM_1BUTTON) || defined(ESP32_S3_TRIP5_ILI9488_PCM_1BUTTON)
#define I2S_DOUT		15
#define I2S_BCLK		7
#define I2S_LRC			6
#define VS1053_CS     255 // set to 255 to disable VS1053
#endif
#if defined(ESP32_S3_TRIP5_SH1106_PCM_REMOTE) || defined(ESP32_S3_TRIP5_SH1106_PCM_1BUTTON) ||\
    defined(ESP32_S3_TRIP5_SSD1306X32_PCM_1BUTTON)
#define I2S_DOUT		12
#define I2S_BCLK		11
#define I2S_LRC			10
#define VS1053_CS     255 // set to 255 to disable VS1053
#endif


/* --- BUTTONS --- */

#if defined(ESP32_S3_TRIP5_SH1106_VS1053_3BUTTONS)
#define BTN_UP                17           /*  Prev, Move Up */
#define BTN_DOWN              18           /*  Next, Move Down */
#define BTN_MODE              16           /*  MODE switcher  */
#define WAKE_PIN              16           /*  Wake from Deepsleep (actually using existing pins kind of disables sleep) */
#endif
#if defined(ESP32_S3_TRIP5_SH1106_PCM_1BUTTON) || defined(ESP32_S3_TRIP5_SSD1306X32_PCM_1BUTTON)
#define BTN_DOWN              17           /*  Next, Move Down */
#endif
#if defined(ESP32_S3_TRIP5_SH1106_PCM_REMOTE)
#define BTN_UP                17          /*  Prev, Move Up */
#define BTN_DOWN              16          /*  Next, Move Down */
#define BTN_CENTER            18          /*  ENTER, Play/pause  */
#define BTN_LEFT              7           /*  VolDown, Prev */
#define BTN_RIGHT             15          /*  VolUp, Next */
#define WAKE_PIN              18          /*  Wake from Deepsleep (actually using existing pins kind of disables sleep) */
#endif
#if defined(ESP32_S3_TRIP5_ST7735_PCM_1BUTTON) || defined(ESP32_S3_TRIP5_ILI9488_PCM_1BUTTON)
#define BTN_DOWN              42           /*  Next, Move Down */
#endif

/* Extras: unused in all */
//#define BTN_INTERNALPULLUP    false         /*  Enable the weak pull up resistors */
//#define BTN_LONGPRESS_LOOP_DELAY    200     /*  Delay between calling DuringLongPress event */
//#define BTN_CLICK_TICKS    300              /*  Event Timing https://github.com/mathertel/OneButton#event-timing */
//#define BTN_PRESS_TICKS    500              /*  Event Timing https://github.com/mathertel/OneButton#event-timing */


/* --- ROTARY ENCODER(S) --- */

#if defined(ESP32_S3_TRIP5_SH1106_VS1053_3BUTTONS) || defined(ESP32_S3_TRIP5_ST7735_PCM_1BUTTON) ||\
    defined(ESP32_S3_TRIP5_ILI9488_PCM_1BUTTON) || defined(ESP32_S3_TRIP5_SH1106_PCM_REMOTE)
#define ENC_BTNR			40
#define ENC_BTNL			39
#define ENC_BTNB			38
#elif defined(ESP32_S3_TRIP5_SH1106_PCM_1BUTTON) || defined(ESP32_S3_TRIP5_SSD1306X32_PCM_1BUTTON)
#define ENC_BTNR			7
#define ENC_BTNL			15
#define ENC_BTNB			16
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

#if defined(ESP32_S3_TRIP5_ST7735_PCM_1BUTTON) || defined(ESP32_S3_TRIP5_SH1106_PCM_REMOTE) ||\
    defined(ESP32_S3_TRIP5_SH1106_PCM_1BUTTON) || defined(ESP32_S3_TRIP5_SSD1306X32_PCM_1BUTTON)
#define SD_SPIPINS	21, 13, 14			/* SCK, MISO, MOSI */
#define SDC_CS			47
#elif defined(ESP32_S3_TRIP5_SH1106_VS1053_3BUTTONS ) || defined(ESP32_S3_TRIP5_ILI9488_PCM_1BUTTON)
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
#define TIMEZONES_JSON_URL "https://raw.githubusercontent.com/trip5/timezones.json/master/timezones.json.gz"
//#define TIMEZONES_JSON_URL "https://github.com/trip5/timezones.json/releases/latest/download/timezones.json.gz"
#define TIMEZONE_NAME "Asia/Seoul"
#define TIMEZONE_POSIX "KST-9"
#define SNTP1 "kr.pool.ntp.org"
#define SNTP2 "pool.ntp.org"

/* Weather Co-ordinates */
#define WEATHERLAT "37.5503" /* latitude */
#define WEATHERLON "126.9971" /* longitude */

/* Use https://www.radio-browser.info/ API to get JSON of radio streams */
#define RADIO_BROWSER_SERVERS_URL "https://all.api.radio-browser.info/json/servers"

/* --- MORE, UNUSED, UNKNOWN --- */

#define HIDE_VOLPAGE /* Hides "Volume" page - use the progress bar instead  */

//#define ROTATE_90 /* rotates 90 degrees? */

//#define ESPFILEUPDATER_DEBUG

/* Extras: unused in all */
//#define L10N_LANGUAGE EN
// #define IR_PIN 4

/* Memory? */
//#define XTASK_MEM_SIZE 4096 /* default 4096*/

/* Does this get carried to SD Lib and allow Exfat? */
//#define FF_FS_EXFAT 1

#endif // myoptions_h
