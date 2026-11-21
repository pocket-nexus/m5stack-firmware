import { Show } from "solid-js";
import { Text, View } from "@pocketjs/framework/solid/components";
import type { Deg, f32, i32 } from "@pocketjs/framework/solid/std";
import HeaderView from "./HeaderView.tsx";

// Metronome: tempo, Italian tempo name, a pendulum (rod pivoting at its
// bottom with a sliding weight) that reaches an end on every beat, a glow
// on each click and four beat dots with the accented first beat.
export default function MetronomeView(props: {
  bpm: string;
  tempo: string;
  running: boolean;
  beat: i32;
  flash: f32;
  swing: Deg;
  weight: i32;
}) {
  return (
    <View debugName="Metronome" class="absolute left-0 top-0 w-[135] h-[240] bg-[#0B0E13] overflow-hidden">
      <HeaderView title="METRONOME" info="4/4" accent={6} />
      <View class="absolute left-[17] top-[26] w-[100] h-[56] rounded-[16] bg-[#4C3D99]" style={{ opacity: props.flash }} />
      <View class="absolute left-0 top-[26] w-[135] h-[56] flex-col items-center">
        <Text class="text-5xl font-bold text-[#F8FAFC]">{props.bpm}</Text>
      </View>
      <View class="absolute left-0 top-[84] w-[135] h-[16] flex-col items-center">
        <Text class="text-xs text-[#A78BFA]">BPM · {props.tempo}</Text>
      </View>
      <View class="absolute left-[33] top-[106] w-[68] h-[90] rounded-[12] bg-[#151A21] border border-[#2A2350]" />
      <View class="absolute left-[25] top-[192] w-[84] h-[8] rounded-[4] bg-[#2A2350]" />
      <View class="absolute left-[66] top-[86] w-[3] h-[100] rounded-[1] bg-[#DDD6FE] origin-bottom" style={{ rotate: props.swing }}>
        <View class="absolute left-[-5] top-0 w-[13] h-[9] rounded-[2] bg-[#A78BFA]" style={{ translateY: 100 - props.weight }} />
      </View>
      <View class="absolute left-[62] top-[181] w-[11] h-[11] rounded-full bg-[#A78BFA]" />
      <View class="absolute left-0 top-[206] w-[135] h-[12] flex-row justify-center items-center gap-[10]">
        <View class={props.beat === 0 ? "w-[12] h-[12] rounded-full bg-[#F5F3FF]" : "w-[12] h-[12] rounded-full bg-[#3B3170]"} />
        <View class={props.beat === 1 ? "w-[9] h-[9] rounded-full bg-[#A78BFA]" : "w-[9] h-[9] rounded-full bg-[#2A2350]"} />
        <View class={props.beat === 2 ? "w-[9] h-[9] rounded-full bg-[#A78BFA]" : "w-[9] h-[9] rounded-full bg-[#2A2350]"} />
        <View class={props.beat === 3 ? "w-[9] h-[9] rounded-full bg-[#A78BFA]" : "w-[9] h-[9] rounded-full bg-[#2A2350]"} />
      </View>
      <Show when={!props.running}>
        <View class="absolute left-0 top-[224] w-[135] h-[14] flex-col items-center">
          <Text class="text-2xs text-[#6B7585]">FRONT start · SIDE tempo</Text>
        </View>
      </Show>
    </View>
  );
}
