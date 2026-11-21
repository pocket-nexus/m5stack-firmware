import { View } from "@pocketjs/framework/solid/components";
import type { Deg, StyleClass } from "@pocketjs/framework/solid/std";

/** lv_spinner: a grey track and an indicator arc the model turns. */
export default function Spinner(props: { track: StyleClass; arc: StyleClass; angle: Deg }) {
  return (
    <View class="absolute left-0 top-0 w-full h-full">
      <View class={props.track} />
      <View class={props.arc} style={{ arcStart: props.angle }} />
    </View>
  );
}
