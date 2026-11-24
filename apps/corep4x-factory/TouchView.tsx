import { Text, View } from "@pocketjs/framework/solid/components";
import Button from "./Button.tsx";

/** TOUCH window info page, in screen coordinates: the controller status
 *  card and the touch indicator switch. */
export default function TouchView(props: { status: string; indicator: boolean; onIndicator: () => void }) {
  return (
    <View class="absolute left-[-10] top-[-52] w-[480] h-[480]">
      <View class="absolute left-[24] top-[66] w-[432] h-[120] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
      <Text class="absolute left-[35] top-[77] text-base text-[#111827]">Touch Panel</Text>
      <Text class="absolute left-[35] top-[101] w-[410] text-base text-[#64748B]">{props.status}</Text>
      <View class="absolute left-[24] top-[192] w-[432] h-[52] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
      <Text class="absolute left-[35] top-[208] w-[176] text-base text-[#64748B]">Touch indicator</Text>
      <Button
        left={219}
        top={202}
        width={146}
        height={32}
        text={props.indicator ? "Indicator ON" : "Indicator OFF"}
        bg={props.indicator ? "#16A34A" : "#F3F4F6"}
        fg={props.indicator ? "#FFFFFF" : "#111827"}
        border="#CBD5E1"
        radius={8}
        size={16}
        enabled={true}
        dim={0.5}
        onPress={() => props.onIndicator()}
      />
    </View>
  );
}
