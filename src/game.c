#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <math.h>
#include <string.h>

#include <SDL.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#include "dv_controller.h"
#include "dv_image_cache.h"
#include "dv_raster_font.h"
#include "parser.h"

double clamp(double d, double min, double max) {
  const double t = d < min ? min : d;
  return t > max ? max : t;
}

static bool RUNNING = true;
static SDL_Window *WINDOW;
static SDL_Renderer *REND;
static SDL_Surface *SCREEN_SURFACE;
static SDL_Texture *SCREEN_TEXTURE;

static double WIN_VW = 640, WIN_VH = 360;
static double WIN_CW = 640, WIN_CH = 360;
static SDL_Rect ACTIVE_RECT = {0, 0, 640, 360};

void recalculate_screen_scale_and_position(void){
  double h_scale = (int)((double)WIN_CW / (double)WIN_VW);
  double v_scale = (int)((double)WIN_CH / (double)WIN_VH);
  double scale = (h_scale < v_scale) ? h_scale : v_scale;
  ACTIVE_RECT.w = (int)(scale * WIN_VW);
  ACTIVE_RECT.h = (int)(scale * WIN_VH);
  ACTIVE_RECT.x = (WIN_CW - ACTIVE_RECT.w)/2;
  ACTIVE_RECT.y = (WIN_CH - ACTIVE_RECT.h)/2;
}

void toggle_fullscreen(void){
  if(SDL_GetWindowFlags(WINDOW) & SDL_WINDOW_FULLSCREEN_DESKTOP){
  }else{
    SDL_SetWindowFullscreen(WINDOW, SDL_WINDOW_FULLSCREEN_DESKTOP);
  }
}

int32_t main_event_watch(void *data, SDL_Event *e){
  if(e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_RESIZED){
    WIN_CW = e->window.data1; WIN_CH = e->window.data2;
  }else if(e->type == SDL_QUIT){
    RUNNING = SDL_FALSE;
  }
  return 0;
}

void game_state_init(void);
void game_state_step(void);

int32_t main(void){
  SDL_Init(SDL_INIT_EVERYTHING);

  RUNNING = true;

  WIN_CW = WIN_VW*2;
  WIN_CH = WIN_VH*2;

  WINDOW = SDL_CreateWindow("game",
              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
              WIN_CW, WIN_CH, SDL_WINDOW_RESIZABLE);

  if(WINDOW == NULL){ printf("%s\n", SDL_GetError()); fflush(stdout); exit(1); }

  REND = SDL_CreateRenderer(WINDOW, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  SCREEN_SURFACE = create_surface(WIN_VW, WIN_VH);
  SCREEN_TEXTURE = SDL_CreateTexture(REND, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, WIN_VW, WIN_VH);

  controller_init();
  game_state_init();

  SDL_AddEventWatch(&main_event_watch, 0);
  double cms = 0, pms = 0, msd = 0, msa = 0, mspf = 10;
  while(RUNNING){
    pms = cms; cms = SDL_GetTicks(); msd = cms - pms; msa += msd;
    if(msa > mspf){ msa -= mspf;
      controller_read();
      game_state_step();

      SDL_UpdateTexture(SCREEN_TEXTURE, NULL, SCREEN_SURFACE->pixels, SCREEN_SURFACE->pitch);
      SDL_RenderClear(REND);
      SDL_RenderCopy(REND, SCREEN_TEXTURE, NULL, NULL);
      SDL_RenderPresent(REND);
    }
    fflush(stdout);
  }
  SDL_Quit();
  return 0;
}

static font_t *font_header;
static font_t *font_bright;
static font_t *font_dimmed;
static font_t *font_golden;
static font_t *font_invert;
static font_t *font_normal;

static SDL_Surface *background_blank;
static SDL_Surface *background_image;
static SDL_Surface *inset_image;
static SDL_Rect bg_dst_rect;    static SDL_Rect bg_src_rect;
static SDL_Rect inset_dst_rect; static SDL_Rect inset_src_rect;

static SDL_Surface *pointer_image;
static SDL_Rect pointer_rect;

static SDL_Rect left_sidebar_rect;
static SDL_Rect right_sidebar_rect;

static SDL_Surface *slide_buffer;
static SDL_Rect slide_position;
static int slide_alpha;

static double current_game_step;

#define NUMBER_OF_OPTIONS 6
#define MAX_OPT_INDEX 5

static int option_height;
static int selected_index;

static scene_t *current_scene = NULL;

void game_state_init(void){
  font_header = font_create("font_alkhemikal_gold.png");

  font_bright = font_create("font_mnemonika_16_bright.png");
  font_dimmed = font_create("font_mnemonika_16_dimmed.png");
  font_golden = font_create("font_mnemonika_16_golden.png");
  font_invert = font_create("font_mnemonika_16_invert.png");
  font_normal = font_create("font_mnemonika_16_normal.png");

  left_sidebar_rect.w = 0;
  right_sidebar_rect.w = 0;

  background_image = background_blank = get_image("art_blank.png");
  bg_dst_rect.w = WIN_VW - (left_sidebar_rect.w + right_sidebar_rect.w);
  bg_dst_rect.h = 360;
  bg_dst_rect.x = left_sidebar_rect.w;
  bg_dst_rect.y = 0;

  bg_src_rect.h = WIN_VH;
  bg_src_rect.w = WIN_VW - (left_sidebar_rect.w + right_sidebar_rect.w);
  bg_src_rect.y = (background_image->h - bg_src_rect.h)/2;
  bg_src_rect.x = (background_image->w - bg_src_rect.w)/2;

  inset_image = get_image("crys_cave_west_tunnel_lores.png");
  inset_dst_rect.w = inset_image->w;
  inset_dst_rect.h = inset_image->h;
  inset_dst_rect.x = WIN_VW - (inset_dst_rect.w+16+right_sidebar_rect.w);
  inset_dst_rect.y = WIN_VH - (inset_dst_rect.h+16);
  inset_image = NULL; // test

  pointer_image = get_image("option_pointer.png");
  pointer_rect.x =  0; pointer_rect.y =  0;
  pointer_rect.w = 16; pointer_rect.h = 16;

  option_height = font_get_height(font_normal)+2;

  slide_buffer = create_surface(WIN_VW, WIN_VH);
  slide_position.x = 0;
  slide_position.y = 0;
  slide_position.w = WIN_VW;
  slide_position.h = WIN_VH;

  ////////////////
  parse_file("content.txt");
  current_scene = get_scene_by_idstr("new_game");
  ///////////////
}

static FILE *content;

void game_state_step(void){
  if(controller_just_pressed(BTN_BACK)){ RUNNING = SDL_FALSE; }
  if(controller_just_pressed(BTN_RB)){
    SDL_SetWindowFullscreen(WINDOW, SDL_WINDOW_FULLSCREEN_DESKTOP); }
  if(controller_just_pressed(BTN_LB)){
    SDL_SetWindowFullscreen(WINDOW, SDL_FALSE);
    SDL_SetWindowSize(WINDOW, WIN_VW*2, WIN_VH*2); }

  if(controller_just_pressed(BTN_U)){ selected_index -= 1; }
  if(controller_just_pressed(BTN_D)){ selected_index += 1; }

  if(controller_just_pressed(BTN_START)){
    if(current_scene->options[selected_index].enabled){
      current_scene = get_scene_by_idstr(current_scene->options[selected_index].target);
      selected_index = 0;
      if(current_scene->bgimg != NULL){
        background_image = current_scene->bgimg;
      }else{
        background_image = background_blank;
      }
      slide_position.y = 0;
      slide_position.x = 0;
      slide_alpha = 255;
      SDL_BlitSurface(SCREEN_SURFACE, NULL, slide_buffer, NULL);
    }
  }

  if(controller_just_pressed(BTN_RS)){ current_scene = get_scene_by_idstr("new_game"); }

  selected_index = clamp(selected_index, 0, MAX_OPT_INDEX);

  SDL_BlitSurface(background_image, &bg_src_rect, SCREEN_SURFACE, &bg_dst_rect);

  current_game_step += 1;

  double y = 16 + bg_dst_rect.y;
  double x = 16 + bg_dst_rect.x;

  y += font_wrap_string(
        font_header,
        current_scene->title,
        x, y, bg_dst_rect.w - 360,
        SCREEN_SURFACE);

  y += font_wrap_string(
        font_normal,
        current_scene->prose,
        x, y, bg_dst_rect.w - 360,
        SCREEN_SURFACE);


  y = WIN_VH - (16 + (option_height * (NUMBER_OF_OPTIONS)));
  char opt_buffer[256];

  if(inset_image != NULL){
    SDL_BlitSurface(inset_image, NULL, SCREEN_SURFACE, &inset_dst_rect);
  }

  x -= pointer_rect.w/2;
  pointer_rect.x = x;
  x = pointer_rect.x + pointer_rect.w + 2;
  for(int opt_index=0; opt_index <= MAX_OPT_INDEX; opt_index++){
    option_t *opt = &current_scene->options[opt_index];
    if(opt->enabled){
      if(opt_index == selected_index ){
        font_draw_string(font_golden, opt->label, x, y, SCREEN_SURFACE);
      }else{
        font_draw_string(font_normal, opt->label, x, y, SCREEN_SURFACE);
      }
    }

    if(opt_index == selected_index ){
      pointer_rect.y = y-1;
    }

    y += option_height;
  }

  SDL_BlitSurface(pointer_image, NULL, SCREEN_SURFACE, &pointer_rect);

  if(slide_position.x < WIN_VW && slide_alpha > 0){
    SDL_SetSurfaceAlphaMod(slide_buffer, slide_alpha);
    //SDL_SetSurfaceColorMod(slide_buffer, slide_alpha, slide_alpha, slide_alpha);
    SDL_BlitSurface(slide_buffer, NULL, SCREEN_SURFACE, &slide_position);
    //slide_position.x += 0.03*WIN_VW;
    slide_alpha -= 20;
  }
}
