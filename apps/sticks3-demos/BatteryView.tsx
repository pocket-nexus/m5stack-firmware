import { Show } from "solid-js";
import { Text, View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";

// Battery percentage, a 18 x 10 battery outline with a 14 px fill, and a
// lightning mark while charging. Right-aligned in a 64 px box.
export default function BatteryView(props: { percent: i32; fill: i32; low: boolean; charging: boolean }) {
  return (
    <View class="absolute left-[67] top-[4] w-[64] h-[14]">
      <View class="absolute left-0 top-0 w-[38] h-[14] flex-row justify-end items-center">
        <Text class="text-xs text-[#C7CDD6]">{props.percent}%</Text>
      </View>
      <View class="absolute left-[42] top-[2] w-[19] h-[10] rounded-[2] border border-[#8A94A3]" />
      <View class="absolute left-[61] top-[5] w-[2] h-[4] rounded-[1] bg-[#8A94A3]" />
      <View
        class={props.charging ? "absolute left-[44] top-[4] h-[6] rounded-[1] bg-[#38BDF8]" : props.low ? "absolute left-[44] top-[4] h-[6] rounded-[1] bg-[#F87171]" : "absolute left-[44] top-[4] h-[6] rounded-[1] bg-[#4ADE80]"}
        style={{ width: props.fill }}
      />
      <Show when={props.charging}>
        <View class="absolute left-[48] top-[1] w-[4] h-[7] bg-[#FDE047] skew-x-[-24]" />
        <View class="absolute left-[50] top-[6] w-[4] h-[7] bg-[#FDE047] skew-x-[-24]" />
      </Show>
    </View>
  );
}
