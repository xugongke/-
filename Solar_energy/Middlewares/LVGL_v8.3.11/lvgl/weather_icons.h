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
#include "lvgl.h"
#endif

/* Weather icon symbols - UTF-8 encoded PUA E000-E00D */
#define WEATHER_ICON_SUN           "\xEE\x80\x80"  /* U+E000 - sunny (fa-sun) */
#define WEATHER_ICON_CLOUD_SUN     "\xEE\x80\x81"  /* U+E001 - partly cloudy (fa-cloud-sun) */
#define WEATHER_ICON_CLOUD         "\xEE\x80\x82"  /* U+E002 - cloudy / overcast (fa-cloud) */
#define WEATHER_ICON_FOG           "\xEE\x80\x83"  /* U+E003 - fog (fa-smog) */
#define WEATHER_ICON_HAZE          "\xEE\x80\x84"  /* U+E004 - haze (fa-smog variant) */
#define WEATHER_ICON_DROPLET       "\xEE\x80\x85"  /* U+E005 - light rain / drizzle / shower (fa-droplet) */
#define WEATHER_ICON_CLOUD_RAIN    "\xEE\x80\x86"  /* U+E006 - moderate rain (fa-cloud-rain) */
#define WEATHER_ICON_HEAVY_RAIN    "\xEE\x80\x87"  /* U+E007 - heavy rain (fa-cloud-showers-heavy) */
#define WEATHER_ICON_SNOWFLAKE     "\xEE\x80\x88"  /* U+E008 - snow (fa-snowflake) */
#define WEATHER_ICON_CLOUD_BOLT    "\xEE\x80\x89"  /* U+E009 - thunderstorm (fa-cloud-bolt) */
#define WEATHER_ICON_BOLT          "\xEE\x80\x8A"  /* U+E00A - hail (fa-bolt) */
#define WEATHER_ICON_WIND          "\xEE\x80\x8B"  /* U+E00B - wind (reserve) (fa-wind) */
#define WEATHER_ICON_MOON          "\xEE\x80\x8D"  /* U+E00D - clear night (fa-moon) */

/* Day/Night indicator icons (use with lv_font_weather_30) */
#define WEATHER_DAY_SUN            "\xEE\x80\x80"  /* U+E000 - sun for daytime */
#define WEATHER_NIGHT_MOON         "\xEE\x80\x8D"  /* U+E00D - moon for nighttime */

/* Declare the external font */
LV_FONT_DECLARE(lv_font_weather_30)

#ifdef __cplusplus
}
#endif

#endif /* WEATHER_ICONS_H */
