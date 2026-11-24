import { Show } from "solid-js";
import { For, Image, Text, View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";
import Button from "./Button.tsx";

/** The TOUCH canvas on the top layer: the board's 480x480 drawing (a
 *  512x512 texture re-bound under the same image name; the keyed row
 *  remounts the image for every new handle), the progress pill and, at
 *  25/25, Info, Clean and Exit (84x36). */
export default function TouchCanvas(props: {
  texture: i32[];
  pill: string;
  pillDark: boolean;
  complete: boolean;
  onInfo: () => void;
  onClean: () => void;
  onExit: () => void;
}) {
  return (
    <View class="absolute left-0 top-0 w-[480] h-[480] overflow-hidden bg-white" focusable onPress={() => {}}>
      <For each={props.texture} by={handle => handle}>
        {_handle => <Image class="absolute left-0 top-0 w-[512] h-[512]" src="assets/touch-canvas.png" />}
      </For>
      <View class={props.pillDark ? "absolute left-[10] top-[10] w-[72] h-[28] rounded-[8] bg-white border-[1] border-white items-center justify-center" : "absolute left-[10] top-[10] w-[72] h-[28] rounded-[8] bg-[#111827] border-[1] border-[#111827] items-center justify-center"}>
        <Text class={props.pillDark ? "text-base text-[#111827]" : "text-base text-white"}>{props.pill}</Text>
      </View>
      <Show when={props.complete}>
        <Button left={202} top={8} width={84} height={36} text="Info" bg="#2563EB" fg="#FFFFFF" border="#CBD5E1" radius={8} size={16} enabled={true} dim={0.5} onPress={() => props.onInfo()} />
        <Button left={294} top={8} width={84} height={36} text="Clean" bg="#2563EB" fg="#FFFFFF" border="#CBD5E1" radius={8} size={16} enabled={true} dim={0.5} onPress={() => props.onClean()} />
        <Button left={386} top={8} width={84} height={36} text="Exit" bg="#D32F2F" fg="#FFFFFF" border="#CBD5E1" radius={8} size={16} enabled={true} dim={0.5} onPress={() => props.onExit()} />
      </Show>
    </View>
  );
}
