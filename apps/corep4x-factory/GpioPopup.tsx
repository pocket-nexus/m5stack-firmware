import { Text, View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";
import Button from "./Button.tsx";

/** The M5BUS GPIO popup (open_m5bus_gpio_config_popup): a large dialog
 *  (460x328) titled "P<n>  GPIO<g>" with a status line, four rows of option
 *  buttons (92x32; the selected one tinted) and Apply / Close. */
export default function GpioPopup(props: {
  title: string;
  status: string;
  output: boolean;
  level: boolean;
  openDrain: boolean;
  pull: i32;
  onMode: (output: boolean) => void;
  onLevel: (high: boolean) => void;
  onDrive: (openDrain: boolean) => void;
  onPull: (pull: i32) => void;
  onApply: () => void;
  onClose: () => void;
}) {
  return (
    <View class="absolute left-0 top-0 w-[480] h-[480] bg-[#11182780]" focusable onPress={() => {}}>
      <View class="absolute left-[10] top-[76] w-[460] h-[328] rounded-[8] bg-white border-[2] border-[#475569] overflow-hidden">
        <View class="absolute left-0 top-0 w-[456] h-[43] bg-[#E0E0E0]">
          <Text class="absolute left-[5] top-[12] text-base text-[#111827]">{props.title}</Text>
        </View>
        <Text class="absolute left-[5] top-[49] w-[446] text-base text-[#64748B]">{props.status}</Text>
        <Text class="absolute left-[5] top-[82] w-[74] text-base text-[#64748B]">Mode</Text>
        <Button left={87} top={75} width={92} height={32} text="Input" bg={props.output ? "#F3F4F6" : "#DCFCE7"} fg="#111827" border="#CBD5E1" radius={8} size={16} enabled={true} dim={0.5} onPress={() => props.onMode(false)} />
        <Button left={187} top={75} width={92} height={32} text="Output" bg={props.output ? "#FEF3C7" : "#F3F4F6"} fg="#111827" border="#CBD5E1" radius={8} size={16} enabled={true} dim={0.5} onPress={() => props.onMode(true)} />
        <Text class="absolute left-[5] top-[122] w-[74] text-base text-[#64748B]">Level</Text>
        <Button left={87} top={115} width={92} height={32} text="Low" bg={props.output && !props.level ? "#CBD5E1" : "#F3F4F6"} fg="#111827" border="#CBD5E1" radius={8} size={16} enabled={props.output} dim={0.5} onPress={() => props.onLevel(false)} />
        <Button left={187} top={115} width={92} height={32} text="High" bg={props.output && props.level ? "#DC2626" : "#F3F4F6"} fg="#111827" border="#CBD5E1" radius={8} size={16} enabled={props.output} dim={0.5} onPress={() => props.onLevel(true)} />
        <Text class="absolute left-[5] top-[162] w-[74] text-base text-[#64748B]">Drive</Text>
        <Button left={87} top={155} width={92} height={32} text="OpenDrain" bg={props.openDrain ? "#DCFCE7" : "#F3F4F6"} fg="#111827" border="#CBD5E1" radius={8} size={16} enabled={true} dim={0.5} onPress={() => props.onDrive(true)} />
        <Button left={187} top={155} width={92} height={32} text="PushPull" bg={props.openDrain ? "#F3F4F6" : "#FEF3C7"} fg="#111827" border="#CBD5E1" radius={8} size={16} enabled={true} dim={0.5} onPress={() => props.onDrive(false)} />
        <Text class="absolute left-[5] top-[202] w-[74] text-base text-[#64748B]">Pull</Text>
        <Button left={87} top={195} width={92} height={32} text="None" bg={props.pull === 0 ? "#DCFCE7" : "#F3F4F6"} fg="#111827" border="#CBD5E1" radius={8} size={16} enabled={true} dim={0.5} onPress={() => props.onPull(0)} />
        <Button left={187} top={195} width={92} height={32} text="Up" bg={props.pull === 1 ? "#DCFCE7" : "#F3F4F6"} fg="#111827" border="#CBD5E1" radius={8} size={16} enabled={true} dim={0.5} onPress={() => props.onPull(1)} />
        <Button left={287} top={195} width={92} height={32} text="Down" bg={props.pull === 2 ? "#DCFCE7" : "#F3F4F6"} fg="#111827" border="#CBD5E1" radius={8} size={16} enabled={true} dim={0.5} onPress={() => props.onPull(2)} />
        <Button left={73} top={282} width={116} height={40} text="Apply" bg="#F3F4F6" fg="#111827" border="#CBD5E1" radius={6} size={16} enabled={true} dim={0.5} onPress={() => props.onApply()} />
        <Button left={267} top={282} width={116} height={40} text="Close" bg="#F3F4F6" fg="#111827" border="#CBD5E1" radius={6} size={16} enabled={true} dim={0.5} onPress={() => props.onClose()} />
      </View>
    </View>
  );
}
