import { For, Text, View } from "@pocketjs/framework/solid/components";
import type { Color, i32 } from "@pocketjs/framework/solid/std";
import type { Bubble } from "./i2c";

/** I2C SCAN window content, in screen coordinates: the bus card with the
 *  two 198x64 bus buttons, and the results card with the summary and the
 *  198x38 result bubbles, two per row. */
export default function I2cView(props: { summary: string; summaryColor: Color; bubbles: Bubble[]; onScan: (bus: i32) => void }) {
  return (
    <View class="absolute left-[-10] top-[-52] w-[480] h-[480]">
      <View class="absolute left-[24] top-[66] w-[432] h-[84] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
      <View class="absolute left-[35] top-[77] w-[198] h-[64] rounded-[8] bg-[#2563EB] border-[1] border-[#2563EB]" focusable onPress={() => props.onScan(0)}>
        <View class="absolute left-0 top-[12] w-[196] h-[22] justify-center">
          <Text class="text-xl text-white">Internal I2C</Text>
        </View>
        <View class="absolute left-0 top-[34] w-[196] h-[16] justify-center">
          <Text class="text-sm text-[#DBEAFE]">SCL9 / SDA11</Text>
        </View>
      </View>
      <View class="absolute left-[241] top-[77] w-[198] h-[64] rounded-[8] bg-[#2563EB] border-[1] border-[#2563EB]" focusable onPress={() => props.onScan(1)}>
        <View class="absolute left-0 top-[12] w-[196] h-[22] justify-center">
          <Text class="text-xl text-white">PORT.A I2C</Text>
        </View>
        <View class="absolute left-0 top-[34] w-[196] h-[16] justify-center">
          <Text class="text-sm text-[#DBEAFE]">SCL16 / SDA18</Text>
        </View>
      </View>
      <View class="absolute left-[24] top-[156] w-[432] h-[300] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
      <Text class="absolute left-[35] top-[172] w-[118] text-base text-[#111827]">Scan Results</Text>
      <View class="absolute left-[161] top-[172] w-[282] h-[18] flex-col items-end">
        <Text class="text-base" style={{ textColor: props.summaryColor }}>
          {props.summary}
        </Text>
      </View>
      <View class="absolute left-[35] top-[201] w-[410] h-[246] overflow-hidden">
        <For each={props.bubbles} by={b => b.id}>
          {b => (
            <View class="absolute w-[198] h-[38] rounded-[12] border-[1]" style={{ insetL: b().left - 35, insetT: b().top - 201, bgColor: b().bg, borderColor: b().border }}>
              <Text class="absolute left-[6] top-[9] text-base" style={{ textColor: b().text }}>
                {b().address}
              </Text>
              <Text class="absolute left-[58] top-[9] text-base" style={{ textColor: b().text }}>
                {b().name}
              </Text>
              <View class="absolute left-[138] top-[9] w-[52] h-[18] overflow-hidden">
                <Text class="absolute left-0 top-0 text-base" style={{ textColor: b().text }}>
                  {b().state}
                </Text>
              </View>
            </View>
          )}
        </For>
      </View>
    </View>
  );
}
