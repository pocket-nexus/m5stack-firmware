import { View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";

// Launcher page indicator dot: 14 px opaque when selected, 8 px at 50 %.
export default function PageDot(props: { x: i32; selected: boolean }) {
  return (
    <View class="absolute left-0 top-[426] w-[14] h-[14] items-center justify-center" style={{ translateX: props.x }}>
      <View class={props.selected ? "w-[14] h-[14] rounded-full bg-white" : "w-[8] h-[8] rounded-full bg-white opacity-50"} />
    </View>
  );
}
