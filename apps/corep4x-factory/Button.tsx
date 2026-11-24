import { Text, View } from "@pocketjs/framework/solid/components";
import type { Color, f32, i32, Px } from "@pocketjs/framework/solid/std";

/** An lv_button as the factory windows style it (add_button): a filled
 *  rectangle with a 1 px border and a centred label. A disabled button is
 *  drawn at `dim` opacity and ignores presses. `size` is the label's px
 *  (10, 12, 14, 16, 20 or 24); `radius` 3, 6, 8 or 12. */
export default function Button(props: {
  left: Px;
  top: Px;
  width: Px;
  height: Px;
  text: string;
  bg: Color;
  fg: Color;
  border: Color;
  radius: i32;
  size: i32;
  enabled: boolean;
  dim: f32;
  onPress: () => void;
}) {
  return (
    <View
      class={
        props.radius === 3
          ? "absolute rounded-[3] border-[1] items-center justify-center"
          : props.radius === 6
            ? "absolute rounded-[6] border-[1] items-center justify-center"
            : props.radius === 12
              ? "absolute rounded-[12] border-[1] items-center justify-center"
              : props.radius === 30
                ? "absolute rounded-[30] border-[1] items-center justify-center"
                : "absolute rounded-[8] border-[1] items-center justify-center"
      }
      style={{
        insetL: props.left,
        insetT: props.top,
        width: props.width,
        height: props.height,
        bgColor: props.bg,
        borderColor: props.border,
        opacity: props.enabled ? 1 : props.dim,
      }}
      focusable
      onPress={() => {
        if (props.enabled) {
          props.onPress();
        }
      }}
    >
      <Text
        class={
          props.size === 10
            ? "text-2xs text-center"
            : props.size === 12
              ? "text-xs text-center"
              : props.size === 14
                ? "text-sm text-center"
                : props.size === 20
                  ? "text-xl text-center"
                  : props.size === 24
                    ? "text-2xl text-center"
                    : "text-base text-center"
        }
        style={{ textColor: props.fg }}
      >
        {props.text}
      </Text>
    </View>
  );
}
