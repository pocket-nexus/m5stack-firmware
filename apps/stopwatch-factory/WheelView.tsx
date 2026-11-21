import { Show } from "solid-js";
import { For, Image, Text, View } from "@pocketjs/framework/solid/components";
import type { Deg, i32 } from "@pocketjs/framework/solid/std";
import type { Sector } from "./wheel";
import OkButton from "./OkButton.tsx";
import Roller from "./RollerView.tsx";

// Factory app_lucky_wheel: option count (selection.cpp) and the wheel
// (wheel.cpp). Each sector is painted as a half-plane rotated to its start
// angle inside a clip of the screen half its start lies in, so later sectors
// cover the rest of earlier ones (sector 0 starts at 12 o'clock).
export default function Wheel(props: {
  screen: i32;
  rightHalf: Sector[];
  leftHalf: Sector[];
  labels: Sector[];
  pointer: Deg;
  rollerRows: string[];
  rollerShift: i32[];
  onConfirm: () => void;
  onSpin: () => void;
}) {
  return (
    <View class="absolute left-0 top-0 w-[466] h-[466] bg-[#3B3B3B]">
      <Show when={props.screen === 0}>
        <View class="absolute left-0 top-[75] w-[466] h-[32] items-center justify-center">
          <Text class="text-2xl font-bold text-white">Number of Options</Text>
        </View>
        <Roller left={46} top={138} width={374} rows={props.rollerRows} base={0} shift={props.rollerShift[0] ?? 0} wheel={true} />
        <OkButton label="OK" onPress={() => props.onConfirm()} />
      </Show>
      <Show when={props.screen === 1}>
        <View class="absolute left-0 top-0 w-[466] h-[466] bg-[#202020]" focusable onPress={() => props.onSpin()}>
          <View class="absolute left-[233] top-0 w-[233] h-[466] overflow-hidden">
            <For each={props.rightHalf} by={sector => sector.id}>
              {sector => <View class={sector().color === 0 ? "absolute left-[-350] top-[233] w-[700] h-[350] bg-[#4AD78C]" : sector().color === 1 ? "absolute left-[-350] top-[233] w-[700] h-[350] bg-[#7AC4F5]" : sector().color === 2 ? "absolute left-[-350] top-[233] w-[700] h-[350] bg-[#F4CA63]" : sector().color === 3 ? "absolute left-[-350] top-[233] w-[700] h-[350] bg-[#D194EA]" : "absolute left-[-350] top-[233] w-[700] h-[350] bg-[#FF77A0]"} style={{ rotate: sector().angle, originY: -0.5 }} />}
            </For>
          </View>
          <View class="absolute left-0 top-0 w-[233] h-[466] overflow-hidden">
            <For each={props.leftHalf} by={sector => sector.id}>
              {sector => <View class={sector().color === 0 ? "absolute left-[-117] top-[233] w-[700] h-[350] bg-[#4AD78C]" : sector().color === 1 ? "absolute left-[-117] top-[233] w-[700] h-[350] bg-[#7AC4F5]" : sector().color === 2 ? "absolute left-[-117] top-[233] w-[700] h-[350] bg-[#F4CA63]" : sector().color === 3 ? "absolute left-[-117] top-[233] w-[700] h-[350] bg-[#D194EA]" : "absolute left-[-117] top-[233] w-[700] h-[350] bg-[#FF77A0]"} style={{ rotate: sector().angle, originY: -0.5 }} />}
            </For>
          </View>
          <For each={props.labels} by={sector => sector.id}>
            {sector => (
              <View class="absolute left-[143] top-[413] w-[180] h-[40] items-center justify-center" style={{ rotate: sector().labelAngle, originY: -5.0 }}>
                <Text class={sector().color === 0 ? "text-2xl font-bold text-[#028B40]" : sector().color === 1 ? "text-2xl font-bold text-[#2E78A9]" : sector().color === 2 ? "text-2xl font-bold text-[#A87E17]" : sector().color === 3 ? "text-2xl font-bold text-[#85489E]" : "text-2xl font-bold text-[#B32B54]"}>{sector().label}</Text>
              </View>
            )}
          </For>
          <View class="absolute left-[186] top-[159] w-[128] h-[128]" style={{ rotate: props.pointer, originX: -0.1328125, originY: 0.078125 }}>
            <Image class="w-[128] h-[128]" src="images/lucky_wheel_pointer.png" />
          </View>
        </View>
      </Show>
    </View>
  );
}
