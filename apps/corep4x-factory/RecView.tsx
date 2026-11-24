import { Show } from "solid-js";
import { For, Image, Text, View } from "@pocketjs/framework/solid/components";
import type { Color, i32 } from "@pocketjs/framework/solid/std";
import Button from "./Button.tsx";

/** REC window content, in screen coordinates (content padding 6). The
 *  board's texture holds the display card with its charts or spectrograms
 *  (448x298 at (16,58), 448x406 in Spectrum mode); the labels are drawn
 *  over it. The button card (REC 404x51, PLAY LAST 404x39) is hidden in
 *  Spectrum mode. */
export default function RecView(props: {
  texture: i32[];
  spectrum: boolean;
  showRef: boolean;
  db: string[];
  recText: string;
  recBg: Color;
  recFg: Color;
  recEnabled: boolean;
  playText: string;
  playBg: Color;
  playFg: Color;
  playEnabled: boolean;
  onRec: () => void;
  onPlay: () => void;
}) {
  return (
    <View class="absolute left-[-10] top-[-52] w-[480] h-[480]">
      <View class={props.spectrum ? "absolute left-[16] top-[58] w-[448] h-[406] overflow-hidden rounded-[8]" : "absolute left-[16] top-[58] w-[448] h-[298] overflow-hidden rounded-[8]"}>
        <For each={props.texture} by={handle => handle}>
          {_handle => <Image class="absolute left-0 top-0 w-[512] h-[512]" src="assets/rec-canvas.png" />}
        </For>
      </View>
      <Show when={!props.spectrum}>
        <Text class="absolute left-[23] top-[67] text-base text-[#64748B]">MIC Monitor</Text>
        <View class="absolute left-[291] top-[67] w-[110] h-[18] flex-col items-end">
          <Text class="text-base text-[#2563EB]">Waveform</Text>
        </View>
        <Text class={props.showRef ? "absolute left-[23] top-[124] text-base text-[#64748B]" : "absolute left-[23] top-[146] text-base text-[#64748B]"}>MIC1</Text>
        <Text class={props.showRef ? "absolute left-[23] top-[212] text-base text-[#64748B]" : "absolute left-[23] top-[278] text-base text-[#64748B]"}>MIC2</Text>
        <View class={props.showRef ? "absolute left-[409] top-[124] w-[48] h-[18] flex-col items-end" : "absolute left-[409] top-[146] w-[48] h-[18] flex-col items-end"}>
          <Text class="text-base text-[#64748B]">{props.db[0] ?? "-inf"}</Text>
        </View>
        <View class={props.showRef ? "absolute left-[409] top-[212] w-[48] h-[18] flex-col items-end" : "absolute left-[409] top-[278] w-[48] h-[18] flex-col items-end"}>
          <Text class="text-base text-[#64748B]">{props.db[1] ?? "-inf"}</Text>
        </View>
        <Show when={props.showRef}>
          <Text class="absolute left-[23] top-[300] text-base text-[#64748B]">REF</Text>
          <View class="absolute left-[409] top-[300] w-[48] h-[18] flex-col items-end">
            <Text class="text-base text-[#64748B]">{props.db[2] ?? "-inf"}</Text>
          </View>
        </Show>
        <View class="absolute left-[16] top-[360] w-[448] h-[104] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
        <Button left={38} top={364} width={404} height={51} text={props.recText} bg={props.recBg} fg={props.recFg} border="#CBD5E1" radius={8} size={16} enabled={props.recEnabled} dim={0.5} onPress={() => props.onRec()} />
        <Button left={38} top={419} width={404} height={39} text={props.playText} bg={props.playBg} fg={props.playFg} border="#CBD5E1" radius={8} size={16} enabled={props.playEnabled} dim={0.5} onPress={() => props.onPlay()} />
      </Show>
      <Show when={props.spectrum}>
        <Text class="absolute left-[23] top-[67] text-base text-[#64748B]">Spectrum</Text>
        <View class="absolute left-[291] top-[67] w-[110] h-[18] flex-col items-end">
          <Text class="text-base text-[#2563EB]">Live</Text>
        </View>
        <View class={props.showRef ? "absolute left-[23] top-[115] w-[20] h-[72] justify-center" : "absolute left-[23] top-[146] w-[20] h-[72] justify-center"}>
          <Text class="text-base text-center text-[#64748B]">{"M\nI\nC\n1"}</Text>
        </View>
        <View class={props.showRef ? "absolute left-[23] top-[239] w-[20] h-[72] justify-center" : "absolute left-[23] top-[332] w-[20] h-[72] justify-center"}>
          <Text class="text-base text-center text-[#64748B]">{"M\nI\nC\n2"}</Text>
        </View>
        <View class={props.showRef ? "absolute left-[47] top-[106] w-[28] h-[90] flex-col items-end" : "absolute left-[47] top-[137] w-[28] h-[90] flex-col items-end"}>
          <Text class="text-base text-[#64748B]">{"8k\n6k\n4k\n2k\n1k"}</Text>
        </View>
        <View class={props.showRef ? "absolute left-[47] top-[230] w-[28] h-[90] flex-col items-end" : "absolute left-[47] top-[323] w-[28] h-[90] flex-col items-end"}>
          <Text class="text-base text-[#64748B]">{"8k\n6k\n4k\n2k\n1k"}</Text>
        </View>
        <Show when={props.showRef}>
          <View class="absolute left-[23] top-[372] w-[20] h-[54] justify-center">
            <Text class="text-base text-center text-[#64748B]">{"R\nE\nF"}</Text>
          </View>
          <View class="absolute left-[47] top-[354] w-[28] h-[90] flex-col items-end">
            <Text class="text-base text-[#64748B]">{"8k\n6k\n4k\n2k\n1k"}</Text>
          </View>
        </Show>
      </Show>
    </View>
  );
}
