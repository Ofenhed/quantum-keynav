#include QMK_KEYBOARD_H
#include "quantum_keynav.h"
ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(1, 0, 0);
#include <math.h>

#ifndef KEYNAV_ASSUME_MAX_HEIGHT
#define KEYNAV_ASSUME_MAX_HEIGHT 0
#endif
#ifndef KEYNAV_ASSUME_MAX_WIDTH
#define KEYNAV_ASSUME_MAX_WIDTH 0
#endif

keynav_state_t keynav_state = {.keynav_in_range = INVALID_DEFERRED_TOKEN,
                               .enabled = true,
                               .x = {.assumed_size = KEYNAV_ASSUME_MAX_WIDTH},
                               .y = {.assumed_size = KEYNAV_ASSUME_MAX_HEIGHT}};

bool keynav_active(void) {
  return keynav_state.keynav_in_range != INVALID_DEFERRED_TOKEN;
}

void keynav_reset(void) {
  keynav_state.x.step = 1;
  keynav_state.y.step = 1;
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

#  ifndef KEYNAV_NO_AUTO_LAYER_OFF
void post_process_record_quantum_keynav(uint16_t keycode, keyrecord_t *record) {
  if (!record->event.pressed) {
    switch (keycode) {
      case KC_KEYNAV_BTN1:
      case KC_KEYNAV_BTN2:
#    ifdef KEYNAV_MOUSE_BUTTONS_LAYER_OFF
      case KC_MS_BTN1:
      case KC_MS_BTN2:
      case KC_MS_BTN3:
#    endif
        layer_off(KEYNAV_LAYER);
    }
  }
  post_process_record_quantum_keynav_kb(keycode, record);
}
#  endif
#endif

#if defined(KEYNAV_REMAP_MOUSE_BUTTONS) || defined(KEYNAV_REMAP_MOUSE_MOVEMENT)
bool pre_process_record_quantum_keynav(uint16_t keycode, keyrecord_t *record) {
  if (keynav_active()) {
    switch (keycode) {
#ifdef KEYNAV_REMAP_MOUSE_MOVEMENT
    case KC_MS_LEFT:
      record->keycode =
          (get_mods() & MOD_MASK_SHIFT) ? KC_KEYNAV_SHIFT_LEFT : KC_KEYNAV_LEFT;
      break;
    case KC_MS_DOWN:
      record->keycode =
          (get_mods() & MOD_MASK_SHIFT) ? KC_KEYNAV_SHIFT_DOWN : KC_KEYNAV_DOWN;
      break;
    case KC_MS_UP:
      record->keycode =
          (get_mods() & MOD_MASK_SHIFT) ? KC_KEYNAV_SHIFT_UP : KC_KEYNAV_UP;
      break;
    case KC_MS_RIGHT:
      record->keycode = (get_mods() & MOD_MASK_SHIFT) ? KC_KEYNAV_SHIFT_RIGHT
                                                      : KC_KEYNAV_RIGHT;
      break;
#endif
#ifdef KEYNAV_REMAP_MOUSE_BUTTONS
    case KC_MS_BTN1:
      record->keycode = KC_KEYNAV_BTN1;
      break;
    case KC_MS_BTN3:
      record->keycode = KC_KEYNAV_BTN2;
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

static void keynav_size_towards(keynav_axis_t *axis, int direction) {
  int step = 1 << axis->step;
  float assumed_pixel_size = 1.0f / axis->assumed_size;
  float closest_next = axis->assumed_size
                           ? (axis->coord + assumed_pixel_size * direction)
                           : nextafter(axis->coord, 2.0f * direction);
  if (step == 0) {
    axis->coord = closest_next;
  }
  float diff = 0.5f / step;
  float new_coord = axis->coord + (diff * direction);
  if ((axis->assumed_size && diff < assumed_pixel_size) ||
      (!axis->assumed_size && new_coord == axis->coord)) {
    axis->coord = closest_next;
  } else {
    axis->coord = new_coord;
  }
  axis->coord = into_valid_percent(axis->coord);
}

void keynav_move(keynav_direction_t direction, bool zoom) {
  int id = zoom ? 1 : 2;
  switch (direction) {
  case KEYNAV_LEFT:
    keynav_size_towards(&keynav_state.x, -id);
    if (false) {
    case KEYNAV_RIGHT:
      keynav_size_towards(&keynav_state.x, id);
    }
    if (zoom)
      ++keynav_state.x.step;
    break;
  case KEYNAV_UP:
    keynav_size_towards(&keynav_state.y, -id);
    if (false) {
    case KEYNAV_DOWN:
      keynav_size_towards(&keynav_state.y, id);
    }
    if (zoom)
      ++keynav_state.y.step;
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
  case KC_KEYNAV_BTN1:
    keynav_state.button_1 = record->event.pressed;
    if (false) {
  case KC_KEYNAV_BTN2:
      keynav_state.button_2 = record->event.pressed;
    }
    keynav_flush_state();
    return process_record_quantum_keynav_kb(keycode, record);
  }

  if (!record->event.pressed)
    return process_record_quantum_keynav_kb(keycode, record);

  switch (keycode) {
  case KC_KEYNAV_LEFT:
    keynav_move(KEYNAV_LEFT, true);
    break;
  case KC_KEYNAV_DOWN:
    keynav_move(KEYNAV_DOWN, true);
    break;
  case KC_KEYNAV_UP:
    keynav_move(KEYNAV_UP, true);
    break;
  case KC_KEYNAV_RIGHT:
    keynav_move(KEYNAV_RIGHT, true);
    break;
  case KC_KEYNAV_SHIFT_LEFT:
    keynav_move(KEYNAV_LEFT, false);
    break;
  case KC_KEYNAV_SHIFT_DOWN:
    keynav_move(KEYNAV_DOWN, false);
    break;
  case KC_KEYNAV_SHIFT_UP:
    keynav_move(KEYNAV_UP, false);
    break;
  case KC_KEYNAV_SHIFT_RIGHT:
    keynav_move(KEYNAV_RIGHT, false);
    break;
#ifdef KEYNAV_LAYER
  case TO(KEYNAV_LAYER):
#endif
  case KC_KEYNAV_RESET:
    keynav_reset();
    break;
  }
  return process_record_quantum_keynav_kb(keycode, record);
}
