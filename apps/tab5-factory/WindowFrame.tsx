import type { JSX } from "solid-js";
import { View } from "@pocketjs/framework/solid/components";
import type { f32, Px, StyleClass } from "@pocketjs/framework/solid/std";

/** ui::Window: a rounded container whose geometry and opacity the model
 *  animates. The content keeps the opened size, centered in the window and
 *  clipped by it, as LVGL centers and clips a window's children. */
export default function WindowFrame(props: {
  tone: StyleClass;
  left: Px;
  top: Px;
  width: Px;
  height: Px;
  opacity: f32;
  contentWidth: Px;
  contentHeight: Px;
  children?: JSX.Element;
}) {
  return (
    <View
      class={props.tone}
      style={{ insetL: props.left, insetT: props.top, width: props.width, height: props.height, opacity: props.opacity }}
    >
      <View class="relative shrink-0" style={{ width: props.contentWidth, height: props.contentHeight }}>
        {props.children}
      </View>
    </View>
  );
}
