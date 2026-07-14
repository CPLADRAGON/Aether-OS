#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "u8g2.h"
#include "generated_icons.h"

/*
 * README documentation renderer
 *
 * This is a native host renderer built against the exact U8g2 C library
 * shipped in firmware/.pio/libdeps. It renders into a real 64x48 U8g2
 * framebuffer using the same fonts selected by display_manager.cpp:
 *
 *   FONT_SMALL  -> u8g2_font_5x7_tf
 *   FONT_NORMAL -> u8g2_font_6x10_tf
 *   FONT_LARGE  -> u8g2_font_10x20_tf
 *   FONT_HUGE   -> u8g2_font_logisoso18_tn
 *
 * Output is PGM (one luminance value per exact framebuffer pixel), then the
 * build script converts/scales it to PNG with nearest-neighbor filtering.
 *
 * Screen text, coordinates, and layout states mirror firmware/src/main.cpp.
 */

#define W 64
#define H 48
static u8g2_t u8g2;

static uint8_t null_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  (void)u8x8; (void)msg; (void)arg_int; (void)arg_ptr;
  return 1;
}

static void init(void) {
  /*
   * Use the exact SSD1306 64x48 full-buffer setup used by firmware's
   * U8G2_SSD1306_64X48_ER_F_HW_I2C constructor. The callbacks are no-ops:
   * this host renderer never sends the buffer to physical I2C hardware.
   */
  u8g2_Setup_ssd1306_64x48_er_f(&u8g2, U8G2_R0, null_cb, null_cb);
  u8g2_SetFontPosTop(&u8g2);
  u8g2_ClearBuffer(&u8g2);
}

static void small(void)  { u8g2_SetFont(&u8g2, u8g2_font_5x7_tf); }
static void normal(void) { u8g2_SetFont(&u8g2, u8g2_font_6x10_tf); }
static void large(void)  { u8g2_SetFont(&u8g2, u8g2_font_10x20_tf); }
static void huge(void)   { u8g2_SetFont(&u8g2, u8g2_font_logisoso18_tn); }

static void text(int x, int y, const char *s) { u8g2_DrawStr(&u8g2, x, y, s); }
static void header(const char *s) {
  u8g2_DrawBox(&u8g2, 0, 0, 64, 10);
  small();
  u8g2_SetDrawColor(&u8g2, 0);
  text(2, 2, s);
  u8g2_SetDrawColor(&u8g2, 1);
}

/* A tiny generic icon placeholder. Actual screen geometry and U8g2 text are
 * exact; icons are intentionally simple until firmware XBM arrays are
 * factored into a shared renderable asset source. */
/* Exact copy of display_manager.cpp's drawIconScaled() nearest-neighbor
 * sampling logic, operating on the generated firmware XBM arrays. */
static void icon_scaled(int cx, int cy, const uint8_t *src, float scale) {
  const int SRC = 24;
  int dst = (int)(SRC * scale + 0.5f);
  int ox, oy, dx, dy;
  if (dst < 1) return;
  ox = cx - dst / 2;
  oy = cy - dst / 2;
  for (dy = 0; dy < dst; dy++) {
    int sy = (dy * SRC) / dst;
    for (dx = 0; dx < dst; dx++) {
      int sx = (dx * SRC) / dst;
      int byte_idx = sy * 3 + sx / 8;
      int bit_idx = sx % 8;
      if (src[byte_idx] & (1 << bit_idx)) u8g2_DrawPixel(&u8g2, ox + dx, oy + dy);
    }
  }
}

static void menu(void) {
  init(); header("MENU");
  u8g2_DrawBox(&u8g2, 9, 13, 46, 8); small(); u8g2_SetDrawColor(&u8g2, 0);
  text(14, 14, "MEASURE"); u8g2_SetDrawColor(&u8g2, 1);
  text(19, 24, "TIME"); text(14, 31, "WEATHER"); text(19, 38, "ROOM");
}
static void measure(void) {
  init(); header("SCAN 3/5"); icon_scaled(15, 25, xbm_measure_lg, 1.0f); normal(); text(26, 14, "29.4C"); small(); text(27, 26, "68%");
  u8g2_DrawBox(&u8g2, 0, 39, 64, 9); u8g2_SetDrawColor(&u8g2, 0); text(7, 40, "420 BRIGHT"); u8g2_SetDrawColor(&u8g2, 1);
}
static void time_main(void) {
  init(); huge(); text(7, 10, "14:32"); u8g2_DrawHLine(&u8g2, 2, 37, 60);
  small(); text(2, 39, "THU"); text(44, 39, "10 JUL");
}
static void time_detail(void) {
  init(); header("14:32:47"); small(); u8g2_SetDrawColor(&u8g2, 0); text(50, 2, "THU"); u8g2_SetDrawColor(&u8g2, 1);
  text(2, 13, "10 JUL 2026"); text(2, 22, "WK28 D191"); u8g2_DrawHLine(&u8g2, 2, 31, 60);
  text(2, 34, "TODAY"); text(48, 34, "61%"); u8g2_DrawFrame(&u8g2, 2, 43, 60, 3); u8g2_DrawBox(&u8g2, 3, 44, 35, 1);
}
static void weather(void) {
  init(); u8g2_DrawBox(&u8g2, 0, 0, 64, 10); u8g2_SetDrawColor(&u8g2, 0); icon_scaled(7, 5, xbm_weather_sun_lg, 0.33f); small(); text(37, 2, "82% HUM"); u8g2_SetDrawColor(&u8g2, 1);
  large(); text(7, 15, "31.5C"); small(); text(2, 39, "FL 37.0C"); text(58, 39, ">");
}
static void weather_detail(void) {
  init(); icon_scaled(32, 13, xbm_weather_sun_lg, 1.0f); small(); text(19, 27, "SUNNY"); u8g2_DrawHLine(&u8g2, 2, 36, 60); text(14, 39, "31.5C 82%");
}
static void room(void) {
  init(); icon_scaled(32, 7, xbm_room_lg, 0.5f); large(); text(7, 14, "29.4C"); u8g2_DrawHLine(&u8g2, 2, 37, 60); small(); text(2, 39, "68%"); text(35, 39, "BRIGHT>");
}
static void room_detail(void) {
  init(); header("STATUS"); small(); text(2, 16, "T:WARM 29.4C"); text(2, 26, "H:HUMID 68%"); text(2, 36, "L:BRIGHT 420");
}
static void timer(void) {
  init(); header("TIMER"); icon_scaled(56, 5, xbm_interval_lg, 0.33f); large(); text(11, 12, "14:28"); u8g2_DrawFrame(&u8g2, 2, 34, 60, 4); u8g2_DrawBox(&u8g2, 3, 35, 28, 2); small(); text(25, 40, "HOLD:END");
}
static void alert(void) {
  init(); header("TIMER"); normal(); text(16, 16, "DONE!"); small(); text(2, 40, "ANY KEY TO DISMISS");
}

static void emit_pgm(const char *path) {
  FILE *f = fopen(path, "wb");
  int x, y;
  if (!f) { perror(path); exit(1); }
  fprintf(f, "P5\n%d %d\n255\n", W, H);
  for (y = 0; y < H; y++) {
    for (x = 0; x < W; x++) {
      uint8_t b = u8g2_GetBufferPtr(&u8g2)[x + (y / 8) * W];
      fputc((b & (1 << (y & 7))) ? 255 : 0, f);
    }
  }
  fclose(f);
}

typedef void (*screen_fn)(void);
struct screen { const char *name; screen_fn draw; };
static struct screen screens[] = {
  {"menu", menu}, {"measure", measure}, {"time", time_main},
  {"time-detail", time_detail}, {"weather", weather},
  {"weather-detail", weather_detail}, {"room", room},
  {"room-detail", room_detail}, {"timer", timer}, {"alert", alert}
};

int main(int argc, char **argv) {
  int i;
  const char *outdir = argc > 1 ? argv[1] : ".";
  char path[1024];
  for (i = 0; i < (int)(sizeof(screens)/sizeof(screens[0])); i++) {
    screens[i].draw();
    snprintf(path, sizeof(path), "%s/%s.pgm", outdir, screens[i].name);
    emit_pgm(path);
  }
  return 0;
}
