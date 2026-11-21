import { Match, Show, Switch } from "solid-js";
import { Text, View } from "@pocketjs/framework/solid/components";
import { idiv, type i32 } from "@pocketjs/framework/solid/std";
import GlyphsLarge0 from "./GlyphsLarge0.tsx";
import MenuItem from "./MenuItem.tsx";
import OkButton from "./OkButton.tsx";
import Roller from "./RollerView.tsx";
import Toggle from "./Toggle.tsx";

// Factory app_setup: menu (view.cpp) and the device, date/time and about
// workers.
export default function Setup(props: {
  screen: i32;
  scroll: i32;
  level: i32;
  levelIds: i32[];
  levelXs: i32[];
  sadIds: i32[];
  sadXs: i32[];
  summary: string;
  dateStage: i32;
  sfx: boolean;
  vibrate: boolean;
  wifi: boolean;
  wifiStatus: string;
  progress: string;
  rollerRows: string[];
  rollerShift: i32[];
  onOpen: (item: i32) => void;
  onVersion: () => void;
  onConfirm: () => void;
  onSfx: () => void;
  onVibrate: () => void;
  onWifi: () => void;
}) {
  return (
    <View class="absolute left-0 top-0 w-[466] h-[466] bg-black">
      <Switch>
        <Match when={props.screen === 0}>
          <View class="absolute left-0 top-0 w-[466] h-[1300]" style={{ translateY: -props.scroll }}>
            <View class="absolute left-0 top-[36] w-[466] h-[32] items-center justify-center">
              <Text class="text-2xl font-bold text-white">Device</Text>
            </View>
            <MenuItem top={88} label="Brightness" onPress={() => props.onOpen(1)} />
            <MenuItem top={228} label="Volume" onPress={() => props.onOpen(2)} />
            <MenuItem top={368} label="Button" onPress={() => props.onOpen(3)} />
            <MenuItem top={508} label="Wi-Fi" onPress={() => props.onOpen(4)} />
            <View class="absolute left-0 top-[653] w-[466] h-[32] items-center justify-center">
              <Text class="text-2xl font-bold text-white">{"Time & Date"}</Text>
            </View>
            <MenuItem top={705} label="Set Time" onPress={() => props.onOpen(5)} />
            <MenuItem top={845} label="Set Date" onPress={() => props.onOpen(6)} />
            <View class="absolute left-0 top-[990] w-[466] h-[32] items-center justify-center">
              <Text class="text-2xl font-bold text-white">Firmware</Text>
            </View>
            <MenuItem top={1042} label="Version: V0.5" onPress={() => props.onVersion()} />
          </View>
        </Match>
        <Match when={props.screen === 1 || props.screen === 2}>
          <View class="absolute left-0 top-[54] w-[466] h-[98]">
            <GlyphsLarge0 ids={props.levelIds} base={0} xs={props.levelXs} />
          </View>
          <View class="absolute left-[46] top-[213] w-[374] h-[24] rounded-[12] bg-[#343434]">
            <View class="absolute left-0 top-0 h-[24] rounded-[12] bg-[#4AD78C]" style={{ width: props.screen === 1 ? idiv((props.level - 10) * 374, 90) : idiv(props.level * 374, 100) }} />
          </View>
          <View
            class="absolute left-[30] top-[209] w-[32] h-[32] rounded-full bg-white"
            style={{ translateX: props.screen === 1 ? idiv((props.level - 10) * 374, 90) : idiv(props.level * 374, 100) }}
          />
          <OkButton label="OK" onPress={() => props.onConfirm()} />
        </Match>
        <Match when={props.screen === 3}>
          <View class="absolute left-[46] top-[88] w-[374] h-[119]">
            <Text class="absolute left-[36] top-[44] w-[220] h-[32] text-2xl text-white">Button SFX</Text>
            <Toggle left={258} top={38} on={props.sfx} onToggle={() => props.onSfx()} />
          </View>
          <View class="absolute left-[46] top-[195] w-[374] h-[119]">
            <Text class="absolute left-[36] top-[44] w-[220] h-[32] text-2xl text-white">Button Vibration</Text>
            <Toggle left={258} top={38} on={props.vibrate} onToggle={() => props.onVibrate()} />
          </View>
          <OkButton label="OK" onPress={() => props.onConfirm()} />
        </Match>
        <Match when={props.screen === 4}>
          <View class="absolute left-0 top-[52] w-[466] h-[32] items-center justify-center">
            <Text class="text-2xl font-bold text-white">Wi-Fi</Text>
          </View>
          <View class="absolute left-[46] top-[100] w-[374] h-[119]">
            <Text class="absolute left-[36] top-[44] w-[220] h-[32] text-2xl text-white">Wi-Fi</Text>
            <Toggle left={258} top={38} on={props.wifi} onToggle={() => props.onWifi()} />
          </View>
          <View class="absolute left-[53] top-[223] w-[360] h-[80] items-center justify-center">
            <Text class="text-xl text-center text-[#D0D0D0]">{props.wifiStatus}</Text>
          </View>
          <OkButton label="OK" onPress={() => props.onConfirm()} />
        </Match>
        <Match when={props.screen === 5 || props.screen === 6}>
          <View class="absolute left-0 top-[60] w-[466] h-[32] items-center justify-center">
            <Text class="text-2xl font-bold text-white">{props.screen === 5 ? "Set Time" : "Set Date"}</Text>
          </View>
          <View class="absolute left-0 top-[100] w-[466] h-[28] items-center justify-center">
            <Text class="text-xl text-[#9A9A9A]">{props.summary}</Text>
          </View>
          <Show when={props.screen === 5}>
            <Roller left={58} top={138} width={110} rows={props.rollerRows} base={0} shift={props.rollerShift[0] ?? 0} wheel={false} />
            <Roller left={178} top={138} width={110} rows={props.rollerRows} base={5} shift={props.rollerShift[1] ?? 0} wheel={false} />
            <Roller left={298} top={138} width={110} rows={props.rollerRows} base={10} shift={props.rollerShift[2] ?? 0} wheel={false} />
          </Show>
          <Show when={props.screen === 6 && props.dateStage === 0}>
            <Roller left={73} top={143} width={170} rows={props.rollerRows} base={0} shift={props.rollerShift[0] ?? 0} wheel={false} />
            <Roller left={273} top={143} width={110} rows={props.rollerRows} base={5} shift={props.rollerShift[1] ?? 0} wheel={false} />
          </Show>
          <Show when={props.screen === 6 && props.dateStage === 1}>
            <Roller left={163} top={143} width={140} rows={props.rollerRows} base={10} shift={props.rollerShift[2] ?? 0} wheel={false} />
          </Show>
          <OkButton label={props.screen === 6 && props.dateStage === 0 ? "Next" : "OK"} onPress={() => props.onConfirm()} />
        </Match>
        <Match when={props.screen === 7}>
          <View class="absolute left-0 top-0 w-[466] h-[466] bg-[#0078D7]">
            <View class="absolute left-0 top-[80] w-[466] h-[98]">
              <GlyphsLarge0 ids={props.sadIds} base={0} xs={props.sadXs} />
            </View>
            <Text class="absolute left-[82] top-[216] w-[340] h-[60] text-sm text-white">
              {"Your StopWatch ran into a problem and\nneeds to restart. We're just collecting some\nerror info, and then we'll restart for you."}
            </Text>
            <Text class="absolute left-[82] top-[293] w-[300] h-[20] text-sm text-white">{props.progress}</Text>
            <Text class="absolute left-[82] top-[336] w-[320] h-[30] text-2xs text-white">
              {"If you call a support person, give them this info:\nStop code: I_DONT_REALLY_KNOW"}
            </Text>
          </View>
        </Match>
      </Switch>
    </View>
  );
}
