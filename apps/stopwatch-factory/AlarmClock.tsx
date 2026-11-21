import { Show } from "solid-js";
import { For, Text, View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";
import type { AlarmRow } from "./app";
import Dialog from "./Dialog.tsx";
import OkButton from "./OkButton.tsx";
import Roller from "./RollerView.tsx";
import Toggle from "./Toggle.tsx";

// Factory app_alarm_clock: alarm list (alarm_list.cpp) and add screen
// (add_alarm.cpp).
export default function AlarmClock(props: {
  screen: i32;
  rows: AlarmRow[];
  scroll: i32;
  addY: i32;
  dialog: boolean;
  rollerRows: string[];
  rollerShift: i32[];
  onToggle: (index: i32) => void;
  onAdd: () => void;
  onConfirmAdd: () => void;
  onDelete: () => void;
  onCancel: () => void;
}) {
  return (
    <View class="absolute left-0 top-0 w-[466] h-[466] bg-black">
      <Show when={props.screen === 0}>
        <View class="absolute left-0 top-0 w-[466] h-[466]" style={{ translateY: -props.scroll }}>
          <View class="absolute left-0 top-[36] w-[466] h-[32] items-center justify-center">
            <Text class="text-2xl font-bold text-white">Alarms</Text>
          </View>
          <For each={props.rows} by={row => row.id}>
            {row => (
              <View class="absolute left-[46] top-0 w-[374] h-[119] rounded-[60] bg-[#4C4C4C]" style={{ translateY: row().y }}>
                <Text class="absolute left-[49] top-[37] w-[200] h-[44] text-4xl text-white">{row().time}</Text>
                <Toggle left={258} top={38} on={row().enabled} onToggle={() => props.onToggle(row().id)} />
              </View>
            )}
          </For>
          <View class="absolute left-[46] top-0 w-[374] h-[119] rounded-[60] bg-[#4C4C4C] items-center justify-center" style={{ translateY: props.addY }} focusable onPress={() => props.onAdd()}>
            <Text class="text-2xl text-white">Add</Text>
          </View>
        </View>
        <Show when={props.dialog}>
          <Dialog question="Delete this alarm?" confirm="Delete" danger={true} onConfirm={() => props.onDelete()} onCancel={() => props.onCancel()} />
        </Show>
      </Show>
      <Show when={props.screen === 1}>
        <View class="absolute left-0 top-[75] w-[466] h-[32] items-center justify-center">
          <Text class="text-2xl font-bold text-white">Add Alarm</Text>
        </View>
        <Roller left={63} top={138} width={160} rows={props.rollerRows} base={0} shift={props.rollerShift[0] ?? 0} wheel={false} />
        <Roller left={243} top={138} width={160} rows={props.rollerRows} base={5} shift={props.rollerShift[1] ?? 0} wheel={false} />
        <OkButton label="OK" onPress={() => props.onConfirmAdd()} />
      </Show>
    </View>
  );
}
