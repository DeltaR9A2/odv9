#ifndef DV_CONTROLLER_H
#define DV_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

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

#endif
