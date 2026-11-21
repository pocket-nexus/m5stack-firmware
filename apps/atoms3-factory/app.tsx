import { Match, Show, Switch } from "solid-js";
import { ActionHandler, For, Image, Text, View } from "@pocketjs/framework/solid/components";
import { onMount } from "@pocketjs/framework/solid/lifecycle";
import { BTN } from "@pocketjs/framework/input";
import {
  adc1Text,
  adc2Text,
  ballX,
  ballY,
  bootStep,
  click,
  func,
  hold,
  i2cColor,
  i2cCount,
  i2cState,
  i2cText,
  irMode,
  irText,
  pwmDutyText,
  pwmMode,
  run,
  screen,
  uartMode,
  uartRxColor,
  uartRxText,
  uartTxColor,
  uartTxText,
  wifiLines,
  wifiListing,
  wifiOverflow,
} from "./app";
import Boot from "./Boot.tsx";
import Menu from "./Menu.tsx";
import RandomText from "./RandomText.tsx";

// Coordinates follow the factory's drawing calls. Canvas functions (Wi-Fi,
// I2C, IMU) draw into a 128x98 sprite pushed at y = 30 over the menu
// artwork; the others redraw the whole screen with their own artwork.
export default function App() {
  onMount(run);
  return (
    <View debugName="AtomS3Factory" class="w-full h-full bg-black overflow-hidden">
      <ActionHandler button={BTN.CIRCLE} onPress={click} />
      <ActionHandler button={BTN.CROSS} onPress={hold} />
      <Switch>
        <Match when={screen() === 0}>
          <Boot step={bootStep()} />
        </Match>
        <Match when={screen() === 1}>
          <Menu index={func()} />
        </Match>
        <Match when={screen() === 2 && (func() === 0 || func() === 1 || func() === 6)}>
          <Menu index={func()} />
          <View debugName="Canvas" class="absolute left-0 top-[30] w-[128] h-[98] bg-black overflow-hidden">
            <Switch>
              <Match when={func() === 0}>
                <Show
                  when={wifiListing()}
                  fallback={
                    <View class="absolute left-0 top-[37] w-[128] h-[16] items-center">
                      <Text class="text-base leading-[16px] text-white">Scaning...</Text>
                    </View>
                  }
                >
                  <View
                    class={
                      wifiOverflow()
                        ? "absolute left-0 top-0 w-[128] h-[98] flex-col justify-end"
                        : "absolute left-0 top-0 w-[128] h-[98] flex-col justify-start"
                    }
                  >
                    <For each={wifiLines()} by={(line) => line.id}>
                      {(line) => (
                        <View class={line().big ? "w-[256] h-[14] shrink-0" : "w-[256] h-[12] shrink-0"}>
                          <Text class={line().big ? "text-sm leading-[14px] text-white" : "text-xs leading-[12px] text-white"}>
                            {line().text}
                          </Text>
                        </View>
                      )}
                    </For>
                  </View>
                </Show>
              </Match>
              <Match when={func() === 1}>
                <Switch>
                  <Match when={i2cState() === 0}>
                    <View class="absolute left-0 top-[37] w-[128] h-[16] items-center">
                      <Text class="text-base leading-[16px] text-white">Scaning...</Text>
                    </View>
                  </Match>
                  <Match when={i2cState() === 1}>
                    <View class="absolute left-0 top-[33] w-[128] h-[24] items-center">
                      <Text class="text-2xl leading-[24px] text-[#ff0000]">Not found</Text>
                    </View>
                  </Match>
                  <Match when={i2cState() === 2}>
                    <View class="absolute left-0 top-[1] w-[128] h-[1] bg-white" />
                    <View class="absolute left-0 top-[32] w-[128] h-[1] bg-white" />
                    <View class="absolute left-0 top-[65] w-[128] h-[1] bg-white" />
                    <View class="absolute left-0 top-[1] w-[1] h-[96] bg-white" />
                    <View class="absolute left-[64] top-[1] w-[1] h-[96] bg-white" />
                    <View class="absolute left-[127] top-[1] w-[1] h-[96] bg-white" />
                    <View class="absolute left-0 top-[8] w-[64] h-[16] items-center">
                      <RandomText color={i2cColor(0)} text={i2cText(0)} small={false} />
                    </View>
                    <Show when={i2cCount() > 1}>
                      <View class="absolute left-[1] top-[40] w-[64] h-[16] items-center">
                        <RandomText color={i2cColor(1)} text={i2cText(1)} small={false} />
                      </View>
                    </Show>
                    <Show when={i2cCount() > 2}>
                      <View class="absolute left-[1] top-[72] w-[64] h-[16] items-center">
                        <RandomText color={i2cColor(2)} text={i2cText(2)} small={false} />
                      </View>
                    </Show>
                    <Show when={i2cCount() > 3}>
                      <View class="absolute left-[1] top-[8] w-[64] h-[16] items-center">
                        <RandomText color={i2cColor(3)} text={i2cText(3)} small={false} />
                      </View>
                    </Show>
                    <Show when={i2cCount() > 4}>
                      <View class="absolute left-[1] top-[40] w-[64] h-[16] items-center">
                        <RandomText color={i2cColor(4)} text={i2cText(4)} small={false} />
                      </View>
                    </Show>
                    <Show when={i2cCount() > 5}>
                      <View class="absolute left-[1] top-[72] w-[64] h-[16] items-center">
                        <RandomText color={i2cColor(5)} text={i2cText(5)} small={false} />
                      </View>
                    </Show>
                  </Match>
                </Switch>
              </Match>
              <Match when={func() === 6}>
                <View
                  class="absolute left-[59] top-[44] w-[11] h-[11] rounded-full bg-[#cccc33]"
                  style={{ translateX: ballX() - 64, translateY: ballY() - 49 }}
                />
                <View class="absolute left-[19] top-[4] w-[91] h-[91] rounded-full border-[1] border-[#cccc33]" />
                <View class="absolute left-[44] top-[29] w-[41] h-[41] rounded-full border-[1] border-[#cccc33]" />
                <View class="absolute left-[19] top-[49] w-[90] h-[1] bg-[#cccc33]" />
                <View class="absolute left-[64] top-[4] w-[1] h-[90] bg-[#cccc33]" />
              </Match>
            </Switch>
          </View>
        </Match>
        <Match when={screen() === 2 && func() === 2}>
          <Switch>
            <Match when={uartMode() === 0}>
              <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/uart_mon_01.png" />
            </Match>
            <Match when={uartMode() === 1}>
              <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/uart_mon_02.png" />
            </Match>
            <Match when={uartMode() === 2}>
              <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/uart_mon_03.png" />
            </Match>
            <Match when={uartMode() === 3}>
              <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/uart_mon_04.png" />
            </Match>
          </Switch>
          <Show when={uartRxText() !== ""}>
            <View class="absolute left-[86] top-[31] w-[42] h-[9] bg-black" />
            <View class="absolute left-[93] top-[33] h-[8]">
              <RandomText color={uartRxColor()} text={uartRxText()} small={true} />
            </View>
          </Show>
          <Show when={uartTxText() !== ""}>
            <View class="absolute left-[86] top-[41] w-[42] h-[9] bg-black" />
            <View class="absolute left-[93] top-[43] h-[8]">
              <RandomText color={uartTxColor()} text={uartTxText()} small={true} />
            </View>
          </Show>
        </Match>
        <Match when={screen() === 2 && func() === 3}>
          <Switch>
            <Match when={pwmMode() === 0}>
              <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/io_pwm_01.png" />
            </Match>
            <Match when={pwmMode() === 1}>
              <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/io_pwm_02.png" />
            </Match>
            <Match when={pwmMode() === 2}>
              <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/io_pwm_03.png" />
            </Match>
            <Match when={pwmMode() === 3}>
              <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/io_pwm_04.png" />
            </Match>
          </Switch>
          <View class="absolute left-[50] top-[52] w-[80] h-[16] items-center">
            <Text class="text-base font-bold leading-[16px] text-white">F: 1Khz</Text>
          </View>
          <View class="absolute left-[80] top-[30] w-[45] h-[18] bg-black" />
          <View class="absolute left-[85] top-[31] h-[16]">
            <Text class="text-base font-bold leading-[16px] text-white">{pwmDutyText()}</Text>
          </View>
        </Match>
        <Match when={screen() === 2 && func() === 4}>
          <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/io_adc_01.png" />
          <Show when={adc1Text() !== ""}>
            <View class="absolute left-0 top-[30] w-[50] h-[16] bg-black" />
            <View class="absolute left-[28] top-[50] w-[50] h-[16] bg-black" />
            <View class="absolute left-0 top-[31] w-[60] h-[16] items-center">
              <Text class="text-base font-bold leading-[16px] text-white">{adc1Text()}</Text>
            </View>
            <View class="absolute left-[25] top-[51] w-[60] h-[16] items-center">
              <Text class="text-base font-bold leading-[16px] text-white">{adc2Text()}</Text>
            </View>
          </Show>
        </Match>
        <Match when={screen() === 2 && func() === 5}>
          <Show
            when={irMode() === 0}
            fallback={<Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/ir_send_02.png" />}
          >
            <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/ir_send_01.png" />
          </Show>
          <Show when={irText() !== ""}>
            <View class="absolute left-[25] top-[105] w-[75] h-[20] bg-white" />
            <View class="absolute left-[24] top-[107] w-[80] h-[16] items-center">
              <Text class="text-base font-bold leading-[16px] text-[#7b007b]">{irText()}</Text>
            </View>
          </Show>
        </Match>
      </Switch>
    </View>
  );
}
