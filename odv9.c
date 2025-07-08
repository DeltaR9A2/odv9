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
  X(ODV9_B1_TO_S1_DOOR)        \
  X(ODV9_CUT_STAIRWELL_DOOR)   \
  X(ODV9_ID_CARD)              \
  X(ODV9_UNLOCK_COMMAND_DECK)  \
  X(ODV9_PRYBAR)               \
  X(ODV9_PRY_OPEN_CRATE)       \
  X(ODV9_MCP_SUIT)             \
  X(ODV9_BAY_TOO_COLD)         \
  X(ODV9_FUEL_CELL)            \
  X(ODV9_REFUEL_REACTOR)       \
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
  // Aliases for things that might have special logic in the future
  NT_CASE = NT_PROP, // Something that contains items
  NT_FLAG = NT_ITEM, // Item that is abstract

} node_type_t;

typedef struct node_t {
  tag_t tag;
  node_type_t type; // the node's type determines how the player interacts with it
  struct node_t *parent;      // pointer to this node's parent
  struct node_t *children[MAX_CHILDREN]; // up to five pointers to child nodes

  char idstr[STR_SIZE_S];  // the unique id of this node (like 'ODV9-B1-C')
  char label[STR_SIZE_S];  // the name of the node (like "cutting torch" or "storage room")
  char asopt[STR_SIZE_M];  // node's label as option, constructed from label based on type
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

void format_idstr(char *dst, const char *src){ for(size_t i=0;i<STR_SIZE_S;i++){ if(src[i]=='\0'){ dst[i]='\0'; break; }else if(src[i]=='_' ){ dst[i]='-'; }else{ dst[i]=toupper(src[i]); } } }

static int8_t  tags[TAG_COUNT];
static node_t  nodes_by_tag[TAG_COUNT];
static node_t *nbt = nodes_by_tag;

static node_t *CNODE = NULL;

void node_select(tag_t t){ 
  CNODE = &nbt[t]; 
  CNODE->tag = t; 
}

/*void node_add_child(node_t *child){
  for(int i=0; i<MAX_CHILDREN; i++){
    if(CNODE->children[i] == child){ return; }
    else if(CNODE->children[i] != NULL){ continue; }
    else{ 
      CNODE->children[i] = child; 
      child->parent = CNODE;
      return;
    }
  }
}*/


void node_init(const char *label, node_type_t type){
  CNODE->type=type;
  format_idstr(CNODE->idstr,tag_names[CNODE->tag]);
  snprintf(CNODE->label,STR_SIZE_S,label);
  if(CNODE->type == NT_ITEM){ 
    CNODE->rehidden_by = CNODE->tag; 
    snprintf(CNODE->asopt, STR_SIZE_M, "Pick up the %s", label);
  }else if(CNODE->type == NT_ROOM){
    snprintf(CNODE->asopt, STR_SIZE_M, "Enter the %s", label);
  }else if(CNODE->type == NT_PROP){
    snprintf(CNODE->asopt, STR_SIZE_M, "Inspect the %s", label);
  }else if(CNODE->type == NT_HALL){
    snprintf(CNODE->asopt, STR_SIZE_M, "Move to the %s", label);
  }else{
    snprintf(CNODE->asopt, STR_SIZE_M, "%s", label);
  }
}

void node_link(tag_t a, tag_t b, tag_t c, tag_t d, tag_t e){ 
  CNODE->children[0] = &nbt[a]; nbt[a].parent = CNODE;
  CNODE->children[1] = &nbt[b]; nbt[b].parent = CNODE;
  CNODE->children[2] = &nbt[c]; nbt[c].parent = CNODE;
  CNODE->children[3] = &nbt[d]; nbt[d].parent = CNODE;
  CNODE->children[4] = &nbt[e]; nbt[e].parent = CNODE;
}

void node_desc(const char *title, const char *bgimg, const char *prose){
  snprintf(CNODE->title,STR_SIZE_S,"%s",title);
  snprintf(CNODE->bgimg,STR_SIZE_M,"%s",bgimg);
  snprintf(CNODE->prose,STR_SIZE_L,"%s",prose);
}

void node_custom_asopt(const char *asopt){
  snprintf(CNODE->asopt,STR_SIZE_M,"%s",asopt);
}

void node_revealed_by(tag_t key){ CNODE->revealed_by = key; }
void node_unlocked_by(tag_t key){ CNODE->unlocked_by = key; }
void node_rehidden_by(tag_t key){ CNODE->rehidden_by = key; }

void populate_the_world_tree(void){
  node_select( TAG_NONE );
  node_init("root of the world tree", NT_NONE);
  node_link(TEST_HALL, 0, 0, 0, 0);
  
  node_select( TEST_HALL );
  node_init("test halld", NT_HALL);
  node_desc("Test Hall", "", "This is the test hall. It is a strange liminal space that makes you feel uneasy.");
  node_link(TEST_ROOM, 0, 0, 0, ODV9_B1_C);

  node_select( TEST_ROOM );
  node_init("test room", NT_ROOM);
  node_desc("Test Room", "", "This is the test room. It's completely unremarkable but somehow seems uniquely well suited to testing.");
  node_link(TEST_ITEM, TEST_PROP, TORN_PAPER, 0, 0 );
  
  node_select( TEST_ITEM );
  node_init("test_item", NT_ITEM);

  node_select( TEST_PROP );
  node_init("test prop", NT_PROP);
  node_desc("Test Prop", "", "This is a test prop. It's the most boring thing you've ever seen.");

  node_select( TORN_PAPER );
  node_init("torn paper", NT_PROP);
  node_desc("Torn Paper", "", "If anybody reads this, please tell my tortoise that I love him.");

  ////////////////////////// REAL GAME CONTENT ///////////////////////
  // Cryo Vault
  node_select( ODV9_B1_C );
  node_init("cryo vault", NT_ROOM);
  node_desc("Cryo Vault", "bg-odv9-pixel-frame-cryo-vault.png", 
            "An empty stasis pod dominates the room, its life support "
            "systems still softly clicking and humming. A warning light "
            "pulses on a control panel and a hand-written note is taped "
            "beside it. There is a large metal cabinet in one corner, "
            "and a reinforced steel door directly across from it." );
  node_link(ODV9_B1_C_DOOR_NOTE, ODV9_B1_C_POD_PANEL, 0, 0, 0);
  
  // Basement Passage
  node_select( ODV9_B1 );
  node_init("basement hallway", NT_HALL );
  node_desc("Outpost Basement", "", 
            "The air of this dimly lit corridor is cold and stale. Pipes "
            "and conduits obscure the ceiling overhead, and every sound "
            "echoes off the bare concrete of the floor and walls. Three "
            "doors have spray-painted stencil lettering; 'STORAGE', "
            "'REACTOR', and 'CRYO'. A fourth door with an 'EXIT' sign "
            "shows visible scorching along the seams.");
  node_link(ODV9_B1_A, ODV9_B1_B, ODV9_B1_C, 0, ODV9_B1_TO_S1_DOOR );
  
  // Storage Room
  node_select( ODV9_B1_A );
  node_init("storage room", NT_ROOM );
  node_desc("Storage Room", "", 
            "This crowded storage room is lined with floor-to-ceiling "
            "racks full of boxes and crates. Decades worth of supplies "
            "and replacement parts. There must be something useful in all this." );
  node_link(ODV9_PRY_OPEN_CRATE,ODV9_MCP_SUIT,0,0,0);
  
  // Reactor Room
  node_select( ODV9_B1_B );
  node_init("reactor room", NT_ROOM );
  node_desc("Reactor Room", "", 
            "A hulking fusion reactor occupies one half of this room. "
            "It looks nearly pristine, but requires specialized fuel cells "
            "to operate. The other half of the room has a long workbench "
            "covered in a mess of tools and parts." );
  node_link(ODV9_CUTTING_TORCH, ODV9_REFUEL_REACTOR, ODV9_RESTART_REACTOR, 0, 0);
  
  // Door between basement and stairwell. Welded, needs to be cut open.
  node_select( ODV9_B1_TO_S1_DOOR );
  node_init("stairwell door", NT_PROP);
  node_desc("Stairwell Door", "", 
            "The door between the basement and the stairwell has been "
            "welded shut from the basement side. The welding is crude but "
            "more than enough to prevent the door from opening. You'll "
            "need some kind of tool to get this door open." );
  node_rehidden_by(ODV9_CUT_STAIRWELL_DOOR);
  node_link(ODV9_CUT_STAIRWELL_DOOR, 0, 0, 0, 0);
  
  // Option to cut basement->stairwell door, visible on the door prop.
  node_select( ODV9_CUT_STAIRWELL_DOOR );
  node_init("welded door", NT_FLAG);
  node_custom_asopt("Cut the weld.");
  node_unlocked_by(ODV9_CUTTING_TORCH);

  // The stairwell. This is actually the parent of all three floors. It would
  // appear locked from all floors, but the player starts in the basement.
  node_select( ODV9_S1 );
  node_init("stairwell", NT_HALL );
  node_desc("Stairwell", "", 
            "This cramped stairwell connects to three floors. The lowest "
            "door shows signs of scorching along the seams. The highest "
            "door says 'ACCESS RESTRICTED' and has an electronic lock "
            "with card reader. The middle door is unlocked and has an "
            "'EXIT' sign above it.");
  node_revealed_by( ODV9_CUT_STAIRWELL_DOOR );
  node_link(ODV9_B1, ODV9_F1, ODV9_F2, 0, 0 );

  // Ground Floor Passage
  node_select( ODV9_F1 );
  node_init("ground floor hallway", NT_HALL );
  node_desc("Ground Floor Hallway", "", 
            "This traffic-worn hallway has four doors. Block lettering on "
            "three read 'COMMON', 'QUARTERS', and 'STAIRS'. A fourth door "
            "is larger, rimed with thick frost, and has an 'EXIT' sign above it.");
  node_link(ODV9_F1_A, ODV9_F1_B, ODV9_F1_C, 0, 0 );
  
  // Command Deck Passage
  node_select( ODV9_F2 );
  node_init("command deck hallway", NT_HALL );
  node_desc("Command Deck Hallway", "", 
            "This narrow passage is cleaner than the rest of the outpost "
            "as if rarely used. There is an 'EXIT' sign above the stairwell "
            "door, and three other doors are marked 'COMMAND', 'COMPCORE', "
            "and 'MONITOR'.");
  node_unlocked_by(ODV9_ID_CARD);
  node_link(ODV9_F2_A, ODV9_F2_B, ODV9_F2_C, 0, 0);

  // Common Room
  node_select( ODV9_F1_A );
  node_init("common room", NT_ROOM );
  node_desc("Common Room", "", 
            "With a central round table, wall mounted entertainment center, "
            "and a corner kitchenette, this common room is surprisingly "
            "comfortable despite its limited size.");
  
  // Crew Quarters
  node_select( ODV9_F1_B );
  node_init("crew quarters", NT_ROOM );
  node_desc("Crew Quarters", "", 
            "This room is quiet and slightly warmer than the rest of the "
            "outpost. It has six recessed cubicles; each has its own bed "
            "and locker, with a curtain for privacy. There is a tiny "
            "bathroom at the far end, barely larger than a closet.");
  node_link(ODV9_ID_CARD,0,0,0,0);
  
  // Maintenance Bay
  node_select( ODV9_F1_C );
  node_init("maintenance bay", NT_ROOM );
  node_desc("Maintenance Bay", "", 
            "The huge bay door is frozen wide open, leaving this space "
            "exposed to arctic conditions. A massive half-tracked vehicle "
            "is parked just inside the bay, beside a large rack of nuclear "
            "fuel cells.");
  node_unlocked_by(ODV9_MCP_SUIT);
  node_link(ODV9_FUEL_CELL, ODV9_REFUEL_CRAWLER, ODV9_UPLOAD_NAV_DATA, ODV9_ESCAPE_THE_OUTPOST, 0);

  // Command Center
  node_select( ODV9_F2_A );
  node_init("command center", NT_ROOM );
  node_desc("Command Center", "", 
            "Huge windows with inches-thick glass give a spectacular "
            "view of snow covered mountains. There are three stations with "
            "various displays and control panels. None seem to be working, "
            "and the equipment at the 'COMMS' station has been smashed to "
            "pieces.");
  node_link(ODV9_PRYBAR,0,0,0,0);
  
  // Surveillance Suite
  node_select( ODV9_F2_B );
  node_init("surveillance suite", NT_ROOM);
  node_desc("Surveillance Suite", "",
            "This room feels out of place in the outpost; the displays "
            "and instruments have a sleek militaristic quality that seems "
            "slightly sinister. A single chair is surrounded by displays "
            "and control panels like the cockpit of some kind of aircraft." );
  node_link(ODV9_NAVIGATION_DATA,0,0,0,0);
  
  // Computer Core
  node_select( ODV9_F2_C );
  node_init("computer core", NT_ROOM);
  node_desc("Computer Core", "", 
            "This claustrophobic room is crammed with more server racks "
            "than seems reasonable for this outpost. They must require a "
            "massive amount of electricity to operate. There is a single "
            "workstation for direct access." );
  node_link(ODV9_REBOOT_COMPUTER, ODV9_REACTOR_CODES,0,0,0);
  
  node_select( ODV9_B1_C_DOOR_NOTE );
  node_init("taped note", NT_PROP );
  node_desc("Taped Note", "", 
            "A hand-written note is taped to the wall beside the door. "
            "It reads:\n\n I won't remember writing this. The drugs are "
            "already working. I nee-YOU- need to leave. They know you're "
            "awake. It could take days but they WILL find you. Get to the "
            "observatory. It will still be there. It has to be." );
  
  node_select( ODV9_B1_C_POD_PANEL );
  node_init("control panel", NT_PROP );
  node_desc("Cryopod Control Panel", "", 
            "The control panel's diagnostics read nominal across the "
            "board. All systems are functioning and the most recent "
            "stasis cycle encountered no errors. A single warning light "
            "pulses next to a switch marked 'MAINTENANCE OVERRIDE'. The "
            "switch is badly damaged, leaving it in the 'ON' position "
            "permanently." );

  node_select( ODV9_CUTTING_TORCH );
  node_init("cutting torch", NT_ITEM);

  node_select(ODV9_ID_CARD);
  node_init("id card", NT_ITEM);
  
  node_select(ODV9_PRYBAR);
  node_init("prybar", NT_ITEM);

  node_select(ODV9_PRY_OPEN_CRATE);
  node_init("sealed crate", NT_FLAG);
  node_custom_asopt("pry open the sealed crate");
  node_unlocked_by(ODV9_PRYBAR);
  
  node_select(ODV9_MCP_SUIT);
  node_init("environmental suit", NT_ITEM);
  node_custom_asopt("Put on the environmental suit.");
  node_revealed_by(ODV9_PRY_OPEN_CRATE);
  
  node_select(ODV9_REACTOR_CODES);
  node_init("authentication module", NT_ITEM);

  node_select( ODV9_FUEL_CELL );
  node_init("fuel cell", NT_ITEM);
  node_custom_asopt("Take one of the fuel cells.");
  
  node_select( ODV9_REFUEL_REACTOR );
  node_init("refuel the reactor", NT_FLAG);
  node_custom_asopt( "Refuel the reactor using a fuel cell.");
  node_unlocked_by(ODV9_FUEL_CELL);

  node_select( ODV9_RESTART_REACTOR );
  node_init("restart the reactor", NT_FLAG);
  node_custom_asopt("Restart the reactor using authentication module.");
  node_revealed_by(ODV9_REFUEL_REACTOR);
  node_unlocked_by(ODV9_REACTOR_CODES);
  
  node_select( ODV9_REBOOT_COMPUTER );
  node_init("reboot the computer", NT_FLAG);
  node_custom_asopt("Reboot the computer core.");
  node_unlocked_by(ODV9_RESTART_REACTOR);
  
  node_select(ODV9_NAVIGATION_DATA);
  node_init("navigation data", NT_ITEM);
  node_unlocked_by(ODV9_REBOOT_COMPUTER);
  
  node_select(ODV9_REFUEL_CRAWLER);
  node_init("refuel the crawler", NT_FLAG);
  node_custom_asopt("Refuel the crawler using a fuel cell.");
  
  node_select(ODV9_UPLOAD_NAV_DATA);
  node_init("upload navigation data", NT_FLAG);
  node_custom_asopt("Update the crawler's nav computer.");
  node_revealed_by(ODV9_REFUEL_CRAWLER);
  node_unlocked_by(ODV9_NAVIGATION_DATA);

  node_select(ODV9_ESCAPE_THE_OUTPOST);
  node_init("escape the outpost", NT_ROOM);
  node_custom_asopt("Escape using the Arctic Crawler.");
  node_desc("Game Over: Escaped the Outpost", "", 
            "You drive away from the outpost in the Arctic Crawler, "
            "headed for the nearby Observatory.\n\nThank you for "
            "playing Outpost DV9! Please look forward to the next chapter." );
  node_unlocked_by(ODV9_UPLOAD_NAV_DATA);
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
    //s->cursor_pos = MAX_OPTIONS-1;
  }else if(n->type == NT_ITEM){
    snprintf(s->title, STR_SIZE_M, "%s", "ERROR: Scene From Item");
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

  font_t *font_super = font_create("font_small_8.png",           0x1ac3e7aa, 0x00000066);
  font_t *font_title = font_create("font-terminess-14.png",     0x5de0fbff, 0x1ac3e766);
  font_t *font_prose = font_create("font-mnemonika-10.png",      0x1ac3e7ee, 0x00000066);
  
  font_t *font_opt_normal = font_create("font-mnemonika-10.png", 0x1ac3e7cc, 0x00000066);
  font_t *font_opt_dimmed = font_create("font-mnemonika-10.png", 0x1ac3e777, 0x00000033);
  font_t *font_opt_select = font_create("font-mnemonika-10.png", 0x5de0fbFF, 0x5de0fb66);

  SDL_Surface *screen_clear = get_image("bg-odv9-pixel-frame.png");
  SDL_Surface *pointer_image = get_image("cursor_arrow.png");
  SDL_Surface *trans_buffer = create_surface(VIRTUAL_SCREEN_SIZE);
  uint8_t trans_alpha = 0;
  
  populate_the_world_tree();
  
  scene_t current_scene;
  node_t *next_node = &nbt[TEST_HALL];
  
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

      font_draw_string(font_super, current_scene.super, 18, 16, SCREEN_SURFACE);
      font_draw_string(font_title, current_scene.title, 18, 26, SCREEN_SURFACE);
      font_wrap_string(font_prose, current_scene.prose, 18, 42, 274, SCREEN_SURFACE);

      for(int i=0; i < 6; i++){
        option_t *opt = &current_scene.options[i];

        int y = 154+(i*(font_get_height(font_opt_normal)+1));

        if(opt->target == NULL){ 
          font_draw_string(font_opt_dimmed, opt->label, 22, y, SCREEN_SURFACE);
        }else if(i != current_scene.cursor_pos ){
          font_draw_string(font_opt_normal, opt->label, 22, y, SCREEN_SURFACE);
        }else{
          font_draw_string(font_opt_select, opt->label, 22, y, SCREEN_SURFACE);
        }
        
        if(i == current_scene.cursor_pos){
          SDL_BlitSurface(pointer_image, NULL, SCREEN_SURFACE, &(struct SDL_Rect){12,y,0,0});
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
