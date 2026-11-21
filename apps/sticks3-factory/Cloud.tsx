import { Show } from "solid-js";
import { Image, Text, View } from "@pocketjs/framework/solid/components";

// CloudApp (and SetupApp's server-ok state): artwork a11, the WiFi and
// SERVER status tags at (98, 27) and (98, 55), the access code and the
// nickname centred at x 67, y 101 and y 151.
export default function Cloud(props: { wifiOk: boolean; serverOk: boolean; code: string; nick: string }) {
  return (
    <View debugName="Cloud" class="absolute left-0 top-0 w-[135] h-[240] overflow-hidden">
      <Image class="absolute left-0 top-0 w-[256] h-[256]" src="images/cloud.png" />
      <View class="absolute left-[98] top-[27] w-[34] h-[24] overflow-hidden">
        <Show when={props.wifiOk} fallback={<Image class="absolute left-0 top-0 w-[64] h-[32]" src="images/ng.png" />}>
          <Image class="absolute left-0 top-0 w-[64] h-[32]" src="images/wifi-ok.png" />
        </Show>
      </View>
      <View class="absolute left-[98] top-[55] w-[34] h-[24] overflow-hidden">
        <Show when={props.serverOk} fallback={<Image class="absolute left-0 top-0 w-[64] h-[32]" src="images/ng.png" />}>
          <Image class="absolute left-0 top-0 w-[64] h-[32]" src="images/server-ok.png" />
        </Show>
      </View>
      <View class="absolute left-0 top-[100] w-[135] h-[29] flex-col items-center">
        <Text class="text-2xl text-black">{props.code}</Text>
      </View>
      <View class="absolute left-[3] top-[150] w-[129] h-[29] flex-col items-center overflow-hidden">
        <Text class="text-2xl text-black">{props.nick}</Text>
      </View>
    </View>
  );
}
