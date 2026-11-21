import { Show } from "solid-js";
import { Image, Text, View } from "@pocketjs/framework/solid/components";
import { idiv, type i32 } from "@pocketjs/framework/solid/std";

// Factory status bar: a #EDF4FF pill with the battery gauge and level.
export default function StatusBar(props: { y: i32; level: i32; text: string; charging: boolean; onTap: () => void }) {
  return (
    <View class="absolute left-[154] top-0 w-[158] h-[65] rounded-[27] bg-[#EDF4FF]" style={{ translateY: props.y }} focusable onPress={() => props.onTap()}>
      <View class="absolute left-[52] top-[35] w-[30] h-[12] rounded-[4] bg-[#EDF4FF] border-[#385179] p-[1]">
        <View class={props.charging ? "h-[8] rounded-[3] bg-[#19C25F]" : "h-[8] rounded-[3] bg-[#385179]"} style={{ width: idiv(props.level * 26, 100) }} />
      </View>
      <View class="absolute left-[80] top-[39] w-[4] h-[4] rounded-[4] bg-[#EDF4FF] border-[#385179]" />
      <Show when={props.charging}>
        <Image class="absolute left-[59] top-[33] w-[16] h-[16]" src="images/icon_bat_lightning.png" />
      </Show>
      <Text class="absolute left-[96] top-[30] w-[44] h-[22] text-base text-left text-[#385179]">{props.text}</Text>
    </View>
  );
}
