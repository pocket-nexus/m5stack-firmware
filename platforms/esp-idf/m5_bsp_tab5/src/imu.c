/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* BMI270 through the Bosch Sensortec BMI270 SensorAPI (src/bmi270/), from
 * components/sensor_bmi270/src/accel_gyro_bmi270.c (init, enable_sensor,
 * motion_irq, get_data, clear_irq_int) and main/hal/components/hal_imu.cpp
 * (axis mapping and scaling) of M5Tab5-UserDemo. */
#include "bmi270.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "m5/tab5.h"
#include <string.h>

static const char *TAG = "m5_tab5_imu";

#define BMI270_ADDRESS 0x68
#define I2C_TIMEOUT_MS 100
#define BURST_LIMIT 32

static i2c_master_dev_handle_t device;
static struct bmi2_dev bmi;
static bool ready;

static int8_t bus_read(uint8_t reg, uint8_t *data, uint32_t length,
                       void *context) {
  (void)context;
  if (!data || length == 0 || length > BURST_LIMIT)
    return BMI2_E_COM_FAIL;
  return i2c_master_transmit_receive(device, &reg, 1, data, length,
                                     I2C_TIMEOUT_MS) == ESP_OK
             ? BMI2_OK
             : BMI2_E_COM_FAIL;
}

static int8_t bus_write(uint8_t reg, const uint8_t *data, uint32_t length,
                        void *context) {
  (void)context;
  uint8_t frame[BURST_LIMIT + 1];
  if (!data || length == 0 || length > BURST_LIMIT)
    return BMI2_E_COM_FAIL;
  frame[0] = reg;
  memcpy(frame + 1, data, length);
  return i2c_master_transmit(device, frame, length + 1, I2C_TIMEOUT_MS) ==
                 ESP_OK
             ? BMI2_OK
             : BMI2_E_COM_FAIL;
}

static void delay_us(uint32_t period, void *context) {
  (void)context;
  esp_rom_delay_us(period);
}

static esp_err_t check(int8_t result, const char *what) {
  if (result == BMI2_OK)
    return ESP_OK;
  ESP_LOGE(TAG, "%s: BMI2 error %d", what, result);
  return ESP_FAIL;
}

esp_err_t m5_tab5_imu_init(void) {
  if (ready)
    return ESP_OK;
  ESP_RETURN_ON_ERROR(m5_tab5_init(), TAG, "board");
  if (!device) {
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BMI270_ADDRESS,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(m5_tab5_i2c(), &config, &device), TAG,
        "BMI270");
  }
  bmi.intf = BMI2_I2C_INTF;
  bmi.read = bus_read;
  bmi.write = bus_write;
  bmi.delay_us = delay_us;
  bmi.read_write_len = 30;
  bmi.config_file_ptr = NULL;
  /* Uploads the 8 KiB feature configuration. */
  ESP_RETURN_ON_ERROR(check(bmi270_init(&bmi), "init"), TAG, "BMI270 init");
  m5_tab5_imu_clear_irq();

  struct bmi2_sens_config config[2] = {{.type = BMI2_ACCEL},
                                       {.type = BMI2_GYRO}};
  ESP_RETURN_ON_ERROR(check(bmi2_get_sensor_config(config, 2, &bmi), "config"),
                      TAG, "configuration");
  /* +-4 g: 8192 LSB/g; +-1000 dps: 32.768 LSB/dps. */
  config[0].cfg.acc.odr = BMI2_ACC_ODR_200HZ;
  config[0].cfg.acc.range = BMI2_ACC_RANGE_4G;
  config[1].cfg.gyr.odr = BMI2_GYR_ODR_200HZ;
  config[1].cfg.gyr.range = BMI2_GYR_RANGE_1000;
  ESP_RETURN_ON_ERROR(
      check(bmi270_set_sensor_config(config, 2, &bmi), "set config"), TAG,
      "configuration");
  const uint8_t sensors[2] = {BMI2_ACCEL, BMI2_GYRO};
  ESP_RETURN_ON_ERROR(check(bmi270_sensor_enable(sensors, 2, &bmi), "enable"),
                      TAG, "enable");
  ready = true;
  return ESP_OK;
}

/* hal_imu.cpp updateImuData: the board mounts the BMI270 rotated against the
 * landscape screen, and the factory divides by 835.92 * 10 (g) and
 * 32.768 * 10. */
esp_err_t m5_tab5_imu_read(m5_tab5_imu_reading_t *out) {
  ESP_RETURN_ON_FALSE(ready && out, ESP_ERR_INVALID_STATE, TAG, "arguments");
  struct bmi2_sens_data data = {0};
  ESP_RETURN_ON_ERROR(check(bmi2_get_sensor_data(&data, &bmi), "read"), TAG,
                      "read");
  out->accel_x = data.acc.y / 835.92f / 10.0f;
  out->accel_y = -data.acc.x / 835.92f / 10.0f;
  out->accel_z = -data.acc.z / 835.92f / 10.0f;
  out->gyro_x = data.gyr.y / 32.768f / 10.0f;
  out->gyro_y = data.gyr.x / 32.768f / 10.0f;
  out->gyro_z = -data.gyr.z / 32.768f / 10.0f;
  return ESP_OK;
}

/* accel_gyro_bmi270_clear_irq_int: both pins open drain, active low,
 * latched, which releases a latched wakeup line. */
esp_err_t m5_tab5_imu_clear_irq(void) {
  ESP_RETURN_ON_FALSE(device, ESP_ERR_INVALID_STATE, TAG, "not initialized");
  uint16_t status = 0;
  bmi2_get_int_status(&status, &bmi);
  struct bmi2_int_pin_config pins = {0};
  pins.pin_type = BMI2_INT_BOTH;
  pins.int_latch = BMI2_INT_LATCH;
  pins.pin_cfg[0].lvl = BMI2_INT_ACTIVE_LOW;
  pins.pin_cfg[0].od = BMI2_INT_OPEN_DRAIN;
  pins.pin_cfg[0].output_en = BMI2_INT_OUTPUT_ENABLE;
  pins.pin_cfg[0].input_en = BMI2_INT_INPUT_DISABLE;
  return check(bmi2_set_int_pin_config(&pins, &bmi), "clear interrupt");
}

/* accel_gyro_bmi270_motion_irq: any-motion with duration 0x32 and threshold
 * 0xFF, mapped to INT1 push-pull, active high, latched. */
esp_err_t m5_tab5_imu_enable_motion_irq(void) {
  ESP_RETURN_ON_FALSE(ready, ESP_ERR_INVALID_STATE, TAG, "not initialized");
  const uint8_t sensors[2] = {BMI2_ACCEL, BMI2_ANY_MOTION};
  ESP_RETURN_ON_ERROR(check(bmi270_sensor_enable(sensors, 2, &bmi), "enable"),
                      TAG, "any motion");
  struct bmi2_sens_config config = {.type = BMI2_ANY_MOTION};
  ESP_RETURN_ON_ERROR(
      check(bmi270_get_sensor_config(&config, 1, &bmi), "get config"), TAG,
      "any motion");
  config.cfg.any_motion.duration = 0x32;
  config.cfg.any_motion.threshold = 0xFF;
  ESP_RETURN_ON_ERROR(
      check(bmi270_set_sensor_config(&config, 1, &bmi), "set config"), TAG,
      "any motion");
  struct bmi2_int_pin_config pins = {0};
  ESP_RETURN_ON_ERROR(check(bmi2_get_int_pin_config(&pins, &bmi), "pins"), TAG,
                      "pins");
  pins.pin_type = BMI2_INT1;
  pins.pin_cfg[0].input_en = BMI2_INT_INPUT_DISABLE;
  pins.pin_cfg[0].lvl = BMI2_INT_ACTIVE_HIGH;
  pins.pin_cfg[0].od = BMI2_INT_PUSH_PULL;
  pins.pin_cfg[0].output_en = BMI2_INT_OUTPUT_ENABLE;
  pins.int_latch = BMI2_INT_LATCH;
  ESP_RETURN_ON_ERROR(check(bmi2_set_int_pin_config(&pins, &bmi), "pins"), TAG,
                      "pins");
  const struct bmi2_sens_int_config map = {.type = BMI2_ANY_MOTION,
                                           .hw_int_pin = BMI2_INT1};
  return check(bmi270_map_feat_int(&map, 1, &bmi), "map");
}
