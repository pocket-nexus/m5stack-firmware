import { For, Text, View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";
import Button from "./Button.tsx";
import type { Row } from "./sd";

/** SD CARD window content, in screen coordinates: the explorer card (header
 *  row with the path and Back, a 410x274 list of 30 px rows) and the status
 *  card (selection size left, card capacity right). */
export default function SdView(props: {
  path: string;
  rows: Row[];
  backEnabled: boolean;
  size: string;
  status: string;
  onRow: (index: i32) => void;
  onBack: () => void;
}) {
  return (
    <View class="absolute left-[-10] top-[-52] w-[480] h-[480]">
      <View class="absolute left-[24] top-[66] w-[432] h-[348] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
      <Text class="absolute left-[35] top-[84] text-base text-[#111827]">Explorer</Text>
      <Text class="absolute left-[118] top-[85] w-[230] text-sm text-[#2563EB]">{props.path}</Text>
      <Button left={385} top={79} width={60} height={28} text="Back" bg="#F3F4F6" fg="#111827" border="#CBD5E1" radius={8} size={14} enabled={props.backEnabled} dim={0.5} onPress={() => props.onBack()} />
      <View class="absolute left-[35] top-[115] w-[410] h-[274] overflow-hidden">
        <For each={props.rows} by={row => row.id}>
          {row => (
            <View
              class="absolute left-0 w-[410] h-[30] rounded-[6] border-[1]"
              style={{ insetT: row().top - 115, bgColor: row().bg, borderColor: row().border }}
              focusable
              onPress={() => props.onRow(row().id)}
            >
              <Text class="absolute left-[12] top-[6] text-sm" style={{ textColor: row().fg }}>
                {row().text}
              </Text>
            </View>
          )}
        </For>
      </View>
      <View class="absolute left-[24] top-[420] w-[432] h-[36] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
      <Text class="absolute left-[37] top-[430] w-[180] text-sm text-[#64748B]">{props.size}</Text>
      <View class="absolute left-[223] top-[430] w-[220] h-[16] flex-col items-end">
        <Text class="text-sm text-[#64748B]">{props.status}</Text>
      </View>
    </View>
  );
}
