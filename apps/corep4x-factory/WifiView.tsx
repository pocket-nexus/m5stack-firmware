import { Text, View } from "@pocketjs/framework/solid/components";
import Button from "./Button.tsx";

/** WiFi window content, in screen coordinates: the "Nearby Networks" card
 *  (Scan AP disabled, the list's message box) under the module overlay
 *  (0xF8FAFC at 232/255) with its text and the Retry button. */
export default function WifiView() {
  return (
    <View class="absolute left-[-10] top-[-52] w-[480] h-[480]">
      <View class="absolute left-[24] top-[66] w-[432] h-[398] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
      <Text class="absolute left-[39] top-[94] text-base text-[#111827]">Nearby Networks</Text>
      <Button left={317} top={81} width={124} height={44} text="Scan AP" bg="#2563EB" fg="#FFFFFF" border="#CBD5E1" radius={8} size={16} enabled={false} dim={0.5} onPress={() => {}} />
      <Text class="absolute left-[35] top-[135] text-sm text-[#64748B]">Ready when the ESP32-C6 module is initialized.</Text>
      <View class="absolute left-[35] top-[157] w-[410] h-[284] rounded-[8] bg-white border-[1] border-[#CBD5E1] items-center justify-center">
        <Text class="text-base text-center text-[#64748B]">Tap Scan AP to browse nearby WiFi networks.</Text>
      </View>
      <View class="absolute left-[24] top-[66] w-[432] h-[390] rounded-[8] bg-[#F8FAFCE8] border-[1] border-[#CBD5E1]" focusable onPress={() => {}} />
      <View class="absolute left-[50] top-[198] w-[380] h-[54] justify-center">
        <Text class="text-base text-center text-[#111827]">{"WiFi needs the module\nA172 Stamp-AddOn C6 For P4.\nThis firmware does not drive it."}</Text>
      </View>
      <Button left={145} top={258} width={190} height={52} text="Retry" bg="#2563EB" fg="#FFFFFF" border="#CBD5E1" radius={8} size={16} enabled={false} dim={0.5} onPress={() => {}} />
    </View>
  );
}
