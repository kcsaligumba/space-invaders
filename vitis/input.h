// input.h
//
// Thin wrapper over Lab 6's lw_usb HID keyboard stack.  Maintains two views:
//   - "live" state    : updated every input_poll() call; tracks the keys
//                       currently in the most-recent USB HID report.
//   - "frame snapshot": frozen at input_snapshot() time; used by
//                       input_held() and input_pressed().  Edge detection
//                       compares this frame's snapshot to the previous
//                       frame's, so input_pressed() is reliable as long as
//                       input_snapshot() is called once per frame (vsync).
//
// Typical main-loop pattern:
//   while (1) {
//       input_poll();                       // every iteration -- feeds USB
//       if (frame_count_changed()) {
//           input_snapshot();               // once per frame
//           if (input_held(KEY_LEFT_ARROW))  player.x--;
//           if (input_pressed(KEY_SPACE))   fire_bullet();
//       }
//   }

#ifndef INPUT_H
#define INPUT_H

#include "xil_types.h"

// HID usage codes for the keys we care about (boot-protocol keyboard).
#define KEY_LEFT_ARROW   0x50
#define KEY_RIGHT_ARROW  0x4F
#define KEY_DOWN_ARROW   0x51
#define KEY_UP_ARROW     0x52
// WASD aliases.  W mirrors UP, A mirrors LEFT, S mirrors DOWN, D mirrors RIGHT.
#define KEY_W            0x1A
#define KEY_A            0x04
#define KEY_S            0x16
#define KEY_D            0x07
#define KEY_SPACE        0x2C
#define KEY_ENTER        0x28
#define KEY_R            0x15
#define KEY_P            0x13
#define KEY_ESC          0x29

// Zero all internal state.  Safe to call before USB_init(); also called
// after a USB drop to clear stuck-key ghosts.
void input_init(void);

// Run MAX3421E_Task() + USB_Task() and, if a new HID report arrived,
// update the live key state.  Call every main-loop iteration.
void input_poll(void);

// Promote live state into the frame snapshot.  Call once per frame
// (after vsync edge, before reading input_held / input_pressed).
void input_snapshot(void);

// Frame-snapshot accessors.
int  input_held(uint8_t hid_code);     // currently held this frame
int  input_pressed(uint8_t hid_code);  // up->down transition this frame

#endif // INPUT_H
