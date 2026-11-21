// SPDX-FileCopyrightText: 2021 M5Stack
// SPDX-License-Identifier: MIT
//
// Click and hold detection of the AtomS3 button, ported to C from
// M5Unified 0.3.0 src/utility/Button_Class.hpp and Button_Class.inl.
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  M5_ATOMS3_BUTTON_NOCHANGE = 0,
  M5_ATOMS3_BUTTON_CLICKED,
  M5_ATOMS3_BUTTON_HOLD,
  M5_ATOMS3_BUTTON_DECIDE_CLICK_COUNT,
} m5_atoms3_button_state_t;

typedef struct {
  uint32_t last_msec;
  uint32_t last_change;
  uint32_t last_raw_change;
  uint32_t last_clicked;
  uint16_t msec_debounce; // M5Unified default 10 ms
  uint16_t msec_hold;     // M5Unified default 500 ms
  uint16_t last_hold_period;
  m5_atoms3_button_state_t current_state;
  bool raw_press;
  uint8_t press; // 0 released, 1 pressed, 2 holding
  uint8_t old_press;
  uint8_t click_count;
} m5_atoms3_button_t;

// Configures GPIO41 as an input (the board has an external pull-up).
esp_err_t m5_atoms3_button_gpio_init(void);
// Raw level: true while the button is pressed.
bool m5_atoms3_button_read(void);

// Resets the state machine to the M5Unified defaults.
void m5_atoms3_button_init(m5_atoms3_button_t *button);
// Feeds one raw sample at `msec` (M5Unified Button_Class::setRawState) and
// returns the event of this sample: CLICKED on release before the hold
// threshold, HOLD once when the hold threshold is reached.
m5_atoms3_button_state_t m5_atoms3_button_update(m5_atoms3_button_t *button,
                                                 uint32_t msec, bool press);

#ifdef __cplusplus
}
#endif
