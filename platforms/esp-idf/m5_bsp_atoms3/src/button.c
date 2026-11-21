// SPDX-FileCopyrightText: 2021 M5Stack
// SPDX-License-Identifier: MIT
//
// Ported to C from M5Unified 0.3.0 src/utility/Button_Class.inl
// (Button_Class::setState, Button_Class::setRawState).
#include "m5_atoms3/button.h"

#include "driver/gpio.h"
#include "m5_atoms3/board.h"
#include <string.h>

esp_err_t m5_atoms3_button_gpio_init(void) {
  const gpio_config_t config = {
      .pin_bit_mask = 1ULL << M5_ATOMS3_BUTTON_PIN,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  return gpio_config(&config);
}

bool m5_atoms3_button_read(void) {
  return gpio_get_level(M5_ATOMS3_BUTTON_PIN) == 0;
}

void m5_atoms3_button_init(m5_atoms3_button_t *button) {
  memset(button, 0, sizeof(*button));
  button->msec_debounce = 10;
  button->msec_hold = 500;
}

static void set_state(m5_atoms3_button_t *b, uint32_t msec,
                      m5_atoms3_button_state_t state) {
  if (b->current_state == M5_ATOMS3_BUTTON_DECIDE_CLICK_COUNT) {
    b->click_count = 0;
  }
  b->last_msec = msec;
  bool timeout = (msec - b->last_clicked) > b->msec_hold;
  switch (state) {
  case M5_ATOMS3_BUTTON_NOCHANGE:
    if (timeout && !b->press && b->click_count) {
      if (b->old_press == 0 && b->current_state == M5_ATOMS3_BUTTON_NOCHANGE) {
        state = M5_ATOMS3_BUTTON_DECIDE_CLICK_COUNT;
      } else {
        b->click_count = 0;
      }
    }
    break;
  case M5_ATOMS3_BUTTON_CLICKED:
    ++b->click_count;
    b->last_clicked = msec;
    break;
  default:
    break;
  }
  b->current_state = state;
}

m5_atoms3_button_state_t m5_atoms3_button_update(m5_atoms3_button_t *b,
                                                 uint32_t msec, bool press) {
  m5_atoms3_button_state_t state = M5_ATOMS3_BUTTON_NOCHANGE;
  bool disable_db = (msec - b->last_msec) > b->msec_debounce;
  uint8_t old_press = b->press;
  b->old_press = old_press;
  if (b->raw_press != press) {
    b->raw_press = press;
    b->last_raw_change = msec;
  }
  if (disable_db || msec - b->last_raw_change >= b->msec_debounce) {
    if (press != (0 != old_press)) {
      b->last_change = msec;
    }
    if (press) {
      uint32_t hold_period = msec - b->last_change;
      b->last_hold_period = (uint16_t)hold_period;
      if (!old_press) {
        b->press = 1;
      } else if (old_press == 1 && hold_period >= b->msec_hold) {
        b->press = 2;
        state = M5_ATOMS3_BUTTON_HOLD;
      }
    } else {
      b->press = 0;
      if (old_press == 1) {
        state = M5_ATOMS3_BUTTON_CLICKED;
      }
    }
  }
  set_state(b, msec, state);
  return b->current_state;
}
