import { Match, Switch } from "solid-js";
import { ActionHandler, View } from "@pocketjs/framework/solid/components";
import { onMount } from "@pocketjs/framework/solid/lifecycle";
import { BTN } from "@pocketjs/framework/input";
import Cloud from "./Cloud.tsx";
import Launcher from "./Launcher.tsx";
import Setup from "./Setup.tsx";
import Usb from "./Usb.tsx";
import {
  battery,
  code,
  isCharging,
  link,
  mode,
  nick,
  page,
  pressA,
  pressB,
  serverOk,
  setupState,
  ssid,
  start,
  version,
  wifiOk,
} from "./app";

// KEY1 (front, M5Unified BtnA) reports CIRCLE; KEY2 (side, BtnB) reports DOWN.
export default function App() {
  onMount(start);
  return (
    <View debugName="StickS3" class="w-full h-full bg-black overflow-hidden">
      <ActionHandler button={BTN.CIRCLE} onPress={pressA} />
      <ActionHandler button={BTN.DOWN} onPress={pressB} />
      <Switch>
        <Match when={page() === "launcher"}>
          <Launcher mode={mode()} link={link()} charging={isCharging()} battery={battery()} version={version()} />
        </Match>
        <Match when={page() === "cloud"}>
          <Cloud wifiOk={wifiOk()} serverOk={serverOk()} code={code()} nick={nick()} />
        </Match>
        <Match when={page() === "usb"}>
          <Usb battery={battery()} />
        </Match>
        <Match when={page() === "setup"}>
          <Setup state={setupState()} ssid={ssid()} wifiOk={wifiOk()} serverOk={serverOk()} code={code()} nick={nick()} />
        </Match>
      </Switch>
    </View>
  );
}
