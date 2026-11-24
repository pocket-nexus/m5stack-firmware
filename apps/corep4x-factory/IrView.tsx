import { Text, View } from "@pocketjs/framework/solid/components";
import type { Color, i32 } from "@pocketjs/framework/solid/std";
import Button from "./Button.tsx";

/** IR TX/RX window content, in screen coordinates: the TX panel (status,
 *  result, four 99x132 byte tiles, Auto+1, Repeat, Send NEC) and the RX
 *  panel (frame count, Clear, the last decoded frame). */
export default function IrView(props: {
  status: string;
  statusColor: Color;
  result: string;
  resultColor: Color;
  bytes: string[];
  pressed: i32;
  autoInc: boolean;
  repeat: boolean;
  sendEnabled: boolean;
  autoEnabled: boolean;
  frames: string;
  shown: boolean;
  repeatText: string;
  address: string;
  command: string;
  raw: string;
  onAuto: () => void;
  onRepeat: () => void;
  onSend: () => void;
  onClear: () => void;
}) {
  return (
    <View class="absolute left-[-10] top-[-52] w-[480] h-[480]">
      <View class="absolute left-[24] top-[66] w-[432] h-[260] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
      <Text class="absolute left-[35] top-[77] text-base text-[#111827]">TX - NEC Encode</Text>
      <View class="absolute left-[213] top-[77] w-[232] h-[18] flex-col items-end">
        <Text class="text-sm" style={{ textColor: props.statusColor }}>
          {props.status}
        </Text>
      </View>
      <Text class="absolute left-[35] top-[103] w-[410] text-sm" style={{ textColor: props.resultColor }}>
        {props.result}
      </Text>
      <View class={props.pressed === 0 ? "absolute left-[35] top-[129] w-[99] h-[132] rounded-[6] bg-white border-[2] border-[#2563EB]" : "absolute left-[35] top-[129] w-[99] h-[132] rounded-[6] bg-white border-[1] border-[#CBD5E1]"} />
      <View class={props.pressed === 1 ? "absolute left-[140] top-[129] w-[99] h-[132] rounded-[6] bg-white border-[2] border-[#2563EB]" : "absolute left-[140] top-[129] w-[99] h-[132] rounded-[6] bg-white border-[1] border-[#CBD5E1]"} />
      <View class={props.pressed === 2 ? "absolute left-[245] top-[129] w-[99] h-[132] rounded-[6] bg-white border-[2] border-[#2563EB]" : "absolute left-[245] top-[129] w-[99] h-[132] rounded-[6] bg-white border-[1] border-[#CBD5E1]"} />
      <View class={props.pressed === 3 ? "absolute left-[350] top-[129] w-[99] h-[132] rounded-[6] bg-white border-[2] border-[#2563EB]" : "absolute left-[350] top-[129] w-[99] h-[132] rounded-[6] bg-white border-[1] border-[#CBD5E1]"} />
      <View class="absolute left-[42] top-[139] w-[87] h-[18] justify-center"><Text class="text-sm text-[#64748B]">ADDR H</Text></View>
      <View class="absolute left-[147] top-[139] w-[87] h-[18] justify-center"><Text class="text-sm text-[#64748B]">ADDR L</Text></View>
      <View class="absolute left-[252] top-[139] w-[87] h-[18] justify-center"><Text class="text-sm text-[#64748B]">CMD H</Text></View>
      <View class="absolute left-[357] top-[139] w-[87] h-[18] justify-center"><Text class="text-sm text-[#64748B]">CMD L</Text></View>
      <View class="absolute left-[42] top-[161] w-[87] h-[18] justify-center"><Text class="text-base text-[#2563EB]">+</Text></View>
      <View class="absolute left-[147] top-[161] w-[87] h-[18] justify-center"><Text class="text-base text-[#2563EB]">+</Text></View>
      <View class="absolute left-[252] top-[161] w-[87] h-[18] justify-center"><Text class="text-base text-[#2563EB]">+</Text></View>
      <View class="absolute left-[357] top-[161] w-[87] h-[18] justify-center"><Text class="text-base text-[#2563EB]">+</Text></View>
      <View class="absolute left-[42] top-[186] w-[87] h-[32] justify-center"><Text class={props.pressed === 0 ? "text-2xl text-[#2563EB]" : "text-2xl text-[#111827]"}>{props.bytes[0] ?? ""}</Text></View>
      <View class="absolute left-[147] top-[186] w-[87] h-[32] justify-center"><Text class={props.pressed === 1 ? "text-2xl text-[#2563EB]" : "text-2xl text-[#111827]"}>{props.bytes[1] ?? ""}</Text></View>
      <View class="absolute left-[252] top-[186] w-[87] h-[32] justify-center"><Text class={props.pressed === 2 ? "text-2xl text-[#2563EB]" : "text-2xl text-[#111827]"}>{props.bytes[2] ?? ""}</Text></View>
      <View class="absolute left-[357] top-[186] w-[87] h-[32] justify-center"><Text class={props.pressed === 3 ? "text-2xl text-[#2563EB]" : "text-2xl text-[#111827]"}>{props.bytes[3] ?? ""}</Text></View>
      <View class="absolute left-[42] top-[224] w-[87] h-[18] justify-center"><Text class="text-base text-[#64748B]">-</Text></View>
      <View class="absolute left-[147] top-[224] w-[87] h-[18] justify-center"><Text class="text-base text-[#64748B]">-</Text></View>
      <View class="absolute left-[252] top-[224] w-[87] h-[18] justify-center"><Text class="text-base text-[#64748B]">-</Text></View>
      <View class="absolute left-[357] top-[224] w-[87] h-[18] justify-center"><Text class="text-base text-[#64748B]">-</Text></View>
      <Button left={35} top={273} width={100} height={40} text={props.autoInc ? "Auto+1 ON" : "Auto+1"} bg={props.autoInc ? "#DBEAFE" : "#F3F4F6"} fg={props.autoInc ? "#2563EB" : "#111827"} border="#CBD5E1" radius={6} size={16} enabled={props.autoEnabled} dim={0.5} onPress={() => props.onAuto()} />
      <Button left={145} top={273} width={100} height={40} text={props.repeat ? "Repeat x2" : "Repeat x1"} bg={props.repeat ? "#FEF3C7" : "#F3F4F6"} fg={props.repeat ? "#D97706" : "#111827"} border="#CBD5E1" radius={6} size={16} enabled={true} dim={0.5} onPress={() => props.onRepeat()} />
      <Button left={267} top={273} width={178} height={40} text="Send NEC" bg="#2563EB" fg="#FFFFFF" border="#CBD5E1" radius={6} size={16} enabled={props.sendEnabled} dim={0.5} onPress={() => props.onSend()} />
      <View class="absolute left-[24] top-[334] w-[432] h-[122] rounded-[8] bg-[#F0FDF4] border-[1] border-[#CBD5E1]" />
      <Text class="absolute left-[35] top-[343] text-base text-[#111827]">RX - NEC Decode</Text>
      <View class="absolute left-[235] top-[345] w-[100] h-[18] flex-col items-end">
        <Text class="text-sm text-[#16A34A]">{props.frames}</Text>
      </View>
      <Button left={349} top={343} width={88} height={28} text="Clear" bg="#F3F4F6" fg="#111827" border="#CBD5E1" radius={6} size={14} enabled={true} dim={0.5} onPress={() => props.onClear()} />
      <Text class={props.shown ? "absolute left-[35] top-[377] text-sm text-[#16A34A]" : "absolute left-[35] top-[377] text-sm text-[#64748B]"}>{props.shown ? "Protocol: NEC" : "Protocol: --"}</Text>
      <Text class={props.shown && props.repeatText === "YES" ? "absolute left-[275] top-[377] text-sm text-[#D97706]" : "absolute left-[275] top-[377] text-sm text-[#64748B]"}>{props.shown ? `Repeat: ${props.repeatText}` : "Repeat: --"}</Text>
      <Text class={props.shown ? "absolute left-[35] top-[399] text-sm text-[#111827]" : "absolute left-[35] top-[399] text-sm text-[#64748B]"}>{props.shown ? `Address: ${props.address}` : "Address: --"}</Text>
      <Text class={props.shown ? "absolute left-[245] top-[399] text-sm text-[#111827]" : "absolute left-[245] top-[399] text-sm text-[#64748B]"}>{props.shown ? `Command: ${props.command}` : "Command: --"}</Text>
      <Text class="absolute left-[35] top-[423] text-sm text-[#64748B]">{props.shown ? `Raw frame: ${props.raw}` : "Raw frame: --"}</Text>
    </View>
  );
}
