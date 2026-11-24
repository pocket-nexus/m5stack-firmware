/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* Ethernet bring-up after M5CoreP4X-UserDemo: the PHY reset sequence of
 * main/main.cpp, the MAC and PHY configuration of
 * third_party_component/p4_ethernet_init/p4_ethernet_init.c and the pin
 * release of m5_corep4_tool's sleep_before_work. */
#include "m5_corep4x/ethernet.h"
#include "esp_check.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_phy_ip101.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5_corep4x/bsp.h"

static const char *TAG = "m5p4x_eth";

static esp_err_t reset_phy(void) {
  const gpio_config_t io = {
      .pin_bit_mask = 1ULL << M5_COREP4X_ETH_RESET,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "reset GPIO");
  gpio_set_level(M5_COREP4X_ETH_RESET, 0);
  vTaskDelay(pdMS_TO_TICKS(50));
  gpio_set_level(M5_COREP4X_ETH_RESET, 1);
  vTaskDelay(pdMS_TO_TICKS(100));
  return ESP_OK;
}

esp_err_t m5_corep4x_eth_new(esp_eth_handle_t *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out");
  *out = NULL;
  ESP_RETURN_ON_ERROR(reset_phy(), TAG, "PHY reset");

  eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
  phy_config.phy_addr = M5_COREP4X_ETH_PHY_ADDR;
  phy_config.reset_gpio_num = M5_COREP4X_ETH_RESET;

  eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
  emac_config.smi_gpio.mdc_num = M5_COREP4X_ETH_MDC;
  emac_config.smi_gpio.mdio_num = M5_COREP4X_ETH_MDIO;
  emac_config.clock_config.rmii.clock_mode = EMAC_CLK_EXT_IN;
  emac_config.clock_config.rmii.clock_gpio = M5_COREP4X_ETH_REF_CLK;
  emac_config.emac_dataif_gpio.rmii.tx_en_num = M5_COREP4X_ETH_TX_EN;
  emac_config.emac_dataif_gpio.rmii.txd0_num = M5_COREP4X_ETH_TXD0;
  emac_config.emac_dataif_gpio.rmii.txd1_num = M5_COREP4X_ETH_TXD1;
  emac_config.emac_dataif_gpio.rmii.crs_dv_num = M5_COREP4X_ETH_CRS_DV;
  emac_config.emac_dataif_gpio.rmii.rxd0_num = M5_COREP4X_ETH_RXD0;
  emac_config.emac_dataif_gpio.rmii.rxd1_num = M5_COREP4X_ETH_RXD1;

  esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
  esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
  esp_err_t err = ESP_ERR_NO_MEM;
  if (mac && phy) {
    const esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    err = esp_eth_driver_install(&config, out);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Ethernet driver install failed: %s", esp_err_to_name(err));
    *out = NULL;
    if (mac) {
      mac->del(mac);
    }
    if (phy) {
      phy->del(phy);
    }
  }
  return err;
}

esp_err_t m5_corep4x_eth_del(esp_eth_handle_t eth) {
  ESP_RETURN_ON_FALSE(eth, ESP_ERR_INVALID_ARG, TAG, "eth");
  esp_eth_mac_t *mac = NULL;
  esp_eth_phy_t *phy = NULL;
  esp_eth_get_mac_instance(eth, &mac);
  esp_eth_get_phy_instance(eth, &phy);
  ESP_RETURN_ON_ERROR(esp_eth_driver_uninstall(eth), TAG, "uninstall");
  if (mac) {
    mac->del(mac);
  }
  if (phy) {
    phy->del(phy);
  }
  return ESP_OK;
}

static void high_z(gpio_num_t pin) {
  gpio_reset_pin(pin);
  gpio_set_direction(pin, GPIO_MODE_INPUT);
  gpio_pullup_dis(pin);
  gpio_pulldown_dis(pin);
}

void m5_corep4x_eth_release_pins(void) {
  static const gpio_num_t pins[] = {
      M5_COREP4X_ETH_REF_CLK, M5_COREP4X_ETH_MDC,  M5_COREP4X_ETH_MDIO,
      M5_COREP4X_ETH_TX_EN,   M5_COREP4X_ETH_TXD0, M5_COREP4X_ETH_TXD1,
      M5_COREP4X_ETH_CRS_DV,  M5_COREP4X_ETH_RXD0, M5_COREP4X_ETH_RXD1,
  };
  gpio_reset_pin(M5_COREP4X_ETH_RESET);
  gpio_set_direction(M5_COREP4X_ETH_RESET, GPIO_MODE_OUTPUT);
  gpio_pullup_dis(M5_COREP4X_ETH_RESET);
  gpio_pulldown_dis(M5_COREP4X_ETH_RESET);
  gpio_set_level(M5_COREP4X_ETH_RESET, 0);
  for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
    high_z(pins[i]);
  }
}
