#ifndef DV_IMAGE_CACHE_H
#define DV_IMAGE_CACHE_H

#include <SDL.h>

SDL_Surface *create_surface(int32_t w, int32_t h);
SDL_Surface *get_image(const char *fn);

#endif
