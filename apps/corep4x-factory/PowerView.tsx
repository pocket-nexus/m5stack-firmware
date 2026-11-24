import { Show } from "solid-js";
import { For, Text, View } from "@pocketjs/framework/solid/components";
import type { f32, i32, Px } from "@pocketjs/framework/solid/std";
import Button from "./Button.tsx";
import type { Pin } from "./power";

/** POWER window content, in screen coordinates. Each tab is one card in the
 *  432x342 page at (24,66), clipped to it. Rail buttons are 198x110 (the
 *  factory's Montserrat 28 labels use the 24 px slot). */
export default function PowerView(props: {
  tab: i32;
  rails: boolean[];
  backlight: i32;
  knob: Px;
  indicator: Px;
  vbat: string;
  charge: i32;
  pins: Pin[];
  onToggle: (rail: i32) => void;
  onCharge: (value: i32) => void;
  onPin: (pin: i32) => void;
}) {
  return (
    <View class="absolute left-[-10] top-[-52] w-[480] h-[480]">
      <View class="absolute left-[24] top-[66] w-[432] h-[342] overflow-hidden">
        <View class="absolute left-[-24] top-[-66] w-[480] h-[480]">
          <Show when={props.tab === 0}>
            <View class="absolute left-[24] top-[66] w-[432] h-[346] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
            <Text class="absolute left-[35] top-[77] text-base text-[#111827]">Peripheral Power</Text>
            <Button left={35} top={102} width={198} height={110} text={(props.rails[4] ?? false) ? "USBA ON" : "USBA OFF"} bg={(props.rails[4] ?? false) ? "#16A34A" : "#F3F4F6"} fg={(props.rails[4] ?? false) ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={24} enabled={true} dim={0.5} onPress={() => props.onToggle(4)} />
            <Button left={241} top={102} width={198} height={110} text={(props.rails[5] ?? false) ? "P4USBC ON" : "P4USBC OFF"} bg={(props.rails[5] ?? false) ? "#16A34A" : "#F3F4F6"} fg={(props.rails[5] ?? false) ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={24} enabled={true} dim={0.5} onPress={() => props.onToggle(5)} />
            <Button left={35} top={220} width={198} height={110} text={(props.rails[6] ?? false) ? "Grove ON" : "Grove OFF"} bg={(props.rails[6] ?? false) ? "#16A34A" : "#F3F4F6"} fg={(props.rails[6] ?? false) ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={24} enabled={true} dim={0.5} onPress={() => props.onToggle(6)} />
            <Button left={241} top={220} width={198} height={110} text={(props.rails[7] ?? false) ? "M5BUS ON" : "M5BUS OFF"} bg={(props.rails[7] ?? false) ? "#16A34A" : "#F3F4F6"} fg={(props.rails[7] ?? false) ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={24} enabled={true} dim={0.5} onPress={() => props.onToggle(7)} />
            <Text class="absolute left-[35] top-[341] w-[120] text-base text-[#64748B]">Backlight</Text>
            <View class="absolute left-[163] top-[341] w-[262] h-[18] flex-col items-end">
              <Text class="text-base text-[#111827]">{`${props.backlight}%`}</Text>
            </View>
            <View class="absolute left-[35] top-[375] w-[390] h-[20] rounded-[10] bg-[#F3F4F6]" />
            <View class="absolute left-[35] top-[375] h-[20] rounded-[10] bg-[#2563EB]" style={{ width: props.indicator }} />
            <View class="absolute top-[369] w-[32] h-[32] rounded-[16] bg-white border-[2] border-[#CBD5E1]" style={{ insetL: props.knob }} />
          </Show>
          <Show when={props.tab === 1}>
            <View class="absolute left-[24] top-[66] w-[432] h-[324] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
            <Text class="absolute left-[35] top-[77] text-base text-[#111827]">System Power</Text>
            <Button left={35} top={104} width={198} height={110} text={(props.rails[0] ?? false) ? "L3B ON" : "L3B OFF"} bg={(props.rails[0] ?? false) ? "#16A34A" : "#F3F4F6"} fg={(props.rails[0] ?? false) ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={24} enabled={true} dim={0.5} onPress={() => props.onToggle(0)} />
            <Button left={241} top={104} width={198} height={110} text={(props.rails[1] ?? false) ? "LCD ON" : "LCD OFF"} bg={(props.rails[1] ?? false) ? "#16A34A" : "#F3F4F6"} fg={(props.rails[1] ?? false) ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={24} enabled={true} dim={0.5} onPress={() => props.onToggle(1)} />
            <Button left={35} top={224} width={198} height={110} text={(props.rails[2] ?? false) ? "AudioIC ON" : "AudioIC OFF"} bg={(props.rails[2] ?? false) ? "#16A34A" : "#F3F4F6"} fg={(props.rails[2] ?? false) ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={24} enabled={true} dim={0.5} onPress={() => props.onToggle(2)} />
            <Button left={241} top={224} width={198} height={110} text={(props.rails[3] ?? false) ? "AudioPA ON" : "AudioPA OFF"} bg={(props.rails[3] ?? false) ? "#16A34A" : "#F3F4F6"} fg={(props.rails[3] ?? false) ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={24} enabled={true} dim={0.5} onPress={() => props.onToggle(3)} />
            <View class="absolute left-[35] top-[343] w-[146] h-[36] rounded-[18] bg-[#E0F2FE] border-[1] border-[#93C5FD]" />
            <Text class="absolute left-[48] top-[352] w-[64] text-base text-[#111827]">Charge</Text>
            <View class="absolute left-[116] top-[352] w-[54] h-[18] flex-col items-end">
              <Text class="text-base text-[#2563EB]">{props.vbat}</Text>
            </View>
            <Button left={189} top={343} width={78} height={36} text="OFF" bg={props.charge === 0 ? "#2563EB" : "#F3F4F6"} fg={props.charge === 0 ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={16} enabled={true} dim={0.5} onPress={() => props.onCharge(0)} />
            <Button left={275} top={343} width={78} height={36} text="180mA" bg={props.charge === 1 ? "#2563EB" : "#F3F4F6"} fg={props.charge === 1 ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={16} enabled={true} dim={0.5} onPress={() => props.onCharge(1)} />
            <Button left={361} top={343} width={78} height={36} text="650mA" bg={props.charge === 2 ? "#2563EB" : "#F3F4F6"} fg={props.charge === 2 ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={16} enabled={true} dim={0.5} onPress={() => props.onCharge(2)} />
          </Show>
          <Show when={props.tab === 2}>
            <View class="absolute left-[24] top-[66] w-[128] h-[12] justify-center">
              <Text class="text-2xs text-black">P01-P10</Text>
            </View>
            <View class="absolute left-[176] top-[66] w-[128] h-[12] justify-center">
              <Text class="text-2xs text-black">P11-P20</Text>
            </View>
            <View class="absolute left-[328] top-[66] w-[128] h-[12] justify-center">
              <Text class="text-2xs text-black">P21-P30</Text>
            </View>
            <View class="absolute left-[24] top-[81] w-[60] h-[294] rounded-[4] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
            <View class="absolute left-[92] top-[81] w-[60] h-[294] rounded-[4] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
            <View class="absolute left-[176] top-[81] w-[60] h-[294] rounded-[4] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
            <View class="absolute left-[244] top-[81] w-[60] h-[294] rounded-[4] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
            <View class="absolute left-[328] top-[81] w-[60] h-[294] rounded-[4] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
            <View class="absolute left-[396] top-[81] w-[60] h-[294] rounded-[4] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
            <View class="absolute left-[29] top-[86] w-[50] h-[20] rounded-[3] bg-[#E2E8F0] items-center justify-center">
              <Text class="text-2xs text-black">L</Text>
            </View>
            <View class="absolute left-[97] top-[86] w-[50] h-[20] rounded-[3] bg-[#E2E8F0] items-center justify-center">
              <Text class="text-2xs text-black">R</Text>
            </View>
            <View class="absolute left-[181] top-[86] w-[50] h-[20] rounded-[3] bg-[#E2E8F0] items-center justify-center">
              <Text class="text-2xs text-black">L</Text>
            </View>
            <View class="absolute left-[249] top-[86] w-[50] h-[20] rounded-[3] bg-[#E2E8F0] items-center justify-center">
              <Text class="text-2xs text-black">R</Text>
            </View>
            <View class="absolute left-[333] top-[86] w-[50] h-[20] rounded-[3] bg-[#E2E8F0] items-center justify-center">
              <Text class="text-2xs text-black">L</Text>
            </View>
            <View class="absolute left-[401] top-[86] w-[50] h-[20] rounded-[3] bg-[#E2E8F0] items-center justify-center">
              <Text class="text-2xs text-black">R</Text>
            </View>
            <For each={props.pins} by={pin => pin.id}>
              {pin => (
                <View
                  class={
                    pin().kind === 0
                      ? pin().output
                        ? "absolute w-[50] h-[50] rounded-[3] border-[1] bg-[#FEF3C7] border-[#FBBF24]"
                        : "absolute w-[50] h-[50] rounded-[3] border-[1] bg-[#DCFCE7] border-[#86EFAC]"
                      : pin().kind === 1
                        ? "absolute w-[50] h-[50] rounded-[3] border-[1] bg-[#FEE2E2] border-[#FCA5A5]"
                        : pin().kind === 2
                          ? "absolute w-[50] h-[50] rounded-[3] border-[1] bg-[#4B5563] border-[#000000]"
                          : "absolute w-[50] h-[50] rounded-[3] border-[1] bg-[#F3F4F6] border-[#CBD5E1]"
                  }
                  style={{ insetL: pin().left, insetT: pin().top }}
                  focusable
                  onPress={() => props.onPin(pin().id)}
                >
                  <Text class={pin().kind === 2 ? "absolute left-[6] top-[4] text-2xs text-white" : pin().kind === 3 ? "absolute left-[6] top-[4] text-2xs text-[#64748B]" : "absolute left-[6] top-[4] text-2xs text-[#111827]"}>{`P${pin().id}`}</Text>
                  <View class={pin().twoLines ? "absolute left-[-4] top-[17] w-[58] h-[28] items-center justify-center" : "absolute left-[-4] top-[22] w-[58] h-[28] items-center justify-center"}>
                    <Text class={pin().kind === 2 ? "text-xs text-center text-white" : pin().kind === 3 ? "text-xs text-center text-[#64748B]" : "text-xs text-center text-[#111827]"}>{pin().label}</Text>
                  </View>
                  <Show when={pin().kind === 0}>
                    <View
                      class={
                        pin().dot === 2
                          ? "absolute left-[32] top-[6] w-[10] h-[10] rounded-full border-[1] bg-[#DC2626] border-[#DC2626]"
                          : pin().dot === 1
                            ? "absolute left-[32] top-[6] w-[10] h-[10] rounded-full border-[1] bg-[#2563EB] border-[#2563EB]"
                            : "absolute left-[32] top-[6] w-[10] h-[10] rounded-full border-[1] bg-[#CBD5E1] border-[#94A3B8]"
                      }
                    />
                  </Show>
                </View>
              )}
            </For>
          </Show>
        </View>
      </View>
    </View>
  );
}
