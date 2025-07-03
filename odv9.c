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

#include "image.h"
#include "font.h"
#include "input.h"

///////////////////// TYPE DEFINITIONS /////////////////////

// These just get the types to highlight.
typedef struct SDL_Rect SDL_Rect;
typedef struct SDL_Window SDL_Window;
typedef struct SDL_Surface SDL_Surface;
typedef struct SDL_Texture SDL_Texture;
typedef struct SDL_Renderer SDL_Renderer;

typedef enum{
  NODE_HALL, // A physical location with many exits
  NODE_ROOM, // A physical location with many objects
  NODE_ITEM, // Something for the player to pick up
  NODE_PROP  // Something for the player to look at
} node_type_t;

typedef struct node_t {
  node_type_t type; // the node's type determines how the player interacts with it
  struct node_t *parent;      // pointer to this node's parent
  struct node_t *children[5]; // up to five pointers to child nodes

  char idstr[64];  // the unique id of this node (like 'odv9-cutting-torch')
  char label[64];  // the node's name in a sentence (like 'a cutting torch')
  char asopt[64];  // node's label as option (like 'Pick up the Cutting Torch.')
  char title[64];  // title at the top of a scene view (like 'Storage Room')
  char prose[1024]; // full text shown in a scene view (like 'The room is lined with shelves... ')
  char bgimg[128];  // image file to display in a scene view (like 'storage-room.png')
  // char audio[128];  // audio file to loop in a scene view (like 'storage-room.mp3')

} node_t;

typedef struct option_t{
  char      label[128];
  node_t   *target;
} option_t;

typedef struct scene_t{
  char super[128];     // Tiny text at the top 
  char title[128];     // Large text near the top
  char prose[1024];    // The main body of text
  SDL_Surface *bgimg;  // The image displayed behind the text
  // sometype *audio;  // The sound sample currently looping
  option_t options[6]; // Up to six options displayed at the bottom
  int8_t  cursor_pos;  // Index of the option the player's cursor is on
} scene_t;

///////////////// NODE TO SCENE CONVERSION /////////////////

void scene_from_node(scene_t *s, node_t *n){
  snprintf(s->super, sizeof(s->super), "%s", n->idstr);
  snprintf(s->title, sizeof(s->title), "%s", n->title);
  snprintf(s->prose, sizeof(s->prose), "%s", n->prose);

  for(int i=0; i < 5; i++){
    if(n->children[i] != NULL){
      snprintf(s->options[i].label, sizeof(s->options[i].label), "%i) %s", i+1, n->children[i]->asopt);
      s->options[i].target = n->children[i];
    }else{
      snprintf(s->options[i].label, sizeof(s->options[i].label), "%i) %s", i+1, "...");
      s->options[i].target = NULL;
    }
  }
  
  if(n->parent != NULL){
    snprintf(s->options[5].label, sizeof(s->options[5].label), "%i) %s", 6, n->parent->asopt);
    s->options[5].target = n->parent;
  }else{
    snprintf(s->options[5].label, sizeof(s->options[5].label), "%i) %s", 6, "...");
    s->options[5].target = NULL;
  }
  
  s->cursor_pos = 0;

  if(n->type == NODE_HALL){
    s->bgimg = get_image(n->bgimg);
  }if(n->type == NODE_ROOM){
    s->bgimg = get_image(n->bgimg);
    snprintf(s->options[5].label, sizeof(s->options[5].label), "%i) %s", 6, "Exit this room.");
  }else if(n->type == NODE_PROP){
    snprintf(s->options[5].label, sizeof(s->options[5].label), "%i) %s", 6, "Return.");
    s->cursor_pos = 5;
  }else if(n->type == NODE_ITEM){
    snprintf(s->title,  128, "%s", "ERROR: Scene From Item");
    snprintf(s->prose, 1024, "%s", "An item node has been passed to the scene_from_node function but items cannot be viewed as scenes.");
    snprintf(s->options[5].label, sizeof(s->options[5].label), "%i) %s", 6, "Return.");
    s->cursor_pos = 5;
  }
}

////////////////////// THE WORLD TREE //////////////////////

static node_t test_hall;
static node_t test_room;
static node_t test_item;
static node_t test_prop;
static node_t torn_paper;

static node_t test_item = {
  .type = NODE_ITEM,
  .parent = &test_room,
  .idstr = "ODV9-TEST-ITEM",
  .label = "a test item",
  .asopt = "Pick up the test item.",
};

static node_t test_room = {
  .type = NODE_ROOM,
  .parent = &test_hall,
  .children = {&test_item,&test_prop,&torn_paper},
  .idstr = "ODV9-TEST-ROOM",
  .label = "the test room",
  .asopt = "Move to the test room.",
  .title = "Test Room",
  .prose = "This is the test room. It's completely unremarkable but somehow seems uniquely well suited to testing.",
  .bgimg = "bg-dv9-cryo-vault.png",
};

static node_t test_prop = {
  .type = NODE_PROP,
  .parent = &test_room,
  .idstr = "ODV9-TEST-PROP",
  .label = "a test prop",
  .asopt = "Look at the test prop.",
  .title = "Test Prop",
  .prose = "This is a test prop. It's the most boring thing you've ever seen.",
};

static node_t test_hall = {
  .type = NODE_HALL,
  .parent = NULL,
  .children = {&test_room,},
  .idstr = "ODV9-TEST-HALL",
  .label = "the test hall",
  .asopt = "Move to the test hall.",
  .title = "Test Hall",
  .prose = "This is the test hall. It is a strange liminal space that makes you feel uneasy.",
  .bgimg = "bg-dv9-default.png",
};

static node_t torn_paper = {
  .type = NODE_PROP,
  .parent = &test_room,
  .idstr = "ODV9-TORN-PAPER",
  .label = "a torn sheet of paper",
  .asopt = "Look at the torn paper.",
  .title = "Torn Paper",
  .prose = "If anybody reads this, please tell my tortoise that I love him.",
};

///////////////// THE STATE OF THE PLAYER //////////////////

struct {
  node_t *cur_node;
} player;

////////////////////// THE MAIN LOOP ///////////////////////

int32_t main(void){
  SDL_Init(SDL_INIT_EVERYTHING);
  controller_init();

  SDL_Window *WINDOW = SDL_CreateWindow("game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 720, 0);
  if(WINDOW == NULL){ printf("%s\n", SDL_GetError()); fflush(stdout); exit(1); }

  SDL_Renderer *REND = SDL_CreateRenderer(WINDOW, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  SDL_Surface *SCREEN_SURFACE = create_surface(320, 240);
  SDL_Texture *SCREEN_TEXTURE = SDL_CreateTexture(REND, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 320, 240);

  font_t *font_super = font_create("font_small_8.png", 0x999999FF, 0x333333FF);
  font_t *font_title = font_create("font_alkhemikal_15.png", 0xEECC33FF, 0x332211FF);
  font_t *font_prose = font_create("font_mnemonika_12.png", 0xEEEEEEFF, 0x222222FF);
  
  font_t *font_opt_normal = font_create("font_mnemonika_12.png", 0xEEEEEEFF, 0x222222FF);
  font_t *font_opt_dimmed = font_create("font_mnemonika_12.png", 0x666666FF, 0x333333FF);
  font_t *font_opt_select = font_create("font_mnemonika_12.png", 0xEECC33FF, 0x332211FF);

  SDL_Surface *pointer_image = get_image("cursor_arrow.png");
  SDL_Surface *trans_buffer = create_surface(320, 240);
  uint8_t trans_alpha = 0;
  
  scene_t current_scene = {
    .super = "ODV9-MAIN-MENU",
    .title = "Main Menu",
    .bgimg = get_image("bg-blank.png"),
    .prose = "The game has not yet begun.",
    .options = { { .label = "1) New Game",  .target=&test_hall },
                 { .label = "2) Load Game", .target=NULL },
                 { .label = "3) Options",   .target=NULL }, 
                 { .label = "4) ...",       .target=NULL }, 
                 { .label = "5) ...",       .target=NULL }, 
                 { .label = "6) Exit Game", .target=NULL } },
    .cursor_pos = 0
  };
  node_t *next_node = NULL;
  
  double cms = 0, pms = 0, msd = 0, msa = 0, mspf = 10;
  int RUNNING = 1;
  while(RUNNING){
    pms = cms; cms = SDL_GetTicks(); msd = cms - pms; msa += msd;
    if(msa > mspf){ msa -= mspf;
      controller_read();

      // Check for manual game exit.
      if(controller_just_pressed(BTN_BACK)){ RUNNING = 0; }
      // Check for cursor movement.
      if(controller_just_pressed(BTN_U)){ current_scene.cursor_pos -= 1; if(current_scene.cursor_pos < 0){ current_scene.cursor_pos = 5; } }
      if(controller_just_pressed(BTN_D)){ current_scene.cursor_pos += 1; if(current_scene.cursor_pos > 5){ current_scene.cursor_pos = 0; } }
      // Check for option activation.
      if(controller_just_pressed(BTN_START)){ 
        next_node = current_scene.options[current_scene.cursor_pos].target;
      }
      
      if(next_node != NULL && next_node != player.cur_node){
        SDL_BlitSurface(SCREEN_SURFACE, NULL, trans_buffer, NULL);
        trans_alpha = 255;
        player.cur_node = next_node; next_node = NULL;
        scene_from_node(&current_scene,player.cur_node);
      }

      SDL_BlitSurface(current_scene.bgimg, NULL, SCREEN_SURFACE, NULL);

      font_draw_string(font_super, current_scene.super, 10, 8, SCREEN_SURFACE);
      font_draw_string(font_title, current_scene.title, 10, 16, SCREEN_SURFACE);
      font_wrap_string(font_prose, current_scene.prose, 10, 32, 290, SCREEN_SURFACE);

      for(int i=0; i < 6; i++){
        option_t *opt = &current_scene.options[i];

        int y = 164+(i*font_get_height(font_opt_normal));

        if(opt->target == NULL){ 
          font_draw_string(font_opt_dimmed, opt->label, 14, y, SCREEN_SURFACE);
        }else if(i != current_scene.cursor_pos ){
          font_draw_string(font_opt_normal, opt->label, 14, y, SCREEN_SURFACE);
        }else{
          font_draw_string(font_opt_select, opt->label, 14, y, SCREEN_SURFACE);
        }
        
        if(i == current_scene.cursor_pos){
          SDL_BlitSurface(pointer_image, NULL, SCREEN_SURFACE, &(struct SDL_Rect){4,y+2,0,0});
        }
      }

      if(trans_alpha > 0){
        if(trans_alpha-10<0){
          trans_alpha = 0;
        }else{
          SDL_SetSurfaceAlphaMod(trans_buffer, trans_alpha);
          SDL_BlitSurface(trans_buffer, NULL, SCREEN_SURFACE, NULL);
          trans_alpha -= 10;
        }
      }
      
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
