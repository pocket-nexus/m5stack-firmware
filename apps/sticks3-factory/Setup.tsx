import { Match, Switch } from "solid-js";
import { Image, Text, View } from "@pocketjs/framework/solid/components";
import Cloud from "./Cloud.tsx";
import type { SetupState } from "./app";

// SetupApp: one artwork per provisioning state. The access point name is
// centred at x 67, y 130 and underlined; the portal QR code (version 2,
// "http://192.168.4.1") sits at (13, 49), 109 px wide.
export default function Setup(props: { state: SetupState; ssid: string; wifiOk: boolean; serverOk: boolean; code: string; nick: string }) {
  return (
    <View debugName="Setup" class="absolute left-0 top-0 w-[135] h-[240] overflow-hidden">
      <Switch>
        <Match when={props.state === "ap"}>
          <Image class="absolute left-0 top-0 w-[256] h-[256]" src="images/setup-ap.png" />
          <View class="absolute left-0 top-[129] w-[135] h-[32] flex-col items-center">
            <View class="flex-col">
              <Text class="text-2xl text-black">{props.ssid}</Text>
              <View class="w-full h-[2] bg-black" />
            </View>
          </View>
        </Match>
        <Match when={props.state === "web"}>
          <Image class="absolute left-0 top-0 w-[256] h-[256]" src="images/setup-web.png" />
          <View class="absolute left-[13] top-[49] w-[109] h-[109] overflow-hidden">
            <Image class="absolute left-0 top-0 w-[128] h-[128]" src="images/setup-qr.png" />
          </View>
        </Match>
        <Match when={props.state === "wait-wifi"}>
          <Image class="absolute left-0 top-0 w-[256] h-[256]" src="images/setup-wifi.png" />
        </Match>
        <Match when={props.state === "wifi-ng"}>
          <Image class="absolute left-0 top-0 w-[256] h-[256]" src="images/setup-wifi-ng.png" />
        </Match>
        <Match when={props.state === "wifi-ok"}>
          <Image class="absolute left-0 top-0 w-[256] h-[256]" src="images/setup-wifi-ok.png" />
        </Match>
        <Match when={props.state === "server-ng"}>
          <Image class="absolute left-0 top-0 w-[256] h-[256]" src="images/setup-server-ng.png" />
        </Match>
        <Match when={props.state === "server-ok"}>
          <Cloud wifiOk={props.wifiOk} serverOk={props.serverOk} code={props.code} nick={props.nick} />
        </Match>
      </Switch>
    </View>
  );
}
