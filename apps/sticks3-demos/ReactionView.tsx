import { Match, Show, Switch } from "solid-js";
import { Text, View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";
import HeaderView from "./HeaderView.tsx";

// Reaction timer. state: 0 ready, 1 waiting (dark red), 2 go (green, full
// screen), 3 result, 4 foul.
export default function ReactionView(props: {
  state: i32;
  ms: string;
  long: boolean;
  rating: string;
  best: string;
  average: string;
  newBest: boolean;
  dots: i32;
}) {
  return (
    <View debugName="Reaction" class="absolute left-0 top-0 w-[135] h-[240] bg-[#0B0E13] overflow-hidden">
      <Switch>
        <Match when={props.state === 1}>
          <View class="absolute left-0 top-0 w-[135] h-[240] bg-[#2A0F14] flex-col items-center">
            <Text class="mt-[86] text-2xl font-bold text-[#FCA5A5]">Wait…</Text>
            <View class="mt-[14] w-[60] h-[10] flex-row justify-between">
              <View class={props.dots >= 1 ? "w-[10] h-[10] rounded-full bg-[#F87171]" : "w-[10] h-[10] rounded-full bg-[#4A1D24]"} />
              <View class={props.dots >= 2 ? "w-[10] h-[10] rounded-full bg-[#F87171]" : "w-[10] h-[10] rounded-full bg-[#4A1D24]"} />
              <View class={props.dots >= 3 ? "w-[10] h-[10] rounded-full bg-[#F87171]" : "w-[10] h-[10] rounded-full bg-[#4A1D24]"} />
            </View>
            <Text class="mt-[58] text-xs text-[#B4777F]">Press on green</Text>
          </View>
        </Match>
        <Match when={props.state === 2}>
          <View class="absolute left-0 top-0 w-[135] h-[240] bg-[#22C55E] items-center justify-center">
            <Text class="text-5xl font-bold text-[#052E16]">GO!</Text>
          </View>
        </Match>
        <Match when={props.state === 3}>
          <HeaderView title="REACTION" info="" accent={2} />
          <View class="absolute left-0 top-[44] w-[135] h-[60] flex-col items-center">
            <Show when={props.long} fallback={<Text class="text-5xl font-bold text-[#F8FAFC]">{props.ms}</Text>}>
              <Text class="mt-[10] text-4xl font-bold text-[#F8FAFC]">{props.ms}</Text>
            </Show>
          </View>
          <View class="absolute left-0 top-[104] w-[135] h-[18] flex-col items-center">
            <Text class="text-sm text-[#94A3B8]">milliseconds</Text>
          </View>
          <View class="absolute left-0 top-[132] w-[135] h-[24] flex-col items-center">
            <Text class="text-xl font-bold text-[#4ADE80]">{props.rating}</Text>
          </View>
          <View class="absolute left-0 top-[162] w-[135] h-[16] flex-col items-center">
            <Show when={props.newBest} fallback={<Text class="text-xs text-[#94A3B8]">Best {props.best}</Text>}>
              <Text class="text-xs font-bold text-[#FDE047]">New best!</Text>
            </Show>
          </View>
          <View class="absolute left-0 top-[180] w-[135] h-[14] flex-col items-center">
            <Text class="text-2xs text-[#94A3B8]">Last 5 average {props.average}</Text>
          </View>
          <View class="absolute left-0 top-[200] w-[135] h-[14] flex-col items-center">
            <Text class="text-2xs text-[#6B7585]">FRONT to go again</Text>
          </View>
        </Match>
        <Match when={props.state === 4}>
          <HeaderView title="REACTION" info="" accent={2} />
          <View class="absolute left-0 top-[78] w-[135] h-[90] flex-col items-center">
            <Text class="text-2xl font-bold text-[#FDBA74]">Too soon!</Text>
            <Text class="mt-[8] text-xs text-[#94A3B8]">Wait for the green</Text>
            <Text class="mt-[22] text-2xs text-[#6B7585]">FRONT to retry</Text>
          </View>
        </Match>
        <Match when={props.state === 0}>
          <HeaderView title="REACTION" info="" accent={2} />
          <View class="absolute left-[19] top-[44] w-[96] h-[96] bg-[#1D3A27] arc-start-[0] arc-sweep-[360] arc-width-[5]" />
          <View class="absolute left-[19] top-[44] w-[96] h-[96] bg-[#4ADE80] arc-start-[-60] arc-sweep-[120] arc-width-[5]" />
          <View class="absolute left-[19] top-[44] w-[96] h-[96] items-center justify-center">
            <Text class="text-xl font-bold text-[#F8FAFC]">Ready</Text>
          </View>
          <View class="absolute left-0 top-[156] w-[135] h-[40] flex-col items-center">
            <Text class="text-xs text-[#C7CDD6]">Press FRONT, then</Text>
            <Text class="text-xs text-[#C7CDD6]">press again on green</Text>
          </View>
          <View class="absolute left-0 top-[196] w-[135] h-[14] flex-col items-center">
            <Text class="text-2xs text-[#94A3B8]">Best {props.best}</Text>
          </View>
        </Match>
      </Switch>
    </View>
  );
}
