#ifndef _my_theme_h
#define _my_theme_h

/*
    Theming of color displays
    DSP_ST7735, DSP_ST7789, DSP_ILI9341, DSP_GC9106, DSP_ILI9225, DSP_ST7789_240
    ***********************************************************************
    *    !!! This file must be in the root directory of the sketch !!!    *
    ***********************************************************************
    Uncomment (remove double slash //) from desired line to apply color
*/
#define ENABLE_THEME
#ifdef  ENABLE_THEME

/*-----------------------------------------------------------------------------------------------*/
/*       | COLORS             |   values (0-255)  |                                              */
/*       | color name         |    R    G    B    |                                              */
/*-----------------------------------------------------------------------------------------------*/
//#define COLOR_BACKGROUND        20,  5,   0     /*  background     (под корпус Colon)             */
#define COLOR_WEATHER            0, 255,  0      /*  weather string       "Lime"                 */
#define COLOR_SNG_TITLE_1       255, 255, 255     /*  first title                                */
#define COLOR_SNG_TITLE_2       220, 220, 220     /*  second title                               */
#define COLOR_VU_MAX           178,  34, 34     /*  max of VU meter      "FireBrick"             */
#define COLOR_VU_MIN             0, 128,  0     /*  min of VU meter      "Green"                 */
//#define COLOR_CLOCK            231, 211,  90    /*  clock color      "Gold"                      */
//#define COLOR_SECONDS             0, 255, 255    /*  seconds color (DSP_ST7789, DSP_ILI9341, DSP_ILI9225)   */
#define COLOR_CLOCK_BG           5,   5,   0      /*  clock color background                     */
//#define COLOR_DAY_OF_W         231, 211, 90      /*  day of week color (for DSP_ST7735)   "Gold"  */
#define COLOR_DAY_OF_W         240, 240, 240     /*  day of week color (for DSP_ST7789, DSP_ILI9341, DSP_ILI9225) */
#define COLOR_DATE             255, 255, 255    /*  date color (DSP_ST7789, DSP_ILI9341, DSP_ILI9225)         */
#define COLOR_HEAP              0, 0, 255       /*  heap string          "Blue"                  */
#define COLOR_BUFFER            0, 0, 255       /*  buffer line          "Blue"                  */
#define COLOR_RSSI              0, 70, 255      /*  rssi                 "SkylBlue"             */
//#define COLOR_RSSI              0, 191, 255     /*  rssi                 "DeepSkyBlue"           */

#define COLOR_PL_CURRENT        255, 255, 0     /*  playlist current item         "Yellow"       */
#define COLOR_PL_CURRENT_BG     30, 30 , 30     /*  playlist current item background             */
#define COLOR_PL_CURRENT_FILL   60, 60, 60      /*  playlist current item fill background    "Gray"     */
#define COLOR_PLAYLIST_0        250, 250, 250   /*  playlist string 0                            */
#define COLOR_PLAYLIST_1        230, 230, 230   /*  playlist string 1                            */
#define COLOR_PLAYLIST_2        210, 210, 210   /*  playlist string 2                            */
#define COLOR_PLAYLIST_3        190, 190, 190   /*  playlist string 3                            */
#define COLOR_PLAYLIST_4        170, 170, 170   /*  playlist string 4                            */


#endif  /* #ifdef  ENABLE_THEME */
#endif  /* #define _my_theme_h  */
