#ifndef DV_RASTER_FONT_H
#define DV_RASTER_FONT_H

#include <SDL.h>

#include "dv_image_cache.h"

#define GLYPH_ARRAY_SIZE 256
#define STRING_BUFFER_SIZE 2048

typedef struct {
  SDL_Surface *glyphs[GLYPH_ARRAY_SIZE];
  uint32_t head_kerns[GLYPH_ARRAY_SIZE];
  uint32_t tail_kerns[GLYPH_ARRAY_SIZE];
} font_t;

font_t *font_create(const char *image_fn, uint32_t fg_color, uint32_t bg_color);
void font_delete(font_t *font);

int32_t font_get_height(font_t *font);
int32_t font_get_width(font_t *font, const char *string);

void font_draw_string(font_t *font, const char *string, int32_t x, int32_t y, SDL_Surface *target);
void font_draw_partial_string(font_t *font, const char *string, int32_t len, int32_t x, int32_t y, SDL_Surface *target);

int32_t font_wrap_string(font_t *font, const char *string, int32_t x, int32_t y, int32_t w, SDL_Surface *target);
int32_t font_wrap_partial_string(font_t *font, const char *string, int32_t len, int32_t x, int32_t y, int32_t w, SDL_Surface *target);

void font_draw_all_glyphs(font_t *font, int32_t x, int32_t y, SDL_Surface *target);

#endif
