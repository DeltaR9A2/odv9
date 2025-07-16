#include <math.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define SDL_MAIN_HANDLED
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

#define GAME_VERSION "VER-1-0-1"

///////////////////// TYPE DEFINITIONS /////////////////////

#define TAG_LIST(X)              \
  X(TAG_NONE)                    \
  X(MAIN_MENU)                   \
  X(GAME_EXIT)                   \
  X(NEW_GAME)                    \
  /* Test Content */             \
  X(TEST_HALL)                   \
  X(TEST_ROOM)                   \
  X(TEST_ITEM)                   \
  X(TEST_PROP)                   \
  X(TORN_PAPER)                  \
  /* ODV9 Structure */           \
  X(ODV9_S1)                     \
  X(ODV9_B1)                     \
  X(ODV9_B1_A)                   \
  X(ODV9_B1_B)                   \
  X(ODV9_B1_C)                   \
  X(ODV9_F1)                     \
  X(ODV9_F1_A)                   \
  X(ODV9_F1_B)                   \
  X(ODV9_F1_C)                   \
  X(ODV9_F2)                     \
  X(ODV9_F2_A)                   \
  X(ODV9_F2_B)                   \
  X(ODV9_F2_C)                   \
  /* ODV9 Critical Flags */      \
  X(LOCK_B1_TO_S1_WELDED)        \
  X(CASE_B1_B_TOOL_BOX)          \
  X(ITEM_B1_B_CUTTING_TORCH)     \
  X(FLAG_B1_TO_S1_IS_CUT)        \
  /* Command Deck Keycard */     \
  X(LOCK_S1_TO_F2_CARDLOCK)      \
  X(CASE_F1_B_LOCKER)            \
  X(ITEM_F1_B_ID_CARD)           \
  X(FLAG_S1_TO_F2_UNLOCKED)      \
  /* Sealed Crate in Storage */  \
  X(LOCK_B1_A_CRATE_SEALED)      \
  X(CASE_F2_A_CONSOLE)           \
  X(ITEM_F2_A_PRYBAR)            \
  X(FLAG_B1_A_CRATE_UNSEALED)    \
  /* Maint Bay Too Cold */       \
  X(LOCK_F1_C_TOO_COLD)          \
  X(CASE_B1_A_CRATE)             \
  X(ITEM_B1_A_SUIT)              \
  X(FLAG_B1_A_WEARING_SUIT)      \
  /* Reactor Has No Cell */      \
  X(LOCK_B1_B_REACTOR_NO_FUEL)   \
  X(CASE_F1_C_FUEL_CELL_RACK)    \
  X(ITEM_F1_C_FUEL_CELL)         \
  X(FLAG_B1_B_REACTOR_REFUELED)  \
  /* Reactor Is Offline */       \
  X(LOCK_B1_B_REACTOR_OFFLINE)   \
  X(CASE_F2_C_DESK_DRAWER)       \
  X(ITEM_F2_C_AUTH_MODULE)       \
  X(FLAG_B1_B_REACTOR_ONLINE)    \
  /* Compcore Server Offline */  \
  X(LOCK_F2_C_SERVER_OFFLINE)    \
  X(FLAG_F2_C_SERVER_ONLINE)     \
  /* Crawler Needs Nav Data */   \
  X(LOCK_F1_C_NO_NAV_DATA)       \
  X(CASE_F2_B_CONSOLE)           \
  X(ITEM_F2_B_NAV_DATA)          \
  X(FLAG_F1_C_NAV_DATA_UPLOAD)   \
  /* End of the Game */          \
  X(ODV9_ESCAPE_THE_OUTPOST)     \
  /* Flavor Frosting */          \
  X(ODV9_PROP_LOST_LABEL)        \
  X(ODV9_PROP_CHECKLIST)         \
  X(ODV9_PROP_CRYO_NOTE)         \
  X(ODV9_PROP_CRYO_PANEL)        \
  X(ODV9_PROP_CRYO_CABINET)      \
  X(ODV9_PROP_FLOOR_STAIN)       \
  X(ODV9_PROP_SAMEKO_PLAYER)     \
  X(ODV9_PROP_WANAU_ENERGY)      \
  X(ODV9_PROP_EMPTY_SYRINGE)     \
  X(ODV9_PROP_STRANGE_TOY)       \
  X(ODV9_PROP_COMMAND_WINDOW)    \
  /* Lock Template */            
  // X(LOCK_)                    
  // X(CASE_)                    
  // X(ITEM_)                    
  // X(FLAG_)                    

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
  NT_FLAG, // Item that is abstract
  NT_CASE, // Something that contains items
  NT_LOCK, // A lock that needs a key
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

void node_init(const char *label, node_type_t type){
  CNODE->type=type;
  format_idstr(CNODE->idstr,tag_names[CNODE->tag]);
  snprintf(CNODE->label,STR_SIZE_S,label);
  if(CNODE->type == NT_ITEM){ 
    CNODE->rehidden_by = CNODE->tag; 
    snprintf(CNODE->asopt, STR_SIZE_M, "Pick up the %s.", label);
  }else if(CNODE->type == NT_FLAG){ 
    CNODE->rehidden_by = CNODE->tag; 
    snprintf(CNODE->asopt, STR_SIZE_M, "Pick up the %s.", label);
  }else if(CNODE->type == NT_ROOM){
    snprintf(CNODE->asopt, STR_SIZE_M, "Enter the %s.", label);
  }else if(CNODE->type == NT_PROP ) {
    snprintf(CNODE->asopt, STR_SIZE_M, "Look at %s.", label);}
  else if ( CNODE->type == NT_LOCK){
    snprintf(CNODE->asopt, STR_SIZE_M, "Inspect the %s.", label);
  }else if(CNODE->type == NT_CASE){
    snprintf(CNODE->asopt, STR_SIZE_M, "Search the %s.", label);
  }else if(CNODE->type == NT_HALL){
    snprintf(CNODE->asopt, STR_SIZE_M, "Move to the %s.", label);
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

void node_add_as_child_to(tag_t p){
  for(size_t i=0;i<MAX_OPTIONS;i++){
    if(nbt[p].children[i] == &nbt[TAG_NONE]){
        nbt[p].children[i] = CNODE; CNODE->parent = &nbt[p];
        return;
    }
  }
  printf("WARNING: node_add_child found no empty slot on %s", CNODE->idstr);
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
  node_link(TEST_HALL, GAME_EXIT, 0, 0, 0);
  
  node_select( TEST_HALL );
  node_init("test hall", NT_HALL);
  node_desc("Test Hall", "", 
            "This is the test hall. It is a strange liminal space "
            "that makes you feel uneasy.");
  node_link(TEST_ROOM, 0, 0, 0, ODV9_B1_C);

  node_select( TEST_ROOM );
  node_init("test room", NT_ROOM);
  node_desc("Test Room", "", 
            "This is the test room. It's completely unremarkable "
            "but somehow seems uniquely well suited to testing.");
  node_link(TEST_ITEM, TEST_PROP, TORN_PAPER, 0, 0 );
  
  node_select( TEST_ITEM );
  node_init("test_item", NT_ITEM);

  node_select( TEST_PROP );
  node_init("test prop", NT_PROP);
  node_desc("Test Prop", "", 
            "This is a test prop. It's the most boring thing you've "
            "ever seen.");

  node_select( TORN_PAPER );
  node_init("torn paper", NT_PROP);
  node_desc("Torn Paper", "", 
            "If anybody reads this, please tell my tortoise that I love him.");

  node_select( GAME_EXIT );
  node_init("game exit", NT_HALL);
  node_custom_asopt("[EXIT GAME]");
  node_link(MAIN_MENU, 0, 0, 0, 0);

  node_select( MAIN_MENU );
  node_init("main menu", NT_HALL);
  node_desc("Outpost DV9 - Main Menu", "", 
            "You've been in stasis for a very long time. It's impossible "
            "to tell how long; only the faintest traces of sensation "
            "reach your slumbering mind. You sense that something is "
            "beginning... that it's time to wake up...\n\n\n[Use arrow keys to select options.]\n[Press enter to confirm selected option.]");
  node_link(NEW_GAME, 0, 0, 0, 0);

  node_select( NEW_GAME );
  node_init("new game", NT_HALL);
  node_custom_asopt("[START GAME]");
  node_desc("Somewhere Very Cold", "", 
            "You struggle awake, shivering violently as air hisses and "
            "latches pop all around you. The fogged glass door of a "
            "cryostasis pod lifts up and out of view, releasing you "
            "from your icy confines.");
  node_link(0, 0, 0, 0, 0);
  nbt[NEW_GAME].parent = &nbt[ODV9_B1_C];

  ////////////////////////// REAL GAME CONTENT ///////////////////////


  // Basement Passage
  node_select( ODV9_B1 );
  node_init("basement hallway", NT_HALL );
  node_link(ODV9_B1_A, ODV9_B1_B, ODV9_B1_C, 0, LOCK_B1_TO_S1_WELDED );
  
      // Storage Room
      node_select( ODV9_B1_A );
      node_init("storage room", NT_ROOM );
      node_link(LOCK_B1_A_CRATE_SEALED,CASE_B1_A_CRATE,0,0,0);

          node_select( LOCK_B1_A_CRATE_SEALED );
          node_init("sealed crate", NT_LOCK);
          node_link(FLAG_B1_A_CRATE_UNSEALED,0,0,0,0);
          node_rehidden_by(FLAG_B1_A_CRATE_UNSEALED);

              node_select( FLAG_B1_A_CRATE_UNSEALED );
              node_init("broken seal", NT_FLAG);
              node_unlocked_by(ITEM_F2_A_PRYBAR);

          node_select(CASE_B1_A_CRATE);
          node_init("unsealed crate", NT_CASE);
          node_link(ITEM_B1_A_SUIT,0,0,0,0);
          node_revealed_by(FLAG_B1_A_CRATE_UNSEALED);
          node_rehidden_by(ITEM_B1_A_SUIT);
          
              node_select(ITEM_B1_A_SUIT);
              node_init("environmental suit", NT_ITEM);
    
      // Reactor Room
      node_select( ODV9_B1_B );
      node_init("reactor room", NT_ROOM );
      node_link(CASE_B1_B_TOOL_BOX, LOCK_B1_B_REACTOR_NO_FUEL, LOCK_B1_B_REACTOR_OFFLINE, 0, 0);

          node_select(CASE_B1_B_TOOL_BOX);
          node_init("tool box", NT_CASE);
          node_link(ITEM_B1_B_CUTTING_TORCH,0,0,0,0);
          node_rehidden_by(ITEM_B1_B_CUTTING_TORCH);

              node_select(ITEM_B1_B_CUTTING_TORCH);
              node_init("cutting torch", NT_ITEM);

          node_select( LOCK_B1_B_REACTOR_NO_FUEL );
          node_init("reactor fueling port", NT_LOCK);
          node_link(FLAG_B1_B_REACTOR_REFUELED, 0, 0, 0, 0);
          node_rehidden_by(FLAG_B1_B_REACTOR_REFUELED);
          
              node_select(FLAG_B1_B_REACTOR_REFUELED);
              node_init("refueled reactor",NT_FLAG);
              node_unlocked_by(ITEM_F1_C_FUEL_CELL);

          node_select( LOCK_B1_B_REACTOR_OFFLINE );
          node_init("reactor control panel", NT_LOCK);
          node_link(FLAG_B1_B_REACTOR_ONLINE,0,0,0,0);
          node_unlocked_by(FLAG_B1_B_REACTOR_REFUELED);
          node_rehidden_by(FLAG_B1_B_REACTOR_ONLINE);
          
              node_select(FLAG_B1_B_REACTOR_ONLINE);
              node_init("online reactor",NT_FLAG);
              node_unlocked_by(ITEM_F2_C_AUTH_MODULE);
        
  
      // Cryo Vault
      node_select( ODV9_B1_C );
      node_init("cryo vault", NT_ROOM);
      node_link(0, 0, 0, 0, 0);
 
      // Door between basement and stairwell. Welded, needs to be cut open.
      node_select( LOCK_B1_TO_S1_WELDED );
      node_init("'EXIT' door", NT_LOCK);
      node_rehidden_by(FLAG_B1_TO_S1_IS_CUT);
      node_link(FLAG_B1_TO_S1_IS_CUT, 0, 0, 0, 0);
      
          // Option to cut basement->stairwell door, visible on the door prop.
          node_select( FLAG_B1_TO_S1_IS_CUT );
          node_init("welded door", NT_FLAG);
          node_custom_asopt("Cut the welded seam.");
          node_unlocked_by(ITEM_B1_B_CUTTING_TORCH);

  // The stairwell. This is actually the parent of all three floors. It would
  // appear locked from all floors, but the player starts in the basement.
  node_select( ODV9_S1 );
  node_init("stairwell", NT_HALL );
  node_unlocked_by( FLAG_B1_TO_S1_IS_CUT );
  node_link(ODV9_F2, LOCK_S1_TO_F2_CARDLOCK, ODV9_F1, 0, ODV9_B1 );

      node_select( LOCK_S1_TO_F2_CARDLOCK );
      node_init("card reader", NT_LOCK);
      node_link( FLAG_S1_TO_F2_UNLOCKED, 0,0,0,0 );
      node_rehidden_by( FLAG_S1_TO_F2_UNLOCKED );
      
          node_select( FLAG_S1_TO_F2_UNLOCKED );
          node_init("unlocked door",NT_FLAG);
          node_unlocked_by( ITEM_F1_B_ID_CARD );

  // Ground Floor Passage
  node_select( ODV9_F1 );
  node_init("ground floor hallway", NT_HALL );
  node_link(0, ODV9_F1_B, ODV9_F1_C, LOCK_F1_C_TOO_COLD, 0 );
  
      node_select( LOCK_F1_C_TOO_COLD );
      node_init("maintenance bay door", NT_PROP );
      node_custom_asopt("Enter the maintenance bay.");
      node_rehidden_by(ITEM_B1_A_SUIT);

      // Crew Quarters
      node_select( ODV9_F1_B );
      node_init("crew quarters", NT_ROOM );
      node_link(CASE_F1_B_LOCKER,0,0,0,0);

          node_select(CASE_F1_B_LOCKER);
          node_init("crew lockers", NT_CASE);
          node_link(ITEM_F1_B_ID_CARD,0,0,0,0);
          node_rehidden_by(ITEM_F1_B_ID_CARD);

            node_select( ITEM_F1_B_ID_CARD ); 
            node_init("id card", NT_ITEM);
      
      // Maintenance Bay
      node_select( ODV9_F1_C );
      node_init("maintenance bay", NT_ROOM );
      node_revealed_by(ITEM_B1_A_SUIT);
      node_link(CASE_F1_C_FUEL_CELL_RACK, LOCK_F1_C_NO_NAV_DATA, ODV9_ESCAPE_THE_OUTPOST, 0, 0);

          node_select( CASE_F1_C_FUEL_CELL_RACK );
          node_init("rack of nuclear fuel cells", NT_CASE);
          node_link(ITEM_F1_C_FUEL_CELL,0,0,0,0);
          node_rehidden_by(ITEM_F1_C_FUEL_CELL);
              node_select( ITEM_F1_C_FUEL_CELL );
              node_init("fuel cell", NT_ITEM);

          node_select(LOCK_F1_C_NO_NAV_DATA);
          node_init("arctic crawler", NT_LOCK);
          node_link(FLAG_F1_C_NAV_DATA_UPLOAD,0,0,0,0);
          node_rehidden_by(FLAG_F1_C_NAV_DATA_UPLOAD);
          
              node_select(FLAG_F1_C_NAV_DATA_UPLOAD);
              node_init("successful upload", NT_FLAG);
              node_unlocked_by(ITEM_F2_B_NAV_DATA);
      
      node_select(ODV9_ESCAPE_THE_OUTPOST);
      node_init("escape the outpost", NT_PROP);
      node_unlocked_by(FLAG_F1_C_NAV_DATA_UPLOAD);
      node_link(GAME_EXIT,0,0,0,0);

  // Command Deck Passage
  node_select( ODV9_F2 );
  node_init("command deck hallway", NT_HALL );
  node_unlocked_by(FLAG_S1_TO_F2_UNLOCKED);
  node_link(ODV9_F2_A, ODV9_F2_B, ODV9_F2_C, 0, 0);

      // Command Center
      node_select(ODV9_F2_A);
      node_init("command center", NT_ROOM );
      node_link(CASE_F2_A_CONSOLE,0,0,0,0);

          node_select(CASE_F2_A_CONSOLE);
          node_init("smashed console", NT_CASE);
          node_link(ITEM_F2_A_PRYBAR,0,0,0,0);
          node_rehidden_by(ITEM_F2_A_PRYBAR);

              node_select(ITEM_F2_A_PRYBAR);
              node_init("prybar", NT_ITEM);
      
      // Surveillance Suite
      node_select( ODV9_F2_B );
      node_init("surveillance suite", NT_ROOM);
      node_link(CASE_F2_B_CONSOLE,0,0,0,0);

          node_select(CASE_F2_B_CONSOLE);
          node_init("surveillance system", NT_CASE);
          node_link(ITEM_F2_B_NAV_DATA,0,0,0,0);
          node_rehidden_by(ITEM_F2_B_NAV_DATA);
          node_unlocked_by(FLAG_F2_C_SERVER_ONLINE);
          
              node_select(ITEM_F2_B_NAV_DATA);
              node_init("navigation data", NT_ITEM);
      
      // Computer Core
      node_select( ODV9_F2_C );
      node_init("computer core", NT_ROOM);
      node_link(LOCK_F2_C_SERVER_OFFLINE, CASE_F2_C_DESK_DRAWER,0,0,0);
      
          node_select( CASE_F2_C_DESK_DRAWER );
          node_init("desk drawer", NT_CASE);
          node_rehidden_by(ITEM_F2_C_AUTH_MODULE);
          node_link(ITEM_F2_C_AUTH_MODULE,0,0,0,0);

              node_select(ITEM_F2_C_AUTH_MODULE);
              node_init("authentication module", NT_ITEM);

          node_select( LOCK_F2_C_SERVER_OFFLINE );
          node_init("main server", NT_LOCK);
          node_link(FLAG_F2_C_SERVER_ONLINE,0,0,0,0);
          node_revealed_by(FLAG_B1_B_REACTOR_ONLINE);
          node_rehidden_by(FLAG_F2_C_SERVER_ONLINE);
          
              node_select(FLAG_F2_C_SERVER_ONLINE);
              node_init("rebooted computer", NT_FLAG);
              node_unlocked_by(FLAG_B1_B_REACTOR_ONLINE);

  
  node_select( ODV9_B1_C );
  node_desc("Cryo Vault", "", 
            "An empty stasis pod dominates the room, its life support "
            "systems still softly clicking and humming. A warning light "
            "pulses on a control panel and a hand-written note is taped "
            "beside it. There is a large metal cabinet in one corner, "
            "and a reinforced steel door directly across from it.");

  node_select( ODV9_B1 );
  node_desc("Outpost Basement", "", 
            "The air of this dimly lit corridor is cold and stale. Pipes "
            "and conduits obscure the ceiling overhead, and every sound "
            "echoes off the bare concrete of the floor and walls. Three "
            "doors have spray-painted stencil lettering; 'STORAGE', "
            "'REACTOR', and 'CRYO'. A fourth door with an 'EXIT' sign "
            "shows visible scorching along the seams.");

  node_select( ODV9_B1_A );
  node_desc("Storage Room", "", 
            "This crowded storage room is lined with floor-to-ceiling "
            "racks full of boxes and crates. Decades worth of supplies "
            "and replacement parts in sealed boxes. The only interesting "
            "thing you find is a large shipping crate near the back. "
            "It's the only thing without a place on the shelves, was "
            "it left here for a reason?");

  node_select( LOCK_B1_A_CRATE_SEALED );
  node_desc("Sealed Storage Crate", "", 
            "There is a large shipping crate in the back corner of the "
            "room, its surface coated in a fine layer of dust. The lid "
            "is fastened shut with thick metal bands and recessed latches "
            "that need to be pried open. You can see faint markings on "
            "the side; something about emergency gear. With the right "
            "tool, you might be able to force it open.");

  node_select( FLAG_B1_A_CRATE_UNSEALED );
  node_custom_asopt("Pry open the sealed crate.");

  node_select(CASE_B1_A_CRATE);
  node_desc("Unsealed Storage Crate", "", 
            "The lid now hangs loose, bent from the force required to open it. "
            "Inside, packed in foam and sealed plastic, you find a full-body "
            "environmental suit. The outer shell is dull gray with reinforced seams, "
            "clearly built for subzero exposure. A helmet with a polarized visor is "
            "tucked beside it, along with a compact heat exchange unit and RTG power cell. "
            "Everything inside appears intact and ready for use. This could protect "
            "you in nearly any climate; you'll definitely want to be wearing it "
            "when you leave the outpost.");

  node_select(ITEM_B1_A_SUIT);
  node_custom_asopt("Put on the environmental suit.");

  node_select( ODV9_B1_B );
  node_desc("Reactor Room", "", 
            "A hulking fusion reactor occupies one half of this room. "
            "It looks nearly pristine, but requires specialized fuel cells "
            "to operate. The other half of the room has a long workbench "
            "covered in rusted parts and scrap metal. There aren't any tools "
            "on the hooks and shelves above the bench, but there is an old "
            "tool box in the corner.");

  node_select(CASE_B1_B_TOOL_BOX);
  node_desc("Old Tool Box", "",
            "The exterior of this tool box is giving way to rust, but it "
            "has done an admirable job preserving its contents. The first "
            "thing that catches your eye is a powerful cutting torch. It "
            "seems out of place here among dirty old hand tools, and "
            "you have a strange feeling like you've seen it before. "
            "Nothing else seems worth taking right now; screwdrivers, "
            "pliers, a ratchet set... nothing that would make a decent "
            "weapon, like a wrench or a crowbar.");

  node_select(LOCK_B1_B_REACTOR_NO_FUEL);
  node_desc("Reactor Fueling Port", "",
            "A compartment juts from the reactor’s outer casing, ringed "
            "with warning labels and instructions. There is a circular slot "
            "marked 'MANUAL FUEL INSERTION'. "
            "If you had a compatible fuel cell, it looks like it could still accept one.");

  node_select(FLAG_B1_B_REACTOR_REFUELED);
  node_custom_asopt("Refuel the reactor using a fuel cell.");

  node_select(LOCK_B1_B_REACTOR_OFFLINE);
  node_desc("Reactor Control Panel", "",
            "The control panel is covered in dust, but the indicator lights "
            "still glow dimly. A dirty screen displays a prompt: 'REACTOR "
            "OFFLINE – FUEL LEVEL CRITICAL – AUTH REQUIRED'. Below it, "
            "a slot marked 'AUTH MODULE' is set into the panel. The system "
            "appears to be waiting for authorization for an automated restart "
            "sequence. With the reactor refueled, this "
            "should be enough to bring it back online.");
            
  node_select(FLAG_B1_B_REACTOR_ONLINE);
  node_custom_asopt("Insert the authentication module.");

  node_select( LOCK_B1_TO_S1_WELDED );
  node_desc("Stairwell Door, Welded Shut", "", 
            "The door between the basement and the stairwell has been "
            "welded shut from the basement side. The welding is crude but "
            "more than enough to prevent the door from opening. You'll "
            "need some kind of tool to get this door open.");

  node_select( LOCK_S1_TO_F2_CARDLOCK );
  node_desc("Stairwell Door, Card Reader Lock", "", 
            "A heavy security door blocks the way to the second floor. A "
            "small panel beside the frame houses a card reader. The plastic "
            "cover is scratched, and the indicator light is red. It says "
            "'COMMAND STAFF ONLY'. You'll need "
            "a valid ID Card to unlock this door.");

  node_select( ODV9_S1 );
  node_desc("Stairwell", "", 
            "This cramped stairwell connects to three floors. The lowest "
            "door, to the basement, shows signs of scorching along the seams. The highest "
            "door, to the command deck, says 'ACCESS RESTRICTED' and has an electronic lock "
            "with card reader. The middle door, to the ground floor, is unlocked and has an "
            "'EXIT' sign above it.");

  node_select( FLAG_S1_TO_F2_UNLOCKED );
  node_custom_asopt("Use an ID Card to unlock the door.");

  node_select( ODV9_F1 );
  node_desc("Ground Floor Hallway", "", 
            "This traffic-worn hallway has four doors. Block lettering on "
            "three read 'COMMON', 'QUARTERS', and 'STAIRS'. A fourth door marked "
            "'MAINTENANCE BAY' is larger, rimed with thick frost, and has an 'EXIT' sign above it.");

  node_select( ODV9_F2 );
  node_desc("Command Deck Hallway", "", 
            "This narrow passage is cleaner than the rest of the outpost "
            "as if rarely used. There is an 'EXIT' sign above the stairwell "
            "door, and three other doors are marked 'COMMAND', 'COMPCORE', "
            "and 'SURVEILLANCE'.");

  node_select( ODV9_F1_B );
  node_desc("Crew Quarters", "", 
            "This room is quiet and slightly warmer than the rest of the "
            "outpost. It has six recessed cubicles; each has its own bed "
            "and locker, with a curtain for privacy. There is a tiny "
            "bathroom at the far end, barely larger than a closet.");

  node_select( CASE_F1_B_LOCKER );
  node_desc("Crew Lockers", "",
            "The crew lockers are mostly empty, with only a few forgotten "
            "personal items; a ripped jacket, a keychain, a cracked handheld "
            "game with no batteries. In the last one, you find a worn ID "
            "card dangling from a faded blue lanyard. The name reads 'G. Murin' "
            "serial number F-1573-R with a small emblem denoting command "
            "clearance. The woman in the photo is smiling. Could she still "
            "be alive? How long has this been here?");

  node_select( LOCK_F1_C_TOO_COLD );
  node_desc("Maintenance Bay Door", "",
            "Thick frost covers this door, and status indicators show "
            "arctic conditions on the other side. You'll need some sort "
            "of protection to enter; more than any normal clothing could "
            "provide.");

  node_select( ODV9_F1_C );
  node_desc("Maintenance Bay", "", 
            "The huge bay door is frozen wide open, leaving this space "
            "exposed to arctic conditions. A massive half-tracked vehicle "
            "is parked just inside the bay, beside a large rack of nuclear "
            "fuel cells.");

  node_select( CASE_F1_C_FUEL_CELL_RACK );
  node_desc("Fuel Cell Rack", "",
            "A metal rack spans the length of the wall, fully loaded with "
            "bright yellow canisters secured in padded brackets. Each one "
            "is covered in safety warning surrounding the same label: "
            "'TYPE-C MICRO FUSION'. They're warm to the touch despite the "
            "arctic conditions of the maintenance bay.");

  node_select( ITEM_F1_C_FUEL_CELL );
  node_custom_asopt("Take one of the fuel cells.");

  node_select( LOCK_F1_C_NO_NAV_DATA );
  node_desc("Arctic Crawler", "",
            "The crawler's control panel comes to life with a muted chime. "
            "Engine systems, life support, and environmental seals all check "
            "green. It's ready to move, but the navigation system shows "
            "no data for some reason. You could try driving blind into the storm, "
            "but you won't find a better shelter just by chance. Beneath the "
            "dashboard is a small port where you could update the crawler's "
            "systems with new data.");
            
  node_select(FLAG_F1_C_NAV_DATA_UPLOAD);
  node_custom_asopt("Update the crawler's nav computer.");

  node_select( ODV9_F2_A );
  node_desc("Command Center", "", 
            "Huge windows with inches-thick glass give a spectacular "
            "view of snow covered mountains. There are three stations with "
            "various displays and control panels. None seem to be working, "
            "and the equipment at the 'COMMS' station has been smashed to "
            "pieces.");

  node_select( CASE_F2_A_CONSOLE );
  node_desc("Smashed Console", "",
            "The communications console has been reduced to a mess of "
            "shattered plastic and twisted metal. The screen is cracked "
            "in half and components are scattered across the floor. "
            "Sticking out of the center is a heavy prybar; the sharp end "
            "is buried deep in the guts of the machine. Whoever did this "
            "wasn’t leaving anything to chance. There's no way to repair "
            "it; if this was the only comms unit in the outpost, you're "
            "completely cut off. ");

  node_select( ODV9_F2_B );
  node_desc("Surveillance Suite", "",
            "This room feels out of place in the outpost; the displays "
            "and instruments have a sleek militaristic quality that seems "
            "slightly sinister. A single chair is surrounded by displays "
            "and control panels like the cockpit of some kind of aircraft. "
            "You might find some useful information here if the outpost's "
            "computer systems are restored.");

  node_select( CASE_F2_B_CONSOLE );
  node_desc("Surveillance Console", "",
            "It seems this station was capable of monitoring the entire "
            "region. An array of monitors stretches above the controls, "
            "each labeled with distant station codes and waypoint IDs. "
            "Most of the feeds are offline, but a few flicker with static "
            "or distorted images. Some kind of removable drive is blinking "
            "a green indicator; the nearest screen shows a progress bar "
            "titled 'NAVDATA BACKUP' at 100%. Was this done before the "
            "outpost was abandoned? Or did it happen just now?");

  node_select( ODV9_F2_C );
  node_desc("Computer Core", "", 
            "This claustrophobic room is crammed with more server racks "
            "than seems reasonable for this outpost. They must require a "
            "massive amount of electricity to operate. There is a single "
            "workstation for direct access, perched atop a tiny desk with "
            "a drawer stuck open at an odd angle.");

  node_select( LOCK_F2_C_SERVER_OFFLINE );
  node_desc("Data Server Core", "",
            "The server towers are humming with power, but the system "
            "hasn't booted. Cables run in tidy bundles along the floor, "
            "and the hum of cooling fans fills the room with a low vibration. "
            "The central terminal displays a simple message: 'POWER RESTORED - "
            "PRESS ANY KEY TO REBOOT'. Why it doesn't just boot on its own is "
            "a puzzle for another time.");

  node_select(FLAG_F2_C_SERVER_ONLINE);
  node_custom_asopt("Reboot the computer core.");

  node_select( CASE_F2_C_DESK_DRAWER );
  node_desc("Desk Drawer", "",
          "The drawer scrapes open on its bent tracks. Inside, you find "
          "scattered office debris: "
          "broken wapens, a notepad with three pages left, a few loose "
          "cables and adaptors. Tucked near the back is a compact plastic "
          "module with a connector on one end - an authentication unit, still "
          "intact. A faint glow tells you it’s active. This seems like a poor "
          "hiding place for something so important, but security regulations "
          "tend to break down with small crews in isolation.");

  node_select(ODV9_ESCAPE_THE_OUTPOST);
  node_custom_asopt("Escape using the arctic crawler.");
  node_desc("Game Over: Escaped the Outpost", "", 
            "You drive away from the outpost in the Arctic Crawler, "
            "headed for the nearby Observatory. Storm clouds gather on "
            "the horizon; the drive will be long and difficult, but "
            "something in the back of your mind tells you to press on. "
            "\n\nGAME OVER: Thank you for "
            "playing Outpost DV9! Please look forward to the next chapter. You can end the game here, or return to the outpost if you want to look around.");

  // FLAVOR ONLY BEYOND THIS POINT

  node_select( ODV9_PROP_CRYO_PANEL );
  node_init("the pod's control panel", NT_PROP );
  node_desc("Cryopod Control Panel", "", 
            "The pod's diagnostics show zero errors during your stasis. "
            "A single warning light pulses next to a key-operated switch "
            "marked 'MAINTENANCE OVERRIDE'. The switch is stuck in the on "
            "position; the key is snapped off inside the lock.\n\nThere are "
            "no logs or biometrics recorded for your stasis cycle... was "
            "that deliberate? Safety protocols normally prevent a person "
            "putting themself in stasis; there needs to be an operator at "
            "the panel, but with the override... it could be done." );
  node_add_as_child_to(ODV9_B1_C);

  node_select( ODV9_PROP_CRYO_NOTE );
  node_init("the note taped to the panel", NT_PROP );
  node_desc("Taped Note", "", 
            "A hand-written note is taped to the pod's control panel. "
            "It reads:\n\nI won't remember writing this. The stasis "
            "will be long. I ne-YOU need to leave. They know you're "
            "awake. It will take some time but they WILL find you. "
            "Get to the observatory.\n\nIt will still be there... "
            "it has to be...");
  node_add_as_child_to(ODV9_B1_C);
  
  node_select( ODV9_PROP_CRYO_CABINET );
  node_init("the metal cabinet in the corner", NT_PROP );
  node_desc("Large Metal Cabinet", "", 
            "This cabinet contains all the specialized parts and "
            "chemicals for running the cryopod. Several of the containers "
            "have been opened, and the missing supplies account for more "
            "than one stasis cycle. Nothing here will help unless you find "
            "a reason to put yourself or someone else in stasis." );
  node_add_as_child_to(ODV9_B1_C);
  
  node_select(ODV9_PROP_FLOOR_STAIN);
  node_init("a stain on the floor", NT_PROP);
  node_desc("Floor Stain", "", 
            "A brownish-red stain streaks across the floor near the "
            "ground floor exit. It looks like it was wiped hastily, but "
            "not completely. There’s a faint trail leading away that "
            "fades before it reaches the stairs upward.");
  node_add_as_child_to(ODV9_S1);

  node_select(ODV9_PROP_COMMAND_WINDOW);
  node_init("the landscape through the windows", NT_PROP);
  node_desc("Command Center Windows", "",
            "The huge windows in the command center give a panoramic view "
            "of the surrounding landscape. It's all rocky slopes and sheer cliff faces "
            "between snow-covered peaks as far as you can see. The sky is gray and "
            "overcast with storm clouds in the distance. Wind howls against "
            "the reinforced glass. You can see the shape of what might be a roadway "
            "leading down the slope but it's covered with snow.");
  node_add_as_child_to(ODV9_F2_A);
  
  node_select(ODV9_PROP_CHECKLIST);
  node_init("a maintenance checklist on the wall", NT_PROP );
  node_desc("Maintenance Checklist", "", 
            "A clipboard is hung from a hook on the wall. Half the "
            "items are marked 'FAILED', and one line is scribbled out "
            "with heavy ink. Someone wrote 'DO NOT TOUCH - ASK CLARKE' at the bottom.");
  node_add_as_child_to(ODV9_B1_B);

  // Common Room
  node_select( ODV9_F1_A );
  node_init("common room", NT_ROOM );
  node_desc("Common Room", "", 
            "With a central round table, wall mounted entertainment center, "
            "and a corner kitchenette, this common room is surprisingly "
            "comfortable despite its limited size. This is where the crew "
            "came to relax and socialize. Where they tried to maintain their "
            "sanity together in the face of boredom and isolation, sheltered "
            "from the hostile conditions outside. Looking around you get "
            "the sense you won't find anything useful here, but it's worth checking. ");
  node_add_as_child_to(ODV9_F1);
  node_link(0,0,0,0,0);
  
  node_select(ODV9_PROP_STRANGE_TOY);
  node_init("the strange toy, still in its box", NT_PROP);
  node_desc("Strange Toy", "",
            "A slightly dented cardboard box with clear plastic front. "
            "There's a plastic toy inside, held securely by the packaging. "
            "It's a gray ball with cat ears, a single yellow eye, and a tail. "
            "The strange creature has a tiny black bow tie below its eye. "
            "The box says 'Grimmi Figs' and it's apparently a limited edition. ");
  node_add_as_child_to(ODV9_F1_A);
            
  node_select(ODV9_PROP_SAMEKO_PLAYER);
  node_init("the handheld game console on the table", NT_PROP);
  node_desc("Handheld Game Console", "", 
            "A handheld game console is lying on the table. Its screen is "
            "cracked and there's no response when you try to turn it on. "
            "The brand name says 'SAMEKO' with a blue fish logo. The game "
            "cartridge in the slot shows a singing girl with green hair.");
  node_add_as_child_to(ODV9_F1_A);
  
  node_select(ODV9_PROP_WANAU_ENERGY);
  node_init("the candy bar with a colorful wrapper",NT_PROP);
  node_desc("WANAU Energy Bar", "",
            "Apparently some kind of food, the wrapper says 'WANAU' with "
            "a speedy looking ghost silhouette after the 'U'. The bar inside "
            "feels rock hard and is surprisingly heavy. It's probably not safe "
            "to eat anymore, and based on the ingredients it never really was.");
  node_add_as_child_to(ODV9_F1_A);
  
  node_select(ODV9_PROP_LOST_LABEL);
  node_init("the smudged label on the floor", NT_PROP );
  node_desc("Shipping Label", "", 
            "A loose shipping label on the floor reads: 'LIQUID RATION SH1-K1-D3W QTY 36'. "
            "The rest is smudged by a heavy boot print. Someone has drawn a smiley face over the barcode with red marker.");
  node_add_as_child_to(ODV9_F1_A);

  node_select(ODV9_PROP_EMPTY_SYRINGE);
  node_init("a discarded syringe", NT_PROP);
  node_desc("Discarded Syringe", "", 
            "An empty syringe lies on the floor in the bathroom. The "
            "plunger is fully depressed, and there's a faint yellowish "
            "residue inside. Dried blood on the label obscures all but "
            "the letter 'T' and the needle is bent sideways like someone "
            "stepped on it.");
  node_add_as_child_to(ODV9_F1_B);
  
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

int node_all_hidden(node_t *n){
  for(size_t i=0;i<MAX_CHILDREN;i++){
    if(node_is_hidden(n->children[i])){
      continue;
    }else{ return 0; } 
  }
  return 1;
}

int node_all_locked(node_t *n){
  for(size_t i=0;i<MAX_OPTIONS;i++){
    if(node_is_locked(n->children[i])){
      continue;
    }else{ return 0; } 
  }
  return 1;
}

///////////////// NODE TO SCENE CONVERSION /////////////////
  
scene_t CURRENT_SCENE;
node_t *NEXT_NODE;

void player_update_node(){
  if( NEXT_NODE->type == NT_ITEM || 
      NEXT_NODE->type == NT_FLAG ){
    player_add_tag(NEXT_NODE->tag);
    player.cur_node = NEXT_NODE;
    NEXT_NODE = player.cur_node->parent;
    return;
  }else{
    player.cur_node = NEXT_NODE;
    NEXT_NODE = NULL;
  }

  if((player.cur_node->type == NT_CASE ||
      player.cur_node->type == NT_LOCK) && 
      node_all_hidden(player.cur_node)){
    NEXT_NODE = player.cur_node->parent;
    return;
  }
  
  scene_t *s = &CURRENT_SCENE;
  node_t *n = player.cur_node;

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

  size_t oi=0;
  for(i=0;i<MAX_OPTIONS;i++){
    option_t *opt = &s->options[i<MAX_CHILDREN ? oi : MAX_OPTIONS-1];
    node_t *child = i<MAX_CHILDREN ? n->children[i] : n->parent;
    if(node_is_hidden(child)){ 
      continue; 
    }else{
      oi += 1; 
      snprintf(opt->label, STR_SIZE_M, "%li) %s", i+1, child->asopt);
      if(node_is_locked(child)){ 
        continue; 
      }else{
        opt->target = child;
      }
    }
  }

  if(n->type == NT_HALL){
    s->bgimg = get_image(n->bgimg);
  }if(n->type == NT_ROOM){
    s->bgimg = get_image(n->bgimg);
    snprintf(s->options[MAX_OPTIONS-1].label, STR_SIZE_M, "%i) %s", MAX_OPTIONS, "Exit this room.");
  }else if(n->type == NT_PROP){
    snprintf(s->options[MAX_OPTIONS-1].label, STR_SIZE_M, "%i) %s", MAX_OPTIONS, "Return.");
  }else if(n->type == NT_CASE || n->type == NT_LOCK){
    snprintf(s->options[MAX_OPTIONS-1].label, STR_SIZE_M, "%i) %s", MAX_OPTIONS, "Return.");
  }else if(n->type == NT_ITEM || n->type == NT_FLAG){
    snprintf(s->title, STR_SIZE_M, "%s", "ERROR: Scene From Item");
    snprintf(s->prose, STR_SIZE_L, "%s", "An item node has been passed to the player_update_node function but items cannot be viewed as scenes. Should have been picked up instead.");
  }
}

////////////////////// THE MAIN LOOP ///////////////////////

int RUNNING = 1;
 
int32_t main_event_watch(void *data, SDL_Event *e){
  (void)(data); // Suppress unused warning
  if(e->type == SDL_QUIT){ RUNNING = SDL_FALSE; }
  return 0;
}

int main(int argc, char *argv[]){
  (void)(argc); // ignore unused arg
  (void)(argv); // ignore unused arg
  
  SDL_Init(SDL_INIT_EVERYTHING);
  SDL_AddEventWatch(&main_event_watch, 0);
  controller_init();

  SDL_Window *WINDOW = SDL_CreateWindow("game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, INITIAL_WINDOW_SIZE, 0);
  if(WINDOW == NULL){ printf("%s\n", SDL_GetError()); fflush(stdout); exit(1); }

  SDL_Renderer *REND = SDL_CreateRenderer(WINDOW, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  SDL_Surface *SCREEN_SURFACE = create_surface(VIRTUAL_SCREEN_SIZE);
  SDL_Texture *SCREEN_TEXTURE = SDL_CreateTexture(REND, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 320, 240);

  ready_static_images();

  font_t *font_super = font_create("font-small-8.png",           0x1ac3e766, 0x00000033);
  font_t *font_title = font_create("font-terminess-14.png",      0x5de0fbff, 0x1ac3e766);
  font_t *font_prose = font_create("font-mnemonika-10.png",      0x1ac3e7ee, 0x00000066);
  
  font_t *font_opt_normal = font_create("font-mnemonika-10.png", 0x1ac3e7cc, 0x00000066);
  font_t *font_opt_dimmed = font_create("font-mnemonika-10.png", 0x1ac3e777, 0x00000033);
  font_t *font_opt_select = font_create("font-mnemonika-10.png", 0x5de0fbFF, 0x5de0fb66);

  SDL_Surface *screen_clear = get_image("bg-odv9-pixel-frame.png");
  SDL_Surface *pointer_image = get_image("cursor-arrow.png");
  SDL_Surface *trans_buffer = create_surface(VIRTUAL_SCREEN_SIZE);
  int trans_alpha = 0;
  
  populate_the_world_tree();
  
  #ifdef DEBUG
  for(size_t i=0;i<TAG_COUNT;i++){
    if(strlen(nbt[i].prose) == 0){ 
      if(nbt[i].type == NT_ITEM){ continue; }
      printf("NO PROSE: %s\n",tag_names[i]); 
    }
  }

  player_add_tag(FLAG_B1_TO_S1_IS_CUT);
  player_add_tag(FLAG_S1_TO_F2_UNLOCKED);
  player_add_tag(ITEM_B1_A_SUIT);
  NEXT_NODE = &nbt[ODV9_S1];
  #endif

  NEXT_NODE = &nbt[MAIN_MENU];

  double cms = 0, pms = 0, msd = 0, msa = 0, mspf = 10;
  while(RUNNING){
    pms = cms; cms = SDL_GetTicks(); msd = cms - pms; msa += msd;
    if(msa > mspf){ msa -= mspf;
      controller_read();

      // Check for manual game exit. (DEBUG MODE)
      // if(controller_just_pressed(BTN_BACK)){ RUNNING = 0; }
      // Check for cursor movement.
      if(controller_just_pressed(BTN_U)){ CURRENT_SCENE.cursor_pos -= 1; if(CURRENT_SCENE.cursor_pos < 0){ CURRENT_SCENE.cursor_pos = 0; } }
      if(controller_just_pressed(BTN_D)){ CURRENT_SCENE.cursor_pos += 1; if(CURRENT_SCENE.cursor_pos > 5){ CURRENT_SCENE.cursor_pos = 5; } }
      // Check for option activation.
      if(controller_just_pressed(BTN_START)){ 
        NEXT_NODE = CURRENT_SCENE.options[CURRENT_SCENE.cursor_pos].target;
      }

      if(NEXT_NODE != NULL){
        SDL_BlitSurface(SCREEN_SURFACE,NULL,trans_buffer,NULL);
        trans_alpha = 255;
        player_update_node();
        if(player.cur_node == &nbt[GAME_EXIT]){
          RUNNING = 0;
          }
      }

      if(CURRENT_SCENE.bgimg != NULL){
        SDL_BlitSurface(CURRENT_SCENE.bgimg, NULL, SCREEN_SURFACE, NULL);
      }else{
        SDL_BlitSurface(screen_clear, NULL, SCREEN_SURFACE, NULL);
      }

      font_draw_string(font_super, CURRENT_SCENE.super, 16, 14, SCREEN_SURFACE);
      font_draw_string(font_title, CURRENT_SCENE.title, 18, 24, SCREEN_SURFACE);
      font_wrap_string(font_prose, CURRENT_SCENE.prose, 18, 40, 274, SCREEN_SURFACE);

      font_draw_string(font_super, GAME_VERSION, 264, 14, SCREEN_SURFACE);

      for(int i=0; i < 6; i++){
        option_t *opt = &CURRENT_SCENE.options[i];

        int y = 158+(i*(font_get_height(font_opt_normal)+1));

        if(opt->target == NULL){ 
          font_draw_string(font_opt_dimmed, opt->label, 22, y, SCREEN_SURFACE);
        }else if(i != CURRENT_SCENE.cursor_pos ){
          font_draw_string(font_opt_normal, opt->label, 22, y, SCREEN_SURFACE);
        }else{
          font_draw_string(font_opt_select, opt->label, 22, y, SCREEN_SURFACE);
        }
        
        if(i == CURRENT_SCENE.cursor_pos){
          SDL_BlitSurface(pointer_image, NULL, SCREEN_SURFACE, &(struct SDL_Rect){12,y,0,0});
        }
      }

      if(trans_alpha > 0){
        SDL_SetSurfaceAlphaMod(trans_buffer, trans_alpha);
        SDL_BlitSurface(trans_buffer, NULL, SCREEN_SURFACE, NULL);
        trans_alpha -= 20;
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
