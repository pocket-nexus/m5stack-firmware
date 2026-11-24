/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * PM1 events and deep sleep, after m5_corep4_tool::p4_pm1_irq_init,
 * sleep_before_work (the rail steps) and enter_deep_sleep in
 * components/m5_corep4_component/m5_corep4_component.cpp of
 * M5CoreP4X-UserDemo.
 */
#include "board.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/pmu_ll.h"
#include "ioe1.h"
#include "m5_corep4x/bsp.h"
#include "pm1.h"
#include "soc/soc_caps.h"

static const char *TAG = "m5p4x_power";

/* IRQ_MASK1 bits of PM1 GPIO0..4 and the "all" value of
 * M5PM1::irqSetGpioMaskAll. */
#define GPIO_MASK_ALL 0x1F
#define SYS_MASK_ALL 0x3F

/* Reset lines held low through deep sleep: the IP101 PHY and the ESP32-C6
 * socket of the optional Wi-Fi module. */
static const gpio_num_t s_reset_pins[] = {M5_COREP4X_ETH_RESET,
                                          M5_COREP4X_C6_RESET};

static void (*s_on_irq)(void *context);
static void *s_irq_context;

bool m5_corep4x_pmic_ready(void) { return m5_corep4x_pm1_ready(); }

bool m5_corep4x_expander_ready(void) { return m5_corep4x_ioe1_ready(); }

static void output_low(gpio_num_t pin) {
  gpio_reset_pin(pin);
  gpio_set_direction(pin, GPIO_MODE_OUTPUT);
  gpio_pullup_dis(pin);
  gpio_pulldown_dis(pin);
  gpio_set_level(pin, 0);
}

void m5_corep4x_gpio_high_z(gpio_num_t pin) {
  if (pin == GPIO_NUM_NC || !GPIO_IS_VALID_GPIO(pin)) {
    return;
  }
  gpio_reset_pin(pin);
  gpio_set_direction(pin, GPIO_MODE_INPUT);
  gpio_pullup_dis(pin);
  gpio_pulldown_dis(pin);
}

/* corep4_unhold_sleep_reset_pins: low first, then release the hold, so the
 * external pull-ups never release the resets in between. */
void m5_corep4x_release_sleep_holds(void) {
#if !SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP
  gpio_deep_sleep_hold_dis();
#endif
  pmu_ll_hp_set_hold_all_hp_pad(&PMU, PMU_MODE_HP_SLEEP, false);
  for (size_t i = 0; i < sizeof(s_reset_pins) / sizeof(s_reset_pins[0]); i++) {
    output_low(s_reset_pins[i]);
    gpio_hold_dis(s_reset_pins[i]);
  }
}

esp_err_t m5_corep4x_wake_source(uint8_t *source) {
  ESP_RETURN_ON_FALSE(source, ESP_ERR_INVALID_ARG, TAG, "source");
  ESP_RETURN_ON_ERROR(
      m5_corep4x_pm1_read(M5_COREP4X_PM1_REG_WAKE_SRC, source, 1), TAG,
      "WAKE_SRC");
  return m5_corep4x_pm1_write(M5_COREP4X_PM1_REG_WAKE_SRC, 0x00);
}

static void IRAM_ATTR pmic_isr(void *arg) {
  (void)arg;
  if (s_on_irq) {
    s_on_irq(s_irq_context);
  }
}

esp_err_t m5_corep4x_pmic_irq_start(void (*on_irq)(void *context),
                                    void *context) {
  ESP_RETURN_ON_FALSE(on_irq, ESP_ERR_INVALID_ARG, TAG, "on_irq");
  ESP_RETURN_ON_FALSE(m5_corep4x_pm1_ready(), ESP_ERR_INVALID_STATE, TAG,
                      "no PM1");
  s_on_irq = on_irq;
  s_irq_context = context;

  gpio_reset_pin(M5_COREP4X_PMIC_IRQ);
  gpio_set_direction(M5_COREP4X_PMIC_IRQ, GPIO_MODE_INPUT);
  gpio_set_intr_type(M5_COREP4X_PMIC_IRQ, GPIO_INTR_NEGEDGE);
  esp_err_t err = gpio_install_isr_service(0);
  ESP_RETURN_ON_FALSE(err == ESP_OK || err == ESP_ERR_INVALID_STATE, err, TAG,
                      "ISR service");
  ESP_RETURN_ON_ERROR(gpio_isr_handler_add(M5_COREP4X_PMIC_IRQ, pmic_isr, NULL),
                      TAG, "ISR handler");

  /* Single-click reset off; double-click power-off left at the PM1
   * default. GPIO IRQs masked but GPIO0 (IMU), system IRQs masked, the
   * button IRQs at their default; GPIO and system flags cleared. */
  esp_err_t first = ESP_OK;
  esp_err_t e[] = {
      m5_corep4x_pm1_update(M5_COREP4X_PM1_REG_BTN_CFG_1, 0x01, 0x01),
      m5_corep4x_pm1_write(M5_COREP4X_PM1_REG_IRQ_MASK1,
                           GPIO_MASK_ALL & ~(1 << M5_COREP4X_PM1_IMU_INT)),
      m5_corep4x_pm1_write(M5_COREP4X_PM1_REG_IRQ_MASK2, SYS_MASK_ALL),
      m5_corep4x_pm1_write(M5_COREP4X_PM1_REG_IRQ_STATUS1, 0x00),
      m5_corep4x_pm1_write(M5_COREP4X_PM1_REG_IRQ_STATUS2, 0x00),
      /* PM1 GPIO2 is the IRQ output: pull-up, push-pull. */
      m5_corep4x_pm1_gpio_function(M5_COREP4X_PM1_IRQ, M5_COREP4X_PM1_FUNC_IRQ),
      m5_corep4x_pm1_gpio_pull(M5_COREP4X_PM1_IRQ, 1),
      m5_corep4x_pm1_gpio_drive(M5_COREP4X_PM1_IRQ, false),
      /* PM1 GPIO0 takes the IMU interrupt: GPIO function, input. */
      m5_corep4x_pm1_gpio_function(M5_COREP4X_PM1_IMU_INT,
                                   M5_COREP4X_PM1_FUNC_GPIO),
      m5_corep4x_pm1_gpio_input(M5_COREP4X_PM1_IMU_INT),
  };
  for (size_t i = 0; i < sizeof(e) / sizeof(e[0]); i++) {
    if (first == ESP_OK && e[i] != ESP_OK) {
      first = e[i];
    }
  }
  if (first != ESP_OK) {
    ESP_LOGW(TAG, "PM1 IRQ setup: %s", esp_err_to_name(first));
  }

  /* A deep-sleep wake leaves the button IRQ of the wake press pending. */
  if (esp_reset_reason() == ESP_RST_DEEPSLEEP) {
    m5_corep4x_pm1_write(M5_COREP4X_PM1_REG_IRQ_STATUS3, 0x00);
    vTaskDelay(pdMS_TO_TICKS(500));
    m5_corep4x_pm1_update(M5_COREP4X_PM1_REG_PWR_CFG,
                          M5_COREP4X_PM1_PWR_CFG_LED_CTRL, 0);
  }
  return first;
}

esp_err_t m5_corep4x_pmic_irq_take(uint8_t *button, uint8_t *gpio) {
  ESP_RETURN_ON_FALSE(button && gpio, ESP_ERR_INVALID_ARG, TAG, "arg");
  *button = 0;
  *gpio = 0;
  ESP_RETURN_ON_ERROR(
      m5_corep4x_pm1_read(M5_COREP4X_PM1_REG_IRQ_STATUS3, button, 1), TAG,
      "button IRQ");
  ESP_RETURN_ON_ERROR(
      m5_corep4x_pm1_write(M5_COREP4X_PM1_REG_IRQ_STATUS3, 0x00), TAG,
      "button IRQ clear");
  ESP_RETURN_ON_ERROR(
      m5_corep4x_pm1_read(M5_COREP4X_PM1_REG_IRQ_STATUS1, gpio, 1), TAG,
      "GPIO IRQ");
  return m5_corep4x_pm1_write(M5_COREP4X_PM1_REG_IRQ_STATUS1, 0x00);
}

esp_err_t m5_corep4x_sleep_rails_off(void) {
  /* The ESP32-C6 socket: reset low, wake and SDIO lines released. */
  output_low(M5_COREP4X_C6_RESET);
  m5_corep4x_gpio_high_z(M5_COREP4X_C6_WAKE);
  for (int pin = M5_COREP4X_C6_SDIO_FIRST; pin <= M5_COREP4X_C6_SDIO_LAST;
       pin++) {
    m5_corep4x_gpio_high_z((gpio_num_t)pin);
  }
  static const m5_corep4x_switch_t rails[] = {
      M5_COREP4X_SWITCH_SPEAKER,  M5_COREP4X_SWITCH_AUDIO,
      M5_COREP4X_SWITCH_GROVE,    M5_COREP4X_SWITCH_USB_C,
      M5_COREP4X_SWITCH_USB_A,    M5_COREP4X_SWITCH_L3B,
      M5_COREP4X_SWITCH_M5BUS_5V,
  };
  esp_err_t first = ESP_OK;
  for (size_t i = 0; i < sizeof(rails) / sizeof(rails[0]); i++) {
    const esp_err_t err = m5_corep4x_set_switch(rails[i], false);
    if (first == ESP_OK && err != ESP_OK) {
      first = err;
    }
  }
  vTaskDelay(pdMS_TO_TICKS(100));
  const esp_err_t pm1 = m5_corep4x_pm1_i2c_sleep(3);
  const esp_err_t ioe1 = m5_corep4x_ioe1_i2c_sleep(3);
  if (first == ESP_OK) {
    first = pm1 != ESP_OK ? pm1 : ioe1;
  }
  return first;
}

static esp_err_t pm1_wake_config(const m5_corep4x_wake_t *wake) {
  ESP_RETURN_ON_ERROR(
      m5_corep4x_pm1_gpio_wake(M5_COREP4X_PM1_IMU_INT, wake->imu, false), TAG,
      "IMU wake");
  /* The PM1 timer is not a wake source here; clear what may be left. */
  ESP_RETURN_ON_ERROR(m5_corep4x_pm1_write(M5_COREP4X_PM1_REG_TIM_CFG, 0x00),
                      TAG, "TIM_CFG");
  ESP_RETURN_ON_ERROR(m5_corep4x_pm1_write(M5_COREP4X_PM1_REG_TIM_KEY, 0xA5),
                      TAG, "TIM_KEY");
  /* The RX8130 timer pulls PM1 GPIO4 low. */
  ESP_RETURN_ON_ERROR(m5_corep4x_pm1_gpio_input(M5_COREP4X_PM1_RTC_INT), TAG,
                      "RTC INT input");
  ESP_RETURN_ON_ERROR(m5_corep4x_pm1_gpio_pull(M5_COREP4X_PM1_RTC_INT, 1), TAG,
                      "RTC INT pull");
  ESP_RETURN_ON_ERROR(
      m5_corep4x_pm1_gpio_wake(M5_COREP4X_PM1_RTC_INT, wake->rtc, false), TAG,
      "RTC wake");
  /* IRQ masks: GPIO all but the wake pins, system all, buttons none ("PM1
   * button wake always on"). */
  uint8_t gpio_mask = GPIO_MASK_ALL;
  if (wake->imu) {
    gpio_mask &= (uint8_t)~(1 << M5_COREP4X_PM1_IMU_INT);
  }
  if (wake->rtc) {
    gpio_mask &= (uint8_t)~(1 << M5_COREP4X_PM1_RTC_INT);
  }
  ESP_RETURN_ON_ERROR(
      m5_corep4x_pm1_write(M5_COREP4X_PM1_REG_IRQ_MASK1, gpio_mask), TAG,
      "GPIO mask");
  ESP_RETURN_ON_ERROR(
      m5_corep4x_pm1_write(M5_COREP4X_PM1_REG_IRQ_MASK2, SYS_MASK_ALL), TAG,
      "system mask");
  ESP_RETURN_ON_ERROR(m5_corep4x_pm1_write(M5_COREP4X_PM1_REG_IRQ_MASK3, 0x00),
                      TAG, "button mask");
  m5_corep4x_pm1_write(M5_COREP4X_PM1_REG_IRQ_STATUS1, 0x00);
  m5_corep4x_pm1_write(M5_COREP4X_PM1_REG_IRQ_STATUS2, 0x00);
  m5_corep4x_pm1_write(M5_COREP4X_PM1_REG_IRQ_STATUS3, 0x00);
  return ESP_OK;
}

esp_err_t m5_corep4x_deep_sleep(const m5_corep4x_wake_t *wake) {
  ESP_RETURN_ON_FALSE(wake, ESP_ERR_INVALID_ARG, TAG, "wake");
  /* EXT1 any-low: the PM1 IRQ (button, RTC, IMU) always, the CST3530
   * interrupt with touch wake. The touch controller stays powered. */
  uint64_t mask = 1ULL << M5_COREP4X_PMIC_IRQ;
  if (wake->touch) {
    mask |= 1ULL << M5_COREP4X_TOUCH_INT;
  }
  ESP_RETURN_ON_ERROR(
      esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW), TAG, "EXT1");
  ESP_RETURN_ON_ERROR(pm1_wake_config(wake), TAG, "PM1 wake");
  /* The PM1 releases its open-drain IRQ line within 20 ms. */
  vTaskDelay(pdMS_TO_TICKS(20));
  ESP_LOGI(TAG, "deep sleep: rtc %d touch %d imu %d, PM1 IRQ level %d",
           wake->rtc, wake->touch, wake->imu,
           gpio_get_level(M5_COREP4X_PMIC_IRQ));

  /* No I2C after this: release SDA/SCL so they do not feed the unpowered
   * L3B devices. */
  m5_corep4x_gpio_high_z(M5_COREP4X_I2C_SCL);
  m5_corep4x_gpio_high_z(M5_COREP4X_I2C_SDA);
  if (!wake->touch) {
    m5_corep4x_gpio_high_z(M5_COREP4X_TOUCH_INT);
  } else {
    /* The CST3530 interrupt is an active-low open drain. */
    rtc_gpio_pulldown_dis(M5_COREP4X_TOUCH_INT);
    rtc_gpio_pullup_en(M5_COREP4X_TOUCH_INT);
  }
  /* Hold only the reset lines low; the PMU HP_SLEEP hold-all keeps the HP
   * pads in the sleep state without freezing flash and UART now. */
  for (size_t i = 0; i < sizeof(s_reset_pins) / sizeof(s_reset_pins[0]); i++) {
    output_low(s_reset_pins[i]);
    gpio_hold_en(s_reset_pins[i]);
  }
#if !SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP
  gpio_deep_sleep_hold_en();
#endif
  pmu_ll_hp_set_hold_all_hp_pad(&PMU, PMU_MODE_HP_SLEEP, true);
  esp_deep_sleep_start();
}
