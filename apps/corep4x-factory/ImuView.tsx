import { Show } from "solid-js";
import { For, Image, Text, View } from "@pocketjs/framework/solid/components";
import type { i32, Px } from "@pocketjs/framework/solid/std";

/** IMU window content: the board's rendering fills the 460x418 content
 *  panel (a 512x512 texture re-bound under the same name; the keyed row
 *  remounts the image for every new handle), the loading/calibration
 *  overlay and the two 28x28 finger markers. */
export default function ImuView(props: {
  hidden: boolean;
  texture: i32[];
  overlay: boolean;
  overlayText: string;
  overlayError: boolean;
  markers: i32;
  marker0X: Px;
  marker0Y: Px;
  marker1X: Px;
  marker1Y: Px;
}) {
  return (
    <Show when={!props.hidden}>
      <View class="absolute left-0 top-0 w-[460] h-[418] overflow-hidden bg-[#F8FAFC]">
        <For each={props.texture} by={handle => handle}>
          {_handle => <Image class="absolute left-0 top-0 w-[512] h-[512]" src="assets/imu-canvas.png" />}
        </For>
        <Show when={props.markers > 0}>
          <View class="absolute w-[28] h-[28] rounded-full border-[2] border-[#2563EBCC]" style={{ insetL: props.marker0X, insetT: props.marker0Y }} />
        </Show>
        <Show when={props.markers > 1}>
          <View class="absolute w-[28] h-[28] rounded-full border-[2] border-[#0891B2CC]" style={{ insetL: props.marker1X, insetT: props.marker1Y }} />
        </Show>
        <Show when={props.overlay}>
          <View class="absolute left-0 top-0 w-[460] h-[418] bg-[#F8FAFC] items-center justify-center">
            <Text class={props.overlayError ? "w-[390] text-xl text-center text-[#DC2626]" : "w-[390] text-xl text-center text-[#64748B]"}>{props.overlayText}</Text>
          </View>
        </Show>
      </View>
    </Show>
  );
}
