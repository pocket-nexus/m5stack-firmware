import { View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";

// lv_switch as the factory styles it: 80 × 44, #3A3A3A off, #53BD65 on.
export default function Toggle(props: { left: i32; top: i32; on: boolean; onToggle: () => void }) {
  return (
    <View
      class={props.on ? "absolute left-0 top-0 w-[80] h-[44] rounded-[22] bg-[#53BD65]" : "absolute left-0 top-0 w-[80] h-[44] rounded-[22] bg-[#3A3A3A]"}
      style={{ translateX: props.left, translateY: props.top }}
      focusable
      onPress={() => props.onToggle()}
    >
      <View class={props.on ? "absolute left-[40] top-[4] w-[36] h-[36] rounded-full bg-white" : "absolute left-[4] top-[4] w-[36] h-[36] rounded-full bg-white"} />
    </View>
  );
}
