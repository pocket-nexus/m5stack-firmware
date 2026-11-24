import { Text, View } from "@pocketjs/framework/solid/components";
import type { Color, i32 } from "@pocketjs/framework/solid/std";
import Button from "./Button.tsx";

/** SLEEP window content, in screen coordinates: a card of five rows (label
 *  column 110 px at x 35, controls from x 153) and the full-width Deep Sleep
 *  button. */
export default function SleepView(props: {
  status: string;
  result: string;
  resultColor: Color;
  buttonSleep: boolean;
  rtcWake: boolean;
  rtcSeconds: i32;
  touchWake: boolean;
  imuWake: boolean;
  enabled: boolean;
  onButtonSleep: () => void;
  onRtc: () => void;
  onSeconds: (seconds: i32) => void;
  onTouch: () => void;
  onImu: () => void;
  onDeepSleep: () => void;
}) {
  return (
    <View class="absolute left-[-10] top-[-52] w-[480] h-[480]">
      <View class="absolute left-[24] top-[66] w-[432] h-[272] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
      <Text class="absolute left-[35] top-[86] w-[110] text-base text-[#64748B]">Status</Text>
      <Text class="absolute left-[153] top-[77] w-[290] text-base text-[#111827]">{props.status}</Text>
      <Text class="absolute left-[35] top-[137] w-[110] text-base text-[#64748B]">Button Sleep</Text>
      <Button left={153} top={127} width={120} height={38} text={props.buttonSleep ? "BTN ON" : "BTN OFF"} bg={props.buttonSleep ? "#16A34A" : "#F3F4F6"} fg={props.buttonSleep ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={16} enabled={props.enabled} dim={0.5} onPress={() => props.onButtonSleep()} />
      <Text class="absolute left-[35] top-[193] w-[110] text-base text-[#64748B]">RTC Wake</Text>
      <Button left={153} top={183} width={78} height={38} text={props.rtcWake ? "RTC ON" : "RTC OFF"} bg={props.rtcWake ? "#16A34A" : "#F3F4F6"} fg={props.rtcWake ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={16} enabled={props.enabled} dim={0.5} onPress={() => props.onRtc()} />
      <Button left={239} top={183} width={64} height={38} text="10 s" bg={props.rtcWake && props.rtcSeconds === 10 ? "#2563EB" : "#F3F4F6"} fg={props.rtcWake && props.rtcSeconds === 10 ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={16} enabled={props.enabled} dim={0.5} onPress={() => props.onSeconds(10)} />
      <Button left={311} top={183} width={64} height={38} text="60 s" bg={props.rtcWake && props.rtcSeconds === 60 ? "#2563EB" : "#F3F4F6"} fg={props.rtcWake && props.rtcSeconds === 60 ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={16} enabled={props.enabled} dim={0.5} onPress={() => props.onSeconds(60)} />
      <View class="absolute left-[383] top-[183] w-[62] h-[38] overflow-hidden">
        <Button left={0} top={0} width={64} height={38} text="300 s" bg={props.rtcWake && props.rtcSeconds === 300 ? "#2563EB" : "#F3F4F6"} fg={props.rtcWake && props.rtcSeconds === 300 ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={16} enabled={props.enabled} dim={0.5} onPress={() => props.onSeconds(300)} />
      </View>
      <Text class="absolute left-[35] top-[249] w-[110] text-base text-[#64748B]">Sensor Wake</Text>
      <Button left={153} top={239} width={120} height={38} text={props.touchWake ? "Touch ON" : "Touch OFF"} bg={props.touchWake ? "#16A34A" : "#F3F4F6"} fg={props.touchWake ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={16} enabled={props.enabled} dim={0.5} onPress={() => props.onTouch()} />
      <Button left={281} top={239} width={110} height={38} text={props.imuWake ? "IMU ON" : "IMU OFF"} bg={props.imuWake ? "#16A34A" : "#F3F4F6"} fg={props.imuWake ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={8} size={16} enabled={props.enabled} dim={0.5} onPress={() => props.onImu()} />
      <Text class="absolute left-[35] top-[300] w-[110] text-base text-[#64748B]">Result</Text>
      <View class="absolute left-[153] top-[291] w-[290] h-[36] overflow-hidden">
        <Text class="absolute left-0 top-0 w-[290] text-base" style={{ textColor: props.resultColor }}>
          {props.result}
        </Text>
      </View>
      <Button left={24} top={344} width={432} height={60} text="Deep Sleep" bg="#2563EB" fg="#FFFFFF" border="#CBD5E1" radius={8} size={16} enabled={props.enabled} dim={0.5} onPress={() => props.onDeepSleep()} />
    </View>
  );
}
