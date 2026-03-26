#ifndef RT_CONFIG_H__
#define RT_CONFIG_H__

/* LVGL configuration */


/* Color settings */

#define LV_COLOR_DEPTH_16
#define LV_COLOR_DEPTH 16
#define LV_COLOR_MIX_ROUND_OFS 128
#define LV_COLOR_CHROMA_KEY_HEX 0x00FF00
/* end of Color settings */

/* Memory settings */

#define LV_MEM_SIZE_KILOBYTES 32
#define LV_MEM_ADDR 0x0
#define LV_MEM_BUF_MAX_NUM 16
/* end of Memory settings */

/* HAL Settings */

#define LV_DISP_DEF_REFR_PERIOD 30
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_DPI_DEF 130
/* end of HAL Settings */

/* Feature configuration */

/* Drawing */

#define LV_DRAW_COMPLEX
#define LV_SHADOW_CACHE_SIZE 0
#define LV_CIRCLE_CACHE_SIZE 4
#define LV_LAYER_SIMPLE_BUF_SIZE 24576
#define LV_IMG_CACHE_DEF_SIZE 0
#define LV_GRADIENT_MAX_STOPS 2
#define LV_GRAD_CACHE_DEF_SIZE 0
#define LV_DISP_ROT_MAX_BUF 10240
/* end of Drawing */

/* GPU */

#define LV_USE_GPU_STM32_DMA2D
#define LV_GPU_DMA2D_CMSIS_INCLUDE ""
/* end of GPU */

/* Logging */

/* end of Logging */

/* Asserts */

#define LV_USE_ASSERT_NULL
#define LV_USE_ASSERT_MALLOC
#define LV_ASSERT_HANDLER_INCLUDE "assert.h"
/* end of Asserts */

/* Others */

#define LV_USE_USER_DATA
/* end of Others */

/* Compiler settings */

#define LV_ATTRIBUTE_MEM_ALIGN_SIZE 1
/* end of Compiler settings */
/* end of Feature configuration */

/* Font usage */

/* Enable built-in fonts */

#define LV_FONT_MONTSERRAT_14
/* end of Enable built-in fonts */
#define LV_FONT_DEFAULT_MONTSERRAT_14
#define LV_USE_FONT_PLACEHOLDER
/* end of Font usage */

/* Text Settings */

#define LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_COLOR_CMD "#"
/* end of Text Settings */

/* Widget usage */

#define LV_USE_ARC
#define LV_USE_BAR
#define LV_USE_BTN
#define LV_USE_BTNMATRIX
#define LV_USE_CANVAS
#define LV_USE_CHECKBOX
#define LV_USE_DROPDOWN
#define LV_USE_IMG
#define LV_USE_LABEL
#define LV_LABEL_TEXT_SELECTION
#define LV_LABEL_LONG_TXT_HINT
#define LV_USE_LINE
#define LV_USE_ROLLER
#define LV_ROLLER_INF_PAGES 7
#define LV_USE_SLIDER
#define LV_USE_SWITCH
#define LV_USE_TEXTAREA
#define LV_TEXTAREA_DEF_PWD_SHOW_TIME 1500
#define LV_USE_TABLE
/* end of Widget usage */

/* Extra Widgets */

#define LV_USE_ANIMIMG
#define LV_USE_CALENDAR
#define LV_USE_CALENDAR_HEADER_ARROW
#define LV_USE_CALENDAR_HEADER_DROPDOWN
#define LV_USE_CHART
#define LV_USE_COLORWHEEL
#define LV_USE_IMGBTN
#define LV_USE_KEYBOARD
#define LV_USE_LED
#define LV_USE_LIST
#define LV_USE_MENU
#define LV_USE_METER
#define LV_USE_MSGBOX
#define LV_USE_SPAN
#define LV_SPAN_SNIPPET_STACK_SIZE 64
#define LV_USE_SPINBOX
#define LV_USE_SPINNER
#define LV_USE_TABVIEW
#define LV_USE_TILEVIEW
#define LV_USE_WIN
/* end of Extra Widgets */

/* Themes */

#define LV_USE_THEME_DEFAULT
#define LV_THEME_DEFAULT_GROW
#define LV_THEME_DEFAULT_TRANSITION_TIME 80
#define LV_USE_THEME_BASIC
/* end of Themes */

/* Layouts */

#define LV_USE_FLEX
#define LV_USE_GRID
/* end of Layouts */

/* 3rd Party Libraries */

/* end of 3rd Party Libraries */

/* Others */

#define LV_USE_SNAPSHOT
/* end of Others */

/* Examples */

#define LV_BUILD_EXAMPLES
/* end of Examples */

/* Demos */

/* end of Demos */
/* end of LVGL configuration */

#endif
