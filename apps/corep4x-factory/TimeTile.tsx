import { Text, View } from "@pocketjs/framework/solid/components";
import type { Px } from "@pocketjs/framework/solid/std";

/** A TIMER field tile (create_field_widget): name, value and -/+ (50x34).
 *  The value uses the 36 px slot for the factory's Montserrat 44. The model
 *  hit-tests the -/+ buttons from the contact (press-and-hold repeats). A
 *  disabled tile is drawn at 50 % with grey text. */
export default function TimeTile(props: { left: Px; top: Px; width: Px; name: string; value: string; enabled: boolean }) {
  return (
    <View class="absolute h-[128] rounded-[8] bg-white border-[1] border-[#CBD5E1]" style={{ insetL: props.left, insetT: props.top, width: props.width, opacity: props.enabled ? 1 : 0.5 }}>
      <View class="absolute left-0 top-[9] h-[20] justify-center" style={{ width: props.width }}>
        <Text class={props.enabled ? "text-base text-[#64748B]" : "text-base text-[#94A3B8]"}>{props.name}</Text>
      </View>
      <View class="absolute left-0 top-[27] h-[46] justify-center" style={{ width: props.width }}>
        <Text class={props.enabled ? "text-4xl text-[#111827]" : "text-4xl text-[#94A3B8]"}>{props.value}</Text>
      </View>
      <View class="absolute left-[7] top-[83] w-[50] h-[34] rounded-[6] bg-[#F8FAFC] border-[1] border-[#CBD5E1] items-center justify-center">
        <Text class={props.enabled ? "text-xl text-[#111827]" : "text-xl text-[#94A3B8]"}>-</Text>
      </View>
      <View class="absolute top-[83] w-[50] h-[34] rounded-[6] bg-[#F8FAFC] border-[1] border-[#CBD5E1] items-center justify-center" style={{ insetL: props.width - 59 }}>
        <Text class={props.enabled ? "text-xl text-[#111827]" : "text-xl text-[#94A3B8]"}>+</Text>
      </View>
    </View>
  );
}
