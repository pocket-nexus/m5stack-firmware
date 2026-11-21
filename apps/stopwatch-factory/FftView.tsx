import { Show } from "solid-js";
import { For, Text, View } from "@pocketjs/framework/solid/components";
import type { f32 } from "@pocketjs/framework/solid/std";
import type { Bar } from "./app";

// Factory app_fft view: 40 mirrored spectrum bars (#C19BFF → #FF66BC by
// length) around a pulsing #F8C6E7 disc with the peak frequency.
export default function Fft(props: { bars: Bar[]; scale: f32; labels: boolean; hz: string; onTap: () => void }) {
  return (
    <View class="absolute left-0 top-0 w-[466] h-[466] bg-[#1F1528]" focusable onPress={() => props.onTap()}>
      <For each={props.bars} by={bar => bar.id}>
        {bar => <View class={bar().level === 0 ? "absolute left-[233] top-[227] h-[12] bg-[#C19BFF]" : bar().level === 1 ? "absolute left-[233] top-[227] h-[12] bg-[#CB93F6]" : bar().level === 2 ? "absolute left-[233] top-[227] h-[12] bg-[#D58AEC]" : bar().level === 3 ? "absolute left-[233] top-[227] h-[12] bg-[#DF82E3]" : bar().level === 4 ? "absolute left-[233] top-[227] h-[12] bg-[#E97AE0]" : bar().level === 5 ? "absolute left-[233] top-[227] h-[12] bg-[#F075D6]" : bar().level === 6 ? "absolute left-[233] top-[227] h-[12] bg-[#F870C9]" : "absolute left-[233] top-[227] h-[12] bg-[#FF66BC]"} style={{ width: bar().length, rotate: bar().angle, originX: -0.5 }} />}
      </For>
      <View class="absolute left-[164] top-[164] w-[137] h-[137] rounded-full bg-[#F8C6E7]" style={{ scale: props.scale }}>
        <Show when={props.labels}>
          <View class="absolute left-0 top-[46] w-[137] h-[34] items-center justify-center">
            <Text class="text-2xl text-[#AE5D92]">{props.hz}</Text>
          </View>
          <View class="absolute left-0 top-[82] w-[137] h-[24] items-center justify-center">
            <Text class="text-lg text-[#D586BA]">Hz</Text>
          </View>
        </Show>
      </View>
    </View>
  );
}
