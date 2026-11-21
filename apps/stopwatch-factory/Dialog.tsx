import { Text, View } from "@pocketjs/framework/solid/components";

// The factory confirmation dialog: 404 × 202, #2B2B2B with a 3 px #6A6A6A
// border, a question and two 148 × 63 pill buttons.
export default function Dialog(props: {
  question: string;
  confirm: string;
  danger: boolean;
  onConfirm: () => void;
  onCancel: () => void;
}) {
  return (
    <View class="absolute left-[31] top-[132] w-[404] h-[202] rounded-[58] bg-[#2B2B2B] border-[#6A6A6A] border-[3]">
      <Text class="absolute left-[49] top-[40] w-[340] h-[32] text-2xl font-bold text-white">{props.question}</Text>
      <View
        class={props.danger ? "absolute left-[36] top-[106] w-[148] h-[63] rounded-[32] bg-[#EA5858] items-center justify-center" : "absolute left-[36] top-[106] w-[148] h-[63] rounded-[32] bg-[#4AD78C] items-center justify-center"}
        focusable
        onPress={() => props.onConfirm()}
      >
        <Text class={props.danger ? "text-2xl text-white" : "text-2xl text-[#0F5831]"}>{props.confirm}</Text>
      </View>
      <View class="absolute left-[220] top-[106] w-[148] h-[63] rounded-[32] bg-[#515151] items-center justify-center" focusable onPress={() => props.onCancel()}>
        <Text class="text-2xl text-white">Cancel</Text>
      </View>
    </View>
  );
}
