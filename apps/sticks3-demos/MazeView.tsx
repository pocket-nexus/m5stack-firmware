import { Show } from "solid-js";
import { For, Text, View } from "@pocketjs/framework/solid/components";
import type { f32, i32 } from "@pocketjs/framework/solid/std";
import HeaderView from "./HeaderView.tsx";
import type { Box, Spot } from "./types";

// Marble maze: walls from the model, trap holes, the pulsing goal hole and
// the ball (10 px, with a highlight) that shrinks when it drops into a hole.
export default function MazeView(props: {
  walls: Box[];
  traps: Spot[];
  ballX: i32;
  ballY: i32;
  ballScale: f32;
  goalX: i32;
  goalY: i32;
  goalScale: f32;
  time: string;
  level: string;
  cleared: boolean;
  best: string;
  newBest: boolean;
}) {
  return (
    <View debugName="Maze" class="absolute left-0 top-0 w-[135] h-[240] bg-[#0B0E13] overflow-hidden">
      <HeaderView title={props.level} info={`${props.time} s`} accent={0} />
      <View
        class="absolute left-[-9] top-[-9] w-[18] h-[18] rounded-full bg-[#5C3F06]"
        style={{ translateX: props.goalX, translateY: props.goalY, scale: props.goalScale }}
      />
      <View class="absolute left-[-6] top-[-6] w-[12] h-[12] rounded-full bg-[#030405]" style={{ translateX: props.goalX, translateY: props.goalY }} />
      <For each={props.traps} by={trap => trap.id}>
        {trap => (
          <View
            class="absolute left-[-6] top-[-6] w-[12] h-[12] rounded-full bg-[#030405] border border-[#6B1D2A]"
            style={{ translateX: trap().x, translateY: trap().y }}
          />
        )}
      </For>
      <For each={props.walls} by={wall => wall.id}>
        {wall => (
          <View
            class="absolute left-0 top-0 rounded-[1] bg-[#3A4456]"
            style={{ translateX: wall().x, translateY: wall().y, width: wall().w, height: wall().h }}
          />
        )}
      </For>
      <View
        class="absolute left-[-5] top-[-5] w-[10] h-[10] rounded-full bg-[#FFB020]"
        style={{ translateX: props.ballX, translateY: props.ballY, scale: props.ballScale }}
      >
        <View class="absolute left-[2] top-[2] w-[3] h-[3] rounded-full bg-[#FFF3D6]" />
      </View>
      <Show when={props.cleared}>
        <View class="absolute left-[12] top-[72] w-[111] h-[112] rounded-[14] bg-[#151A21] border border-[#FFB020] flex-col items-center justify-center">
          <Text class="text-sm font-bold text-[#FFB020]">Cleared!</Text>
          <Text class="mt-[2] text-2xl font-bold text-[#F8FAFC]">{props.time} s</Text>
          <Show when={props.newBest} fallback={<Text class="mt-[2] text-xs text-[#94A3B8]">Best {props.best}</Text>}>
            <Text class="mt-[2] text-xs font-bold text-[#FDE047]">New best!</Text>
          </Show>
          <Text class="mt-[8] text-2xs text-[#6B7585]">FRONT next level</Text>
        </View>
      </Show>
    </View>
  );
}
