import { Show } from "solid-js";
import { For, Text, View } from "@pocketjs/framework/solid/components";
import type { f32, i32 } from "@pocketjs/framework/solid/std";
import HeaderView from "./HeaderView.tsx";
import type { Spot } from "./types";

// Snake: 15 x 24 cells of 9 px below the title bar. Segment kind 0 is the
// head, 1 and 2 alternate along the body. state: 0 ready, 1 playing,
// 2 dying (the body blinks), 3 game over.
export default function SnakeView(props: {
  segments: Spot[];
  foodX: i32;
  foodY: i32;
  foodScale: f32;
  state: i32;
  score: string;
  best: string;
  visible: boolean;
  newBest: boolean;
}) {
  return (
    <View debugName="Snake" class="absolute left-0 top-0 w-[135] h-[240] bg-[#0B0E13] overflow-hidden">
      <HeaderView title="SNAKE" info={`${props.score} · best ${props.best}`} accent={3} />
      <View class="absolute left-0 top-[24] w-[135] h-[216] bg-[#0E141B]" />
      <View
        class="absolute left-[1] top-[1] w-[7] h-[7] rounded-full bg-[#FF5470]"
        style={{ translateX: props.foodX, translateY: props.foodY, scale: props.foodScale }}
      />
      <Show when={props.visible}>
        <For each={props.segments} by={segment => segment.id}>
          {segment => (
            <View
              class={segment().kind === 0 ? "absolute left-0 top-0 w-[8] h-[8] rounded-[3] bg-[#99F6E4]" : segment().kind === 1 ? "absolute left-0 top-0 w-[8] h-[8] rounded-[2] bg-[#14B8A6]" : "absolute left-0 top-0 w-[8] h-[8] rounded-[2] bg-[#0D9488]"}
              style={{ translateX: segment().x, translateY: segment().y }}
            />
          )}
        </For>
      </Show>
      <Show when={props.state === 0}>
        <View class="absolute left-[12] top-[66] w-[111] h-[96] rounded-[14] bg-[#151A21] border border-[#2DD4BF] flex-col items-center">
          <Text class="mt-[10] text-lg font-bold text-[#2DD4BF]">Snake</Text>
          <Text class="mt-[6] text-xs text-[#C7CDD6]">FRONT turns left</Text>
          <Text class="text-xs text-[#C7CDD6]">SIDE turns right</Text>
          <Text class="mt-[10] text-2xs text-[#6B7585]">Press a key to start</Text>
        </View>
      </Show>
      <Show when={props.state === 3}>
        <View class="absolute left-[12] top-[66] w-[111] h-[96] rounded-[14] bg-[#151A21] border border-[#2DD4BF] flex-col items-center">
          <Text class="mt-[10] text-lg font-bold text-[#F8FAFC]">Game over</Text>
          <Text class="mt-[4] text-2xl font-bold text-[#2DD4BF]">{props.score}</Text>
          <Show when={props.newBest} fallback={<Text class="text-xs text-[#94A3B8]">Best {props.best}</Text>}>
            <Text class="text-xs font-bold text-[#FDE047]">New best!</Text>
          </Show>
          <Text class="mt-[6] text-2xs text-[#6B7585]">Press to play</Text>
        </View>
      </Show>
    </View>
  );
}
