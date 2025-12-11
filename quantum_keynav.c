#include QMK_KEYBOARD_H
#include "quantum_keynav.h"
ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(1, 0, 0);
#include <math.h>

#ifndef KEYNAV_ASSUME_MAX_HEIGHT
#error "No max height"
#define KEYNAV_ASSUME_MAX_HEIGHT 0
#endif
#ifndef KEYNAV_ASSUME_MAX_WIDTH
#error "No max width"
#define KEYNAV_ASSUME_MAX_WIDTH 0
#endif

keynav_state_t keynav_state = {
    .keynav_in_range = INVALID_DEFERRED_TOKEN,
    .enabled = true,
    .assume_screen_size = true,
    .x = {.assumed_pixel_size = KEYNAV_ASSUME_MAX_WIDTH
                                    ? 1.0f / KEYNAV_ASSUME_MAX_WIDTH
                                    : 0.0 / 0.0},
    .y = {.assumed_pixel_size = KEYNAV_ASSUME_MAX_HEIGHT
                                    ? 1.0f / KEYNAV_ASSUME_MAX_HEIGHT
                                    : 0.0 / 0.0}};

bool keynav_active(void) {
  return keynav_state.keynav_in_range != INVALID_DEFERRED_TOKEN;
}

void keynav_reset(void) {
  keynav_state.x.window_width = 1.0f / 4;
  keynav_state.y.window_width = 1.0f / 4;
  keynav_state.x.coord = 0.5f;
  keynav_state.y.coord = 0.5f;
  keynav_state.button_1 = false;
  keynav_state.button_2 = false;
  if (keynav_active()) {
    digitizer_state.in_range = true;
    digitizer_state.dirty = true;
    keynav_flush_state();
  }
}

void keynav_end(void) {
  if (keynav_active()) {
    digitizer_state.in_range = false;
    digitizer_state.dirty = true;
    cancel_deferred_exec(keynav_state.keynav_in_range);
    keynav_state.keynav_in_range = INVALID_DEFERRED_TOKEN;
    keynav_flush_state();
  }
}

#ifndef KEYNAV_PROXIMITY_INTERVAL
#define KEYNAV_PROXIMITY_INTERVAL 20
#endif

static uint32_t keynav_ping_proximity_callback(uint32_t trigger_time,
                                               void *cb_arg) {
  digitizer_state.dirty = true;
  keynav_flush_state();
  return KEYNAV_PROXIMITY_INTERVAL;
}

void keynav_start(void) {
  if (keynav_state.enabled) {
    if (!keynav_active()) {
      keynav_state.keynav_in_range = defer_exec(
          KEYNAV_PROXIMITY_INTERVAL, keynav_ping_proximity_callback, NULL);
      keynav_reset();
    }
  }
}

void keynav_flush_state(void) {
  bool in_keynav_mode = keynav_active();
  digitizer_state.dirty |= digitizer_state.x != keynav_state.x.coord ||
                           digitizer_state.y != keynav_state.y.coord ||
                           digitizer_state.in_range != in_keynav_mode ||
                           digitizer_state.tip != keynav_state.button_1 ||
                           digitizer_state.barrel != keynav_state.button_2;
  if (digitizer_state.dirty) {
    digitizer_state.x = keynav_state.x.coord;
    digitizer_state.y = keynav_state.y.coord;
    digitizer_state.tip = keynav_state.button_1;
    digitizer_state.barrel = keynav_state.button_2;
    digitizer_state.in_range = in_keynav_mode;
    digitizer_flush();
  }
}

#ifdef KEYNAV_LAYER
layer_state_t layer_state_set_quantum_keynav(layer_state_t state) {
  if (layer_state_cmp(state, KEYNAV_LAYER)) {
    if (!keynav_active()) {
      keynav_start();
    }
  } else if (keynav_active()) {
    keynav_end();
  }
  return state;
}

#if !defined(KEYNAV_NO_AUTO_LAYER_OFF) && defined(KEYNAV_LAYER)
void post_process_record_quantum_keynav(uint16_t keycode, keyrecord_t *record) {
  if (IS_LAYER_ON(KEYNAV_LAYER) && record->event.pressed) {
    switch (keycode) {
    case QK_KEYNAV_BUTTON_1:
    case QK_KEYNAV_BUTTON_2:
#ifdef KEYNAV_MOUSE_BUTTONS_LAYER_OFF
    case QK_MOUSE_BUTTON_1:
    case QK_MOUSE_BUTTON_2:
    case QK_MOUSE_BUTTON_3:
#endif
      layer_off(KEYNAV_LAYER);
    }
  }
  post_process_record_quantum_keynav_kb(keycode, record);
}
#endif
#endif

#if defined(KEYNAV_REMAP_MOUSE_BUTTONS) || defined(KEYNAV_REMAP_MOUSE_MOVEMENT)
bool pre_process_record_quantum_keynav(uint16_t keycode, keyrecord_t *record) {
  if (keynav_active()) {
    switch (keycode) {
#ifdef KEYNAV_REMAP_MOUSE_MOVEMENT
    case QK_MOUSE_CURSOR_LEFT:
      record->keycode = (get_mods() & MOD_MASK_SHIFT) ? QK_KEYNAV_MOVE_LEFT
                                                      : QK_KEYNAV_CUT_LEFT;
      break;
    case QK_MOUSE_CURSOR_DOWN:
      record->keycode = (get_mods() & MOD_MASK_SHIFT) ? QK_KEYNAV_MOVE_DOWN
                                                      : QK_KEYNAV_CUT_DOWN;
      break;
    case QK_MOUSE_CURSOR_UP:
      record->keycode =
          (get_mods() & MOD_MASK_SHIFT) ? QK_KEYNAV_MOVE_UP : QK_KEYNAV_CUT_UP;
      break;
    case QK_MOUSE_CURSOR_RIGHT:
      record->keycode = (get_mods() & MOD_MASK_SHIFT) ? QK_KEYNAV_MOVE_RIGHT
                                                      : QK_KEYNAV_CUT_RIGHT;
      break;
#endif
#ifdef KEYNAV_REMAP_MOUSE_BUTTONS
    case QK_MOUSE_BUTTON_1:
      record->keycode = QK_KEYNAV_BUTTON_1;
      break;
    case QK_MOUSE_BUTTON_3:
      record->keycode = QK_KEYNAV_BUTTON_2;
      break;
#endif
    }
  }
  return pre_process_record_quantum_keynav_kb(record->keycode, record);
}
#endif

static float into_valid_percent(float val) {
  if (val > 1.0f)
    return 1.0f;
  if (val < 0.0f)
    return 0.0f;
  return val;
}

static void keynav_move_towards(keynav_axis_t *axis, int direction) {
  const float minimal_change =
      isnan(axis->assumed_pixel_size)
          ? nextafter(axis->coord, 2.0f * direction) - axis->coord
          : axis->assumed_pixel_size * direction;
  const float diff = axis->window_width < fabs(minimal_change)
                         ? minimal_change
                         : axis->window_width * direction;
  axis->coord = into_valid_percent(axis->coord + diff);
}

void keynav_move(keynav_direction_t direction, bool zoom) {
  int id = zoom ? 1 : 2;
  switch (direction) {
  case KEYNAV_LEFT:
    keynav_move_towards(&keynav_state.x, -id);
    if (false) {
    case KEYNAV_RIGHT:
      keynav_move_towards(&keynav_state.x, id);
    }
    if (zoom)
      keynav_state.x.window_width /= 2;
    break;
  case KEYNAV_UP:
    keynav_move_towards(&keynav_state.y, -id);
    if (false) {
    case KEYNAV_DOWN:
      keynav_move_towards(&keynav_state.y, id);
    }
    if (zoom)
      keynav_state.y.window_width /= 2;
    break;
  default:
    return;
  }
  keynav_flush_state();
}

bool process_record_quantum_keynav(uint16_t keycode, keyrecord_t *record) {
  if (!keynav_active())
    return true;

  switch (keycode) {
  case QK_KEYNAV_BUTTON_1:
    keynav_state.button_1 = record->event.pressed;
    if (false) {
    case QK_KEYNAV_BUTTON_2:
      keynav_state.button_2 = record->event.pressed;
    }
    keynav_flush_state();
    return process_record_quantum_keynav_kb(keycode, record);
  }

  if (!record->event.pressed)
    return process_record_quantum_keynav_kb(keycode, record);

  switch (keycode) {
  case QK_KEYNAV_CUT_LEFT:
    keynav_move(KEYNAV_LEFT, true);
    break;
  case QK_KEYNAV_CUT_DOWN:
    keynav_move(KEYNAV_DOWN, true);
    break;
  case QK_KEYNAV_CUT_UP:
    keynav_move(KEYNAV_UP, true);
    break;
  case QK_KEYNAV_CUT_RIGHT:
    keynav_move(KEYNAV_RIGHT, true);
    break;
  case QK_KEYNAV_MOVE_LEFT:
    keynav_move(KEYNAV_LEFT, false);
    break;
  case QK_KEYNAV_MOVE_DOWN:
    keynav_move(KEYNAV_DOWN, false);
    break;
  case QK_KEYNAV_MOVE_UP:
    keynav_move(KEYNAV_UP, false);
    break;
  case QK_KEYNAV_MOVE_RIGHT:
    keynav_move(KEYNAV_RIGHT, false);
    break;
  case QK_KEYNAV_TOGGLE_ASSUMED_SCREEN_SIZE:
    keynav_state.assume_screen_size = !keynav_state.assume_screen_size;
    break;
#ifdef KEYNAV_LAYER
  case TO(KEYNAV_LAYER):
#endif
  case QK_KEYNAV_RESET:
    keynav_reset();
    break;
  }
  return process_record_quantum_keynav_kb(keycode, record);
}
