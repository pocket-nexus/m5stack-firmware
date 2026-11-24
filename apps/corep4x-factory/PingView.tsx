import { Show } from "solid-js";
import { For, Image, Text, View } from "@pocketjs/framework/solid/components";
import type { Color, i32 } from "@pocketjs/framework/solid/std";
import type { Target } from "./net";

/** PING window content, in screen coordinates: the Ethernet card, the four
 *  target rows (dot, name, result, checkbox; a tap on a row toggles it) and
 *  the summary card. */
export default function PingView(props: {
  ethText: string;
  ethDot: Color;
  ethDetail: string;
  targets: Target[];
  enabled: boolean;
  summary: string;
  summaryColor: Color;
  onToggle: (index: i32) => void;
}) {
  return (
    <View class="absolute left-[-10] top-[-52] w-[480] h-[480]">
      <View class="absolute left-[24] top-[66] w-[432] h-[90] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
      <Text class="absolute left-[35] top-[77] text-base text-[#111827]">Ethernet Status</Text>
      <View class="absolute left-[35] top-[107] w-[10] h-[10] rounded-full" style={{ bgColor: props.ethDot }} />
      <Text class="absolute left-[53] top-[103] text-base text-[#111827]">{props.ethText}</Text>
      <Text class="absolute left-[35] top-[129] text-base text-[#64748B]">{props.ethDetail}</Text>
      <View class="absolute left-[24] top-[162] w-[432] h-[246] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
      <Text class="absolute left-[35] top-[173] text-base text-[#111827]">{"Ping Check Targets & Results"}</Text>
      <For each={props.targets} by={t => t.id}>
        {t => (
          <View class="absolute left-[35] w-[414] h-[40]" style={{ insetT: t().top, opacity: props.enabled ? 1 : 0.5 }} focusable onPress={() => props.onToggle(t().id)}>
            <View class="absolute left-0 top-[15] w-[10] h-[10] rounded-full" style={{ bgColor: t().dot }} />
            <Text class="absolute left-[18] top-[11] w-[200] text-base text-[#111827]">{t().info}</Text>
            <View class="absolute left-[226] top-[11] w-[140] h-[18] flex-col items-end">
              <Text class="text-base" style={{ textColor: t().resultColor }}>
                {t().result}
              </Text>
            </View>
            <Show when={t().checked}>
              <View class="absolute left-[376] top-[10] w-[20] h-[20] rounded-[4] bg-[#2196F3] border-[2] border-[#2196F3] items-center justify-center">
                <Image class="w-[16] h-[16]" src="assets/check.png" />
              </View>
            </Show>
            <Show when={!t().checked}>
              <View class="absolute left-[376] top-[10] w-[20] h-[20] rounded-[4] bg-white border-[2] border-[#2196F3]" />
            </Show>
          </View>
        )}
      </For>
      <View class="absolute left-[24] top-[414] w-[432] h-[52] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
      <Text class="absolute left-[33] top-[423] w-[414] text-base" style={{ textColor: props.summaryColor }}>
        {props.summary}
      </Text>
    </View>
  );
}
