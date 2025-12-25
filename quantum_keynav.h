#pragma once

typedef enum {
  KEYNAV_LEFT,
  KEYNAV_DOWN,
  KEYNAV_UP,
  KEYNAV_RIGHT,
  KEYNAV_INVALID = -1,
} keynav_direction_t;

typedef struct {
  float coord;
  float window_width;
  float assumed_pixel_size;
} keynav_axis_t;

typedef struct {
  deferred_token keynav_in_range;

  keynav_axis_t x;
  keynav_axis_t y;
  unsigned int button_1 : 1;
  unsigned int button_2 : 1;
  unsigned int enabled : 1;

  unsigned int assume_screen_size : 1;
} keynav_state_t;

extern keynav_state_t keynav_state;

bool keynav_active(void);
void keynav_reset(void);
void keynav_end(void);
void keynav_start(void);
void keynav_flush_state(void);
void keynav_move(keynav_direction_t direction, bool zoom);
