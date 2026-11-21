import { Show } from "solid-js";
import { Image, Text, View } from "@pocketjs/framework/solid/components";
import { idiv, type i32 } from "@pocketjs/framework/solid/std";
import Dialog from "./Dialog.tsx";

// Factory app_badge device screens: the active badge picture (a 512 × 512
// power-of-two texture the board decodes and registers, centred on the
// screen) or the
// hint, the edit dialog and the access-point instructions.
export default function Badge(props: {
  dialog: boolean;
  editing: boolean;
  image: boolean;
  canvas: i32;
  message: string;
  onEdit: () => void;
  onCancel: () => void;
}) {
  return (
    <View class="absolute left-0 top-0 w-[466] h-[466] bg-black">
      <Show when={!props.editing}>
        <Show when={props.image}>
          <View
            class="absolute left-[233] top-[233] w-[512] h-[512]"
            style={{ translateX: -idiv(props.canvas, 2), translateY: -idiv(props.canvas, 2), width: props.canvas, height: props.canvas }}
          >
            <Image class="w-full h-full" src="images/badge_slot.png" />
          </View>
        </Show>
        <Show when={!props.image}>
          <View class="absolute left-[73] top-[183] w-[320] h-[100] items-center justify-center">
            <Text class="text-2xl text-center text-[#D9D9D9]">Tap and hold to change image</Text>
          </View>
        </Show>
        <Show when={props.dialog}>
          <Dialog question="Enter badge edit?" confirm="Edit" danger={false} onConfirm={() => props.onEdit()} onCancel={() => props.onCancel()} />
        </Show>
      </Show>
      <Show when={props.editing}>
        <View class="absolute left-0 top-0 w-[466] h-[466] bg-[#2B2B2B] items-center justify-center">
          <Text class="w-[320] text-2xl text-center text-white">{props.message}</Text>
        </View>
      </Show>
    </View>
  );
}
