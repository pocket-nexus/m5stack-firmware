import { Show } from "solid-js";
import { Text, View } from "@pocketjs/framework/solid/components";

// Key legend of a demo, centred in the bottom row (y 220..238). The "Hold
// FRONT for menu" pill of app.tsx uses the same row; the legend is hidden
// while the pill shows, so the two never overlap.
export default function FooterView(props: { text: string; hidden: boolean }) {
  return (
    <View class="absolute left-0 top-[222] w-[135] h-[14]">
      <Show when={!props.hidden}>
        <View class="absolute left-0 top-0 w-[135] h-[14] flex-col items-center">
          <Text class="text-2xs text-[#6B7585]">{props.text}</Text>
        </View>
      </Show>
    </View>
  );
}
