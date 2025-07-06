#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
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

#define STR_SIZE_S 64
#define STR_SIZE_M 128
#define STR_SIZE_L 1024

#define MAX_OPTIONS 6
#define MAX_CHILDREN 5

#define VIRTUAL_SCREEN_SIZE 320,240
#define INITIAL_WINDOW_SIZE 960,720

///////////////////// TYPE DEFINITIONS /////////////////////

#define TAG_LIST(X)            \
  X(TAG_NONE)                  \
  /* Test Content */           \
  X(TEST_HALL)                 \
  X(TEST_ROOM)                 \
  X(TEST_ITEM)                 \
  X(TEST_PROP)                 \
  X(TORN_PAPER)                \
  /* ODV9 Structure */         \
  X(ODV9_S1)                   \
  X(ODV9_B1)                   \
  X(ODV9_B1_A)                 \
  X(ODV9_B1_B)                 \
  X(ODV9_B1_C)                 \
  X(ODV9_F1)                   \
  X(ODV9_F1_A)                 \
  X(ODV9_F1_B)                 \
  X(ODV9_F1_C)                 \
  X(ODV9_F2)                   \
  X(ODV9_F2_A)                 \
  X(ODV9_F2_B)                 \
  X(ODV9_F2_C)                 \
  /* ODV9 Props */             \
  X(ODV9_B1_C_DOOR_NOTE)       \
  X(ODV9_B1_C_POD_PANEL)       \
  /* ODV9 Critical Flags */    \
  X(ODV9_CUTTING_TORCH)        \
  X(ODV9_CUT_STAIRWELL_DOOR)   \
  X(ODV9_ID_CARD)              \
  X(ODV9_UNLOCK_COMMAND_DECK)  \
  X(ODV9_PRYBAR)               \
  X(ODV9_PRY_OPEN_CRATE)       \
  X(ODV9_MCP_SUIT)             \
  X(ODV9_REFUEL_REACTOR)       \
  X(ODV9_FUEL_CELL)            \
  X(ODV9_REACTOR_CODES)        \
  X(ODV9_RESTART_REACTOR)      \
  X(ODV9_REBOOT_COMPUTER)      \
  X(ODV9_NAVIGATION_DATA)      \
  X(ODV9_REFUEL_CRAWLER)       \
  X(ODV9_UPLOAD_NAV_DATA)      \
  X(ODV9_ESCAPE_THE_OUTPOST)   \


#define X(name) name,
typedef enum { TAG_LIST(X) TAG_COUNT } tag_t;
#undef X

#define X(name) #name,
const char *tag_names[] = { TAG_LIST(X) };
#undef X

typedef enum{
  NT_NONE, // A node that is not used for anything
  NT_HALL, // A physical location with many exits
  NT_ROOM, // A physical location with many objects
  NT_PROP, // Something for the player to look at
  NT_ITEM, // Something for the player to pick up
  // Aliases for things that might have special logic
  NT_CASE = NT_PROP, // Something that contains items
  NT_FLAG = NT_ITEM, // Item that is abstract

} node_type_t;

typedef struct node_t {
  tag_t tag;
  node_type_t type; // the node's type determines how the player interacts with it
  struct node_t *parent;      // pointer to this node's parent
  struct node_t *children[MAX_CHILDREN]; // up to five pointers to child nodes

  char idstr[STR_SIZE_S];  // the unique id of this node (like 'ODV9-B1-C')
  char asopt[STR_SIZE_S];  // node's label as option (like 'Pick up the Cutting Torch.')
  char title[STR_SIZE_S];  // title at the top of a scene view (like 'Storage Room')
  char prose[STR_SIZE_L];  // full text shown in a scene view (like 'The room is lined with shelves... ')
  char bgimg[STR_SIZE_M];  // image file to display in a scene view (like 'storage-room.png')
  // char audio[STR_SIZE_M];  // audio file to loop in a scene view (like 'storage-room.mp3')

  tag_t revealed_by; // If set, node is hidden while this flag is false.
  tag_t unlocked_by; // If set, node is locked while this flag is false.
  tag_t rehidden_by; // If set, node is hidden while this flag is true.
  // rehidden overrides revealed

} node_t;

typedef struct option_t{
  char      label[STR_SIZE_M];
  node_t   *target;
} option_t;

typedef struct scene_t{
  char super[STR_SIZE_S];     // Tiny text at the top 
  char title[STR_SIZE_S];     // Large text near the top
  char prose[STR_SIZE_L];    // The main body of text
  SDL_Surface *bgimg;  // The image displayed behind the text
  // sometype *audio;  // The sound sample currently looping
  option_t options[MAX_OPTIONS]; // The options displayed at the bottom
  int8_t  cursor_pos;  // Index of the option the player's cursor is on
} scene_t;

////////////////////// THE WORLD TREE //////////////////////

static int8_t  tags[TAG_COUNT];
static node_t  nodes_by_tag[TAG_COUNT];
static node_t *nbt = nodes_by_tag;

void node_add_child(node_t *n, node_t *c){
  for(int i=0; i<MAX_CHILDREN; i++){
    if(n->children[i] == c){ return; }
    else if(n->children[i] != NULL){ continue; }
    else{ 
      n->children[i] = c; 
      c->parent = n;
      return;
    }
  }
}

void idupper(char *dst, const char *src){ for(size_t i=0;i<STR_SIZE_S;i++){ if(src[i]=='\0'){ dst[i]='\0'; break; }else if(src[i]=='_' ){ dst[i]='-'; }else{ dst[i]=toupper(src[i]); } } }

void node_init(tag_t t, node_type_t type){
  node_t *n = &nbt[t];
  n->tag=t;
  n->type=type;
  if(n->type == NT_ITEM){ n->rehidden_by = t; }
  idupper(n->idstr,tag_names[t]);
}

void node_link(tag_t n, tag_t p, const char *asopt){ 
  node_add_child(&nbt[p],&nbt[n]);
  sprintf(nbt[n].asopt,"%s",asopt);
}

void node_desc(tag_t t, const char *title, const char *bgimg, const char *prose){
  node_t *n = &nbt[t];
  sprintf(n->title,"%s",title);
  sprintf(n->prose,"%s",prose);
  sprintf(n->bgimg,"%s",bgimg);
}

void node_revealed_by(tag_t t, tag_t key){ nbt[t].revealed_by = key; }
void node_unlocked_by(tag_t t, tag_t key){ nbt[t].unlocked_by = key; }
void node_rehidden_by(tag_t t, tag_t key){ nbt[t].rehidden_by = key; }

void populate_the_world_tree(void){
  node_init( TAG_NONE, NT_NONE );
  
  node_init( TEST_HALL, NT_HALL );
  node_link( TEST_HALL, TAG_NONE, "Move to the test hall." );
  node_desc( TEST_HALL, "Test Hall", "", "This is the test hall. It is a strange liminal space that makes you feel uneasy.");

  node_init( TEST_ITEM, NT_ITEM );
  node_link( TEST_ITEM, TEST_ROOM, "Pick up the test item." );

  node_init( TEST_ROOM, NT_ROOM );
  node_link( TEST_ROOM, TEST_HALL, "Move to the test room." );
  node_desc( TEST_ROOM, "Test Room", "", "This is the test room. It's completely unremarkable but somehow seems uniquely well suited to testing.");
  
  node_init( TEST_PROP, NT_PROP );
  node_link( TEST_PROP, TEST_ROOM, "Look at the test prop." );
  node_desc( TEST_PROP, "Test Prop", "", "This is a test prop. It's the most boring thing you've ever seen.");

  node_init( TORN_PAPER, NT_PROP );
  node_link( TORN_PAPER, TEST_ROOM, "Look at the torn paper." );
  node_desc( TORN_PAPER, "Torn Paper", "", "If anybody reads this, please tell my tortoise that I love him.");
  
  // Stairwell
  node_init( ODV9_S1, NT_HALL );
  node_link( ODV9_S1, TAG_NONE, "Move to the stairwell." );
  node_desc( ODV9_S1, "Central Stairwell", "", "This cramped stairwell connects to three floors. The lowest door shows signs of scorching along the seams. The highest door says 'ACCESS RESTRICTED' and has an electronic lock with card reader. The middle door is unlocked and has an 'EXIT' sign above it." );
  nbt[ODV9_S1].unlocked_by = ODV9_CUT_STAIRWELL_DOOR;

  // Basement Passage
  node_init( ODV9_B1, NT_HALL );
  node_link( ODV9_B1, ODV9_S1, "Move to the basement.");
  node_desc( ODV9_B1, "Outpost Basement", "", "The air of this dimly lit corridor is cold and stale. Pipes and conduits obscure the ceiling overhead, and every sound echoes off the bare concrete of the floor and walls. Three doors have spray-painted stencil lettering; 'STORAGE', 'REACTOR', and 'CRYO'. A fourth door with an 'EXIT' sign shows visible scorching along the seams.");
  
  // Ground Floor Passage
  node_init( ODV9_F1, NT_HALL );
  node_link( ODV9_F1, ODV9_S1, "Move to the ground floor." );
  node_desc( ODV9_F1, "Ground Floor", "", "This traffic-worn hallway has four doors. Block lettering on three read 'COMMON', 'QUARTERS', and 'STAIRS'. A fourth door is larger, rimed with thick frost, and has an 'EXIT' sign above it." );
  
  // Command Deck Passage
  node_init( ODV9_F2, NT_HALL );
  node_link( ODV9_F2, ODV9_S1, "Move to the command deck." );
  node_desc( ODV9_F2, "Command Deck", "", "This narrow passage is cleaner than the rest of the outpost as if rarely used. There is an 'EXIT' sign above the stairwell door, and three other doors are marked 'COMMAND', 'COMPCORE', and 'MONITOR'." );
  nbt[ODV9_F2].unlocked_by = ODV9_ID_CARD;
  
  // Storage Room
  node_init( ODV9_B1_A, NT_ROOM );
  node_link( ODV9_B1_A, ODV9_B1, "Go to the storage room." );
  node_desc( ODV9_B1_A, "Storage Room", "", "This crowded storage room is lined with floor-to-ceiling racks full of boxes and crates. Decades worth of supplies and replacement parts. There must be something useful in all this." );
  
  // Reactor Room
  node_init( ODV9_B1_B, NT_ROOM );
  node_link( ODV9_B1_B, ODV9_B1, "Go to the reactor room." );
  node_desc( ODV9_B1_B, "Reactor Room", "", "A hulking fusion reactor occupies one half of this room. It looks nearly pristine, but requires specialized fuel cells to operate. The other half of the room has a long workbench covered in a mess of tools and parts." );
  
  // Cryo Vault
  node_init(ODV9_B1_C, NT_ROOM );
  node_link(ODV9_B1_C, ODV9_B1, "Go to the cryo vault." );
  node_desc(ODV9_B1_C, "Cryo Vault", "", "An empty stasis pod dominates the room, its life support systems still softly clicking and humming. A warning light pulses on a control panel and a hand-written note is taped to the door." );
  
  // Common Room
  node_init( ODV9_F1_A, NT_ROOM );
  node_link( ODV9_F1_A, ODV9_F1, "Go to the common room." );
  node_desc( ODV9_F1_A, "Common Room", "", "With a central round table, wall mounted entertainment center, and a corner kitchenette, this common room is surprisingly comfortable despite its limited size." );
  
  // Crew Quarters
  node_init( ODV9_F1_B, NT_ROOM );
  node_link( ODV9_F1_B, ODV9_F1, "Go to the crew quarters." );
  node_desc( ODV9_F1_B, "Crew Quarters", "", "This room is quiet and slightly warmer than the rest of the outpost. It has bunks and lockers for six people, and an attached bathroom the size of a closet." );
  
  // Maintenance Bay
  node_init( ODV9_F1_C, NT_ROOM );
  node_link( ODV9_F1_C, ODV9_F1, "Go to the maintenance bay." );
  node_desc( ODV9_F1_C, "Maintenance Bay", "", "The huge bay door is frozen wide open, leaving this space exposed to arctic conditions. A massive half-tracked vehicle is parked just inside the bay, beside a large rack of nuclear fuel cells." );
  nbt[ODV9_F1_C].unlocked_by = ODV9_MCP_SUIT;

  // Command Center
  node_init( ODV9_F2_A, NT_ROOM );
  node_link( ODV9_F2_A, ODV9_F2, "Go to the command center." );
  node_desc( ODV9_F2_A, "Command Center", "", "Huge windows with inches-thick glass give a spectacular view of snow covered mountains. There are several workstations; none seem functional, and the communications console been smashed to pieces." );
  
  // Surveillance Suite
  node_init( ODV9_F2_B, NT_ROOM );
  node_link( ODV9_F2_B, ODV9_F2, "Go to the surveillance suite." );
  node_desc( ODV9_F2_B, "Surveillance Suite", "", "This room feels out of place in the outpost; the displays and instruments have a sleek militaristic quality that seems slightly sinister. A single chair is surrounded by displays and control panels like the cockpit of some kind of aircraft." );
  
  // Computer Core
  node_init( ODV9_F2_C, NT_ROOM );
  node_link( ODV9_F2_C, ODV9_F2, "Go to the computer core." );
  node_desc( ODV9_F2_C, "Computer Core", "bg-odv9-computer-core.png", "This claustrophobic room is crammed with more server racks than seems reasonable for this outpost. They must require a massive amount of electricity to operate. There is a single workstation for direct access." );

  node_init( ODV9_B1_C_DOOR_NOTE, NT_PROP );
  node_link( ODV9_B1_C_DOOR_NOTE, ODV9_B1_C, "Read the note on the door." );
  node_desc( ODV9_B1_C_DOOR_NOTE, "Hand-Written Note", "", "A hand-written note is taped to the wall beside the door. The paper is old and weathered. It reads:\n\nI won't remember writing this. The drugs are already working. I n-YOU- need to leave. They'll know I'm awake as soon as I'm out of the pod. It could take hours or days or weeks but they WILL find me. Get to the -/-///-/- observatory. It will still be there. It has to be." );
  
  node_init( ODV9_B1_C_POD_PANEL, NT_PROP );
  node_link( ODV9_B1_C_POD_PANEL, ODV9_B1_C, "Examine the control panel." );
  node_desc( ODV9_B1_C_POD_PANEL, "Cryopod Control Panel", "", "The control panel's diagnostics read nominal across the board. All systems are functioning and the most recent stasis cycle encountered no errors. A single warning light pulses next to a switch marked 'MAINTENANCE OVERRIDE'. The switch is badly damaged, leaving it in the 'ON' position permanently." );

  node_init(ODV9_CUTTING_TORCH, NT_ITEM);
  node_link(ODV9_CUTTING_TORCH, ODV9_B1_B, "Take the cutting torch.");

  node_init(ODV9_CUT_STAIRWELL_DOOR, NT_FLAG);
  node_link(ODV9_CUT_STAIRWELL_DOOR, ODV9_B1, "Cut the weld on the stairwell door.");
  nbt[ODV9_CUT_STAIRWELL_DOOR].unlocked_by = ODV9_CUTTING_TORCH;

  node_init(ODV9_ID_CARD, NT_ITEM);
  node_link(ODV9_ID_CARD, ODV9_F1_B, "Take the id card.");
  
  node_init(ODV9_PRYBAR, NT_ITEM);
  node_link(ODV9_PRYBAR, ODV9_F2_A, "Take the prybar.");
  
  node_init(ODV9_PRY_OPEN_CRATE, NT_FLAG);
  node_link(ODV9_PRY_OPEN_CRATE, ODV9_B1_A, "Pry open the crate.");
  nbt[ODV9_PRY_OPEN_CRATE].unlocked_by = ODV9_PRYBAR;
  
  node_init(ODV9_MCP_SUIT, NT_ITEM);
  node_link(ODV9_MCP_SUIT, ODV9_B1_A, "Put on the environmental suit.");
  nbt[ODV9_MCP_SUIT].revealed_by = ODV9_PRY_OPEN_CRATE;
  
  node_init(ODV9_REACTOR_CODES, NT_ITEM);
  node_link(ODV9_REACTOR_CODES, ODV9_F2_C, "Take the authentication module.");
  
  node_init(ODV9_FUEL_CELL, NT_ITEM);
  node_link(ODV9_FUEL_CELL, ODV9_F1_C, "Take one of the fuel cells.");

  node_init(ODV9_REFUEL_REACTOR, NT_FLAG);
  node_link(ODV9_REFUEL_REACTOR, ODV9_B1_B, "Refuel the reactor using a fuel cell.");
  nbt[ODV9_REFUEL_REACTOR].unlocked_by = ODV9_FUEL_CELL;

  node_init(ODV9_RESTART_REACTOR, NT_FLAG);
  node_link(ODV9_RESTART_REACTOR, ODV9_B1_B, "Restart the reactor using authentication module.");
  nbt[ODV9_RESTART_REACTOR].revealed_by = ODV9_REFUEL_REACTOR;
  nbt[ODV9_RESTART_REACTOR].unlocked_by = ODV9_REACTOR_CODES;
  
  node_init(ODV9_REBOOT_COMPUTER, NT_FLAG);
  node_link(ODV9_REBOOT_COMPUTER, ODV9_F2_C, "Reboot the computer core.");
  nbt[ODV9_REBOOT_COMPUTER].unlocked_by = ODV9_RESTART_REACTOR;
  
  node_init(ODV9_NAVIGATION_DATA, NT_ITEM);
  node_link(ODV9_NAVIGATION_DATA, ODV9_F2_B, "Take the navigation data.");
  nbt[ODV9_NAVIGATION_DATA].unlocked_by = ODV9_REBOOT_COMPUTER;
  
  node_init(ODV9_REFUEL_CRAWLER, NT_FLAG);
  node_link(ODV9_REFUEL_CRAWLER, ODV9_F1_C, "Refuel the crawler using a fuel cell.");
  
  node_init(ODV9_UPLOAD_NAV_DATA, NT_FLAG);
  node_link(ODV9_UPLOAD_NAV_DATA, ODV9_F1_C, "Update the crawler's nav computer.");
  nbt[ODV9_UPLOAD_NAV_DATA].revealed_by = ODV9_REFUEL_CRAWLER;
  nbt[ODV9_UPLOAD_NAV_DATA].unlocked_by = ODV9_NAVIGATION_DATA;

  node_init(ODV9_ESCAPE_THE_OUTPOST, NT_ROOM);
  node_link(ODV9_ESCAPE_THE_OUTPOST, ODV9_F1_C, "Escape using the Arctic Crawler.");
  node_desc(ODV9_ESCAPE_THE_OUTPOST, "Game Over: Escaped the Outpost", "", "You drive away from the outpost in the Arctic Crawler, headed for the nearby Observatory.\n\nThank you for playing Outpost DV9! Please look forward to the next chapter." );
  
  nbt[ODV9_ESCAPE_THE_OUTPOST].unlocked_by = ODV9_UPLOAD_NAV_DATA;
  
}
///////////////// THE STATE OF THE PLAYER //////////////////

struct {
  node_t *cur_node;
  struct { char *key; int value; } *tags;
} player;

void player_add_tag(tag_t t){ tags[t]=1; }
void player_del_tag(tag_t t){ tags[t]=0; }
int  player_has_tag(tag_t t){ return tags[t]; }

int node_is_hidden(node_t *n){
  return (n == NULL) || (n->type == NT_NONE) ||
         (n->revealed_by != TAG_NONE && !player_has_tag(n->revealed_by)) ||
         (n->rehidden_by != TAG_NONE &&  player_has_tag(n->rehidden_by)); 
}
           
int node_is_locked(node_t *n){
  return (n->unlocked_by != TAG_NONE) && !player_has_tag(n->unlocked_by);
}

///////////////// NODE TO SCENE CONVERSION /////////////////

void scene_from_node(scene_t *s, node_t *n){
  snprintf(s->super, STR_SIZE_S, "%s", n->idstr);
  snprintf(s->title, STR_SIZE_S, "%s", n->title);
  snprintf(s->prose, STR_SIZE_L, "%s", n->prose);

  s->cursor_pos = 0;

  size_t i;
  for(i=0;i<MAX_OPTIONS;i++){
    option_t *opt = &s->options[i];
    snprintf(opt->label, STR_SIZE_M, "%li) %s", i+1, "...");
    opt->target = NULL;
  }

  for(i=0;i<MAX_OPTIONS;i++){
    option_t *opt = &s->options[i];
    node_t *child = i<MAX_CHILDREN ? n->children[i] : n->parent;
    if(node_is_hidden(child)){ continue; }
    snprintf(opt->label, STR_SIZE_M, "%li) %s", i+1, child->asopt);
    if(node_is_locked(child)){ continue; }
    opt->target = child;
  }

  if(n->type == NT_HALL){
    s->bgimg = get_image(n->bgimg);
  }if(n->type == NT_ROOM){
    s->bgimg = get_image(n->bgimg);
    snprintf(s->options[MAX_OPTIONS-1].label, STR_SIZE_M, "%i) %s", MAX_OPTIONS, "Exit this room.");
  }else if(n->type == NT_PROP){
    snprintf(s->options[MAX_OPTIONS-1].label, STR_SIZE_M, "%i) %s", MAX_OPTIONS, "Return.");
    s->cursor_pos = MAX_OPTIONS-1;
  }else if(n->type == NT_ITEM){
    snprintf(s->title,  STR_SIZE_M, "%s", "ERROR: Scene From Item");
    snprintf(s->prose, STR_SIZE_L, "%s", "An item node has been passed to the scene_from_node function but items cannot be viewed as scenes. Should have been picked up instead.");
  }
}

////////////////////// THE MAIN LOOP ///////////////////////

int32_t main(void){
  SDL_Init(SDL_INIT_EVERYTHING);
  controller_init();

  SDL_Window *WINDOW = SDL_CreateWindow("game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, INITIAL_WINDOW_SIZE, 0);
  if(WINDOW == NULL){ printf("%s\n", SDL_GetError()); fflush(stdout); exit(1); }

  SDL_Renderer *REND = SDL_CreateRenderer(WINDOW, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  SDL_Surface *SCREEN_SURFACE = create_surface(VIRTUAL_SCREEN_SIZE);
  SDL_Texture *SCREEN_TEXTURE = SDL_CreateTexture(REND, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 320, 240);

  font_t *font_super = font_create("font_small_8.png",           0x1ac3e7dd, 0x1ac3e766);
  font_t *font_title = font_create("font_alkhemikal_15.png",     0x1ac3e7FF, 0x1ac3e766);
  font_t *font_prose = font_create("font_mnemonika_12.png",      0x1ac3e7FF, 0x22222266);
  
  font_t *font_opt_normal = font_create("font_mnemonika_12.png", 0x1ac3e7ff, 0x1ac3e766);
  font_t *font_opt_dimmed = font_create("font_mnemonika_12.png", 0x1ac3e766, 0x1ac3e733);
  font_t *font_opt_select = font_create("font_mnemonika_12.png", 0x5de0fbFF, 0x1ac3e766);

  SDL_Surface *screen_clear = get_image("bg-odv9-pixel-frame.png");
  SDL_Surface *pointer_image = get_image("cursor_arrow.png");
  SDL_Surface *trans_buffer = create_surface(VIRTUAL_SCREEN_SIZE);
  uint8_t trans_alpha = 0;
  
  populate_the_world_tree();
  
  scene_t current_scene;
  node_t *next_node = &nbt[ODV9_B1_C];
  
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
        if(next_node->type == NT_ITEM){
          player_add_tag(next_node->tag);
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

      font_draw_string(font_super, current_scene.super, 12, 10, SCREEN_SURFACE);
      font_draw_string(font_title, current_scene.title, 12, 20, SCREEN_SURFACE);
      font_wrap_string(font_prose, current_scene.prose, 12, 36, 290, SCREEN_SURFACE);

      for(int i=0; i < 6; i++){
        option_t *opt = &current_scene.options[i];

        int y = 158+(i*font_get_height(font_opt_normal));

        if(opt->target == NULL){ 
          font_draw_string(font_opt_dimmed, opt->label, 20, y, SCREEN_SURFACE);
        }else if(i != current_scene.cursor_pos ){
          font_draw_string(font_opt_normal, opt->label, 20, y, SCREEN_SURFACE);
        }else{
          font_draw_string(font_opt_select, opt->label, 20, y, SCREEN_SURFACE);
        }
        
        if(i == current_scene.cursor_pos){
          SDL_BlitSurface(pointer_image, NULL, SCREEN_SURFACE, &(struct SDL_Rect){10,y+2,0,0});
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
