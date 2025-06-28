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

#define INITIAL_WINDOW_W 960
#define INITIAL_WINDOW_H 720

#define VIRTUAL_SCREEN_W 320
#define VIRTUAL_SCREEN_H 240

#define PROSE_WRAP_W 300

static bool RUNNING = true;
static SDL_Window *WINDOW;
static SDL_Renderer *REND;
static SDL_Surface *SCREEN_SURFACE;
static SDL_Texture *SCREEN_TEXTURE;

int32_t main_event_watch(void *data, SDL_Event *e){
  if(e->type == SDL_QUIT){ RUNNING = SDL_FALSE; }
  return 0;
}

void game_draw_screen(void){
  SDL_UpdateTexture(SCREEN_TEXTURE, NULL, SCREEN_SURFACE->pixels, SCREEN_SURFACE->pitch);
  SDL_RenderClear(REND);
  SDL_RenderCopy(REND, SCREEN_TEXTURE, NULL, NULL);
  SDL_RenderPresent(REND);
}

void game_state_init(void);
void game_state_step(void);

int32_t main(void){
  SDL_Init(SDL_INIT_EVERYTHING);

  RUNNING = true;

  WINDOW = SDL_CreateWindow("game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, INITIAL_WINDOW_W, INITIAL_WINDOW_H, 0);
  if(WINDOW == NULL){ printf("%s\n", SDL_GetError()); fflush(stdout); exit(1); }

  REND = SDL_CreateRenderer(WINDOW, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  SCREEN_SURFACE = create_surface(VIRTUAL_SCREEN_W, VIRTUAL_SCREEN_H);
  SCREEN_TEXTURE = SDL_CreateTexture(REND, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, VIRTUAL_SCREEN_W, VIRTUAL_SCREEN_H);

  controller_init();
  game_state_init();

  SDL_AddEventWatch(&main_event_watch, 0);
  double cms = 0, pms = 0, msd = 0, msa = 0, mspf = 10;
  while(RUNNING){
    pms = cms; cms = SDL_GetTicks(); msd = cms - pms; msa += msd;
    if(msa > mspf){ msa -= mspf;
      controller_read();
      game_state_step();
      game_draw_screen();
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
char current_prose[MAX_PROSE_LENGTH];

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
static font_t *font_dimmed;
static font_t *font_active;
static font_t *font_normal;
static font_t *font_console;

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
  font_header = font_create("font_alkhemikal_15.png", 0xEECC33FF, 0x443322FF);
  font_dimmed = font_create("font_mnemonika_12.png",  0x666666FF, 0x333333FF);
  font_active = font_create("font_mnemonika_12.png",  0xEECC33FF, 0x443322FF);
  font_normal = font_create("font_mnemonika_12.png",  0xDDDDDDFF, 0x333333FF);
  
  font_console = font_create("font_nokia_10.png", 0x33FF33FF, 0x113311FF);

  background_image = background_blank = get_image("bg_blank.png");
  pointer_image = get_image("cursor_arrow.png");

  trans_buffer = create_surface(VIRTUAL_SCREEN_W, VIRTUAL_SCREEN_H);
  
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
  // Check for option selection
  if(controller_just_pressed(BTN_U)){ selected_option_index -= 1; if(selected_option_index<0){selected_option_index=MAX_OPT_INDEX;}}
  if(controller_just_pressed(BTN_D)){ selected_option_index += 1; if(selected_option_index>MAX_OPT_INDEX){selected_option_index=0;}}
  
  SDL_BlitSurface(background_image, NULL, SCREEN_SURFACE, NULL);

  current_game_step += 1;

  font_draw_string(font_header, current_title, 8, 8, SCREEN_SURFACE);
  
  font_wrap_string(font_normal, current_prose, 8, 32, PROSE_WRAP_W, SCREEN_SURFACE);

  for(int i=0; i < MAX_OPT_COUNT; i++){
    option_t *opt = &current_options[i];

    int y = 160+(i*font_get_height(font_normal));

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
      selected_font = font_active;
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

void scn_outpost_stairwell(void);

void scn_outpost_basement_hub(void);
void scn_outpost_basement_cryo(void);
void scn_outpost_basement_storage(void);
void scn_outpost_basement_reactor(void);

void scn_outpost_ground_floor_hub(void);
void scn_outpost_ground_floor_lounge(void);
void scn_outpost_ground_floor_quarters(void);
void scn_outpost_ground_floor_garage(void);

void scn_outpost_upper_floor_hub(void);
void scn_outpost_upper_floor_command(void);
void scn_outpost_upper_floor_compcore(void);
void scn_outpost_upper_floor_surveillance(void);

void scn_new_game(void){
  reset_scene();
  sprintf(current_title, "%s", "New Game");
  sprintf(current_prose, "%s", "This is the default new game landing prose. It needs to be long enough to wrap so I can evaluate font sizes. Like really quite long and including longer words like calibration or justification.");
  set_option(0,scn_outpost_basement_cryo,"Go to Cryostasis Bay");
}

void scn_outpost_stairwell(void){
  reset_scene();
  background_image = get_image("sn_bg_outpost_stairwell.png");
  sprintf(current_title, "%s", "Outpost Stairwell");
  set_option(0,scn_outpost_basement_hub,"Outpost Basement");
  set_option(1,scn_outpost_ground_floor_hub,"Outpost Main Floor");
  set_option(2,scn_outpost_upper_floor_hub,"Outpost Upper Floor");
}

void scn_outpost_basement_hub(void){
  reset_scene();
  background_image = get_image("sn_bg_outpost_basement_hub.png");
  sprintf(current_title, "%s", "Outpost Basement");

  set_option(0,scn_outpost_basement_cryo,"Cryostasis Bay");
  set_option(1,scn_outpost_basement_storage,"Storage Room");
  set_option(2,scn_outpost_basement_reactor,"Reactor Room");
  set_option(5,scn_outpost_stairwell,"Stairwell");
}

void scn_outpost_basement_cryo(void){
  reset_scene();
  background_image = get_image("bg-dr9-cryo-vault.png");
  sprintf(current_title, "%s", "Cryostasis Vault");
  sprintf(current_prose, "%s", "A single stasis pod dominates the room, "
                               "its glass fogged with condensation. "
                               "Dim lights flicker overhead, casting deep shadows. " 
                               "The machinery hums quietly, and frost clings to every surface."
                               "\n  \nAn amber warning light pulses near the pod's control panel." 
                               "\n  \nA hand-written note is taped to wall beside the door." );
  set_option(0,NULL,"Check the control panel.");
  set_option(1,NULL,"Read the hand-written note.");
  set_option(5,scn_outpost_basement_hub,"Exit the room.");
}

void scn_outpost_basement_storage(void){
  reset_scene();
  background_image = get_image("sn_bg_outpost_basement_storage.png");
  sprintf(current_title, "%s", "Storage Room");
  set_option(5,scn_outpost_basement_hub,"Exit the room.");
}
  
void scn_outpost_basement_reactor(void){
  reset_scene();
  background_image = get_image("sn_bg_outpost_basement_reactor.png");
  sprintf(current_title, "%s", "Reactor Room");
  set_option(5,scn_outpost_basement_hub,"Exit the room.");
}

void scn_outpost_ground_floor_hub(void){
  reset_scene();
  background_image = get_image("sn_bg_outpost_ground_floor_hub.png");
  sprintf(current_title, "%s", "Outpost Main Floor");

  set_option(0,scn_outpost_ground_floor_lounge,"Lounge");
  set_option(1,scn_outpost_ground_floor_garage,"Garage");
  set_option(2,scn_outpost_ground_floor_quarters,"Living Quarters");
  set_option(5,scn_outpost_stairwell,"Stairwell");
}

void scn_outpost_ground_floor_lounge(void){
  reset_scene();
  background_image = get_image("sn_bg_outpost_ground_floor_lounge.png");
  sprintf(current_title, "%s", "Lounge");
  set_option(5,scn_outpost_ground_floor_hub,"Exit the room.");
}
  
void scn_outpost_ground_floor_garage(void){
  reset_scene();
  background_image = get_image("sn_bg_outpost_ground_floor_garage.png");
  sprintf(current_title, "%s", "Garage");
  set_option(5,scn_outpost_ground_floor_hub,"Exit the room.");
}

void scn_outpost_ground_floor_quarters(void){
  reset_scene();
  background_image = get_image("sn_bg_outpost_ground_floor_quarters.png");
  sprintf(current_title, "%s", "Quarters");
  set_option(5,scn_outpost_ground_floor_hub,"Exit the room.");
}

void scn_outpost_upper_floor_hub(void){
  reset_scene();
  background_image = get_image("sn_bg_outpost_upper_floor_hub.png");
  sprintf(current_title, "%s", "Outpost Upper Floor");

  set_option(0,scn_outpost_upper_floor_command,"Command Center");
  set_option(1,scn_outpost_upper_floor_compcore,"Computer Core");
  set_option(2,scn_outpost_upper_floor_surveillance,"Surveillance");
  set_option(5,scn_outpost_stairwell,"Stairwell");
}

void scn_outpost_upper_floor_command(void){
  reset_scene();
  background_image = get_image("sn_bg_outpost_upper_floor_command.png");
  sprintf(current_title, "%s", "Command Center");
  set_option(5,scn_outpost_upper_floor_hub,"Exit the room.");
}

void scn_outpost_upper_floor_compcore(void){
  reset_scene();
  background_image = get_image("sn_bg_outpost_upper_floor_compcore.png");
  sprintf(current_title, "%s", "Computer Core");
  set_option(5,scn_outpost_upper_floor_hub,"Exit the room.");
}

void scn_outpost_upper_floor_surveillance(void){
  reset_scene();
  background_image = get_image("sn_bg_outpost_upper_floor_surveillance.png");
  sprintf(current_title, "%s", "Surveillance Suite");
  set_option(5,scn_outpost_upper_floor_hub,"Exit the room.");
}
