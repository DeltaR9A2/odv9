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

  char *revealed_by; // If set, node is hidden until this flag is true. Never hidden otherwise.
  char *unlocked_by; // If set, node is locked until this flag is true. Never locked otherwise.
  char *disabled_by; // If set, node is disabled when this flag is true. Always enabled otherwise.

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

////////////////////// THE WORLD TREE //////////////////////

static struct {
  node_t test_hall;
  node_t test_room;
  node_t test_item;
  node_t test_prop;
  node_t torn_paper;

  node_t odv9_s1;

  node_t odv9_b1;    // Basement Passage
  node_t odv9_b1_a;  // Storage
  node_t odv9_b1_b;  // Reactor
  node_t odv9_b1_c;  // Cryo Vault
  
  node_t odv9_f1;    // Ground Floor Passage
  node_t odv9_f1_a;  // Common Room
  node_t odv9_f1_b;  // Crew Quarters
  node_t odv9_f1_c;  // Maintenance Bay

  node_t odv9_f2;    // Command Deck Passage
  node_t odv9_f2_a;  // Command Center
  node_t odv9_f2_b;  // Computer Core
  node_t odv9_f2_c;  // Surveillance Suite

  node_t odv9_b1_c_door_note;
  node_t odv9_b1_c_pod_panel;
} wt;

void node_add_child(node_t *n, node_t *c){
  for(int i=0; i<6; i++){
    if(n->children[i] == c){ return; }
    else if(n->children[i] != NULL){ continue; }
    else{ 
      n->children[i] = c; 
      c->parent = n;
      return;
    }
  }
}

void populate_the_world_tree(void){
  wt.test_item = (struct node_t){
    .type = NODE_ITEM,
    .idstr = "ODV9-TEST-ITEM",
    .label = "a test item",
    .asopt = "Pick up the test item.",
  };

  wt.test_room = (struct node_t){
    .type = NODE_ROOM,
    .idstr = "ODV9-TEST-ROOM",
    .label = "the test room",
    .asopt = "Move to the test room.",
    .title = "Test Room",
    .prose = "This is the test room. It's completely unremarkable but somehow seems uniquely well suited to testing.",
    .bgimg = "bg-dv9-cryo-vault.png",
  };

  wt.test_prop = (struct node_t){
    .type = NODE_PROP,
    .idstr = "ODV9-TEST-PROP",
    .label = "a test prop",
    .asopt = "Look at the test prop.",
    .title = "Test Prop",
    .prose = "This is a test prop. It's the most boring thing you've ever seen.",
  };

  wt.test_hall = (struct node_t){
    .type = NODE_HALL,
    .idstr = "ODV9-TEST-HALL",
    .label = "the test hall",
    .asopt = "Move to the test hall.",
    .title = "Test Hall",
    .prose = "This is the test hall. It is a strange liminal space that makes you feel uneasy.",
    .bgimg = "bg-dv9-default.png",
  };

  wt.torn_paper = (struct node_t){
    .type = NODE_PROP,
    .idstr = "ODV9-TORN-PAPER",
    .label = "a torn sheet of paper",
    .asopt = "Look at the torn paper.",
    .title = "Torn Paper",
    .prose = "If anybody reads this, please tell my tortoise that I love him.",
  };
  
  node_add_child(&wt.test_room, &wt.test_prop);
  node_add_child(&wt.test_room, &wt.test_item);
  node_add_child(&wt.test_room, &wt.torn_paper);

  node_add_child(&wt.test_hall, &wt.test_room);
  node_add_child(&wt.test_hall, &wt.odv9_s1);
  
  // Stairwell
  wt.odv9_s1 = (struct node_t){
    .type = NODE_HALL,
    .idstr = "ODV9-S1",
    .label = "",
    .asopt = "Move to the stairwell.",
    .title = "Central Stairwell",
    .bgimg = "bg-blank.png",
    .unlocked_by = "ODV9-TEST-ITEM",
  };

  // Basement Passage
  wt.odv9_b1 = (struct node_t){
    .type = NODE_HALL,
    .idstr = "ODV9-B1",
    .label = "",
    .asopt = "Move to the basement.",
    .title = "Outpost Basement",
    .bgimg = "bg-blank.png",
  };
  
  // Ground Floor Passage
  wt.odv9_f1 = (struct node_t){
    .type = NODE_HALL,
    .idstr = "ODV9-F1",
    .label = "",
    .asopt = "Move to the ground floor.",
    .title = "Ground Floor",
    .bgimg = "bg-blank.png",
  };
  
  // Command Deck Passage
  wt.odv9_f2 = (struct node_t){
    .type = NODE_HALL,
    .idstr = "ODV9-F2",
    .label = "",
    .asopt = "Move to the command deck.",
    .title = "Command Deck",
    .bgimg = "bg-blank.png",
  };

  node_add_child(&wt.odv9_s1, &wt.odv9_b1);
  node_add_child(&wt.odv9_s1, &wt.odv9_f1);
  node_add_child(&wt.odv9_s1, &wt.odv9_f2);

  // Storage Room
  wt.odv9_b1_a = (struct node_t){
    .type = NODE_ROOM,
    .idstr = "ODV9-B1-A",
    .label = "",
    .asopt = "Go to the storage room.",
    .title = "Storage Room",
  };
  
  // Reactor Room
  wt.odv9_b1_b = (struct node_t){
    .type = NODE_ROOM,
    .idstr = "ODV9-B1-B",
    .label = "",
    .asopt = "Go to the reactor room.",
    .title = "Reactor Room",
  };
  
  // Cryo Vault
  wt.odv9_b1_c = (struct node_t){
    .type = NODE_ROOM,
    .idstr = "ODV9-B1-C",
    .label = "",
    .asopt = "Go to the cryo vault.",
    .title = "Cryo Vault",
    .bgimg = "bg-odv9-cryo-vault.png",
    .prose = "A single stasis pod dominates the room, its glass fogged with condensation. A warning light pulses on a control panel. A hand-written note is taped to the door.",
  };
  
  node_add_child(&wt.odv9_b1, &wt.odv9_b1_a);
  node_add_child(&wt.odv9_b1, &wt.odv9_b1_b);
  node_add_child(&wt.odv9_b1, &wt.odv9_b1_c);
  
  // Common Room
  wt.odv9_f1_a = (struct node_t){
    .type = NODE_ROOM,
    .idstr = "ODV9-F1-A",
    .label = "",
    .asopt = "Go to the common room.",
    .title = "Common Croom",
  };  // Common Room
  
  // Crew Quarters
  wt.odv9_f1_b = (struct node_t){
    .type = NODE_ROOM,
    .idstr = "ODV9-F1-B",
    .label = "",
    .asopt = "Go to the crew quarters.",
    .title = "Crew Quarters",
  };  // Crew Quarters
  
  // Maintenance Bay
  wt.odv9_f1_c = (struct node_t){
    .type = NODE_ROOM,
    .idstr = "ODV9-F1-C",
    .label = "",
    .asopt = "Go to the maintenance bay.",
    .title = "Maintenance Bay",
  };  // Maintenance Bay

  node_add_child(&wt.odv9_f1, &wt.odv9_f1_a);
  node_add_child(&wt.odv9_f1, &wt.odv9_f1_b);
  node_add_child(&wt.odv9_f1, &wt.odv9_f1_c);

  // Command Center
  wt.odv9_f2_a = (struct node_t){
    .type = NODE_ROOM,
    .idstr = "ODV9-F2-A",
    .label = "",
    .asopt = "Go to the command center.",
    .title = "Command Center",
  };
  
  // Computer Core
  wt.odv9_f2_b = (struct node_t){
    .type = NODE_ROOM,
    .idstr = "ODV9-F2-B",
    .label = "",
    .asopt = "Go to the computer core.",
    .title = "Computer Core",
  };

  // Surveillance Suite
  wt.odv9_f2_c = (struct node_t){
    .type = NODE_ROOM,
    .idstr = "ODV9-F2-C",
    .label = "",
    .asopt = "Go to the surveillance suite.",
    .title = "Surveillance Suite",
  };

  node_add_child(&wt.odv9_f2, &wt.odv9_f2_a);
  node_add_child(&wt.odv9_f2, &wt.odv9_f2_b);
  node_add_child(&wt.odv9_f2, &wt.odv9_f2_c);

  wt.odv9_b1_c_door_note = (struct node_t){
    .type = NODE_PROP,
    .idstr = "ODV0-B1-C-DOOR-NOTE",
    .label = "",
    .asopt = "Read the note on the door.",
    .title = "Hand-Written Note",
    .prose = "A hand-written note is taped to the wall beside the door. The paper is old and weathered. It reads:\n\nI won't remember writing this. The drugs are already working. I n-YOU- need to leave. They'll know I'm awake as soon as I'm out of the pod. It could take hours or days or weeks but they WILL find me. Get to the -/-///-/- observatory. It will still be there. It has to be.",
  };
  
  wt.odv9_b1_c_pod_panel = (struct node_t){
    .type = NODE_PROP,
    .idstr = "ODV9-B1-C-POD-PANEL",
    .label = "",
    .asopt = "Examine the control panel.",
    .title = "Cryopod Control Panel",
    .prose = "The control panel's diagnostics read nominal across the board. All systems are functioning and the most recent stasis cycle encountered no errors. A single warning light pulses next to a switch marked 'MAINTENANCE OVERRIDE'. The switch is badly damaged, leaving it in the 'ON' position permanently.",
  };
  node_add_child(&wt.odv9_b1_c, &wt.odv9_b1_c_pod_panel);
  node_add_child(&wt.odv9_b1_c, &wt.odv9_b1_c_door_note);
  
}
///////////////// THE STATE OF THE PLAYER //////////////////

struct {
  node_t *cur_node;
  struct { char *key; int value; } *tags;
} player;

void player_add_tag(const char *tag_name){        shput(player.tags, tag_name, 1); }
void player_del_tag(const char *tag_name){        shput(player.tags, tag_name, 0); }
int  player_has_tag(const char *tag_name){ return shget(player.tags, tag_name); }

///////////////// NODE TO SCENE CONVERSION /////////////////

void scene_from_node(scene_t *s, node_t *n){
  snprintf(s->super, sizeof(s->super), "%s", n->idstr);
  snprintf(s->title, sizeof(s->title), "%s", n->title);
  snprintf(s->prose, sizeof(s->prose), "%s", n->prose);

  for(int i=0; i < 5; i++){
    option_t *opt = &s->options[i];
    snprintf(opt->label, sizeof(opt->label), "%i) %s", i+1, "...");
    opt->target = NULL;
  }

  for(int i=0,oi=0; i < 5; i++){
    node_t *child = n->children[i];
    option_t *opt = &s->options[oi];
    
    if( (child == NULL) ||
        (child->revealed_by != NULL && !player_has_tag(child->revealed_by)) ||
        (child->disabled_by != NULL &&  player_has_tag(child->disabled_by)) ||
        (child->type == NODE_ITEM   &&  player_has_tag(child->idstr)) 
    ){ continue; }
    
    snprintf(opt->label, sizeof(opt->label), "%i) %s", oi+1, child->asopt);
    
    if( (child->unlocked_by == NULL) || player_has_tag(child->unlocked_by) ){
      opt->target = child;
    }
    
    oi++;
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

  SDL_Surface *screen_clear = get_image("bg-blank.png");
  SDL_Surface *pointer_image = get_image("cursor_arrow.png");
  SDL_Surface *trans_buffer = create_surface(320, 240);
  uint8_t trans_alpha = 0;
  
  populate_the_world_tree();
  
  scene_t current_scene = {
    .super = "ODV9-MAIN-MENU",
    .title = "Main Menu",
    .bgimg = get_image("bg-blank.png"),
    .prose = "The game has not yet begun.",
    .options = { { .label = "1) New Game",  .target=NULL },
                 { .label = "2) Load Game", .target=NULL },
                 { .label = "3) Options",   .target=NULL }, 
                 { .label = "4) ...",       .target=NULL }, 
                 { .label = "5) ...",       .target=NULL }, 
                 { .label = "6) Exit Game", .target=NULL } },
    .cursor_pos = 0
  };
  node_t *next_node = &wt.test_hall;
  
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
        if(next_node->type == NODE_ITEM){
          player_add_tag(next_node->idstr);
        }else{
          player.cur_node = next_node;
        }
        next_node = NULL;
        scene_from_node(&current_scene,player.cur_node);
        SDL_BlitSurface(SCREEN_SURFACE, NULL, trans_buffer, NULL);
        trans_alpha = 255;
      }

      if(current_scene.bgimg != NULL){
        SDL_BlitSurface(current_scene.bgimg, NULL, SCREEN_SURFACE, NULL);
      }else{
        SDL_BlitSurface(screen_clear, NULL, SCREEN_SURFACE, NULL);
      }

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
