import { Show } from "solid-js";
import { Text, View } from "@pocketjs/framework/solid/components";
import Dialog from "./Dialog.tsx";

// Factory app_badge device screens: the badge area (the uploaded JPEG is not
// drawn, see README), the edit dialog and the access-point instructions.
export default function Badge(props: { dialog: boolean; editing: boolean; message: string; onEdit: () => void; onCancel: () => void }) {
  return (
    <View class="absolute left-0 top-0 w-[466] h-[466] bg-black">
      <Show when={!props.editing}>
        <View class="absolute left-[73] top-[183] w-[320] h-[100] items-center justify-center">
          <Text class="text-2xl text-center text-[#D9D9D9]">Tap and hold to change image</Text>
        </View>
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
