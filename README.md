This is a generic version of [keynav](https://github.com/jordansissel/keynav),
but installed on a QMK keyboard instead of as a program on the computer.

## Pros compared to keynav:
- It should be more generic, working the same way between X11, Windows, and probably (albeit untested) Wayland and OS X.

## Cons compared to keynav:
- There is no visual indicator of where the mouse pointer can
  jump.
- The mouse is instantly moved instead of only moving an indicator. This is
  quite problematic if you have focus following the mouse pointer. This could
  be mitigated by disabling focus follows mouse when the `proximity-in` event
  is received, and turning it back on when `proximity-out` is received.
- Not compatible with keynav config files.
- The linux operation is unfortunately not plug and play at this time. To make
  this run with X11 (and probably Wayland), see below.

## How to make this work with Linux (with libinput)
`/usr/lib/udev/hwdb.d/60-tablet-resolution.hwdb`: (see [`remarkable_mouse#18`](https://github.com/Evidlo/remarkable_mouse/issues/18#issuecomment-576887912))
```
evdev:YOUR_SYS_INPUT_MODALIAS_GOES_HERE
 EVDEV_ABS_00=::20
 EVDEV_ABS_01=::20
```

`/usr/local/lib/udev/rules.d/80-tables.rules`: *(Add more granular tablet
detection if you use other tablets on the same computer)*
```udev
ENV{ID_INPUT_TABLET}!="1", GOTO="not_a_tablet"
ENV{LIBINPUT_DEVICE_CAP_TABLET_TOOL}="1"
ENV{LIBINPUT_TABLET_TOOL_TYPE_PEN}="1"
LABEL="not_a_tablet"
```

Available options:
- `KEYNAV_LAYER`: If you dedicate a layer to keynav, it will automatically
  enable/disable keynav when you activate/deactivate the specified layer,
  otherwise you have to manually do it with `keynav_start()` and
  `keynav_end()`.
- `KEYNAV_NO_AUTO_LAYER_OFF`: Default behavior when a `KEYNAV_LAYER` is defined
  is to deactivate this layer when `KC_KEYNAV_BTN1` or `KC_KEYNAV_BTN2` is
  pressed. With this option defined, the layer will stay active.
- `KEYNAV_REMAP_MOUSE`: Defining this will add a `pre_process_record` function
  that remaps mouse movement buttons (e.g. `KC_MS_UP`) to keynav buttons (e.g.
  `KC_KEYNAV_UP`) and primary and secondary mouse buttons to keynav buttons.
  This is useful when you use a dedicated keynav layer and a mapping tool
  without support for custom keys (such as [ORYX](https://configure.zsa.io)).
- `KEYNAV_ASSUME_MAX_HEIGHT` and `KEYNAV_ASSUME_MAX_WIDTH`: The keyboard is not
  aware of the resolution of your desktop. Defining an assumed resolution makes
  it so that the pointer doesn't go (very far) into subpixel resolution.
- `KEYNAV_PROXIMITY_INTERVAL`: To mitigate bugs from tablets that doesn't
  always return `proximity-out` events, [libinput will
  assume](https://who-t.blogspot.com/2019/06/libinput-and-tablet-proximity-handling.html)
  that the touchscreen has lost proximity if it gets no status updates within
  50 ms (on my machine). To avoid timeouts, the keyboard will send updates at
  an interval of ~20 ms. If you want another interval, then you can set it with
  this option.

Available keys:
- Select a subwindow of the current navigation window:
  - `KC_KEYNAV_LEFT`
  - `KC_KEYNAV_DOWN`
  - `KC_KEYNAV_UP`
  - `KC_KEYNAV_RIGHT`
- Move the keynav window without changing the size of the window:
  - `KC_KEYNAV_SHIFT_LEFT`
  - `KC_KEYNAV_SHIFT_DOWN`
  - `KC_KEYNAV_SHIFT_UP`
  - `KC_KEYNAV_SHIFT_RIGHT`
- `KC_KEYNAV_BTN1` (also known as `TABLET_TOOL_TIP`)
- `KC_KEYNAV_BTN2` (also known as `TABLET_TOOL_BUTTON`/`BTN_STYLUS`)
- `KC_KEYNAV_RESET`: Reset the window to the full size and move the mouse to the center of the window.
