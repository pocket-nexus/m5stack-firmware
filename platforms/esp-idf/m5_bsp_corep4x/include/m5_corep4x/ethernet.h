/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* Ethernet: an IP101GRI PHY at MDIO address 1 on the ESP32-P4 EMAC over
 * RMII, as the factory's p4_ethernet_init.c sets it up. The 50 MHz RMII
 * reference clock is an input on GPIO32 (EMAC_CLK_EXT_IN). The L3B rail
 * powers the PHY. The driver is the registry component espressif/ip101. */
#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_eth_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

#define M5_COREP4X_ETH_PHY_ADDR 1
#define M5_COREP4X_ETH_REF_CLK GPIO_NUM_32
#define M5_COREP4X_ETH_TX_EN GPIO_NUM_33
#define M5_COREP4X_ETH_TXD0 GPIO_NUM_41
#define M5_COREP4X_ETH_TXD1 GPIO_NUM_35
#define M5_COREP4X_ETH_CRS_DV GPIO_NUM_28
#define M5_COREP4X_ETH_RXD0 GPIO_NUM_29
#define M5_COREP4X_ETH_RXD1 GPIO_NUM_30
#define M5_COREP4X_ETH_MDC GPIO_NUM_31
#define M5_COREP4X_ETH_MDIO GPIO_NUM_36
/* M5_COREP4X_ETH_RESET (bsp.h): GPIO52, active low. */

/* The factory's boot sequence: PHY reset (GPIO52) low for 50 ms, then high
 * and 100 ms for the PHY to start; then the EMAC and the IP101 driver are
 * installed (the driver pulses the reset again, 10 ms low and 10 ms
 * settle, and checks the PHY identifier). The driver is returned stopped;
 * esp_eth_start blocks for up to 4 s of autonegotiation without a cable.
 * Needs the L3B rail on; without the PHY the install fails. */
esp_err_t m5_corep4x_eth_new(esp_eth_handle_t *out);

/* Uninstalls a stopped driver (no netif glue attached) and deletes its MAC
 * and PHY objects. */
esp_err_t m5_corep4x_eth_del(esp_eth_handle_t eth);

/* The factory's Ethernet steps before deep sleep, also used while the L3B
 * rail is off: the PHY reset driven low, the RMII and SMI pins (GPIO32,
 * 31, 36, 33, 41, 35, 28, 29, 30) reset to inputs without pulls, so no pin
 * feeds the unpowered PHY. */
void m5_corep4x_eth_release_pins(void);

#ifdef __cplusplus
}
#endif
