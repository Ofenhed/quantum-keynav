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
#if !defined(DYNAMIC_MACRO_ENABLE) && defined(DYNAMIC_MACRO_KEYNAV_ONLY_FINAL)
#error "DYNAMIC_MACRO_KEYNAV_ONLY_FINAL without dynamic macros enabled"
#endif

keynav_state_t keynav_state = {
    .active = INVALID_DEFERRED_TOKEN,
    .in_range = false,
    .enabled = true,
    .button_1 = false,
    .button_2 = false,
#ifdef DYNAMIC_MACRO_KEYNAV_ONLY_FINAL
    .macro_blocked = false,
#endif // DYNAMIC_MACRO_KEYNAV_ONLY_FINAL
    .assume_screen_size = true,
    .x = {.assumed_pixel_size = KEYNAV_ASSUME_MAX_WIDTH
                                    ? 1.0f / KEYNAV_ASSUME_MAX_WIDTH
                                    : 0.0 / 0.0},
    .y = {.assumed_pixel_size = KEYNAV_ASSUME_MAX_HEIGHT
                                    ? 1.0f / KEYNAV_ASSUME_MAX_HEIGHT
                                    : 0.0 / 0.0}};

bool keynav_active(void) {
  return keynav_state.enabled && keynav_state.active != INVALID_DEFERRED_TOKEN;
}

#ifdef DYNAMIC_MACRO_KEYNAV_ONLY_FINAL
static uint8_t macro_state = 0;
#endif

void keynav_reset(void) {
  keynav_state.x.window_width = 1.0f / 4;
  keynav_state.y.window_width = 1.0f / 4;
  keynav_state.x.coord = 0.5f;
  keynav_state.y.coord = 0.5f;
  keynav_state.in_range = false;
  if (keynav_active()) {
    digitizer_state.dirty = true;
    keynav_flush_state();
  }
}

void keynav_end(void) {
  if (keynav_active()) {
#ifdef DYNAMIC_MACRO_KEYNAV_ONLY_FINAL
    keynav_state.macro_blocked = false;
    keynav_state.in_range = true;
    keynav_flush_state();
#endif // DYNAMIC_MACRO_KEYNAV_ONLY_FINAL
#ifndef KEYNAV_NO_AUTO_BUTTON_RELEASE
    keynav_state.button_1 = false;
    keynav_state.button_2 = false;
#endif // KEYNAV_NO_AUTO_BUTTON_RELEASE
    digitizer_state.dirty = true;

    cancel_deferred_exec(keynav_state.active);
    keynav_state.in_range = false;
    keynav_state.active = INVALID_DEFERRED_TOKEN;
    keynav_flush_state();
#if defined(DYNAMIC_MACRO_KEYNAV_ONLY_FINAL) &&                                \
    defined(DYNAMIC_MACRO_KEYNAV_DELAY)
    wait_ms(DYNAMIC_MACRO_KEYNAV_DELAY);
#endif
  }
}

#ifndef KEYNAV_PROXIMITY_INTERVAL
#define KEYNAV_PROXIMITY_INTERVAL 20
#endif

static uint32_t keynav_ping_proximity_callback(uint32_t trigger_time,
                                               void *cb_arg) {
#ifdef DYNAMIC_MACRO_KEYNAV_ONLY_FINAL
  if (keynav_state.macro_blocked != 0) {
    return KEYNAV_PROXIMITY_INTERVAL;
  }
#endif
  digitizer_state.dirty = true;
  keynav_flush_state();
  return KEYNAV_PROXIMITY_INTERVAL;
}

void keynav_start(void) {
  if (keynav_state.enabled) {
    if (!keynav_active()) {
      keynav_state.active = defer_exec(KEYNAV_PROXIMITY_INTERVAL,
                                       keynav_ping_proximity_callback, NULL);
#ifdef DYNAMIC_MACRO_KEYNAV_ONLY_FINAL
      keynav_state.macro_blocked = macro_state != 0;
#endif
      keynav_reset();
    }
  }
}

void keynav_enter(void) {
  if (!keynav_state.in_range) {
    keynav_state.in_range = true;
    keynav_flush_state();
  }
}

void keynav_flush_state(void) {
  const bool in_range = keynav_active() && keynav_state.in_range;

  const bool pointer_moved = digitizer_state.x != keynav_state.x.coord ||
                             digitizer_state.y != keynav_state.y.coord;
  const bool button_changed = digitizer_state.tip != keynav_state.button_1 ||
                              digitizer_state.barrel != keynav_state.button_2;
  const bool in_range_changed = digitizer_state.in_range != in_range;
  digitizer_state.dirty |= pointer_moved || button_changed || in_range_changed;

  const bool macro_blocked =
#ifdef DYNAMIC_MACRO_KEYNAV_ONLY_FINAL
      keynav_state.macro_blocked;
#else
      false;
#endif
  if (digitizer_state.dirty && !macro_blocked) {
    digitizer_state.x = keynav_state.x.coord;
    digitizer_state.y = keynav_state.y.coord;
    digitizer_state.tip = keynav_state.button_1;
    digitizer_state.barrel = keynav_state.button_2;
    digitizer_state.in_range = in_range;

    digitizer_flush();
  }
}

#ifdef KEYNAV_LAYER
layer_state_t layer_state_set_quantum_keynav(layer_state_t state) {
  if (IS_LAYER_ON_STATE(state, KEYNAV_LAYER)) {
    if (!keynav_active()) {
      keynav_start();
      keynav_enter();
    }
  } else if (keynav_active()) {
    keynav_end();
  }
  return state;
}
#endif // KEYNAV_LAYER

#if !defined(KEYNAV_NO_AUTO_LAYER_OFF) && defined(KEYNAV_LAYER)
void post_process_record_quantum_keynav(uint16_t keycode, keyrecord_t *record) {
  if (IS_LAYER_ON(KEYNAV_LAYER) && !record->event.pressed) {
    switch (keycode) {
    case QK_KEYNAV_BUTTON_1:
    case QK_KEYNAV_BUTTON_2:
#ifdef KEYNAV_MOUSE_BUTTONS_LAYER_OFF
    case QK_MOUSE_BUTTON_1:
    case QK_MOUSE_BUTTON_2:
    case QK_MOUSE_BUTTON_3:
#endif // KEYNAV_MOUSE_BUTTONS_LAYER_OFF
      layer_off(KEYNAV_LAYER);
    }
  }
  post_process_record_quantum_keynav_kb(keycode, record);
}
#endif // !defined(KEYNAV_NO_AUTO_LAYER_OFF) && defined(KEYNAV_LAYER)

void keynav_remap_quantum(uint16_t *keycode) {
  switch (*keycode) {
#ifdef KEYNAV_REMAP_MOUSE_MOVEMENT
  case QK_MOUSE_CURSOR_LEFT:
    *keycode = (get_mods() & MOD_MASK_SHIFT) ? QK_KEYNAV_MOVE_LEFT
                                             : QK_KEYNAV_CUT_LEFT;
    break;
  case QK_MOUSE_CURSOR_DOWN:
    *keycode = (get_mods() & MOD_MASK_SHIFT) ? QK_KEYNAV_MOVE_DOWN
                                             : QK_KEYNAV_CUT_DOWN;
    break;
  case QK_MOUSE_CURSOR_UP:
    *keycode =
        (get_mods() & MOD_MASK_SHIFT) ? QK_KEYNAV_MOVE_UP : QK_KEYNAV_CUT_UP;
    break;
  case QK_MOUSE_CURSOR_RIGHT:
    *keycode = (get_mods() & MOD_MASK_SHIFT) ? QK_KEYNAV_MOVE_RIGHT
                                             : QK_KEYNAV_CUT_RIGHT;
    break;
#endif // KEYNAV_REMAP_MOUSE_MOVEMENT
#ifdef KEYNAV_REMAP_MOUSE_BUTTONS
  case QK_MOUSE_BUTTON_1:
    *keycode = QK_KEYNAV_BUTTON_1;
    break;
  case QK_MOUSE_BUTTON_3:
    *keycode = QK_KEYNAV_BUTTON_2;
    break;
#endif // KEYNAV_REMAP_MOUSE_BUTTONS
  }
}

uint16_t keycode_at_keymap_location_raw(uint8_t layer_num, uint8_t row,
                                        uint8_t col);
uint16_t keynav_keycode_at_keymap_location(uint8_t layer_num, uint8_t row,
                                           uint8_t col) {
  uint16_t keycode = keycode_at_keymap_location_raw(layer_num, row, col);
#ifdef KEYNAV_LAYER
  if (layer_num == KEYNAV_LAYER)
#else
  if (keynav_active())
#endif
    keynav_remap_quantum(&keycode);
  return keycode;
}

#if !defined(KEYNAV_NO_HIJACK_KEYCODE_AT_KEYMAP_LOCATION) && \
    (defined(KEYNAV_REMAP_MOUSE_MOVEMENT) || \
     defined(KEYNAV_REMAP_MOUSE_BUTTONS))
uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row,
                                    uint8_t col) {
  return keynav_keycode_at_keymap_location(layer_num, row, col);
}
#endif // KEYNAV_NO_HIJACK_KEYCODE_AT_KEYMAP_LOCATION && (KEYNAV_REMAP_MOUSE_MOVEMENT || KEYNAV_REMAP_MOUSE_BUTTONS)

#ifdef DYNAMIC_MACRO_KEYNAV_ONLY_FINAL
bool pre_process_record_quantum_keynav(uint16_t keycode, keyrecord_t *record) {
  bool process_kb = pre_process_record_quantum_keynav_kb(keycode, record);
  if (!process_kb)
    return false;
  switch (keycode) {
    int before;
  case QK_DYNAMIC_MACRO_PLAY_1:
    before = macro_state & 1;
    macro_state |= 1;
    process_dynamic_macro(QK_DYNAMIC_MACRO_PLAY_1, record);
    macro_state &= ~before;
    return false;
  case QK_DYNAMIC_MACRO_PLAY_2:
    before = macro_state & 2;
    macro_state |= 2;
    process_dynamic_macro(QK_DYNAMIC_MACRO_PLAY_2, record);
    macro_state &= ~before;
    return false;
  }
  return true;
}
#endif // DYNAMIC_MACRO_KEYNAV_ONLY_FINAL

static float into_valid_percent(float val) {
  if (val > 1.0f)
    return 1.0f;
  if (val < 0.0f)
    return 0.0f;
  return val;
}

__attribute__((weak)) double nextafter(double x, double y) {
  if (y > 0) {
    return x + 1e-10;
  } else {
    return x - 1e-10;
  }
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

#ifdef DYNAMIC_MACRO_KEYNAV_ONLY_FINAL
bool process_dynamic_macro(uint16_t keycode, keyrecord_t *record);
#endif

bool process_record_quantum_keynav(uint16_t keycode, keyrecord_t *record) {
  bool process_kb = process_record_quantum_keynav_kb(keycode, record);
  if (!process_kb)
    return false;

  switch (keycode) {
  case QK_KEYNAV_BUTTON_1:
  case QK_KEYNAV_BUTTON_2:
#ifdef DYNAMIC_MACRO_KEYNAV_ONLY_FINAL
    if (keynav_state.macro_blocked) {
      keynav_state.macro_blocked = false;
      keynav_flush_state();
    }
#endif // DYNAMIC_MACRO_KEYNAV_ONLY_FINAL
    if (keycode == QK_KEYNAV_BUTTON_1) {
      keynav_state.button_1 = record->event.pressed != 0;
    } else {
      keynav_state.button_2 = record->event.pressed != 0;
    }
    keynav_flush_state();
    return true;
  }

  switch (keycode) {
    bool cut;
    keynav_direction_t dir;
  case QK_KEYNAV_CUT_LEFT:
    dir = KEYNAV_LEFT;
    if (false) {
    case QK_KEYNAV_CUT_DOWN:
      dir = KEYNAV_DOWN;
    }
    if (false) {
    case QK_KEYNAV_CUT_UP:
      dir = KEYNAV_UP;
    }
    if (false) {
    case QK_KEYNAV_CUT_RIGHT:
      dir = KEYNAV_RIGHT;
    }
    cut = true;
    if (false) {
    case QK_KEYNAV_MOVE_LEFT:
      dir = KEYNAV_LEFT;
      if (false) {
      case QK_KEYNAV_MOVE_DOWN:
        dir = KEYNAV_DOWN;
      }
      if (false) {
      case QK_KEYNAV_MOVE_UP:
        dir = KEYNAV_UP;
      }
      if (false) {
      case QK_KEYNAV_MOVE_RIGHT:
        dir = KEYNAV_RIGHT;
      }
      cut = false;
    }
    if (record->event.pressed)
      keynav_move(dir, cut);
    return true;
  case QK_KEYNAV_TOGGLE_ASSUMED_SCREEN_SIZE:
    if (record->event.pressed)
      keynav_state.assume_screen_size = !keynav_state.assume_screen_size;
    break;
#ifdef KEYNAV_LAYER
  case TO(KEYNAV_LAYER):
    if (!keynav_active())
      return true;
#endif // KEYNAV_LAYER
  case QK_KEYNAV_RESET:
    if (record->event.pressed)
      keynav_reset();
    break;
  }
  return true;
}
