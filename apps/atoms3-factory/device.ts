// Device state module: every host-provided value and every request to the
// host (docs/APP-HOST.md). The board publishes values under these field
// names (m5_pocket_bridge_*); in JS mode main.tsx delivers them through the
// host* functions, in AOT mode the Rust host writes device_<name>_mut().
// Without a board (desktop and web previews) `attached` stays false and the
// root model simulates the values (simulate* below).
import { clear, copy, f32, i32, push, sin, type f64 } from "@pocketjs/framework/solid/std";

// Board -> app ---------------------------------------------------------------

// 1 once the board publishes; the model then stops simulating.
export let attached: boolean = false;
// IR address: the low 16 bits of (mac[2..5]), as the factory's ir_addr.
export let irAddress: i32 = 0;
// IMU sample [ax, ay, az] in g and [gx, gy, gz] in dps, published every
// frame while the IMU function runs; imuSamples counts deliveries.
export let imu: f32[] = [];
export let imuSamples: i32 = 0;
// Wi-Fi scan: "wifi1".."wifi5" hold SSIDs, wifiRssi their RSSI, event
// "wifiDone" carries the network count (-1: scan failed).
export let wifiSsids: string[] = ["", "", "", "", ""];
export let wifiRssi: f32[] = [];
export let wifiDone: i32 = 0;
export let wifiCount: i32 = 0;
// Port A I2C scan: addresses, event "i2cDone" with their count.
export let i2cAddresses: f32[] = [];
export let i2cDone: i32 = 0;
// Bytes: event "uartRx" (Grove -> USB), "uartTx" (USB -> Grove), "usbRx"
// (USB bytes for the PWM and IR functions). Queued until the model reads.
export let uartRx: i32[] = [];
export let uartTx: i32[] = [];
export let usbRx: i32[] = [];
// Port A ADC means of G1 and G2, raw 12-bit, after an "adc" request.
export let adc: f32[] = [];
export let adcSamples: i32 = 0;

// App -> board: request strings "<name> <arg>...", drained after each frame.
//   enter <function> / leave <function>   start or stop a function's hardware
//   wifiScan, i2cScan                      start a scan
//   uart <baud> <rxPin> <txPin>            (re)open the Port A UART bridge
//   pwm <dutyG1> <dutyG2>                  Port A PWM duties (0..255)
//   adc                                    read both Port A ADC channels
//   ir <address> <command>                 send one NEC frame + repeat
export let requests: string[] = [];

export function request(command: string): void {
  push(requests, command);
}

export function hostNumber(name: string, value: f64): void {
  switch (name) {
    case "attached":
      attached = value !== 0;
      break;
    case "irAddress":
      irAddress = i32(value);
      break;
  }
}

export function hostText(name: string, value: string): void {
  switch (name) {
    case "wifi1":
      wifiSsids[0] = value;
      break;
    case "wifi2":
      wifiSsids[1] = value;
      break;
    case "wifi3":
      wifiSsids[2] = value;
      break;
    case "wifi4":
      wifiSsids[3] = value;
      break;
    case "wifi5":
      wifiSsids[4] = value;
      break;
  }
}

export function hostArray(name: string, values: f32[]): void {
  switch (name) {
    case "imu":
      imu = values;
      imuSamples = imuSamples + 1;
      break;
    case "wifiRssi":
      wifiRssi = values;
      break;
    case "i2cAddresses":
      i2cAddresses = values;
      break;
    case "adc":
      adc = values;
      adcSamples = adcSamples + 1;
      break;
  }
}

export function hostEvent(name: string, value: f64): void {
  switch (name) {
    case "wifiDone":
      wifiCount = i32(value);
      wifiDone = wifiDone + 1;
      break;
    case "i2cDone":
      i2cDone = i2cDone + 1;
      break;
    case "uartRx":
      push(uartRx, i32(value));
      break;
    case "uartTx":
      push(uartTx, i32(value));
      break;
    case "usbRx":
      push(usbRx, i32(value));
      break;
  }
}

export function takeRequests(): string[] {
  const taken = copy(requests);
  clear(requests);
  return taken;
}

// Simulated board, used while `attached` is false ----------------------------

export function simulateWifi(): void {
  wifiSsids = ["M5Stack", "AtomS3-Lab", "Guest", "Office-2.4G", "IoT"];
  wifiRssi = [-38.0, -52.0, -61.0, -70.0, -83.0];
  wifiCount = 9;
  wifiDone = wifiDone + 1;
}

export function simulateI2c(): void {
  i2cAddresses = [56.0, 104.0];
  i2cDone = i2cDone + 1;
}

export function simulateImu(frame: i32): void {
  const t = f32(frame) * 0.05;
  imu = [0.45 * sin(t), 0.45 * sin(t * 0.7 + 1.0), 0.9, 0.0, 0.0, 0.0];
  imuSamples = imuSamples + 1;
}

export function simulateAdc(): void {
  adc = [1985.0, 620.0];
  adcSamples = adcSamples + 1;
}
