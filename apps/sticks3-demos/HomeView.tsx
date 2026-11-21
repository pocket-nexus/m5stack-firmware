import { Text, View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";
import BatteryView from "./BatteryView.tsx";

// Home carousel: seven 135 px cards on a strip whose offsets the model eases,
// each with a vector icon drawn from Views in the demo's accent colour, the
// title and a one-line description; page dots and the key legend below.
export default function HomeView(props: {
  offsets: i32[];
  selected: i32;
  percent: i32;
  fill: i32;
  low: boolean;
  charging: boolean;
}) {
  return (
    <View debugName="Home" class="absolute left-0 top-0 w-[135] h-[240] bg-[#0B0E13] overflow-hidden">
      <Text class="absolute left-[8] top-[3] text-sm font-bold text-[#F1F5F9]">StickS3</Text>
      <BatteryView percent={props.percent} fill={props.fill} low={props.low} charging={props.charging} />

      <View class="absolute left-0 top-[26] w-[135] h-[176]" style={{ translateX: props.offsets[0] ?? 0 }}>
        <View class="absolute left-[23] top-[12] w-[88] h-[88] rounded-[22] bg-[#151A21] border border-[#252D39]">
          <View class="absolute left-[14] top-[14] w-[60] h-[60] rounded-[8] border-[3] border-[#FFB020]" />
          <View class="absolute left-[14] top-[34] w-[32] h-[3] bg-[#FFB020]" />
          <View class="absolute left-[44] top-[34] w-[3] h-[22] bg-[#FFB020]" />
          <View class="absolute left-[28] top-[52] w-[3] h-[22] bg-[#FFB020]" />
          <View class="absolute left-[56] top-[14] w-[3] h-[26] bg-[#FFB020]" />
          <View class="absolute left-[21] top-[20] w-[10] h-[10] rounded-full bg-[#FFF3D6]" />
          <View class="absolute left-[55] top-[55] w-[12] h-[12] rounded-full bg-[#0B0E13] border-2 border-[#FFB020]" />
        </View>
        <View class="absolute left-0 top-[110] w-[135] h-[26] flex-col items-center">
          <Text class="text-xl font-bold text-[#F8FAFC]">Marble Maze</Text>
        </View>
        <View class="absolute left-0 top-[138] w-[135] h-[16] flex-col items-center">
          <Text class="text-xs text-[#FFB020]">Tilt to roll the ball</Text>
        </View>
      </View>

      <View class="absolute left-0 top-[26] w-[135] h-[176]" style={{ translateX: props.offsets[1] ?? 0 }}>
        <View class="absolute left-[23] top-[12] w-[88] h-[88] rounded-[22] bg-[#151A21] border border-[#252D39]">
          <View class="absolute left-[19] top-[19] w-[50] h-[50] rounded-[12] bg-[#FF5470] rotate-12">
            <View class="absolute left-[8] top-[8] w-[9] h-[9] rounded-full bg-[#FFF1F3]" />
            <View class="absolute left-[33] top-[8] w-[9] h-[9] rounded-full bg-[#FFF1F3]" />
            <View class="absolute left-[20] top-[20] w-[9] h-[9] rounded-full bg-[#FFF1F3]" />
            <View class="absolute left-[8] top-[33] w-[9] h-[9] rounded-full bg-[#FFF1F3]" />
            <View class="absolute left-[33] top-[33] w-[9] h-[9] rounded-full bg-[#FFF1F3]" />
          </View>
        </View>
        <View class="absolute left-0 top-[110] w-[135] h-[26] flex-col items-center">
          <Text class="text-xl font-bold text-[#F8FAFC]">Dice</Text>
        </View>
        <View class="absolute left-0 top-[138] w-[135] h-[16] flex-col items-center">
          <Text class="text-xs text-[#FF5470]">Shake to throw</Text>
        </View>
      </View>

      <View class="absolute left-0 top-[26] w-[135] h-[176]" style={{ translateX: props.offsets[2] ?? 0 }}>
        <View class="absolute left-[23] top-[12] w-[88] h-[88] rounded-[22] bg-[#151A21] border border-[#252D39]">
          <View class="absolute left-[40] top-[9] w-[8] h-[7] rounded-[2] bg-[#4ADE80]" />
          <View class="absolute left-[16] top-[18] w-[56] h-[56] bg-[#24452F] arc-start-[0] arc-sweep-[360] arc-width-[6]" />
          <View class="absolute left-[16] top-[18] w-[56] h-[56] bg-[#4ADE80] arc-start-[0] arc-sweep-[250] arc-width-[6]" />
          <View class="absolute left-[37] top-[39] w-[14] h-[14] rounded-full bg-[#4ADE80]" />
          <View class="absolute left-[43] top-[24] w-[2] h-[20] rounded-[1] bg-[#4ADE80]" />
        </View>
        <View class="absolute left-0 top-[110] w-[135] h-[26] flex-col items-center">
          <Text class="text-xl font-bold text-[#F8FAFC]">Reaction</Text>
        </View>
        <View class="absolute left-0 top-[138] w-[135] h-[16] flex-col items-center">
          <Text class="text-xs text-[#4ADE80]">Press on green</Text>
        </View>
      </View>

      <View class="absolute left-0 top-[26] w-[135] h-[176]" style={{ translateX: props.offsets[3] ?? 0 }}>
        <View class="absolute left-[23] top-[12] w-[88] h-[88] rounded-[22] bg-[#151A21] border border-[#252D39]">
          <View class="absolute left-[16] top-[22] w-[12] h-[12] rounded-[3] bg-[#0F766E]" />
          <View class="absolute left-[30] top-[22] w-[12] h-[12] rounded-[3] bg-[#14B8A6]" />
          <View class="absolute left-[44] top-[22] w-[12] h-[12] rounded-[3] bg-[#0F766E]" />
          <View class="absolute left-[44] top-[36] w-[12] h-[12] rounded-[3] bg-[#14B8A6]" />
          <View class="absolute left-[44] top-[50] w-[12] h-[12] rounded-[3] bg-[#0F766E]" />
          <View class="absolute left-[30] top-[50] w-[12] h-[12] rounded-[3] bg-[#14B8A6]" />
          <View class="absolute left-[16] top-[50] w-[12] h-[12] rounded-[3] bg-[#2DD4BF]" />
          <View class="absolute left-[19] top-[54] w-[2] h-[2] bg-[#0B0E13]" />
          <View class="absolute left-[23] top-[54] w-[2] h-[2] bg-[#0B0E13]" />
          <View class="absolute left-[62] top-[64] w-[10] h-[10] rounded-full bg-[#FF5470]" />
        </View>
        <View class="absolute left-0 top-[110] w-[135] h-[26] flex-col items-center">
          <Text class="text-xl font-bold text-[#F8FAFC]">Snake</Text>
        </View>
        <View class="absolute left-0 top-[138] w-[135] h-[16] flex-col items-center">
          <Text class="text-xs text-[#2DD4BF]">Two keys, one snake</Text>
        </View>
      </View>

      <View class="absolute left-0 top-[26] w-[135] h-[176]" style={{ translateX: props.offsets[4] ?? 0 }}>
        <View class="absolute left-[23] top-[12] w-[88] h-[88] rounded-[22] bg-[#151A21] border border-[#252D39]">
          <View class="absolute left-[24] top-[10] w-[40] h-[40] rounded-full border-2 border-[#38BDF8]" />
          <View class="absolute left-[38] top-[24] w-[12] h-[12] rounded-full bg-[#38BDF8]" />
          <View class="absolute left-[10] top-[58] w-[68] h-[18] rounded-full border-2 border-[#38BDF8]" />
          <View class="absolute left-[34] top-[62] w-[20] h-[10] rounded-full bg-[#38BDF8]" />
          <View class="absolute left-[31] top-[56] w-[2] h-[22] bg-[#1E4A61]" />
          <View class="absolute left-[55] top-[56] w-[2] h-[22] bg-[#1E4A61]" />
        </View>
        <View class="absolute left-0 top-[110] w-[135] h-[26] flex-col items-center">
          <Text class="text-xl font-bold text-[#F8FAFC]">Spirit Level</Text>
        </View>
        <View class="absolute left-0 top-[138] w-[135] h-[16] flex-col items-center">
          <Text class="text-xs text-[#38BDF8]">Bubble and angles</Text>
        </View>
      </View>

      <View class="absolute left-0 top-[26] w-[135] h-[176]" style={{ translateX: props.offsets[5] ?? 0 }}>
        <View class="absolute left-[23] top-[12] w-[88] h-[88] rounded-[22] bg-[#151A21] border border-[#252D39]">
          <View class="absolute left-[14] top-[50] w-[6] h-[16] rounded-[3] bg-[#E879F9]" />
          <View class="absolute left-[23] top-[36] w-[6] h-[30] rounded-[3] bg-[#E879F9]" />
          <View class="absolute left-[32] top-[20] w-[6] h-[46] rounded-[3] bg-[#E879F9]" />
          <View class="absolute left-[41] top-[30] w-[6] h-[36] rounded-[3] bg-[#E879F9]" />
          <View class="absolute left-[50] top-[14] w-[6] h-[52] rounded-[3] bg-[#F0ABFC]" />
          <View class="absolute left-[59] top-[34] w-[6] h-[32] rounded-[3] bg-[#E879F9]" />
          <View class="absolute left-[68] top-[46] w-[6] h-[20] rounded-[3] bg-[#E879F9]" />
          <View class="absolute left-[12] top-[72] w-[64] h-[2] rounded-[1] bg-[#5B2A66]" />
        </View>
        <View class="absolute left-0 top-[110] w-[135] h-[26] flex-col items-center">
          <Text class="text-xl font-bold text-[#F8FAFC]">Sound Meter</Text>
        </View>
        <View class="absolute left-0 top-[138] w-[135] h-[16] flex-col items-center">
          <Text class="text-xs text-[#E879F9]">Microphone level</Text>
        </View>
      </View>

      <View class="absolute left-0 top-[26] w-[135] h-[176]" style={{ translateX: props.offsets[6] ?? 0 }}>
        <View class="absolute left-[23] top-[12] w-[88] h-[88] rounded-[22] bg-[#151A21] border border-[#252D39]">
          <View class="absolute left-[26] top-[16] w-[36] h-[56] rounded-[8] border-2 border-[#A78BFA]" />
          <View class="absolute left-[20] top-[70] w-[48] h-[6] rounded-[3] bg-[#A78BFA]" />
          <View class="absolute left-[43] top-[22] w-[3] h-[44] rounded-[1] bg-[#DDD6FE] rotate-22 origin-bottom" />
          <View class="absolute left-[50] top-[34] w-[12] h-[8] rounded-[2] bg-[#A78BFA] rotate-22" />
          <View class="absolute left-[40] top-[62] w-[8] h-[8] rounded-full bg-[#A78BFA]" />
        </View>
        <View class="absolute left-0 top-[110] w-[135] h-[26] flex-col items-center">
          <Text class="text-xl font-bold text-[#F8FAFC]">Metronome</Text>
        </View>
        <View class="absolute left-0 top-[138] w-[135] h-[16] flex-col items-center">
          <Text class="text-xs text-[#A78BFA]">Keep the beat</Text>
        </View>
      </View>

      <View class="absolute left-0 top-[206] w-[135] h-[6] flex-row justify-center items-center gap-[5]">
        <View class={props.selected === 0 ? "w-[14] h-[6] rounded-full bg-[#FFB020]" : "w-[6] h-[6] rounded-full bg-[#2D3644]"} />
        <View class={props.selected === 1 ? "w-[14] h-[6] rounded-full bg-[#FF5470]" : "w-[6] h-[6] rounded-full bg-[#2D3644]"} />
        <View class={props.selected === 2 ? "w-[14] h-[6] rounded-full bg-[#4ADE80]" : "w-[6] h-[6] rounded-full bg-[#2D3644]"} />
        <View class={props.selected === 3 ? "w-[14] h-[6] rounded-full bg-[#2DD4BF]" : "w-[6] h-[6] rounded-full bg-[#2D3644]"} />
        <View class={props.selected === 4 ? "w-[14] h-[6] rounded-full bg-[#38BDF8]" : "w-[6] h-[6] rounded-full bg-[#2D3644]"} />
        <View class={props.selected === 5 ? "w-[14] h-[6] rounded-full bg-[#E879F9]" : "w-[6] h-[6] rounded-full bg-[#2D3644]"} />
        <View class={props.selected === 6 ? "w-[14] h-[6] rounded-full bg-[#A78BFA]" : "w-[6] h-[6] rounded-full bg-[#2D3644]"} />
      </View>
      <View class="absolute left-0 top-[220] w-[135] h-[14] flex-col items-center">
        <Text class="text-2xs text-[#6B7585]">SIDE next · FRONT open</Text>
      </View>
    </View>
  );
}
