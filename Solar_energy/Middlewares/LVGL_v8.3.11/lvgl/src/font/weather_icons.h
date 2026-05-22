/**
 * Weather icon definitions using Font Awesome 6 glyphs
 * Mapped to Unicode Private Use Area (E000-E00D)
 * Font file: lv_font_weather_30.c (size 30px, bpp 4)
 */

#ifndef WEATHER_ICONS_H
#define WEATHER_ICONS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/* Weather icon symbols - mapped to PUA E000-E00D */
#define WEATHER_ICON_SUN           "\xEE\x80\x80"  /* U+E000 - sunny */
#define WEATHER_ICON_CLOUD_SUN     "\xEE\x80\x81"  /* U+E001 - partly cloudy */
#define WEATHER_ICON_CLOUD         "\xEE\x80\x82"  /* U+E002 - cloudy / overcast */
#define WEATHER_ICON_FOG           "\xEE\x80\x83"  /* U+E003 - fog */
#define WEATHER_ICON_HAZE          "\xEE\x80\x84"  /* U+E004 - haze */
#define WEATHER_ICON_DROPLET       "\xEE\x80\x85"  /* U+E005 - light rain / drizzle / shower */
#define WEATHER_ICON_CLOUD_RAIN    "\xEE\x80\x86"  /* U+E006 - moderate rain */
#define WEATHER_ICON_HEAVY_RAIN    "\xEE\x80\x87"  /* U+E007 - heavy rain */
#define WEATHER_ICON_SNOWFLAKE     "\xEE\x80\x88"  /* U+E008 - snow */
#define WEATHER_ICON_CLOUD_BOLT    "\xEE\x80\x89"  /* U+E009 - thunderstorm */
#define WEATHER_ICON_BOLT          "\xEE\x80\x8A"  /* U+E00A - hail */
#define WEATHER_ICON_WIND          "\xEE\x80\x8B"  /* U+E00B - wind (reserve) */
#define WEATHER_ICON_MOON          "\xEE\x80\x8D"  /* U+E00D - clear night */

/* Day/Night indicator icons (use with lv_font_weather_30) */
#define WEATHER_DAY_SUN            "\xEE\x80\x80"  /* U+E000 - sun for daytime */
#define WEATHER_NIGHT_MOON         "\xEE\x80\x8D"  /* U+E00D - moon for nighttime */

/* Declare the external font */
LV_FONT_DECLARE(lv_font_weather_30)

#ifdef __cplusplus
}
#endif

#endif /* WEATHER_ICONS_H */
