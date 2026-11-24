import { Text, View } from "@pocketjs/framework/solid/components";
import type { Color, Px } from "@pocketjs/framework/solid/std";
import Button from "./Button.tsx";

/** OFF window content, in screen coordinates: one card (pad 14, rows 10)
 *  with the status lines, Boot Sound, Power Off, Restart and the countdown
 *  bar. */
export default function OffView(props: {
  status: string;
  version: string;
  action: string;
  actionColor: Color;
  soundOn: boolean;
  soundEnabled: boolean;
  powerText: string;
  powerBg: Color;
  powerEnabled: boolean;
  restartText: string;
  restartBg: Color;
  restartEnabled: boolean;
  bar: Px;
  barColor: Color;
  onSound: () => void;
  onPowerOff: () => void;
  onRestart: () => void;
}) {
  return (
    <View class="absolute left-[-10] top-[-52] w-[480] h-[480]">
      <View class="absolute left-[24] top-[66] w-[432] h-[340] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
      <Text class="absolute left-[39] top-[119] w-[110] text-base text-[#64748B]">Status</Text>
      <Text class="absolute left-[157] top-[102] w-[278] text-sm text-[#111827]">{props.status}</Text>
      <Text class="absolute left-[157] top-[120] w-[278] text-sm text-[#64748B]">{props.version}</Text>
      <Text class="absolute left-[157] top-[138] w-[278] text-sm" style={{ textColor: props.actionColor }}>
        {props.action}
      </Text>
      <Text class="absolute left-[39] top-[199] w-[110] text-base text-[#64748B]">Boot Sound</Text>
      <Button
        left={157}
        top={185}
        width={282}
        height={46}
        text={props.soundEnabled ? (props.soundOn ? "Sound ON" : "Sound OFF") : "Sound --"}
        bg={props.soundOn ? "#16A34A" : "#F3F4F6"}
        fg={props.soundEnabled ? (props.soundOn ? "#FFFFFF" : "#111827") : "#64748B"}
        border="#CBD5E1"
        radius={8}
        size={16}
        enabled={props.soundEnabled}
        dim={0.5}
        onPress={() => props.onSound()}
      />
      <Text class="absolute left-[39] top-[261] w-[110] text-base text-[#64748B]">Power Off</Text>
      <Button left={157} top={247} width={282} height={46} text={props.powerText} bg={props.powerBg} fg="#FFFFFF" border="#CBD5E1" radius={8} size={16} enabled={props.powerEnabled} dim={0.4} onPress={() => props.onPowerOff()} />
      <Text class="absolute left-[39] top-[329] w-[110] text-base text-[#64748B]">Restart</Text>
      <Button left={157} top={315} width={282} height={46} text={props.restartText} bg={props.restartBg} fg="#FFFFFF" border="#CBD5E1" radius={8} size={16} enabled={props.restartEnabled} dim={0.4} onPress={() => props.onRestart()} />
      <View class="absolute left-[39] top-[377] w-[402] h-[14] rounded-[4] bg-[#E5E7EB] overflow-hidden">
        <View class="absolute left-0 top-0 h-[14] rounded-[4]" style={{ width: props.bar, bgColor: props.barColor }} />
      </View>
    </View>
  );
}
