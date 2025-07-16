#pragma once

#include "font_small_8_png.h"
#include "font_mnemonika_10_png.h"
#include "font_terminess_14_png.h"
#include "cursor_arrow_png.h"
#include "bg_odv9_pixel_frame_png.h"

static struct { char *key; SDL_Surface *value; } *image_cache = NULL;

SDL_Surface *create_surface(int32_t w, int32_t h);
SDL_Surface *get_image(const char *fn);

SDL_Surface *create_surface(int32_t w, int32_t h){
  return SDL_CreateRGBSurface(0,w,h,32,
    0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);
}

void cache_static_image(const unsigned char *static_img_data, unsigned int len, const char *fake_fn){
  SDL_Surface *image = shget(image_cache, fake_fn);
  if(image == NULL){
    int32_t w, h, of;
    unsigned char *data = stbi_load_from_memory(static_img_data, len, &w, &h, &of, 4);
    SDL_Surface *tmp = SDL_CreateRGBSurfaceFrom((void*)data, w, h, 32, 4*w,0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);
    image = SDL_ConvertSurfaceFormat(tmp, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(tmp);
    shput(image_cache, fake_fn, image);
  }
}

void ready_static_images(void){
  cache_static_image(cursor_arrow_png,         cursor_arrow_png_len,         "cursor-arrow.png");
  cache_static_image(font_small_8_png,         font_small_8_png_len,         "font-small-8.png");
  cache_static_image(font_mnemonika_10_png,    font_mnemonika_10_png_len,    "font-mnemonika-10.png");
  cache_static_image(font_terminess_14_png,    font_terminess_14_png_len,    "font-terminess-14.png");
  cache_static_image(bg_odv9_pixel_frame_png,  bg_odv9_pixel_frame_png_len,  "bg-odv9-pixel-frame.png");
}

SDL_Surface *get_image(const char *fn){
  SDL_Surface *image = shget(image_cache, fn);

  if(image == NULL && strlen(fn) > 0){
    printf("WARNING: Loading non-static image: %s \n", fn);
  }
  
  /*
  if(image == NULL){
    int32_t w, h, of;

    unsigned char *data = stbi_load(fn, &w, &h, &of, 4);

    if(data == NULL){
        printf("ERROR: Image failed to load: %s\n", fn); fflush(stdout);
        return NULL;
    }

    SDL_Surface *tmp = SDL_CreateRGBSurfaceFrom((void*)data, w, h, 32, 4*w,0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);
    image = SDL_ConvertSurfaceFormat(tmp, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(tmp);
    stbi_image_free(data);

    shput(image_cache, fn, image);
  } */

  return image;
}


