#pragma once

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

