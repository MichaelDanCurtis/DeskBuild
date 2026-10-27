// LVGL v8 configuration: only what differs from LVGL's defaults.
#if 1
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1  // the QSPI panel wants big-endian RGB565

// Use the system heap (it spills into PSRAM) instead of LVGL's fixed pool.
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "esp_timer.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR ((uint32_t)(esp_timer_get_time() / 1000))

#define LV_DISP_DEF_REFR_PERIOD 16
#define LV_DPI_DEF 300

// Called from the SPI "transfer done" interrupt, so it must live in IRAM.
#include "esp_attr.h"
#define LV_ATTRIBUTE_FLUSH_READY IRAM_ATTR

#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

#define LV_USE_LOG 0

#endif  // LV_CONF_H
#endif
