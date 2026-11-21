// The Tab5 factory demo views. Positions are the factory firmware's 1280x720
// layout halved onto the 640x360 logical viewport (raster density 2).
import { Show } from "solid-js";
import { For, Image, Text, View } from "@pocketjs/framework/solid/components";
import { onMount } from "@pocketjs/framework/solid/lifecycle";
import Chart from "./Chart.tsx";
import Spinner from "./Spinner.tsx";
import WindowFrame from "./WindowFrame.tsx";
import {
  accelXText,
  accelYText,
  accelZText,
  antennaExternal,
  applyRtc,
  brightness,
  brightnessDown,
  brightnessShift,
  brightnessUp,
  calendarCells,
  calendarMonth,
  calendarYear,
  cameraFullscreen,
  cameraMessage,
  cameraVisible,
  cancelPower,
  charging,
  chargeOn,
  chartA,
  chartB,
  comLines,
  countdown,
  cpuText,
  currentText,
  dateText,
  dismissToast,
  dismissWindow,
  dotScale,
  dotShiftX,
  dotShiftY,
  dropdown,
  ext5vOn,
  extPortRows,
  extPortScroll,
  gpioReady,
  headphoneDetected,
  i2cFound,
  i2cInternal,
  logo5Left,
  logoTabOpacity,
  logoTabTop,
  mbusRows,
  mbusScroll,
  monthName,
  monthOptions,
  musicOn,
  openPanel,
  panelName,
  pickDay,
  pickMonth,
  pickYear,
  powerOff,
  quickChargeOn,
  recordState,
  rollerHour,
  rollerMinute,
  rollerSecond,
  rtcReady,
  spinnerAngle,
  scanInternal,
  scanPortA,
  screen,
  sdEntries,
  sdError,
  sdMessage,
  selectedDay,
  selectedMonth,
  selectedYear,
  sendHello,
  showWifiInfo,
  sleepRtc,
  sleepShake,
  sleepTouch,
  start,
  startDualMicTest,
  startHeadphoneTest,
  timeText,
  toasts,
  toggleAntenna,
  toggleCameraSize,
  toggleCharge,
  toggleDropdown,
  toggleExt5v,
  toggleExtPortPin,
  toggleMbusPin,
  toggleMusic,
  togglePortAPower,
  toggleQuickCharge,
  toggleUsbA5v,
  usbA5vOn,
  usbADetected,
  usbCDetected,
  voltageText,
  volume,
  volumeDown,
  volumeShift,
  volumeUp,
  windows,
  yearOptions,
} from "./app";

export default function App() {
  onMount(start);
  return (
    <View debugName="Tab5Factory" class="w-full h-full overflow-hidden bg-white">
      <Show when={screen() === 0}>
        {/* AppStartupAnim: white screen, "Tab" rises and fades in, "5" slides in. */}
        <View debugName="Startup" class="absolute left-0 top-0 w-[640] h-[360] bg-white">
          <View class="absolute left-[252] top-0 w-[128] h-[64]" style={{ insetT: logoTabTop(), opacity: logoTabOpacity() }}>
            <Image class="w-[128] h-[64]" src="assets/logo-tab.png" />
          </View>
          <View class="absolute left-0 top-[154] w-[32] h-[64]" style={{ insetL: logo5Left() }}>
            <Image class="w-[32] h-[64]" src="assets/logo-5.png" />
          </View>
          <View class="absolute left-[590] top-[332] w-[40] h-[16] items-center justify-center">
            <Text class="text-xs text-[#CCCCCC]">V0.5</Text>
          </View>
        </View>
      </Show>
      <Show when={screen() === 1}>
        <View debugName="Launcher" class="absolute left-0 top-0 w-[640] h-[360]">
          {/* launcher_bg, cut into 512 px textures */}
          <Image class="absolute left-0 top-0 w-[256] h-[256]" src="assets/launcher-bg-0-0.png" />
          <Image class="absolute left-[256] top-0 w-[256] h-[256]" src="assets/launcher-bg-0-1.png" />
          <Image class="absolute left-[512] top-0 w-[128] h-[256]" src="assets/launcher-bg-0-2.png" />
          <Image class="absolute left-0 top-[256] w-[256] h-[128]" src="assets/launcher-bg-1-0.png" />
          <Image class="absolute left-[256] top-[256] w-[256] h-[128]" src="assets/launcher-bg-1-1.png" />
          <Image class="absolute left-[512] top-[256] w-[128] h-[128]" src="assets/launcher-bg-1-2.png" />

          {/* PanelRtc */}
          <View class="absolute left-[437.5] top-[47.5] w-[100] h-[16] items-center justify-center">
            <Text class="text-xs text-[#D86037]">{timeText()}</Text>
          </View>
          <View class="absolute left-[437.5] top-[61.5] w-[100] h-[14] items-center justify-center">
            <Text class="text-2xs text-[#D86037]">{dateText()}</Text>
          </View>
          <View debugName="RtcButton" class="absolute left-[505.25] top-[42.25] w-[51.5] h-[47.5]" focusable onPress={() => openPanel(0)} />

          {/* PanelLcdBacklight */}
          <View class="absolute left-[597.5] top-[221.5] w-[40] h-[24] items-center justify-center" style={{ translateY: brightnessShift() }}>
            <Text class="text-lg text-[#FEFEFE]">{brightness()}</Text>
          </View>
          <View class="absolute right-[3.5] top-[210] w-[150] h-[14] items-end flex-col justify-center">
            <Text class="text-2xs text-[#55866D]">{panelName()}</Text>
          </View>
          <View debugName="BrightnessUp" class="absolute left-[552.25] top-[246.75] w-[40.5] h-[42.5]" focusable onPress={brightnessUp} />
          <View debugName="BrightnessDown" class="absolute left-[599.25] top-[246.75] w-[40.5] h-[42.5]" focusable onPress={brightnessDown} />

          {/* PanelSpeakerVolume */}
          <View class="absolute left-[608] top-[295.5] w-[30] h-[16] items-center justify-center" style={{ translateY: volumeShift() }}>
            <Text class="text-xs text-[#FEFEFE]">{volume()}</Text>
          </View>
          <View debugName="VolumeUp" class="absolute left-[552.25] top-[315.75] w-[40.5] h-[42.5]" focusable onPress={volumeUp} />
          <View debugName="VolumeDown" class="absolute left-[599.25] top-[315.75] w-[40.5] h-[42.5]" focusable onPress={volumeDown} />

          {/* PanelPowerMonitor */}
          <View class="absolute right-[221] top-[4.5] w-[80] h-[16] items-end flex-col justify-center">
            <Text class="text-xs text-[#333333]">{voltageText()}</Text>
          </View>
          <View class="absolute right-[221] top-[20.5] w-[80] h-[16] items-end flex-col justify-center">
            <Text class="text-xs text-[#333333]">{currentText()}</Text>
          </View>
          <View class="absolute left-[292.5] top-[214] w-[30] h-[14] items-center justify-center">
            <Text class="text-2xs text-[#535353]">{cpuText()}</Text>
          </View>
          <Show when={charging()}>
            <Image class="absolute left-[458] top-[34.5] w-[16] h-[16]" src="assets/chg-arrow-up.png" />
            <Image class="absolute left-[423] top-[43] w-[16] h-[16]" src="assets/chg-arrow-down.png" />
          </Show>

          {/* PanelImu */}
          <View class="absolute left-[509.5] top-[249.5] w-[60] h-[14] flex-col justify-center">
            <Text class="text-2xs text-[#606060]">{accelXText()}</Text>
          </View>
          <View class="absolute left-[475.5] top-[282] w-[60] h-[14] flex-col justify-center">
            <Text class="text-2xs text-[#606060]">{accelYText()}</Text>
          </View>
          <View class="absolute left-[509.5] top-[295.5] w-[60] h-[14] flex-col justify-center">
            <Text class="text-2xs text-[#606060]">{accelZText()}</Text>
          </View>
          <View
            debugName="ImuDot"
            class="absolute left-[491] top-[264.5] w-[29] h-[29] rounded-full bg-[#FF589D]"
            style={{ translateX: dotShiftX(), translateY: dotShiftY(), scale: dotScale() }}
          />

          {/* PanelSwitches */}
          <Show when={chargeOn()} fallback={<Image class="absolute left-[443.75] top-[133] w-[64] h-[32]" src="assets/sw-chg-off.png" />}>
            <Image class="absolute left-[443.75] top-[133] w-[64] h-[32]" src="assets/sw-chg-on.png" />
          </Show>
          <Show when={quickChargeOn()} fallback={<Image class="absolute left-[443.75] top-[79] w-[64] h-[32]" src="assets/sw-qc-off.png" />}>
            <Image class="absolute left-[443.75] top-[79] w-[64] h-[32]" src="assets/sw-qc-on.png" />
          </Show>
          <Show when={ext5vOn()} fallback={<Image class="absolute left-[247.75] top-[201.5] w-[64] h-[32]" src="assets/sw-off.png" />}>
            <Image class="absolute left-[247.75] top-[201.5] w-[64] h-[32]" src="assets/sw-on.png" />
          </Show>
          <Show when={usbA5vOn()} fallback={<Image class="absolute left-[200.25] top-[156] w-[64] h-[32]" src="assets/sw-off.png" />}>
            <Image class="absolute left-[200.25] top-[156] w-[64] h-[32]" src="assets/sw-on.png" />
          </Show>
          <Show when={antennaExternal()} fallback={<Image class="absolute left-[109.75] top-[74] w-[64] h-[32]" src="assets/sw-rf-l.png" />}>
            <Image class="absolute left-[109.75] top-[74] w-[64] h-[32]" src="assets/sw-rf-h.png" />
          </Show>
          <View debugName="ChargeSwitch" class="absolute left-[430] top-[124.75] w-[65] h-[37.5]" focusable onPress={toggleCharge} />
          <View debugName="QuickChargeSwitch" class="absolute left-[430] top-[70.75] w-[65] h-[37.5]" focusable onPress={toggleQuickCharge} />
          <View debugName="Ext5vSwitch" class="absolute left-[234] top-[193.25] w-[65] h-[37.5]" focusable onPress={toggleExt5v} />
          <View debugName="UsbA5vSwitch" class="absolute left-[186.5] top-[147.75] w-[65] h-[37.5]" focusable onPress={toggleUsbA5v} />
          <View debugName="AntennaSwitch" class="absolute left-[96] top-[65.75] w-[65] h-[37.5]" focusable onPress={toggleAntenna} />
          <View debugName="WifiInfo" class="absolute left-[150.5] top-[71.5] w-[30] h-[30]" focusable onPress={showWifiInfo} />
          <Show when={usbCDetected()}>
            <Image class="absolute left-[2] top-[173.5] w-[16] h-[16]" src="assets/arrow-state-on.png" />
          </Show>
          <Show when={usbADetected()}>
            <Image class="absolute left-[2] top-[86] w-[16] h-[16]" src="assets/arrow-state-on.png" />
          </Show>
          <Show when={headphoneDetected()}>
            <Image class="absolute left-[2] top-[249] w-[16] h-[16]" src="assets/arrow-state-on.png" />
          </Show>

          {/* PanelPower, PanelCamera, PanelDualMic, PanelHeadphone, PanelSdCard,
              PanelI2cScan, PanelGpioTest, PanelMusic, PanelComMonitor */}
          <View debugName="PowerOff" class="absolute left-[564.5] top-[3] w-[73] h-[35]" focusable onPress={() => openPanel(1)} />
          <View debugName="SleepTouch" class="absolute left-[564.5] top-[40] w-[73] h-[35]" focusable onPress={() => openPanel(2)} />
          <View debugName="SleepShake" class="absolute left-[564.5] top-[76.5] w-[73] h-[35]" focusable onPress={() => openPanel(3)} />
          <View debugName="SleepRtc" class="absolute left-[564.5] top-[113.5] w-[73] h-[35]" focusable onPress={() => openPanel(4)} />
          <View debugName="Camera" class="absolute left-[598.25] top-[160.75] w-[41.5] h-[48.5]" focusable onPress={() => openPanel(5)} />
          <View debugName="DualMic" class="absolute left-[217.25] top-[0.75] w-[51.5] h-[47.5]" focusable onPress={() => openPanel(6)} />
          <View debugName="Headphone" class="absolute left-[140] top-[305.5] w-[50] h-[50]" focusable onPress={() => openPanel(7)} />
          <View debugName="SdCard" class="absolute left-[272] top-[305] w-[50] h-[50]" focusable onPress={() => openPanel(8)} />
          <View debugName="I2cScan" class="absolute left-[42.5] top-[269.5] w-[50] h-[50]" focusable onPress={() => openPanel(9)} />
          <View debugName="GpioTest" class="absolute left-[501.5] top-[97.5] w-[50] h-[50]" focusable onPress={() => openPanel(10)} />
          <View debugName="Music" class="absolute left-[407.5] top-[294.5] w-[50] h-[50]" focusable onPress={() => openPanel(12)} />
          <View debugName="ComMonitor" class="absolute left-[543.5] top-[157.5] w-[50] h-[50]" focusable onPress={() => openPanel(13)} />

          {/* WifiApMsgWindow (no background close, a close button) */}
          <Show when={windows()[14]?.visible ?? false}>
            <WindowFrame
              tone="absolute overflow-hidden rounded-[12] border-[#000000] bg-[#483333] items-center justify-center"
              left={windows()[14]?.left ?? 0}
              top={windows()[14]?.top ?? 0}
              width={windows()[14]?.width ?? 0}
              height={windows()[14]?.height ?? 0}
              opacity={windows()[14]?.opacity ?? 0}
              contentWidth={301}
              contentHeight={166}
            >
              <View class="absolute right-[10] top-[8] w-[10] h-[10] rounded-full bg-[#FF5F57]" />
              <View class="absolute left-[16] top-[35] h-[16] flex-col justify-center">
                <Text class="text-xs text-white">Connect to:</Text>
              </View>
              <View class="absolute left-[16] top-[75] h-[16] flex-col justify-center">
                <Text class="text-xs text-white">And open url:</Text>
              </View>
              <View class="absolute left-[99.25] top-[30.25] w-[189.5] h-[25.5] rounded-[11] bg-[#725151] items-center justify-center">
                <Text class="text-xs text-white">M5Tab5-UserDemo-WiFi</Text>
              </View>
              <View class="absolute left-[119] top-[70.25] w-[168] h-[25.5] rounded-[11] bg-[#725151] items-center justify-center">
                <Text class="text-xs text-white">http://192.168.4.1</Text>
              </View>
              <View class="absolute left-[13.75] top-[111.75] w-[273.5] h-[38.5] rounded-[11] bg-[#5A4141] items-center justify-center">
                <Show
                  when={antennaExternal()}
                  fallback={<Text class="text-xs text-white">Using internal antenna.</Text>}
                >
                  <Text class="text-xs text-white">Using external antenna</Text>
                  <Text class="text-xs text-white">please make sure it's connected.</Text>
                </Show>
              </View>
            </WindowFrame>
            <Show when={windows()[14]?.open ?? false}>
              <View debugName="WifiClose" class="absolute left-[452.5] top-[55] w-[40] h-[40]" focusable onPress={() => dismissWindow(14)} />
            </Show>
          </Show>

          {/* RtcSettingWindow */}
          <Show when={windows()[0]?.open ?? false}>
            <View class="absolute left-0 top-0 w-[640] h-[360]" focusable onPress={() => dismissWindow(0)} />
          </Show>
          <Show when={windows()[0]?.visible ?? false}>
            <WindowFrame
              tone="absolute overflow-hidden rounded-[12] border-[#000000] bg-[#232323] items-center justify-center"
              left={windows()[0]?.left ?? 0}
              top={windows()[0]?.top ?? 0}
              width={windows()[0]?.width ?? 0}
              height={windows()[0]?.height ?? 0}
              opacity={windows()[0]?.opacity ?? 0}
              contentWidth={400}
              contentHeight={240}
            >
              <View class="absolute left-0 top-[7.5] w-[400] h-[12] items-center justify-center">
                <Text class="text-2xs text-[#D8D8D8]">RTC Setting</Text>
              </View>
              <Show
                when={rtcReady()}
                fallback={
                  <View class="absolute left-0 top-[112] w-[400] h-[16] items-center justify-center">
                    <Text class="text-xs text-[#ECEBEB]">Loading ...</Text>
                  </View>
                }
              >
                {/* lv_calendar with the dropdown header */}
                <View class="absolute left-[15] top-[27.5] w-[370] h-[165]">
                  <View class="absolute left-[2] top-0 w-[180] h-[20] rounded-[4] bg-[#2F3237] flex-row items-center justify-between px-[6]" focusable onPress={() => toggleDropdown(1)}>
                    <Text class="text-2xs text-[#EEEEEE]">{calendarYear()}</Text>
                    <View class="relative w-[8] h-[6]"><View class="absolute left-0 top-[2] w-[5] h-[1.5] bg-[#EEEEEE] rotate-45" /><View class="absolute left-[3] top-[2] w-[5] h-[1.5] bg-[#EEEEEE] rotate-315" /></View>
                  </View>
                  <View class="absolute left-[188] top-0 w-[180] h-[20] rounded-[4] bg-[#2F3237] flex-row items-center justify-between px-[6]" focusable onPress={() => toggleDropdown(2)}>
                    <Text class="text-2xs text-[#EEEEEE]">{monthName()}</Text>
                    <View class="relative w-[8] h-[6]"><View class="absolute left-0 top-[2] w-[5] h-[1.5] bg-[#EEEEEE] rotate-45" /><View class="absolute left-[3] top-[2] w-[5] h-[1.5] bg-[#EEEEEE] rotate-315" /></View>
                  </View>
                  <View class="absolute left-[3] top-[24] w-[364] h-[16] flex-row">
                    <View class="w-[52] h-[16] items-center justify-center"><Text class="text-2xs text-[#9A9A9A]">Su</Text></View>
                    <View class="w-[52] h-[16] items-center justify-center"><Text class="text-2xs text-[#9A9A9A]">Mo</Text></View>
                    <View class="w-[52] h-[16] items-center justify-center"><Text class="text-2xs text-[#9A9A9A]">Tu</Text></View>
                    <View class="w-[52] h-[16] items-center justify-center"><Text class="text-2xs text-[#9A9A9A]">We</Text></View>
                    <View class="w-[52] h-[16] items-center justify-center"><Text class="text-2xs text-[#9A9A9A]">Th</Text></View>
                    <View class="w-[52] h-[16] items-center justify-center"><Text class="text-2xs text-[#9A9A9A]">Fr</Text></View>
                    <View class="w-[52] h-[16] items-center justify-center"><Text class="text-2xs text-[#9A9A9A]">Sa</Text></View>
                  </View>
                  <View class="absolute left-[3] top-[41] w-[364] h-[120] flex-row flex-wrap">
                    <For each={calendarCells()} by={(cell) => cell.id}>
                      {(cell) => (
                        <View
                          class={cell().current && cell().day === selectedDay() && calendarMonth() === selectedMonth() && calendarYear() === selectedYear()
                            ? "w-[52] h-[20] items-center justify-center rounded-[4] border-2 border-[#2196F3] bg-[#2196F3]"
                            : "w-[52] h-[20] items-center justify-center"}
                          focusable
                          onPress={() => pickDay(cell().id)}
                        >
                          <Text class={cell().current ? "text-2xs text-[#EEEEEE]" : "text-2xs text-[#6A6A6A]"}>{cell().day}</Text>
                        </View>
                      )}
                    </For>
                  </View>
                  <Show when={dropdown() === 1}>
                    <View class="absolute left-[2] top-[21] w-[180] h-[143] rounded-[4] bg-[#2F3237] flex-row flex-wrap px-[4] py-[2]">
                      <For each={yearOptions()} by={(year) => year}>
                        {(year) => (
                          <View class="w-[34] h-[11] items-center justify-center" focusable onPress={() => pickYear(year())}>
                            <Text class="text-2xs text-[#EEEEEE]">{year()}</Text>
                          </View>
                        )}
                      </For>
                    </View>
                  </Show>
                  <Show when={dropdown() === 2}>
                    <View class="absolute left-[188] top-[21] w-[180] h-[143] rounded-[4] bg-[#2F3237] flex-col px-[6] py-[2]">
                      <For each={monthOptions()} by={(option) => option.month}>
                        {(option) => (
                          <View class="w-[168] h-[11.5] flex-col justify-center" focusable onPress={() => pickMonth(option().month)}>
                            <Text class="text-2xs text-[#EEEEEE]">{option().name}</Text>
                          </View>
                        )}
                      </For>
                    </View>
                  </Show>
                </View>
                {/* Rollers h : m : s and Apply */}
                <View class="absolute left-[17.5] top-[201.5] w-[75] h-[22] rounded-[6] bg-[#2F3237] items-center justify-center">
                  <Text class="text-xs text-white">{rollerHour() < 10 ? `0${rollerHour()}` : `${rollerHour()}`}</Text>
                </View>
                <View class="absolute left-[95] top-[203.5] w-[10] h-[16] items-center justify-center">
                  <Text class="text-xs text-[#EEEEEE]">:</Text>
                </View>
                <View class="absolute left-[107.5] top-[201.5] w-[75] h-[22] rounded-[6] bg-[#2F3237] items-center justify-center">
                  <Text class="text-xs text-white">{rollerMinute() < 10 ? `0${rollerMinute()}` : `${rollerMinute()}`}</Text>
                </View>
                <View class="absolute left-[185] top-[203.5] w-[10] h-[16] items-center justify-center">
                  <Text class="text-xs text-[#EEEEEE]">:</Text>
                </View>
                <View class="absolute left-[197.5] top-[201.5] w-[75] h-[22] rounded-[6] bg-[#2F3237] items-center justify-center">
                  <Text class="text-xs text-white">{rollerSecond() < 10 ? `0${rollerSecond()}` : `${rollerSecond()}`}</Text>
                </View>
                <View debugName="RtcApply" class="absolute left-[301.25] top-[201.25] w-[77.5] h-[21.5] rounded-[9] bg-[#F26F42] items-center justify-center" focusable onPress={applyRtc}>
                  <Text class="text-xs text-white">Apply</Text>
                </View>
              </Show>
            </WindowFrame>
          </Show>

          {/* PowerOffWindow */}
          <Show when={windows()[1]?.open ?? false}>
            <View class="absolute left-0 top-0 w-[640] h-[360]" focusable onPress={() => dismissWindow(1)} />
          </Show>
          <Show when={windows()[1]?.visible ?? false}>
            <WindowFrame
              tone="absolute overflow-hidden rounded-[12] border-[#783A39] bg-[#B14948] items-center justify-center"
              left={windows()[1]?.left ?? 0}
              top={windows()[1]?.top ?? 0}
              width={windows()[1]?.width ?? 0}
              height={windows()[1]?.height ?? 0}
              opacity={windows()[1]?.opacity ?? 0}
              contentWidth={351.5}
              contentHeight={145.5}
            >
              <View class="absolute left-[25] top-[14.75] h-[24] flex-col justify-center">
                <Text class="text-lg text-white">Your device will shut down.</Text>
              </View>
              <View class="absolute left-[25] top-[54.25] h-[16] flex-col justify-center">
                <Text class="text-xs text-white">Shut down automatically in {countdown()} seconds.</Text>
              </View>
              <View class="absolute left-[118] top-[97.25] w-[91.5] h-[29] rounded-[9] bg-[#783A39] items-center justify-center" focusable onPress={() => cancelPower(1)}>
                <Text class="text-xs text-white">Cancel</Text>
              </View>
              <View class="absolute left-[233.5] top-[97.25] w-[91.5] h-[29] rounded-[9] bg-[#E65A57] items-center justify-center" focusable onPress={powerOff}>
                <Text class="text-xs text-white">Shut Down</Text>
              </View>
            </WindowFrame>
          </Show>

          {/* SleepTouchWakeupWindow */}
          <Show when={windows()[2]?.open ?? false}>
            <View class="absolute left-0 top-0 w-[640] h-[360]" focusable onPress={() => dismissWindow(2)} />
          </Show>
          <Show when={windows()[2]?.visible ?? false}>
            <WindowFrame
              tone="absolute overflow-hidden rounded-[12] border-[#54553D] bg-[#6B6D48] items-center justify-center"
              left={windows()[2]?.left ?? 0}
              top={windows()[2]?.top ?? 0}
              width={windows()[2]?.width ?? 0}
              height={windows()[2]?.height ?? 0}
              opacity={windows()[2]?.opacity ?? 0}
              contentWidth={351.5}
              contentHeight={145.5}
            >
              <View class="absolute left-[25] top-[14.75] h-[24] flex-col justify-center">
                <Text class="text-lg text-white">Your device will enter sleepmode.</Text>
              </View>
              <View class="absolute left-[25] top-[54.25] h-[16] flex-col justify-center">
                <Text class="text-xs text-white">Touch the screen to wake it up.</Text>
              </View>
              <View class="absolute left-[118] top-[97.25] w-[91.5] h-[29] rounded-[9] bg-[#54553D] items-center justify-center" focusable onPress={() => cancelPower(2)}>
                <Text class="text-xs text-white">Cancel</Text>
              </View>
              <View class="absolute left-[233.5] top-[97.25] w-[91.5] h-[29] rounded-[9] bg-[#979A60] items-center justify-center" focusable onPress={sleepTouch}>
                <Text class="text-xs text-white">Sleep</Text>
              </View>
            </WindowFrame>
          </Show>

          {/* SleepShakeWakeupWindow */}
          <Show when={windows()[3]?.open ?? false}>
            <View class="absolute left-0 top-0 w-[640] h-[360]" focusable onPress={() => dismissWindow(3)} />
          </Show>
          <Show when={windows()[3]?.visible ?? false}>
            <WindowFrame
              tone="absolute overflow-hidden rounded-[12] border-[#896B4D] bg-[#B18F6C] items-center justify-center"
              left={windows()[3]?.left ?? 0}
              top={windows()[3]?.top ?? 0}
              width={windows()[3]?.width ?? 0}
              height={windows()[3]?.height ?? 0}
              opacity={windows()[3]?.opacity ?? 0}
              contentWidth={351.5}
              contentHeight={145.5}
            >
              <View class="absolute left-[25] top-[14.75] h-[24] flex-col justify-center">
                <Text class="text-lg text-white">Your device will shut down.</Text>
              </View>
              <View class="absolute left-[25] top-[46.25] h-[32] flex-col justify-center">
                <Text class="text-xs text-white">Please place it still before confirming.</Text>
                <Text class="text-xs text-white">Shake the device to wake it up.</Text>
              </View>
              <View class="absolute left-[118] top-[97.25] w-[91.5] h-[29] rounded-[9] bg-[#896B4D] items-center justify-center" focusable onPress={() => cancelPower(3)}>
                <Text class="text-xs text-white">Cancel</Text>
              </View>
              <View class="absolute left-[233.5] top-[97.25] w-[91.5] h-[29] rounded-[9] bg-[#D5AA7E] items-center justify-center" focusable onPress={sleepShake}>
                <Text class="text-xs text-white">Sleep</Text>
              </View>
            </WindowFrame>
          </Show>

          {/* SleepRtcWakeupWindow */}
          <Show when={windows()[4]?.open ?? false}>
            <View class="absolute left-0 top-0 w-[640] h-[360]" focusable onPress={() => dismissWindow(4)} />
          </Show>
          <Show when={windows()[4]?.visible ?? false}>
            <WindowFrame
              tone="absolute overflow-hidden rounded-[12] border-[#435A73] bg-[#637991] items-center justify-center"
              left={windows()[4]?.left ?? 0}
              top={windows()[4]?.top ?? 0}
              width={windows()[4]?.width ?? 0}
              height={windows()[4]?.height ?? 0}
              opacity={windows()[4]?.opacity ?? 0}
              contentWidth={351.5}
              contentHeight={145.5}
            >
              <View class="absolute left-[25] top-[14.75] h-[24] flex-col justify-center">
                <Text class="text-lg text-white">Your device will shut down.</Text>
              </View>
              <View class="absolute left-[25] top-[54.25] h-[16] flex-col justify-center">
                <Text class="text-xs text-white">It will automatically wake up in 10 seconds.</Text>
              </View>
              <View class="absolute left-[118] top-[97.25] w-[91.5] h-[29] rounded-[9] bg-[#435A73] items-center justify-center" focusable onPress={() => cancelPower(4)}>
                <Text class="text-xs text-white">Cancel</Text>
              </View>
              <View class="absolute left-[233.5] top-[97.25] w-[91.5] h-[29] rounded-[9] bg-[#709DCE] items-center justify-center" focusable onPress={sleepRtc}>
                <Text class="text-xs text-white">Sleep</Text>
              </View>
            </WindowFrame>
          </Show>

          {/* CameraWindow; the preview canvas is a screen child */}
          <Show when={windows()[5]?.open ?? false}>
            <View class="absolute left-0 top-0 w-[640] h-[360]" focusable onPress={() => dismissWindow(5)} />
          </Show>
          <Show when={windows()[5]?.visible ?? false}>
            <WindowFrame
              tone="absolute overflow-hidden rounded-[12] border-[#000000] bg-[#232323] items-center justify-center"
              left={windows()[5]?.left ?? 0}
              top={windows()[5]?.top ?? 0}
              width={windows()[5]?.width ?? 0}
              height={windows()[5]?.height ?? 0}
              opacity={windows()[5]?.opacity ?? 0}
              contentWidth={400}
              contentHeight={240}
            >
              <View class="absolute left-0 top-[100] w-[400] h-[16] items-center justify-center">
                <Text class="text-xs text-[#EEEEEE]">{cameraMessage()}</Text>
              </View>
            </WindowFrame>
          </Show>
          <Show when={cameraVisible()}>
            <View
              debugName="CameraCanvas"
              class={cameraFullscreen() ? "absolute left-0 top-0 w-[640] h-[360] bg-black" : "absolute left-[200.5] top-[70] w-[380] h-[220] rounded-[6] bg-black"}
              focusable
              onPress={toggleCameraSize}
            />
          </Show>

          {/* MicTestWindow */}
          <Show when={windows()[6]?.open ?? false}>
            <View class="absolute left-0 top-0 w-[640] h-[360]" focusable onPress={() => dismissWindow(6)} />
          </Show>
          <Show when={windows()[6]?.visible ?? false}>
            <WindowFrame
              tone="absolute overflow-hidden rounded-[12] border-[#000000] bg-[#232323] items-center justify-center"
              left={windows()[6]?.left ?? 0}
              top={windows()[6]?.top ?? 0}
              width={windows()[6]?.width ?? 0}
              height={windows()[6]?.height ?? 0}
              opacity={windows()[6]?.opacity ?? 0}
              contentWidth={335}
              contentHeight={85.5}
            >
              <View class="absolute left-[12.25] top-[12.75]">
                <Chart columns={chartA()} />
              </View>
              <View class="absolute left-[146.75] top-[12.75]">
                <Chart columns={chartB()} />
              </View>
              <Show when={recordState() !== 0}>
                <View class="absolute left-[282] top-[23.75] w-[38] h-[38]">
                  <Spinner
                    angle={spinnerAngle()}
                    track="absolute left-0 top-0 w-[38] h-[38] bg-[#2F3237] arc-start-[0] arc-sweep-[360] arc-width-[1.5]"
                    arc={recordState() === 1
                      ? "absolute left-0 top-0 w-[38] h-[38] bg-[#E17822] arc-start-[0] arc-sweep-[60] arc-width-[1.5]"
                      : "absolute left-0 top-0 w-[38] h-[38] bg-[#32CC72] arc-start-[0] arc-sweep-[60] arc-width-[1.5]"}
                  />
                </View>
              </Show>
              <View
                debugName="MicRecord"
                class={recordState() === 1
                  ? "absolute left-[283] top-[24.75] w-[36] h-[36] rounded-full border-2 border-[#383838] bg-[#E17822] flex-col items-center justify-center"
                  : recordState() === 2
                    ? "absolute left-[283] top-[24.75] w-[36] h-[36] rounded-full border-2 border-[#383838] bg-[#32CC72] flex-col items-center justify-center"
                    : "absolute left-[283] top-[24.75] w-[36] h-[36] rounded-full border-2 border-[#383838] bg-[#D04848] flex-col items-center justify-center"}
                focusable
                onPress={() => {
                  if (recordState() === 0) startDualMicTest();
                }}
              >
                <Text class="text-2xs text-[#F4F3F3]">{recordState() === 2 ? "PLAY" : "REC"}</Text>
                <Show when={recordState() === 0}>
                  <Text class="text-2xs text-[#F4F3F3]">3S</Text>
                </Show>
              </View>
            </WindowFrame>
          </Show>

          {/* HeadphoneTestWindow */}
          <Show when={windows()[7]?.open ?? false}>
            <View class="absolute left-0 top-0 w-[640] h-[360]" focusable onPress={() => dismissWindow(7)} />
          </Show>
          <Show when={windows()[7]?.visible ?? false}>
            <WindowFrame
              tone="absolute overflow-hidden rounded-[12] border-[#000000] bg-[#232323] items-center justify-center"
              left={windows()[7]?.left ?? 0}
              top={windows()[7]?.top ?? 0}
              width={windows()[7]?.width ?? 0}
              height={windows()[7]?.height ?? 0}
              opacity={windows()[7]?.opacity ?? 0}
              contentWidth={201.5}
              contentHeight={104}
            >
              <View class="absolute left-[12.5] top-[9]">
                <Chart columns={chartA()} />
              </View>
              <Show when={recordState() !== 0}>
                <View class="absolute left-[148.75] top-[20] w-[38] h-[38]">
                  <Spinner
                    angle={spinnerAngle()}
                    track="absolute left-0 top-0 w-[38] h-[38] bg-[#2F3237] arc-start-[0] arc-sweep-[360] arc-width-[1.5]"
                    arc={recordState() === 1
                      ? "absolute left-0 top-0 w-[38] h-[38] bg-[#E17822] arc-start-[0] arc-sweep-[60] arc-width-[1.5]"
                      : "absolute left-0 top-0 w-[38] h-[38] bg-[#32CC72] arc-start-[0] arc-sweep-[60] arc-width-[1.5]"}
                  />
                </View>
              </Show>
              <View
                debugName="HeadphoneRecord"
                class={recordState() === 1
                  ? "absolute left-[149.75] top-[21] w-[36] h-[36] rounded-full border-2 border-[#383838] bg-[#E17822] flex-col items-center justify-center"
                  : recordState() === 2
                    ? "absolute left-[149.75] top-[21] w-[36] h-[36] rounded-full border-2 border-[#383838] bg-[#32CC72] flex-col items-center justify-center"
                    : "absolute left-[149.75] top-[21] w-[36] h-[36] rounded-full border-2 border-[#383838] bg-[#D04848] flex-col items-center justify-center"}
                focusable
                onPress={() => {
                  if (recordState() === 0) startHeadphoneTest();
                }}
              >
                <Text class="text-2xs text-[#F4F3F3]">{recordState() === 2 ? "PLAY" : "REC"}</Text>
                <Show when={recordState() === 0}>
                  <Text class="text-2xs text-[#F4F3F3]">3S</Text>
                </Show>
              </View>
              <View class="absolute left-0 top-[80] w-[201.5] h-[14] items-center justify-center">
                <Text class={headphoneDetected() ? "text-2xs text-[#2FC57B]" : "text-2xs text-[#D04848]"}>
                  {headphoneDetected() ? "Headphone Connected" : "Headphone Disconnected"}
                </Text>
              </View>
            </WindowFrame>
          </Show>

          {/* SdCardScanWindow */}
          <Show when={windows()[8]?.open ?? false}>
            <View class="absolute left-0 top-0 w-[640] h-[360]" focusable onPress={() => dismissWindow(8)} />
          </Show>
          <Show when={windows()[8]?.visible ?? false}>
            <WindowFrame
              tone="absolute overflow-hidden rounded-[12] border-[#000000] bg-[#232323] items-center justify-center"
              left={windows()[8]?.left ?? 0}
              top={windows()[8]?.top ?? 0}
              width={windows()[8]?.width ?? 0}
              height={windows()[8]?.height ?? 0}
              opacity={windows()[8]?.opacity ?? 0}
              contentWidth={283}
              contentHeight={205.5}
            >
              <View class="absolute left-0 top-[7.5] w-[283] h-[12] items-center justify-center">
                <Text class="text-2xs text-[#D8D8D8]">SD-Card File Scan</Text>
              </View>
              <View class="absolute left-[7.75] top-[25.5] w-[267.5] h-[172.5] rounded-[4] bg-[#393939] overflow-hidden">
                <For each={sdEntries()} by={(entry) => entry.id}>
                  {(entry) => (
                    <View class="h-[21] flex-row items-center pl-[12]">
                      <View class={entry().directory ? "w-[12] h-[9] rounded-[1] bg-[#FDBE1A]" : "w-[9] h-[11] rounded-[1] bg-[#43D2FF]"} />
                      <View class={entry().directory ? "w-[6] h-[1]" : "w-[9] h-[1]"} />
                      <Text class={entry().directory ? "text-xs text-[#FDBE1A]" : "text-xs text-[#43D2FF]"}>{entry().name}</Text>
                    </View>
                  )}
                </For>
              </View>
              <Show when={sdMessage() !== ""}>
                <View class="absolute left-0 top-[82.75] w-[283] h-[16] items-center justify-center">
                  <Text class={sdError() ? "text-xs text-[#FD4444]" : "text-xs text-[#EEEEEE]"}>{sdMessage()}</Text>
                </View>
              </Show>
            </WindowFrame>
          </Show>

          {/* I2cScanWindow */}
          <Show when={windows()[9]?.open ?? false}>
            <View class="absolute left-0 top-0 w-[640] h-[360]" focusable onPress={() => dismissWindow(9)} />
          </Show>
          <Show when={windows()[9]?.visible ?? false}>
            <WindowFrame
              tone="absolute overflow-hidden rounded-[12] border-[#000000] bg-[#1A1A1A] items-center justify-center"
              left={windows()[9]?.left ?? 0}
              top={windows()[9]?.top ?? 0}
              width={windows()[9]?.width ?? 0}
              height={windows()[9]?.height ?? 0}
              opacity={windows()[9]?.opacity ?? 0}
              contentWidth={256}
              contentHeight={178}
            >
              <Show when={i2cInternal()} fallback={<Image class="absolute left-[3] top-[5.5] w-[256] h-[256]" src="assets/porta-i2c-dev-chart.png" />}>
                <Image class="absolute left-[3] top-[5.5] w-[256] h-[256]" src="assets/internal-i2c-dev-chart.png" />
              </Show>
              <For each={i2cFound()} by={(label) => label.address}>
                {(label) => (
                  <View class="absolute w-[14] h-[12] items-center justify-center" style={{ insetL: label().left, insetT: label().top }}>
                    <Text class="text-2xs text-[#352B2A]">{label().text}</Text>
                  </View>
                )}
              </For>
              <View debugName="I2cPortA" class="absolute left-[47.5] top-[10] w-[61] h-[26]" focusable onPress={scanPortA} />
              <View debugName="I2cInternal" class="absolute left-[147.5] top-[10] w-[61] h-[26]" focusable onPress={scanInternal} />
              <Show when={!i2cInternal() && ext5vOn()}>
                <View debugName="I2cExt5v" class="absolute left-[6.5] top-[8.75] w-[32] h-[32]" focusable onPress={togglePortAPower}>
                  <Image class="w-[32] h-[32]" src="assets/porta-i2c-ext5v-on.png" />
                </View>
              </Show>
              <Show when={!i2cInternal() && !ext5vOn()}>
                <View class="absolute left-[6.5] top-[8.75] w-[31] h-[27.5]" focusable onPress={togglePortAPower} />
              </Show>
            </WindowFrame>
          </Show>

          {/* ExtPortWindow (Ext.Port1) and MbusWindow */}
          <Show when={windows()[10]?.open ?? false}>
            <View class="absolute left-0 top-0 w-[640] h-[360]" focusable onPress={() => dismissWindow(10)} />
          </Show>
          <Show when={windows()[10]?.visible ?? false}>
            <WindowFrame
              tone="absolute overflow-hidden rounded-[12] border-[#000000] bg-[#232323] items-center justify-center"
              left={windows()[10]?.left ?? 0}
              top={windows()[10]?.top ?? 0}
              width={windows()[10]?.width ?? 0}
              height={windows()[10]?.height ?? 0}
              opacity={windows()[10]?.opacity ?? 0}
              contentWidth={136.5}
              contentHeight={190}
            >
              <View class="absolute left-0 top-0 w-[136.5] h-[273]" style={{ translateY: extPortScroll() }}>
                <View class="absolute left-0 top-[7.5] w-[136.5] h-[12] items-center justify-center">
                  <Text class="text-2xs text-[#D8D8D8]">Ext.Port1</Text>
                </View>
                <Show
                  when={gpioReady()}
                  fallback={
                    <View class="absolute left-0 top-[87] w-[136.5] h-[16] items-center justify-center">
                      <Text class="text-xs text-[#ECEBEB]">Loading ...</Text>
                    </View>
                  }
                >
                  <For each={extPortRows()} by={(row) => row.pin}>
                    {(row) => (
                      <View class="absolute w-[115] h-[31] rounded-[9] bg-[#383838]" style={{ insetL: row().left, insetT: row().top }}>
                        <View class="absolute left-[6] top-0 w-[36] h-[31] items-center justify-center">
                          <Text class="text-sm text-[#EEEEEE]">G{row().pin}</Text>
                        </View>
                        <View
                          class={row().high
                            ? "absolute left-[52.5] top-[5] w-[57] h-[21] rounded-[8] bg-[#DD380D] items-center justify-center"
                            : "absolute left-[52.5] top-[5] w-[57] h-[21] rounded-[8] bg-[#58B358] items-center justify-center"}
                          focusable
                          onPress={() => toggleExtPortPin(row().pin)}
                        >
                          <Text class="text-xs text-white">{row().high ? "HIGH" : "LOW"}</Text>
                        </View>
                      </View>
                    )}
                  </For>
                </Show>
              </View>
            </WindowFrame>
          </Show>
          <Show when={windows()[11]?.visible ?? false}>
            <WindowFrame
              tone="absolute overflow-hidden rounded-[12] border-[#000000] bg-[#232323] items-center justify-center"
              left={windows()[11]?.left ?? 0}
              top={windows()[11]?.top ?? 0}
              width={windows()[11]?.width ?? 0}
              height={windows()[11]?.height ?? 0}
              opacity={windows()[11]?.opacity ?? 0}
              contentWidth={260}
              contentHeight={273.5}
            >
              <View class="absolute left-0 top-0 w-[260] h-[395]" style={{ translateY: mbusScroll() }}>
                <View class="absolute left-0 top-[7.5] w-[260] h-[12] items-center justify-center">
                  <Text class="text-2xs text-[#D8D8D8]">MBUS</Text>
                </View>
                <Show
                  when={gpioReady()}
                  fallback={
                    <View class="absolute left-0 top-[128.75] w-[260] h-[16] items-center justify-center">
                      <Text class="text-xs text-[#ECEBEB]">Loading ...</Text>
                    </View>
                  }
                >
                  <For each={mbusRows()} by={(row) => row.pin}>
                    {(row) => (
                      <View class="absolute w-[115] h-[31] rounded-[9] bg-[#383838]" style={{ insetL: row().left, insetT: row().top }}>
                        <View class="absolute left-[6] top-0 w-[36] h-[31] items-center justify-center">
                          <Text class="text-sm text-[#EEEEEE]">G{row().pin}</Text>
                        </View>
                        <View
                          class={row().high
                            ? "absolute left-[52.5] top-[5] w-[57] h-[21] rounded-[8] bg-[#DD380D] items-center justify-center"
                            : "absolute left-[52.5] top-[5] w-[57] h-[21] rounded-[8] bg-[#58B358] items-center justify-center"}
                          focusable
                          onPress={() => toggleMbusPin(row().pin)}
                        >
                          <Text class="text-xs text-white">{row().high ? "HIGH" : "LOW"}</Text>
                        </View>
                      </View>
                    )}
                  </For>
                </Show>
              </View>
            </WindowFrame>
          </Show>

          {/* MusicTestWindow */}
          <Show when={windows()[12]?.open ?? false}>
            <View class="absolute left-0 top-0 w-[640] h-[360]" focusable onPress={() => dismissWindow(12)} />
          </Show>
          <Show when={windows()[12]?.visible ?? false}>
            <WindowFrame
              tone="absolute overflow-hidden rounded-[12] border-[#000000] bg-[#232323] items-center justify-center"
              left={windows()[12]?.left ?? 0}
              top={windows()[12]?.top ?? 0}
              width={windows()[12]?.width ?? 0}
              height={windows()[12]?.height ?? 0}
              opacity={windows()[12]?.opacity ?? 0}
              contentWidth={206.5}
              contentHeight={102}
            >
              <View class="absolute left-[12.5] top-[12.75] w-[121.5] h-[76.5] rounded-[6] bg-[#5A5A5A]" />
              <View class="absolute left-[12.5] top-[12.5]">
                <Chart columns={chartA()} />
              </View>
              <View class="absolute left-[12.5] top-[74] w-[121.5] h-[13] items-center justify-center">
                <Text class="text-2xs text-[#D6D6D6]">Speaker Output Capture</Text>
              </View>
              <Show when={musicOn()}>
                <View class="absolute left-[148.25] top-[28.5] w-[45] h-[45]">
                  <Spinner
                    angle={spinnerAngle()}
                    track="absolute left-0 top-0 w-[45] h-[45] bg-[#2F3237] arc-start-[0] arc-sweep-[360] arc-width-[2]"
                    arc="absolute left-0 top-0 w-[45] h-[45] bg-[#31D584] arc-start-[0] arc-sweep-[60] arc-width-[2]"
                  />
                </View>
              </Show>
              <View
                debugName="MusicPlay"
                class="absolute left-[148.75] top-[29] w-[44] h-[44] rounded-full border-[3px] border-[#383838] bg-[#31D584] flex-col items-center justify-center"
                focusable
                onPress={toggleMusic}
              >
                <Show when={musicOn()} fallback={<><Text class="text-2xs text-[#0B4D2C]">PLAY</Text><Text class="text-2xs text-[#0B4D2C]">MUSIC</Text></>}>
                  <Text class="text-2xs text-[#0B4D2C]">STOP</Text>
                </Show>
              </View>
            </WindowFrame>
          </Show>

          {/* ComMonitorWindow */}
          <Show when={windows()[13]?.open ?? false}>
            <View class="absolute left-0 top-0 w-[640] h-[360]" focusable onPress={() => dismissWindow(13)} />
          </Show>
          <Show when={windows()[13]?.visible ?? false}>
            <WindowFrame
              tone="absolute overflow-hidden rounded-[12] border-[#000000] bg-[#232323] items-center justify-center"
              left={windows()[13]?.left ?? 0}
              top={windows()[13]?.top ?? 0}
              width={windows()[13]?.width ?? 0}
              height={windows()[13]?.height ?? 0}
              opacity={windows()[13]?.opacity ?? 0}
              contentWidth={321.5}
              contentHeight={217.5}
            >
              <View class="absolute left-[10.75] top-[9] w-[300] h-[166.5] rounded-[4] bg-[#383838] flex-col justify-end overflow-hidden px-[4] py-[4]">
                <For each={comLines()} by={(line) => line.id}>
                  {(line) => <Text class="text-2xs text-[#EEEEEE]">{line().text}</Text>}
                </For>
              </View>
              <View class="absolute left-[23] top-[180.75] h-[28] flex-col justify-center">
                <Text class="text-2xs text-[#DEDEDE]">Port: RS485</Text>
                <Text class="text-2xs text-[#DEDEDE]">Baud: 115200</Text>
              </View>
              <View debugName="ComSend" class="absolute left-[146.25] top-[183.75] w-[157] h-[24] rounded-[9] bg-[#616161] items-center justify-center" focusable onPress={sendHello}>
                <Text class="text-xs text-[#E7E7E7]">Send "Hello M5Stack!"</Text>
              </View>
            </WindowFrame>
          </Show>

          {/* Toasts (ui::ToastManager, the dark style) */}
          <For each={toasts()} by={(toast) => toast.id}>
            {(toast) => (
              <View
                class="absolute h-[40] rounded-[12] border-[#222222] bg-[#383838] items-center justify-center overflow-hidden"
                style={{ insetL: toast().left, insetT: toast().top, width: toast().width }}
                focusable
                onPress={() => dismissToast(toast().id)}
              >
                <Text class="text-xs text-[#F4F3F3]">{toast().text}</Text>
              </View>
            )}
          </For>
        </View>
      </Show>
    </View>
  );
}
