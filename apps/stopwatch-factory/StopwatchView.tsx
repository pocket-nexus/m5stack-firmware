import { Show } from "solid-js";
import { For, Text, View } from "@pocketjs/framework/solid/components";
import { len, type i32 } from "@pocketjs/framework/solid/std";
import type { LapRow } from "./app";
import GlyphsElapsed from "./GlyphsElapsed.tsx";
import GlyphsLap from "./GlyphsLap.tsx";
import GlyphsLapEmpty from "./GlyphsLapEmpty.tsx";

// Factory app_stopwatch view: two 105 × 72 buttons under the arc clock, the
// elapsed time on a #41484B panel and the lap list below a divider.
export default function Stopwatch(props: {
  state: i32;
  timeIds: i32[];
  timeXs: i32[];
  emptyIds: i32[];
  emptyXs: i32[];
  laps: LapRow[];
  scroll: i32;
  leftOffset: i32;
  rightOffset: i32;
  onLeft: () => void;
  onRight: () => void;
}) {
  return (
    <View class="absolute left-0 top-0 w-[466] h-[466] bg-black">
      <View
        class="absolute left-[109] top-[54] w-[105] h-[72] rounded-[34] items-center justify-center bg-[#B3CDFF] shadow"
        style={{ translateX: props.leftOffset, translateY: props.leftOffset }}
        focusable
        onPress={() => props.onLeft()}
      >
        <Text class="text-2xl text-[#2E487A]">{props.state === 2 ? "RESET" : "LAP"}</Text>
      </View>
      <View
        class={props.state === 1 ? "absolute left-[253] top-[54] w-[105] h-[72] rounded-[34] items-center justify-center bg-[#FF9EAB] shadow" : "absolute left-[253] top-[54] w-[105] h-[72] rounded-[34] items-center justify-center bg-[#9CF1B6] shadow"}
        style={{ translateX: -props.rightOffset, translateY: props.rightOffset }}
        focusable
        onPress={() => props.onRight()}
      >
        <Text class={props.state === 1 ? "text-2xl text-[#7A1926]" : "text-2xl text-[#176C31]"}>{props.state === 1 ? "STOP" : "START"}</Text>
      </View>
      <View class="absolute left-0 top-[148] w-[466] h-[330] rounded-[60] bg-[#41484B]">
        <View class="absolute left-0 top-[26] w-[466] h-[55]">
          <GlyphsElapsed ids={props.timeIds} base={0} xs={props.timeXs} />
        </View>
        <View class="absolute left-[153] top-[98] w-[160] h-[4] rounded-[6] bg-[#58646A]" />
        <View class="absolute left-0 top-[104] w-[466] h-[214] rounded-[60] bg-[#41484B] overflow-hidden">
          <Show when={len(props.laps) === 0}>
            <View class="absolute left-0 top-[42] w-[466] h-[33]">
              <GlyphsLapEmpty ids={props.emptyIds} base={0} xs={props.emptyXs} />
            </View>
          </Show>
          <View class="absolute left-0 top-[24] w-[466] h-[2000]" style={{ translateY: -props.scroll }}>
            <For each={props.laps} by={row => row.id}>
              {row => (
                <View class="absolute left-0 top-0 w-[466] h-[34]" style={{ translateY: row().y }}>
                  <GlyphsLap ids={row().titleIds} base={0} xs={row().titleXs} />
                  <GlyphsLap ids={row().timeIds} base={0} xs={row().timeXs} />
                </View>
              )}
            </For>
          </View>
        </View>
      </View>
    </View>
  );
}
