#ifndef PARSER_H
#define PARSER_H
#include <stdint.h>
#include <stdio.h>
#include <SDL.h>

typedef struct{
  char target[128];
  char label[128];
  int enabled;
} option_t;

typedef struct{
  char title[128];
  char prose[2048];
  option_t options[6];
  SDL_Surface *bgimg;
} scene_t;


void parse_file(const char *fn);

scene_t *get_scene_by_idstr(const char *idstr);


#endif
