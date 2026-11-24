import { Show } from "solid-js";
import { Text, View } from "@pocketjs/framework/solid/components";

/** USB window hub page, in screen coordinates: the title row, the three
 *  device counters (all 0 without a USB host) and the result line. */
export default function UsbView(props: { result: string }) {
  return (
    <View class="absolute left-[-10] top-[-52] w-[480] h-[480]">
      <View class="absolute left-[24] top-[66] w-[432] h-[398] rounded-[8] bg-[#F8FAFC] border-[1] border-[#CBD5E1]" />
      <Text class="absolute left-[35] top-[89] text-base text-[#111827]">USB 2.0 Hub</Text>
      <Text class="absolute left-[215] top-[89] text-base text-[#64748B]">Check to refresh</Text>
      <View class="absolute left-[35] top-[127] w-[132] h-[42] rounded-[8] bg-[#E5E7EB] items-center justify-center">
        <Text class="text-base text-[#111827]">CAM 0</Text>
      </View>
      <View class="absolute left-[175] top-[127] w-[132] h-[42] rounded-[8] bg-[#E5E7EB] items-center justify-center">
        <Text class="text-base text-[#111827]">HID 0</Text>
      </View>
      <View class="absolute left-[315] top-[127] w-[130] h-[42] overflow-hidden">
        <View class="absolute left-0 top-0 w-[132] h-[42] rounded-[8] bg-[#E5E7EB] items-center justify-center">
          <Text class="text-base text-[#111827]">OTHER 0</Text>
        </View>
      </View>
      <Show when={props.result !== ""}>
        <Text class="absolute left-[35] top-[421] w-[410] text-base text-[#D97706]">{props.result}</Text>
      </Show>
    </View>
  );
}
