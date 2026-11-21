import { Show } from "solid-js";
import { For, Text, View } from "@pocketjs/framework/solid/components";
import HeaderView from "./HeaderView.tsx";
import type { Box } from "./types";

// Sound meter: level in dBFS, a 20-segment bar (kind 0..2 lit green, yellow,
// red; 3..5 unlit; 6 the held peak) and a 44-bar history of the last 4.4 s.
export default function SoundView(props: {
  segments: Box[];
  bars: Box[];
  level: string;
  peak: string;
  max: string;
  slow: boolean;
  micOn: boolean;
}) {
  return (
    <View debugName="Sound" class="absolute left-0 top-0 w-[135] h-[240] bg-[#0B0E13] overflow-hidden">
      <HeaderView title="SOUND" info={props.slow ? "slow" : "fast"} accent={5} />
      <View class="absolute left-[6] top-[28] w-[80] h-[44] flex-row items-end">
        <Text class="text-4xl font-bold text-[#F8FAFC]">{props.level}</Text>
      </View>
      <View class="absolute left-[88] top-[34] w-[44] h-[40] flex-col">
        <Text class="text-xs font-bold text-[#E879F9]">dBFS</Text>
        <Text class="mt-[4] text-2xs text-[#94A3B8]">max {props.max}</Text>
      </View>
      <View class="absolute left-0 top-[80] w-[135] h-[22]">
        <For each={props.segments} by={segment => segment.id}>
          {segment => (
            <View
              class={
                segment().kind === 0
                  ? "absolute left-0 top-0 w-[5] h-[22] rounded-[1] bg-[#4ADE80]"
                  : segment().kind === 1
                    ? "absolute left-0 top-0 w-[5] h-[22] rounded-[1] bg-[#FACC15]"
                    : segment().kind === 2
                      ? "absolute left-0 top-0 w-[5] h-[22] rounded-[1] bg-[#F43F5E]"
                      : segment().kind === 3
                        ? "absolute left-0 top-0 w-[5] h-[22] rounded-[1] bg-[#16301F]"
                        : segment().kind === 4
                          ? "absolute left-0 top-0 w-[5] h-[22] rounded-[1] bg-[#352C0B]"
                          : segment().kind === 5
                            ? "absolute left-0 top-0 w-[5] h-[22] rounded-[1] bg-[#3A1018]"
                            : "absolute left-0 top-0 w-[5] h-[22] rounded-[1] bg-[#F8FAFC]"
              }
              style={{ translateX: segment().x }}
            />
          )}
        </For>
      </View>
      <View class="absolute left-[4] top-[106] w-[127] h-[12] flex-row justify-between">
        <Text class="text-2xs text-[#6B7585]">-84</Text>
        <Text class="text-2xs text-[#6B7585]">-54</Text>
        <Text class="text-2xs text-[#6B7585]">-24</Text>
      </View>
      <View class="absolute left-0 top-[126] w-[135] h-[80] rounded-[6] bg-[#10151C]" />
      <View class="absolute left-0 top-[130] w-[135] h-[72]">
        <For each={props.bars} by={bar => bar.id}>
          {bar => (
            <View
              class={bar().kind === 0 ? "absolute left-0 top-0 w-[2] rounded-[1] bg-[#C026D3]" : bar().kind === 1 ? "absolute left-0 top-0 w-[2] rounded-[1] bg-[#E879F9]" : "absolute left-0 top-0 w-[2] rounded-[1] bg-[#F5D0FE]"}
              style={{ translateX: bar().x, translateY: bar().y, height: bar().h }}
            />
          )}
        </For>
      </View>
      <Show when={!props.micOn}>
        <View class="absolute left-[12] top-[146] w-[111] h-[40] rounded-[10] bg-[#151A21] items-center justify-center">
          <Text class="text-xs text-[#C7CDD6]">Microphone off</Text>
        </View>
      </Show>
      <View class="absolute left-0 top-[222] w-[135] h-[14] flex-col items-center">
        <Text class="text-2xs text-[#6B7585]">FRONT max · SIDE fast/slow</Text>
      </View>
    </View>
  );
}
