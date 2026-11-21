import { Show } from "solid-js";
import { Text, View } from "@pocketjs/framework/solid/components";
import type { Deg, i32 } from "@pocketjs/framework/solid/std";
import HeaderView from "./HeaderView.tsx";

// Spirit level. Flat: a bull's-eye with the bubble, the X and Y angles and a
// horizontal vial. On an edge: the roll angle and a horizon line that stays
// level while the stick turns. Everything turns green when level.
export default function LevelView(props: {
  flat: boolean;
  bubbleX: i32;
  bubbleY: i32;
  vialX: i32;
  textX: string;
  textY: string;
  textRoll: string;
  roll: Deg;
  level: boolean;
  zeroed: boolean;
}) {
  return (
    <View debugName="Level" class="absolute left-0 top-0 w-[135] h-[240] bg-[#0B0E13] overflow-hidden">
      <HeaderView title="LEVEL" info={props.zeroed ? "zeroed" : props.flat ? "flat" : "edge"} accent={4} />
      <Show
        when={props.flat}
        fallback={
          <View class="absolute left-0 top-0 w-[135] h-[240]">
            <View class="absolute left-[6] top-[129] w-[14] h-[2] bg-[#38BDF8]" />
            <View class="absolute left-[115] top-[129] w-[14] h-[2] bg-[#38BDF8]" />
            <View
              class={props.level ? "absolute left-[-33] top-[129] w-[201] h-[2] bg-[#4ADE80]" : "absolute left-[-33] top-[129] w-[201] h-[2] bg-[#E2E8F0]"}
              style={{ rotate: props.roll }}
            />
            <View
              class={props.level ? "absolute left-[66] top-[70] w-[2] h-[120] bg-[#4ADE80]" : "absolute left-[66] top-[70] w-[2] h-[120] bg-[#475569]"}
              style={{ rotate: props.roll }}
            />
            <View class="absolute left-[61] top-[124] w-[12] h-[12] rounded-full bg-[#38BDF8]" />
            <View class="absolute left-0 top-[34] w-[135] h-[40] flex-col items-center">
              <Show when={props.level} fallback={<Text class="text-4xl font-bold text-[#F8FAFC]">{props.textRoll}</Text>}>
                <Text class="text-4xl font-bold text-[#4ADE80]">{props.textRoll}</Text>
              </Show>
            </View>
            <View class="absolute left-0 top-[196] w-[135] h-[16] flex-col items-center">
              <Text class="text-xs text-[#94A3B8]">Roll against vertical</Text>
            </View>
          </View>
        }
      >
        <View class="absolute left-[16] top-[30] w-[104] h-[104] rounded-full bg-[#0E1A24] border-2 border-[#38BDF8]" />
        <View class="absolute left-[42] top-[56] w-[52] h-[52] rounded-full border border-[#1E4A61]" />
        <View class="absolute left-[17] top-[81] w-[102] h-[1] bg-[#1E3A4C]" />
        <View class="absolute left-[67] top-[31] w-[1] h-[102] bg-[#1E3A4C]" />
        <View
          class={props.level ? "absolute left-[56] top-[70] w-[24] h-[24] rounded-full border-2 border-[#4ADE80]" : "absolute left-[56] top-[70] w-[24] h-[24] rounded-full border-2 border-[#2B6684]"}
        />
        <View
          class={props.level ? "absolute left-[59] top-[73] w-[18] h-[18] rounded-full bg-[#4ADE80]" : "absolute left-[59] top-[73] w-[18] h-[18] rounded-full bg-[#7DD3FC]"}
          style={{ translateX: props.bubbleX, translateY: props.bubbleY }}
        >
          <View class="absolute left-[4] top-[3] w-[5] h-[4] rounded-full bg-[#F0F9FF]" />
        </View>
        <View class="absolute left-[8] top-[140] w-[58] h-[40] flex-col items-center">
          <Text class="text-2xs text-[#6B7585]">X</Text>
          <Text class="text-lg font-bold text-[#F8FAFC]">{props.textX}</Text>
        </View>
        <View class="absolute left-[69] top-[140] w-[58] h-[40] flex-col items-center">
          <Text class="text-2xs text-[#6B7585]">Y</Text>
          <Text class="text-lg font-bold text-[#F8FAFC]">{props.textY}</Text>
        </View>
        <View class="absolute left-[10] top-[188] w-[116] h-[18] rounded-full bg-[#0E1A24] border-2 border-[#38BDF8]" />
        <View class="absolute left-[55] top-[188] w-[1] h-[18] bg-[#1E4A61]" />
        <View class="absolute left-[80] top-[188] w-[1] h-[18] bg-[#1E4A61]" />
        <View
          class={props.level ? "absolute left-[57] top-[191] w-[22] h-[12] rounded-full bg-[#4ADE80]" : "absolute left-[57] top-[191] w-[22] h-[12] rounded-full bg-[#7DD3FC]"}
          style={{ translateX: props.vialX }}
        />
      </Show>
      <View class="absolute left-0 top-[222] w-[135] h-[14] flex-col items-center">
        <Text class="text-2xs text-[#6B7585]">FRONT zero · SIDE clear</Text>
      </View>
    </View>
  );
}
