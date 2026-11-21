import { Text, View } from "@pocketjs/framework/solid/components";

// The factory's green confirm button: 374 × 130, radius 77, centred 175 px
// below the screen centre (its lower edge runs past the round panel).
export default function OkButton(props: { label: string; onPress: () => void }) {
  return (
    <View class="absolute left-[46] top-[343] w-[374] h-[130] rounded-[77] bg-[#4AD78C] items-center justify-center" focusable onPress={() => props.onPress()}>
      <Text class="text-2xl text-[#0F5831]">{props.label}</Text>
    </View>
  );
}
