#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

#include "dv_raster_font.h"

#define INITIAL_WINDOW_W 960
#define INITIAL_WINDOW_H 720

#define VIRTUAL_SCREEN_W 320
#define VIRTUAL_SCREEN_H 240

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
        printf("ERROR: Image failed to load: %s\n", fn); fflush(stdout);
    }

    SDL_Surface *tmp = SDL_CreateRGBSurfaceFrom((void*)data, w, h, 32, 4*w,0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);
    image = SDL_ConvertSurfaceFormat(tmp, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(tmp);
    stbi_image_free(data);

    shput(image_cache, fn, image);
  }

  return image;
}

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

const char *glyph_order = " 1234567890-=`!@#$%^&*()_+~abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ[]\\;',./{}|:\"<>?";

font_t *font_create(const char *image_fn, uint32_t fg_color, uint32_t bg_color){
  font_t *font = malloc(sizeof(font_t));
  memset(font, 0, sizeof(font_t));

  SDL_Surface *load_img = get_image(image_fn);
  SDL_Surface *font_img = create_surface(load_img->w,load_img->h);
  SDL_BlitSurface(load_img,NULL,font_img,NULL);
  uint32_t *pixels = font_img->pixels;

  for(int i=0; i<(font_img->w*font_img->h); i+=1) { 
    if(pixels[i] == 0xFFFFFFFF){ pixels[i] = fg_color; } 
    if(pixels[i] == 0x000000FF){ pixels[i] = bg_color; } 
  }

  int32_t this_mark = 0;
  int32_t prev_mark = 0;

  uint32_t mark_color = pixels[0];

  uint32_t glyph_index = 0;
  SDL_Rect glyph_rect;
  SDL_Surface *glyph_surface;

  uint32_t glyph_count = strlen(glyph_order);

  int32_t kern_mark = 0;
  uint32_t kern_color = pixels[1];
  uint32_t kern_counter = 0;

  while(glyph_index < glyph_count){
    uint8_t ascii_code = (int)glyph_order[glyph_index];

    // Seek to next glyph
    while(pixels[this_mark] == mark_color){
      if(this_mark > font_img->w){ break; }
      this_mark += 1;
    }

    // Measure Head Kern
    kern_mark = this_mark;
    kern_counter = 0;
    while(pixels[kern_mark] == kern_color){
      kern_mark += 1;
      kern_counter += 1;
    }
    font->head_kerns[ascii_code] = kern_counter;

    // Measure Glyph
    prev_mark = this_mark;
    while(pixels[this_mark] != mark_color){
      if(this_mark > font_img->w){ break; }
      this_mark += 1;
    }

    // Measure Tail Kern
    kern_mark = this_mark - 1;
    kern_counter = 0;
    while(pixels[kern_mark] == kern_color){
      kern_mark -= 1;
      kern_counter += 1;
    }
    font->tail_kerns[ascii_code] = kern_counter;

    // Check if we have run out of glyphs early.
    if(this_mark > font_img->w){
      fprintf(stderr, "Warning: font_init: Font source shorter than glyph list.\n");
      break;
    }

    // Record glyph to its own surface
    glyph_rect.x = prev_mark;
    glyph_rect.w = this_mark - prev_mark;
    glyph_rect.y = 1;
    glyph_rect.h = font_img->h-1;

    glyph_surface = create_surface(glyph_rect.w, glyph_rect.h);
    SDL_BlitSurface(font_img, &glyph_rect, glyph_surface, NULL);

    font->glyphs[(int)glyph_order[glyph_index]] = glyph_surface;

    glyph_index += 1;
  }

  return font;
}

void font_delete(font_t *font){
  for(int32_t i=0; i<GLYPH_ARRAY_SIZE; i++){
    if(font->glyphs[i] != NULL){
      SDL_FreeSurface(font->glyphs[i]);
    }
  }

  free(font);
}

void font_draw_string(font_t *font, const char *string, int32_t x, int32_t y, SDL_Surface *target){
  if(string == NULL){ return; }
  SDL_Rect target_rect;
  target_rect.x = x;
  target_rect.y = y;
  for(uint32_t i=0; i<strlen(string); i++){
    uint8_t ascii_code = (int)string[i];

    if(font->glyphs[ascii_code] != NULL){
      target_rect.x -= font->head_kerns[ascii_code];
      SDL_BlitSurface(font->glyphs[ascii_code], NULL, target, &target_rect);
      target_rect.x += font->glyphs[ascii_code]->w;
      target_rect.x -= font->tail_kerns[ascii_code];
      target_rect.x += 1;
    }
  }
}


void font_draw_partial_string(font_t *font, const char *string, int32_t len, int32_t x, int32_t y, SDL_Surface *target){
  if(string == NULL){ return; }
  char temp[STRING_BUFFER_SIZE];

  if(len >= strlen(string)){
    font_draw_string(font, string, x, y, target);
  }else{
    strncpy(temp, string, len);
    temp[len] = '\0';
    font_draw_string(font, temp, x, y, target);
  }
}

int32_t font_get_width(font_t *font, const char *string){
  if(string == NULL){ return 0; }
  int32_t w = 0;
  for(uint32_t i=0; i<strlen(string); i++){
    uint8_t ascii_code = (int)string[i];
    if(font->glyphs[ascii_code] != NULL){
      w -= font->head_kerns[ascii_code];
      w += font->glyphs[ascii_code]->w;
      w -= font->tail_kerns[ascii_code];
      w += 1;
    }
  }
  return w;
}

int32_t font_get_height(font_t *font){
  return font->glyphs[(int)glyph_order[0]]->h;
}

int32_t font_wrap_string(font_t *font, const char *string, int32_t x, int32_t y, int32_t w, SDL_Surface *target){
  if(string == NULL){ return 0; }
  char temp[STRING_BUFFER_SIZE];
  int32_t h = font_get_height(font);
  int32_t line_start = 0;
  int32_t line_end = 0;
  bool wrap_now = false;

  int32_t total_height = 0;

  while(line_end <= strlen(string)){
    wrap_now = false;
    strncpy(temp, &(string[line_start]), line_end-line_start);
    temp[line_end-line_start] = '\0';

    if(string[line_end] == '\n'){
      wrap_now = true;
    }else if(font_get_width(font, temp) > w){
      int32_t temp_end = line_end;
      while(string[line_end] != ' '){
        line_end -= 1;
        if(line_end == 0){
          line_end = temp_end;
          break;
        }
      }
      wrap_now = true;
    }else if(line_end == strlen(string)){
      wrap_now = true;
    }

    if(wrap_now){
      font_draw_partial_string(font, &(string[line_start]), line_end-line_start, x, y, target);
      line_start = line_end;
      if(string[line_start] == ' '){ line_start += 1; }
      line_end = line_start + 1;
      y += h;
      total_height += h;
    }else{
      line_end += 1;
    }
  }
  return total_height;
}

int32_t font_wrap_partial_string(font_t *font, const char *string, int32_t len, int32_t x, int32_t y, int32_t w, SDL_Surface *target){
  if(string == NULL){ return 0; }
  char temp[STRING_BUFFER_SIZE];

  if(len >= strlen(string)){
    return font_wrap_string(font, string, x, y, w, target);
  }else{
    strncpy(temp, string, len);
    temp[len] = '\0';
    return font_wrap_string(font, temp, x, y, w, target);
  }
}

void font_draw_all_glyphs(font_t *font, int32_t x, int32_t y, SDL_Surface *target){
  const char *string = glyph_order;
  SDL_Rect target_rect;
  target_rect.x = x;
  target_rect.y = y;
  for(uint32_t i=0; i<strlen(string); i++){
    uint8_t ascii_code = (int)string[i];

    if(font->glyphs[ascii_code] != NULL){
      SDL_BlitSurface(font->glyphs[ascii_code], NULL, target, &target_rect);
      target_rect.x += font->glyphs[ascii_code]->w;
      target_rect.x += 4;
    }
  }
}

extern const uint32_t BTN_L;
extern const uint32_t BTN_R;
extern const uint32_t BTN_U;
extern const uint32_t BTN_D;
extern const uint32_t BTN_DIR_ANY;
extern const uint32_t BTN_A;
extern const uint32_t BTN_B;
extern const uint32_t BTN_X;
extern const uint32_t BTN_Y;
extern const uint32_t BTN_LB;
extern const uint32_t BTN_RB;
extern const uint32_t BTN_LT;
extern const uint32_t BTN_RT;
extern const uint32_t BTN_LS;
extern const uint32_t BTN_RS;
extern const uint32_t BTN_BACK;
extern const uint32_t BTN_START;
extern const uint32_t BTN_NONE;

typedef struct {
  uint32_t pressed;
  uint32_t previous;
} controller_t;

void controller_init(void);
void controller_reset(void);
bool controller_pressed(uint32_t buttons);
bool controller_released(uint32_t buttons);
bool controller_just_pressed(uint32_t buttons);
bool controller_just_released(uint32_t buttons);
void controller_read(void);

static controller_t CN;

static uint32_t KEYMAP_L =     SDL_SCANCODE_LEFT;
static uint32_t KEYMAP_R =     SDL_SCANCODE_RIGHT;
static uint32_t KEYMAP_U =     SDL_SCANCODE_UP;
static uint32_t KEYMAP_D =     SDL_SCANCODE_DOWN;

static uint32_t KEYMAP_A =     SDL_SCANCODE_Z;
static uint32_t KEYMAP_B =     SDL_SCANCODE_X;
static uint32_t KEYMAP_X =     SDL_SCANCODE_C;
static uint32_t KEYMAP_Y =     SDL_SCANCODE_V;

static uint32_t KEYMAP_LB =    SDL_SCANCODE_F1;
static uint32_t KEYMAP_RB =    SDL_SCANCODE_F2;
static uint32_t KEYMAP_LT =    SDL_SCANCODE_1;
static uint32_t KEYMAP_RT =    SDL_SCANCODE_2;

static uint32_t KEYMAP_LS =    SDL_SCANCODE_F3;
static uint32_t KEYMAP_RS =    SDL_SCANCODE_F4;
static uint32_t KEYMAP_BACK =  SDL_SCANCODE_BACKSPACE;
static uint32_t KEYMAP_START = SDL_SCANCODE_RETURN;

const uint32_t BTN_L =       0x00000001;
const uint32_t BTN_R =       0x00000002;
const uint32_t BTN_U =       0x00000004;
const uint32_t BTN_D =       0x00000008;
const uint32_t BTN_DIR_ANY = 0x0000000F;
const uint32_t BTN_A =       0x00000010;
const uint32_t BTN_B =       0x00000020;
const uint32_t BTN_X =       0x00000040;
const uint32_t BTN_Y =       0x00000080;
const uint32_t BTN_LB =       0x00000100;
const uint32_t BTN_RB =       0x00000200;
const uint32_t BTN_LT =       0x00000400;
const uint32_t BTN_RT =       0x00000800;
const uint32_t BTN_LS =       0x00001000;
const uint32_t BTN_RS =       0x00002000;
const uint32_t BTN_BACK =    0x00004000;
const uint32_t BTN_START =   0x00008000;
const uint32_t BTN_NONE =    0x00000000;

void controller_init(void){  CN.pressed = BTN_NONE; CN.previous = BTN_NONE; SDL_JoystickOpen(0); }
void controller_reset(void){ CN.pressed = BTN_NONE; CN.previous = BTN_NONE; }
bool controller_pressed(uint32_t buttons){ return ((CN.pressed & buttons) == buttons); }
bool controller_released(uint32_t buttons){  return !((CN.pressed & buttons) == buttons); }
bool controller_just_pressed(uint32_t buttons){ return ((CN.pressed & buttons) == buttons) && !((CN.previous & buttons) == buttons); }
bool controller_just_released(uint32_t buttons){ return !((CN.pressed & buttons) == buttons) && ((CN.previous & buttons) == buttons); }

void controller_read(void){
  SDL_Event e;

  CN.previous = CN.pressed;
  while(SDL_PollEvent(&e)){
    if(e.type == SDL_KEYDOWN){
      uint32_t key = e.key.keysym.scancode;
            if(key == KEYMAP_U){  CN.pressed |= BTN_U;
      }else if(key == KEYMAP_D){  CN.pressed |= BTN_D;
      }else if(key == KEYMAP_L){  CN.pressed |= BTN_L;
      }else if(key == KEYMAP_R){  CN.pressed |= BTN_R;
      }else if(key == KEYMAP_A){  CN.pressed |= BTN_A;
      }else if(key == KEYMAP_B){  CN.pressed |= BTN_B;
      }else if(key == KEYMAP_X){  CN.pressed |= BTN_X;
      }else if(key == KEYMAP_Y){  CN.pressed |= BTN_Y;
      }else if(key == KEYMAP_LB){ CN.pressed |= BTN_LB;
      }else if(key == KEYMAP_RB){ CN.pressed |= BTN_RB;
      }else if(key == KEYMAP_LT){ CN.pressed |= BTN_LT;
      }else if(key == KEYMAP_RT){ CN.pressed |= BTN_RT;
      }else if(key == KEYMAP_LS){ CN.pressed |= BTN_LS;
      }else if(key == KEYMAP_RS){ CN.pressed |= BTN_RS;
      }else if(key == KEYMAP_START){ CN.pressed |= BTN_START;
      }else if(key == KEYMAP_BACK){  CN.pressed |= BTN_BACK; }
    }else if(e.type == SDL_KEYUP){
      uint32_t key = e.key.keysym.scancode;
            if(key == KEYMAP_U){ CN.pressed &= ~BTN_U;
      }else if(key == KEYMAP_D){ CN.pressed &= ~BTN_D;
      }else if(key == KEYMAP_L){ CN.pressed &= ~BTN_L;
      }else if(key == KEYMAP_R){ CN.pressed &= ~BTN_R;
      }else if(key == KEYMAP_A){ CN.pressed &= ~BTN_A;
      }else if(key == KEYMAP_B){ CN.pressed &= ~BTN_B;
      }else if(key == KEYMAP_X){ CN.pressed &= ~BTN_X;
      }else if(key == KEYMAP_Y){ CN.pressed &= ~BTN_Y;
      }else if(key == KEYMAP_LB){ CN.pressed &= ~BTN_LB;
      }else if(key == KEYMAP_RB){ CN.pressed &= ~BTN_RB;
      }else if(key == KEYMAP_LT){ CN.pressed &= ~BTN_LT;
      }else if(key == KEYMAP_RT){ CN.pressed &= ~BTN_RT;
      }else if(key == KEYMAP_LS){ CN.pressed &= ~BTN_LS;
      }else if(key == KEYMAP_RS){ CN.pressed &= ~BTN_RS;
      }else if(key == KEYMAP_START){ CN.pressed &= ~BTN_START;
      }else if(key == KEYMAP_BACK){ CN.pressed &= ~BTN_BACK; }
    }else if(e.type == SDL_JOYHATMOTION){
      CN.pressed &= ~BTN_DIR_ANY;
      if(e.jhat.value == SDL_HAT_UP){ CN.pressed |= BTN_U;
      }else if(e.jhat.value == SDL_HAT_LEFT){      CN.pressed |= BTN_L;
      }else if(e.jhat.value == SDL_HAT_RIGHT){     CN.pressed |= BTN_R;
      }else if(e.jhat.value == SDL_HAT_DOWN){      CN.pressed |= BTN_D;
      }else if(e.jhat.value == SDL_HAT_LEFTUP){    CN.pressed |= BTN_L | BTN_U;
      }else if(e.jhat.value == SDL_HAT_RIGHTUP){   CN.pressed |= BTN_R | BTN_U;
      }else if(e.jhat.value == SDL_HAT_LEFTDOWN){  CN.pressed |= BTN_L | BTN_D;
      }else if(e.jhat.value == SDL_HAT_RIGHTDOWN){ CN.pressed |= BTN_R | BTN_D; }
    }else if(e.type == SDL_JOYBUTTONDOWN){
            if(e.jbutton.button == 0){ CN.pressed |= BTN_A;
      }else if(e.jbutton.button == 1){ CN.pressed |= BTN_B;
      }else if(e.jbutton.button == 2){ CN.pressed |= BTN_X;
      }else if(e.jbutton.button == 3){ CN.pressed |= BTN_Y;
      }else if(e.jbutton.button == 4){ CN.pressed |= BTN_LB;
      }else if(e.jbutton.button == 5){ CN.pressed |= BTN_RB;
      }else if(e.jbutton.button == 6){ CN.pressed |= BTN_BACK;
      }else if(e.jbutton.button == 7){ CN.pressed |= BTN_START; }
    }else if(e.type == SDL_JOYBUTTONUP){
            if(e.jbutton.button == 0){ CN.pressed &= ~BTN_A;
      }else if(e.jbutton.button == 1){ CN.pressed &= ~BTN_B;
      }else if(e.jbutton.button == 2){ CN.pressed &= ~BTN_X;
      }else if(e.jbutton.button == 3){ CN.pressed &= ~BTN_Y;
      }else if(e.jbutton.button == 4){ CN.pressed &= ~BTN_LB;
      }else if(e.jbutton.button == 5){ CN.pressed &= ~BTN_RB;
      }else if(e.jbutton.button == 6){ CN.pressed &= ~BTN_BACK;
      }else if(e.jbutton.button == 7){ CN.pressed &= ~BTN_START; }
    }
  }
}

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
  next_scene = scn_new_game;
}

void game_state_step(void){
  if( next_scene != NULL ){ 
    prev_scene = this_scene;
    this_scene = next_scene;
    next_scene = NULL;

    selected_option_index = 0;
    
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
  if(controller_just_pressed(BTN_START)){ next_scene = current_options[selected_option_index].target; }

  SDL_BlitSurface(background_image, NULL, SCREEN_SURFACE, NULL);

  current_game_step += 1;

  font_draw_string(font_header, current_title, 10, 8, SCREEN_SURFACE);
  
  font_wrap_string(font_normal, current_prose, 10, 32, 290, SCREEN_SURFACE);

  for(int i=0; i < MAX_OPT_COUNT; i++){
    option_t *opt = &current_options[i];

    int y = 164+(i*font_get_height(font_normal));

    if(opt->target == NULL){ 
      font_draw_string(font_dimmed, opt->label, 14, y, SCREEN_SURFACE);
    }else if(i != selected_option_index ){
      font_draw_string(font_normal, opt->label, 14, y, SCREEN_SURFACE);
    }else{
      font_draw_string(font_active, opt->label, 14, y, SCREEN_SURFACE);
    }
    
    if(i == selected_option_index){
      SDL_BlitSurface(pointer_image, NULL, SCREEN_SURFACE, &(struct SDL_Rect){4,y+2,0,0});
    }
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

void rbn_cryo_control_panel(void);
void rbn_cryo_vault_note(void);

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
                               "its glass fogged with condensation. Dim "
                               "lights flicker overhead, casting deep shadows. "
                               "The machinery hums quietly, and frost clings to every surface."
                               "\n\nA warning light pulses on a control panel."
                               "\n\nA hand-written note is taped to the door." );
  set_option(0,rbn_cryo_control_panel,"Check the control panel.");
  set_option(1,rbn_cryo_vault_note,"Read the hand-written note.");
  set_option(5,scn_outpost_basement_hub,"Exit the room.");
}

void rbn_cryo_control_panel(void){
  reset_scene();
  background_image = get_image("bg-dr9-cryo-vault.png");
  sprintf(current_title, "%s", "Cryo Pod Control Panel");
  sprintf(current_prose, "%s", "The control panel's diagnostics read nominal "
                               "across the board. All systems are functioning and "
                               "the most recent stasis cycle encountered no errors. " 
                               "A single warning light pulses next to a switch "
                               "marked 'MAINTENANCE OVERRIDE'. The switch is badly "
                               "damaged, leaving it in the 'ON' position permanently. " );
  set_option(0,prev_scene,"Return.");
}

void rbn_cryo_vault_note(void){
  reset_scene();
  background_image = get_image("bg-dr9-cryo-vault.png");
  sprintf(current_title, "%s", "Cryo Vault Note");
  sprintf(current_prose, "%s", "A hand-written note is taped to the wall beside "
                               "the door. The paper is old and weathered. It reads: "
                               "\n  \n  " 
                               "I won't remember writing this. The drugs are already "
                               "working. I n-YOU- need to leave. They'll know I'm "
                               "awake as soon as I'm out of the pod. It could take "
                               "hours or days or weeks but they WILL find me. Get to "
                               "the -/-///-/- observatory. It will still be "
                               "there. It has to be." );
  set_option(0,prev_scene,"Return.");
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
  sprintf(current_title, "%s", "Crew Quarters");
  sprintf(current_prose, "%s", "DEV put cryo drugs here ");
  set_option(4,NULL,"(look at the cryo drugs)");
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
