import { View } from "@pocketjs/framework/solid/components";
import type { StyleClass } from "@pocketjs/framework/solid/std";

/** lv_spinner: a grey track and an indicator arc turning once a second. */
export default function Spinner(props: { track: StyleClass; arc: StyleClass }) {
  return (
    <View class="absolute left-0 top-0 w-full h-full">
      <View class={props.track} />
      <View class="absolute left-0 top-0 w-full h-full animate-spin">
        <View class={props.arc} />
      </View>
    </View>
  );
}
