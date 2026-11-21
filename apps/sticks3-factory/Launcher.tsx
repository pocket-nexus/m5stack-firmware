import { Match, Show, Switch } from "solid-js";
import { Image, Text, View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";
import type { LinkState, Mode } from "./app";

// LauncherApp: full-screen mode artwork, Wi-Fi/cloud state dot at (6, 6),
// charge icon at (59, 3), battery label right-aligned at x 132 and the
// firmware version centred at x 67, y 152.
export default function Launcher(props: { mode: Mode; link: LinkState; charging: boolean; battery: i32; version: string }) {
  return (
    <View debugName="Launcher" class="absolute left-0 top-0 w-[135] h-[240] overflow-hidden">
      <Switch>
        <Match when={props.mode === "cloud"}>
          <Image class="absolute left-0 top-0 w-[256] h-[256]" src="images/launcher-cloud.png" />
        </Match>
        <Match when={props.mode === "usb"}>
          <Image class="absolute left-0 top-0 w-[256] h-[256]" src="images/launcher-usb.png" />
        </Match>
        <Match when={props.mode === "setup"}>
          <Image class="absolute left-0 top-0 w-[256] h-[256]" src="images/launcher-setup.png" />
        </Match>
      </Switch>
      <View class="absolute left-[6] top-[6] w-[16] h-[16] overflow-hidden">
        <Switch>
          <Match when={props.link === "never-set"}>
            <Image class="absolute left-0 top-0 w-[16] h-[16]" src="images/state-wifi-never-set.png" />
          </Match>
          <Match when={props.link === "wifi-ng"}>
            <Image class="absolute left-0 top-0 w-[16] h-[16]" src="images/state-wifi-ng.png" />
          </Match>
          <Match when={props.link === "server-ng"}>
            <Image class="absolute left-0 top-0 w-[16] h-[16]" src="images/state-wifi-ok-server-ng.png" />
          </Match>
          <Match when={props.link === "server-ok"}>
            <Image class="absolute left-0 top-0 w-[16] h-[16]" src="images/state-wifi-ok-server-ok.png" />
          </Match>
        </Switch>
      </View>
      <View class="absolute left-[59] top-[3] w-[16] h-[22] overflow-hidden">
        <Show when={props.charging} fallback={<Image class="absolute left-0 top-0 w-[16] h-[32]" src="images/no-charge.png" />}>
          <Image class="absolute left-0 top-0 w-[16] h-[32]" src="images/charge.png" />
        </Show>
      </View>
      <View class="absolute left-[85] top-[4] w-[47] h-[22] flex-col items-end">
        <Text class="text-lg text-black">{props.battery}%</Text>
      </View>
      <View class="absolute left-0 top-[151] w-[135] h-[22] flex-col items-center">
        <Text class="text-lg text-black">{props.version}</Text>
      </View>
    </View>
  );
}
