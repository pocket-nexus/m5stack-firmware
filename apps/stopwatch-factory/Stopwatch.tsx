import { Show } from "solid-js";
import { For, Text, View } from "@pocketjs/framework/solid/components";
import { len, type i32 } from "@pocketjs/framework/solid/std";
import type { LapRow } from "./app";

// Factory app_stopwatch view: two 105 × 72 buttons under the arc clock, the
// elapsed time on a #41484B panel and the lap list below a divider.
export default function Stopwatch(props: {
  state: i32;
  time: string;
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
        <Text class="text-xl text-[#2E487A]">{props.state === 2 ? "RESET" : "LAP"}</Text>
      </View>
      <View
        class={props.state === 1 ? "absolute left-[253] top-[54] w-[105] h-[72] rounded-[34] items-center justify-center bg-[#FF9EAB] shadow" : "absolute left-[253] top-[54] w-[105] h-[72] rounded-[34] items-center justify-center bg-[#9CF1B6] shadow"}
        style={{ translateX: -props.rightOffset, translateY: props.rightOffset }}
        focusable
        onPress={() => props.onRight()}
      >
        <Text class={props.state === 1 ? "text-xl text-[#7A1926]" : "text-xl text-[#176C31]"}>{props.state === 1 ? "STOP" : "START"}</Text>
      </View>
      <View class="absolute left-0 top-[148] w-[466] h-[330] rounded-[60] bg-[#41484B]">
        <View class="absolute left-0 top-[26] w-[466] h-[50] items-center">
          <Text class="text-4xl text-[#D8F2FF]">{props.time}</Text>
        </View>
        <View class="absolute left-[153] top-[98] w-[160] h-[4] rounded-[6] bg-[#58646A]" />
        <View class="absolute left-0 top-[104] w-[466] h-[214] rounded-[60] bg-[#41484B] overflow-hidden">
          <Show when={len(props.laps) === 0}>
            <View class="absolute left-0 top-[42] w-[466] h-[34] items-center">
              <Text class="text-2xl text-[#738086]">-.-</Text>
            </View>
          </Show>
          <View class="absolute left-0 top-[24] w-[466] h-[2000]" style={{ translateY: -props.scroll }}>
            <For each={props.laps} by={row => row.id}>
              {row => (
                <View class="absolute left-0 top-0 w-[466] h-[34]" style={{ translateY: row().y }}>
                  <Text class="absolute left-[64] top-0 w-[160] h-[34] text-2xl text-[#D8F2FF]">{row().title}</Text>
                  <Text class="absolute right-[64] top-0 w-[200] h-[34] text-2xl text-right text-[#D8F2FF]">{row().time}</Text>
                </View>
              )}
            </For>
          </View>
        </View>
      </View>
    </View>
  );
}
