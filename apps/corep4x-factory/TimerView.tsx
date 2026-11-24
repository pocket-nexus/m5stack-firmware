import { Text, View } from "@pocketjs/framework/solid/components";
import Button from "./Button.tsx";
import TimeTile from "./TimeTile.tsx";

/** TIMER window content, in screen coordinates: the DATE panel (teal) and
 *  the TIME panel (blue), 432x190 each, 8 px apart. */
export default function TimerView(props: {
  year: string;
  month: string;
  day: string;
  hour: string;
  minute: string;
  second: string;
  enabled: boolean;
  onSaveDate: () => void;
  onSaveTime: () => void;
}) {
  return (
    <View class="absolute left-[-10] top-[-52] w-[480] h-[480]">
      <View class="absolute left-[24] top-[66] w-[432] h-[190] rounded-[8] bg-[#F0FDFA] border-[1] border-[#0F766E66]" />
      <Text class="absolute left-[38] top-[79] text-xl text-[#0F766E]">DATE</Text>
      <Button left={352} top={74} width={92} height={34} text="Save" bg="#0F766E" fg="#FFFFFF" border="#0F766E" radius={6} size={16} enabled={props.enabled} dim={0.5} onPress={() => props.onSaveDate()} />
      <TimeTile left={34} top={114} width={144} name="Year" value={props.year} enabled={props.enabled} />
      <TimeTile left={184} top={114} width={126} name="Month" value={props.month} enabled={props.enabled} />
      <TimeTile left={316} top={114} width={126} name="Day" value={props.day} enabled={props.enabled} />
      <View class="absolute left-[24] top-[264] w-[432] h-[190] rounded-[8] bg-[#EFF6FF] border-[1] border-[#2563EB66]" />
      <Text class="absolute left-[38] top-[277] text-xl text-[#2563EB]">TIME</Text>
      <Button left={352} top={272} width={92} height={34} text="Save" bg="#2563EB" fg="#FFFFFF" border="#2563EB" radius={6} size={16} enabled={props.enabled} dim={0.5} onPress={() => props.onSaveTime()} />
      <TimeTile left={34} top={312} width={132} name="Hour" value={props.hour} enabled={props.enabled} />
      <TimeTile left={172} top={312} width={132} name="Minute" value={props.minute} enabled={props.enabled} />
      <TimeTile left={310} top={312} width={132} name="Second" value={props.second} enabled={props.enabled} />
    </View>
  );
}
