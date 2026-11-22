import { Match, Show, Switch } from "solid-js";
import { For, Image, Text, View } from "@pocketjs/framework/solid/components";
import type { Color, Deg, i32 } from "@pocketjs/framework/solid/std";
import type { Tick } from "./app";
import BigNumber from "./BigNumber.tsx";
import DigitStrip from "./DigitStrip.tsx";
import GlyphsClassicTime from "./GlyphsClassicTime.tsx";
import GlyphsLarge0 from "./GlyphsLarge0.tsx";
import GlyphsLarge1 from "./GlyphsLarge1.tsx";
import GlyphsLarge2 from "./GlyphsLarge2.tsx";
import GlyphsLarge3 from "./GlyphsLarge3.tsx";
import GlyphsLarge4 from "./GlyphsLarge4.tsx";
import GlyphsLarge5 from "./GlyphsLarge5.tsx";
import GlyphsLarge6 from "./GlyphsLarge6.tsx";
import GlyphsLarge7 from "./GlyphsLarge7.tsx";
import GlyphsLarge8 from "./GlyphsLarge8.tsx";
import GlyphsLarge9 from "./GlyphsLarge9.tsx";

// Factory app_watch_face: classic, number flow, big number and simple faces.
// As in the factory (watch_face_manager.cpp), one 466 px panel turns about
// the screen centre with the accelerometer, so the faces are laid out
// upright. Parts that look the same at every angle stay out of the panel:
// the face backgrounds and the classic dial (a turned full circle would be
// drawn as a polygon). The number-flow tiles and digit windows stay upright
// and only move with the panel (anchor offsets dx/dy of the root model):
// overflow-hidden clips to the axis-aligned bounds of a turned window, which
// would show the neighbouring rows of the rolling strips. They are drawn
// before the panel, so the date stays above them as in the factory order.
export default function WatchFace(props: {
  face: i32;
  rotation: Deg;
  mode: i32;
  hourAngle: Deg;
  minuteAngle: Deg;
  secondAngle: Deg;
  timeIds: i32[];
  timeXs: i32[];
  plainIds: i32[];
  plainXs: i32[];
  weekday: string;
  day: string;
  date: string;
  hh: string;
  mm: string;
  ss: string;
  background: Color;
  panel: Color;
  theme: i32;
  dot: boolean;
  tips: boolean;
  dotX: i32;
  dotY: i32;
  ticks: Tick[];
  digits: i32[];
  flow: i32[];
  dx: i32[];
  dy: i32[];
  onTap: () => void;
}) {
  return (
    <View class="absolute left-0 top-0 w-[466] h-[466] bg-black" focusable onPress={() => props.onTap()}>
      <Switch>
        <Match when={props.face === 0 && props.mode === 0}>
          <View class="absolute left-[54] top-[54] w-[358] h-[358] rounded-full bg-[#1B1B1B]" />
        </Match>
        <Match when={props.face === 1 || props.face === 3}>
          <View class="absolute left-0 top-0 w-[466] h-[466]" style={{ bgColor: props.background }} />
        </Match>
      </Switch>
      <Show when={props.face === 1}>
        <View class="absolute left-[66] top-[145] w-[100] h-[132] rounded-[42] items-center justify-center" style={{ bgColor: props.panel, translateX: props.dx[0] ?? 0, translateY: props.dy[0] ?? 0 }}>
          <DigitStrip left={7} dx={(props.dx[3] ?? 0) - (props.dx[0] ?? 0)} dy={(props.dy[3] ?? 0) - (props.dy[0] ?? 0)} offset={props.flow[0] ?? 0} theme={props.theme} />
          <DigitStrip left={43} dx={(props.dx[4] ?? 0) - (props.dx[0] ?? 0)} dy={(props.dy[4] ?? 0) - (props.dy[0] ?? 0)} offset={props.flow[1] ?? 0} theme={props.theme} />
        </View>
        <View class="absolute left-[183] top-[145] w-[100] h-[132] rounded-[42] items-center justify-center" style={{ bgColor: props.panel, translateX: props.dx[1] ?? 0, translateY: props.dy[1] ?? 0 }}>
          <DigitStrip left={7} dx={(props.dx[5] ?? 0) - (props.dx[1] ?? 0)} dy={(props.dy[5] ?? 0) - (props.dy[1] ?? 0)} offset={props.flow[2] ?? 0} theme={props.theme} />
          <DigitStrip left={43} dx={(props.dx[6] ?? 0) - (props.dx[1] ?? 0)} dy={(props.dy[6] ?? 0) - (props.dy[1] ?? 0)} offset={props.flow[3] ?? 0} theme={props.theme} />
        </View>
        <View class="absolute left-[300] top-[145] w-[100] h-[132] rounded-[42] items-center justify-center" style={{ bgColor: props.panel, translateX: props.dx[2] ?? 0, translateY: props.dy[2] ?? 0 }}>
          <DigitStrip left={7} dx={(props.dx[7] ?? 0) - (props.dx[2] ?? 0)} dy={(props.dy[7] ?? 0) - (props.dy[2] ?? 0)} offset={props.flow[4] ?? 0} theme={props.theme} />
          <DigitStrip left={43} dx={(props.dx[8] ?? 0) - (props.dx[2] ?? 0)} dy={(props.dy[8] ?? 0) - (props.dy[2] ?? 0)} offset={props.flow[5] ?? 0} theme={props.theme} />
        </View>
      </Show>
      <View class="absolute left-0 top-0 w-[466] h-[466]" style={{ rotate: props.rotation }}>
        <Switch>
          <Match when={props.face === 0}>
            <Show when={props.mode < 2}>
              <For each={props.ticks} by={tick => tick.id}>
                {tick => (
                  <View
                    class={tick().hour ? "absolute left-[219] top-[232] w-[28] h-[2] bg-[#6E6E6E]" : "absolute left-[226] top-[232] w-[14] h-[2] bg-[#454545]"}
                    style={{ translateX: tick().x, translateY: tick().y, rotate: tick().angle }}
                  />
                )}
              </For>
            </Show>
            <Show when={props.mode === 0}>
              <View class="absolute left-0 top-[320] w-[466] h-[34] items-center justify-center">
                <GlyphsClassicTime ids={props.timeIds} base={0} xs={props.timeXs} />
              </View>
              <View class="absolute left-0 top-[357] w-[466] h-[30] items-center justify-center">
                <Text class="text-2xl text-[#717171]">{props.weekday}</Text>
              </View>
              <View class="absolute left-[334] top-[216] w-[48] h-[34] rounded-full bg-[#101010] items-center justify-center">
                <Text class="text-2xl text-[#8B8B8B]">{props.day}</Text>
              </View>
            </Show>
            <View class="absolute left-[221] top-[226] w-[128] h-[16]" style={{ rotate: props.hourAngle, originX: -0.40625, originY: -0.0625 }}>
              <Image class="w-[128] h-[16]" src="images/classic_hour_hand.png" />
            </View>
            <View class="absolute left-[214] top-[229] w-[256] h-[8]" style={{ rotate: props.minuteAngle, originX: -0.42578 }}>
              <Image class="w-[256] h-[8]" src="images/classic_minute_hand.png" />
            </View>
            <View class="absolute left-[204] top-[226] w-[256] h-[16]" style={{ rotate: props.secondAngle, originX: -0.38672, originY: -0.0625 }}>
              <Image class="w-[256] h-[16]" src="images/classic_second_hand.png" />
            </View>
          </Match>
          <Match when={props.face === 1}>
            <View class="absolute left-0 top-[294] w-[466] h-[30] items-center justify-center">
              <Text class={props.theme === 0 ? "text-2xl text-[#A0A0A0]" : props.theme === 1 ? "text-2xl text-[#525252]" : props.theme === 2 ? "text-2xl text-[#6A938F]" : props.theme === 3 ? "text-2xl text-[#948063]" : props.theme === 4 ? "text-2xl text-[#668323]" : props.theme === 5 ? "text-2xl text-[#CBCBFF]" : props.theme === 6 ? "text-2xl text-[#FFD3BC]" : props.theme === 7 ? "text-2xl text-[#A4789D]" : props.theme === 8 ? "text-2xl text-[#70CFC9]" : "text-2xl text-[#DCF3FF]"}>{props.date}</Text>
            </View>
          </Match>
          <Match when={props.face === 2}>
            <BigNumber
              palette={props.theme}
              d0={props.digits[0] ?? 0}
              d1={props.digits[1] ?? 0}
              d2={props.digits[2] ?? 0}
              d3={props.digits[3] ?? 0}
            />
          </Match>
          <Match when={props.face === 3}>
            <View class="absolute left-0 top-[160] w-[466] h-[98]">
              <Switch>
                <Match when={props.theme === 0}>
                  <GlyphsLarge0 ids={props.plainIds} base={0} xs={props.plainXs} />
                </Match>
                <Match when={props.theme === 1}>
                  <GlyphsLarge1 ids={props.plainIds} base={0} xs={props.plainXs} />
                </Match>
                <Match when={props.theme === 2}>
                  <GlyphsLarge2 ids={props.plainIds} base={0} xs={props.plainXs} />
                </Match>
                <Match when={props.theme === 3}>
                  <GlyphsLarge3 ids={props.plainIds} base={0} xs={props.plainXs} />
                </Match>
                <Match when={props.theme === 4}>
                  <GlyphsLarge4 ids={props.plainIds} base={0} xs={props.plainXs} />
                </Match>
                <Match when={props.theme === 5}>
                  <GlyphsLarge5 ids={props.plainIds} base={0} xs={props.plainXs} />
                </Match>
                <Match when={props.theme === 6}>
                  <GlyphsLarge6 ids={props.plainIds} base={0} xs={props.plainXs} />
                </Match>
                <Match when={props.theme === 7}>
                  <GlyphsLarge7 ids={props.plainIds} base={0} xs={props.plainXs} />
                </Match>
                <Match when={props.theme === 8}>
                  <GlyphsLarge8 ids={props.plainIds} base={0} xs={props.plainXs} />
                </Match>
                <Match when={props.theme === 9}>
                  <GlyphsLarge9 ids={props.plainIds} base={0} xs={props.plainXs} />
                </Match>
              </Switch>
            </View>
            <View class="absolute left-0 top-[275] w-[466] h-[30] items-center justify-center">
              <Text class={props.theme === 0 ? "text-2xl text-[#A0A0A0]" : props.theme === 1 ? "text-2xl text-[#525252]" : props.theme === 2 ? "text-2xl text-[#6A938F]" : props.theme === 3 ? "text-2xl text-[#948063]" : props.theme === 4 ? "text-2xl text-[#668323]" : props.theme === 5 ? "text-2xl text-[#CBCBFF]" : props.theme === 6 ? "text-2xl text-[#FFD3BC]" : props.theme === 7 ? "text-2xl text-[#A4789D]" : props.theme === 8 ? "text-2xl text-[#70CFC9]" : "text-2xl text-[#DCF3FF]"}>{props.date}</Text>
            </View>
            <Show when={props.tips}>
              <View class="absolute left-0 top-[338] w-[466] h-[52] items-center justify-center">
                <Text class="text-xl text-center text-[#EAE48D]">{"Long press\nto toggle second dot"}</Text>
              </View>
            </Show>
            <Show when={props.dot}>
              <View class="absolute left-[226] top-[226] w-[14] h-[14] rounded-full" style={{ translateX: props.dotX, translateY: props.dotY, bgColor: props.panel }} />
            </Show>
          </Match>
        </Switch>
      </View>
    </View>
  );
}
