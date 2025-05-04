#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <assert.h>
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

#define INITIAL_WINDOW_W 1280
#define INITIAL_WINDOW_H 720

#define VIRTUAL_SCREEN_W 640
#define VIRTUAL_SCREEN_H 360

#define PROSE_WRAP_W 260

static bool RUNNING = true;
static SDL_Window *WINDOW;
static SDL_Renderer *REND;
static SDL_Surface *SCREEN_SURFACE;
static SDL_Texture *SCREEN_TEXTURE;

static double WIN_VW = VIRTUAL_SCREEN_W, WIN_VH = VIRTUAL_SCREEN_H;
static double WIN_CW = INITIAL_WINDOW_W, WIN_CH = INITIAL_WINDOW_H;
static SDL_Rect ACTIVE_RECT = {0, 0, 0, 0};

void recalculate_screen_scale_and_position(void){
  double h_scale = (int)((double)WIN_CW / (double)WIN_VW);
  double v_scale = (int)((double)WIN_CH / (double)WIN_VH);
  double scale = (h_scale < v_scale) ? h_scale : v_scale;
  ACTIVE_RECT.w = (int)(scale * WIN_VW);
  ACTIVE_RECT.h = (int)(scale * WIN_VH);
  ACTIVE_RECT.x = (WIN_CW - ACTIVE_RECT.w)/2;
  ACTIVE_RECT.y = (WIN_CH - ACTIVE_RECT.h)/2;
}

int32_t main_event_watch(void *data, SDL_Event *e){
  if(e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_RESIZED){
    WIN_CW = e->window.data1; WIN_CH = e->window.data2;
    recalculate_screen_scale_and_position();
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

  WINDOW = SDL_CreateWindow("game",
              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
              WIN_CW, WIN_CH, SDL_WINDOW_RESIZABLE);
  if(WINDOW == NULL){ printf("%s\n", SDL_GetError()); fflush(stdout); exit(1); }

  REND = SDL_CreateRenderer(WINDOW, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  SCREEN_SURFACE = create_surface(WIN_VW, WIN_VH);
  SCREEN_TEXTURE = SDL_CreateTexture(REND, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, WIN_VW, WIN_VH);

  controller_init();
  game_state_init();

  recalculate_screen_scale_and_position();

  SDL_AddEventWatch(&main_event_watch, 0);
  double cms = 0, pms = 0, msd = 0, msa = 0, mspf = 10;
  while(RUNNING){
    pms = cms; cms = SDL_GetTicks(); msd = cms - pms; msa += msd;
    if(msa > mspf){ msa -= mspf;
      controller_read();
      game_state_step();

      SDL_UpdateTexture(SCREEN_TEXTURE, NULL, SCREEN_SURFACE->pixels, SCREEN_SURFACE->pitch);
      SDL_RenderClear(REND);
      SDL_RenderCopy(REND, SCREEN_TEXTURE, NULL, &ACTIVE_RECT);
      SDL_RenderPresent(REND);
    }
    fflush(stdout);
  }
  SDL_Quit();
  return 0;
}


#define MAX_TITLE_LENGTH 256
#define MAX_PROSE_LENGTH 1024

#define MAX_OPT_COUNT 6
#define MAX_OPT_INDEX 5
#define MAX_OPT_LABEL_LENGTH 128


char current_title[MAX_TITLE_LENGTH];
char current_prose[MAX_TITLE_LENGTH];

typedef void (*scene_t)(void);
static scene_t prev_scene; 
static scene_t this_scene; 
static scene_t next_scene; 

typedef struct{
  scene_t target;
  char label[MAX_OPT_LABEL_LENGTH];
} option_t;

static option_t current_options[MAX_OPT_COUNT];

static font_t *font_header;
static font_t *font_bright;
static font_t *font_dimmed;
static font_t *font_grayed;
static font_t *font_nogood;
static font_t *font_golden;
static font_t *font_invert;
static font_t *font_normal;

static SDL_Surface *background_blank;
static SDL_Surface *background_image;
static SDL_Surface *pointer_image;

static SDL_Surface *trans_buffer;
static SDL_Rect trans_position;
static int trans_alpha;

static double current_game_step;

static int selected_option_index;

void scn_new_game(void);

void game_state_init(void){
  font_header = font_create("font_alkhemikal_16_golden.png");

  font_bright = font_create("font_mnemonika_16_bright.png");
  font_dimmed = font_create("font_mnemonika_16_dimmed.png");
  font_grayed = font_create("font_mnemonika_16_grayed.png");
  font_nogood = font_create("font_mnemonika_16_nogood.png");
  font_golden = font_create("font_mnemonika_16_golden.png");
  font_invert = font_create("font_mnemonika_16_invert.png");
  font_normal = font_create("font_mnemonika_16_normal.png");

  background_image = background_blank = get_image("art_blank.png");
  pointer_image = get_image("cursor_arrow_small.png");

  trans_buffer = create_surface(WIN_VW, WIN_VH);
  
  prev_scene = NULL;
  next_scene = NULL;
  next_scene = scn_new_game;
}

void game_state_step(void){
  if( next_scene != NULL ){ 
    prev_scene = this_scene;
    this_scene = next_scene;
    next_scene = NULL;
    
    SDL_BlitSurface(SCREEN_SURFACE, NULL, trans_buffer, NULL);
    trans_alpha = 255;
    
    this_scene();
    
    return;
  } 
  
  // Check for manual game exit.
  if(controller_just_pressed(BTN_BACK)){ RUNNING = SDL_FALSE; }

  // Check for activate fullscreen mode.
  if(controller_just_pressed(BTN_RB)){
    SDL_SetWindowFullscreen(WINDOW, SDL_WINDOW_FULLSCREEN_DESKTOP); }
  // Check for disable fullscreen mode.
  if(controller_just_pressed(BTN_LB)){
    SDL_SetWindowFullscreen(WINDOW, SDL_FALSE);
    SDL_SetWindowSize(WINDOW, WIN_VW*2, WIN_VH*2); }
  // Check for option selection
  if(controller_just_pressed(BTN_U)){ selected_option_index -= 1; if(selected_option_index<0){selected_option_index=MAX_OPT_INDEX;}}
  if(controller_just_pressed(BTN_D)){ selected_option_index += 1; if(selected_option_index>MAX_OPT_INDEX){selected_option_index=0;}}
  
  SDL_BlitSurface(background_image, NULL, SCREEN_SURFACE, NULL);

  current_game_step += 1;

  font_draw_string(font_header, current_title, 16, 16, SCREEN_SURFACE);
  
  font_wrap_string(font_normal, current_prose, 16, 42, PROSE_WRAP_W, SCREEN_SURFACE);

  for(int i=0; i < MAX_OPT_COUNT; i++){
    option_t *opt = &current_options[i];

    int y = 250+(i*font_get_height(font_normal));

    if( 
        opt->target != NULL && 
        i == selected_option_index && 
        controller_just_pressed(BTN_START) 
    ){        
      next_scene = current_options[selected_option_index].target;
      selected_option_index = 0;
      break;
    }

    font_t *selected_font = font_normal;

    if(i == selected_option_index ){ 
      selected_font = font_golden;
      SDL_BlitSurface(pointer_image, NULL, SCREEN_SURFACE, &(struct SDL_Rect){8,y-1,0,0});
    }
    
    if(opt->target == NULL){ 
      selected_font = font_dimmed; 
    }
    
    font_draw_string(selected_font, opt->label, 24, y, SCREEN_SURFACE);
  }


  if(trans_alpha > 0){
    SDL_SetSurfaceAlphaMod(trans_buffer, trans_alpha);
    SDL_BlitSurface(trans_buffer, NULL, SCREEN_SURFACE, NULL);
    trans_alpha -= 10;
  }
}

void reset_scene(void){
  background_image = background_blank;
  sprintf(current_title, "%s", "??? ??? ???");
  sprintf(current_prose, "%s", "... ... ...");
  for(int i=0; i<MAX_OPT_COUNT; i++){
    current_options[i].target = NULL;
    sprintf(current_options[i].label, "%i) %s", i+1, "...");
  }
}

void set_option(int index, scene_t target, const char *label){
  sprintf(current_options[index].label, "%i) %s", index+1, label);
  current_options[index].target = target;
}

static int PLAYER_HAS_STICK = 0;

void scn_forest_one(void);
void scn_forest_two(void);
void scn_forest_three(void);
void scn_forest_four(void);
void act_pick_up_stick(void);
void act_put_down_stick(void);

void scn_new_game(void){
  reset_scene();
  background_image = get_image("sn_pixel_frame_background.png");
  sprintf(current_title, "%s", "New Game");
  sprintf(current_prose, "%s", "This is the default new game landing prose. It needs to be long enough to wrap so I can evaluate font sizes. Like really quite long and including longer words like calibration or justification.");
  set_option(0,scn_forest_one,"Go to The Forest 001");
}

void scn_forest_one(void){
  reset_scene();
  background_image = get_image("bg_forest.png");
  sprintf(current_title, "%s", "The Forest 001");
  sprintf(current_prose, "%s", "This is the placeholder prose for The Forest 001.");
  set_option(1,scn_forest_two,"Go to The Forest 002");
  if(!PLAYER_HAS_STICK){
    set_option(2,act_pick_up_stick,"Pick up a stick.");
  }else{
    set_option(2,act_put_down_stick,"Put down the stick.");
  }
  set_option(5,scn_new_game,"Return to New Game");
}

void scn_forest_two(void){
  reset_scene();
  background_image = get_image("bg_forest_wandering.png");
  sprintf(current_title, "%s", "The Forest 002");
  sprintf(current_prose, "%s", "This is the placeholder prose for The Forest 002.");
  set_option(2,scn_forest_three,"Go to The Forest 003");
  set_option(5,scn_new_game,"Return to New Game");
}

void scn_forest_three(void){
  static int times_visited = 0;
  times_visited += 1;
  reset_scene();
  background_image = get_image("bg_forest_wilderness.png");
  sprintf(current_title, "%s", "The Forest 003");
  sprintf(current_prose, "You have been here %i times. You %s carrying a stick.", times_visited, PLAYER_HAS_STICK?"are":"are not");
  set_option(5,scn_new_game,"Return to New Game");
}

void act_pick_up_stick(void){
  PLAYER_HAS_STICK = 1;
  next_scene = prev_scene;
}
void act_put_down_stick(void){
  PLAYER_HAS_STICK = 0;
  next_scene = prev_scene;
}
