#pragma once

static struct { char *key; SDL_Surface *value; } *image_cache = NULL;

SDL_Surface *create_surface(int32_t w, int32_t h);
SDL_Surface *get_image(const char *fn);

SDL_Surface *create_surface(int32_t w, int32_t h){
  return SDL_CreateRGBSurface(0,w,h,32,
    0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);
}

SDL_Surface *get_image(const char *fn){
  SDL_Surface *image = shget(image_cache, fn);

  if(image == NULL){
    int32_t w, h, of;

    unsigned char *data = stbi_load(fn, &w, &h, &of, 4);

    if(data == NULL){
        //printf("ERROR: Image failed to load: %s\n", fn); fflush(stdout);
        return NULL;
    }

    SDL_Surface *tmp = SDL_CreateRGBSurfaceFrom((void*)data, w, h, 32, 4*w,0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);
    image = SDL_ConvertSurfaceFormat(tmp, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(tmp);
    stbi_image_free(data);

    shput(image_cache, fn, image);
  }

  return image;
}
