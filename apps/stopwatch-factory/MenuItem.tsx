import { Text, View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";

// Settings menu entry: 374 × 119, radius 60, #4C4C4C, Montserrat 28 label.
export default function MenuItem(props: { top: i32; label: string; onPress: () => void }) {
  return (
    <View class="absolute left-[46] top-0 w-[374] h-[119] rounded-[60] bg-[#4C4C4C] items-center justify-center" style={{ translateY: props.top }} focusable onPress={() => props.onPress()}>
      <Text class="text-2xl text-white">{props.label}</Text>
    </View>
  );
}
