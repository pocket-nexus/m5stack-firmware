import { Text, View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";

// Demo title bar: the title in the demo's accent colour on the left, a
// status text on the right, a hairline below. accent: 0 maze, 1 dice,
// 2 reaction, 3 snake, 4 level, 5 sound, 6 metronome.
export default function HeaderView(props: { title: string; info: string; accent: i32 }) {
  return (
    <View class="absolute left-0 top-0 w-[135] h-[22]">
      <Text
        class={
          props.accent === 0
            ? "absolute left-[7] top-[4] text-xs font-bold text-[#FFB020]"
            : props.accent === 1
              ? "absolute left-[7] top-[4] text-xs font-bold text-[#FF5470]"
              : props.accent === 2
                ? "absolute left-[7] top-[4] text-xs font-bold text-[#4ADE80]"
                : props.accent === 3
                  ? "absolute left-[7] top-[4] text-xs font-bold text-[#2DD4BF]"
                  : props.accent === 4
                    ? "absolute left-[7] top-[4] text-xs font-bold text-[#38BDF8]"
                    : props.accent === 5
                      ? "absolute left-[7] top-[4] text-xs font-bold text-[#E879F9]"
                      : "absolute left-[7] top-[4] text-xs font-bold text-[#A78BFA]"
        }
      >
        {props.title}
      </Text>
      <View class="absolute left-[60] top-[4] w-[68] h-[16] flex-row justify-end">
        <Text class="text-xs text-[#C7CDD6]">{props.info}</Text>
      </View>
      <View class="absolute left-0 top-[21] w-[135] h-[1] bg-[#1C232D]" />
    </View>
  );
}
