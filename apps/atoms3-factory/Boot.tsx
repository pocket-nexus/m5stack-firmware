import { Show } from "solid-js";
import { View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";

// boot_animation(): eight 23-pixel rings around the bottom-left corner, one
// every 200 ms (M5.Display.fillArc(0, 128, c * 23, (c + 1) * 23, 270, 0)).
// Each ring is a disc; a later, larger disc sits below the earlier ones.
export default function Boot(props: { step: i32 }) {
  return (
    <View class="absolute left-0 top-0 w-[128] h-[128] bg-black overflow-hidden">
      <Show when={props.step > 7}>
        <View class="absolute left-[-184] top-[-56] w-[368] h-[368] rounded-full bg-[#990099]" />
      </Show>
      <Show when={props.step > 6}>
        <View class="absolute left-[-161] top-[-33] w-[322] h-[322] rounded-full bg-[#ff3399]" />
      </Show>
      <Show when={props.step > 5}>
        <View class="absolute left-[-138] top-[-10] w-[276] h-[276] rounded-full bg-[#0000ff]" />
      </Show>
      <Show when={props.step > 4}>
        <View class="absolute left-[-115] top-[13] w-[230] h-[230] rounded-full bg-[#00ffff]" />
      </Show>
      <Show when={props.step > 3}>
        <View class="absolute left-[-92] top-[36] w-[184] h-[184] rounded-full bg-[#33cc33]" />
      </Show>
      <Show when={props.step > 2}>
        <View class="absolute left-[-69] top-[59] w-[138] h-[138] rounded-full bg-[#ffff66]" />
      </Show>
      <Show when={props.step > 1}>
        <View class="absolute left-[-46] top-[82] w-[92] h-[92] rounded-full bg-[#ff6633]" />
      </Show>
      <Show when={props.step > 0}>
        <View class="absolute left-[-23] top-[105] w-[46] h-[46] rounded-full bg-[#cc3300]" />
      </Show>
    </View>
  );
}
