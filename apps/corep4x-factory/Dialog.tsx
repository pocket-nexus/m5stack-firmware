import { For, Text, View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";
import type { PlacedButton } from "./dialog";

/** The factory dialog (corep4_ui_open_function_dialog on lv_msgbox): a
 *  460 px box with a 2 px 0x475569 border over a 50 % 0x111827 backdrop.
 *  Header 43 px (LVGL default theme grey 0xE0E0E0, padding 5), text below
 *  it, footer 43 px with the visible buttons placed by the model. */
export default function Dialog(props: {
  title: string;
  text: string;
  top: i32;
  height: i32;
  buttons: PlacedButton[];
  onButton: (index: i32) => void;
}) {
  return (
    <View class="absolute left-0 top-0 w-[480] h-[480] bg-[#11182780]" focusable onPress={() => {}}>
      <View
        class="absolute left-[10] w-[460] rounded-[8] bg-white border-[2] border-[#475569] overflow-hidden"
        style={{ insetT: props.top, height: props.height }}
      >
        <View class="absolute left-0 top-0 w-[456] h-[43] bg-[#E0E0E0]">
          <Text class="absolute left-[5] top-[12] w-[446] text-base text-[#111827]">{props.title}</Text>
        </View>
        <Text class="absolute left-[5] top-[48] w-[420] text-base text-[#111827]">{props.text}</Text>
        <For each={props.buttons} by={b => b.id}>
          {b => (
            <View
              class="absolute w-[116] h-[40] rounded-[6] border-[1] border-[#CBD5E1] items-center justify-center"
              style={{ insetL: b().x, insetT: props.height - 4 - 43 + 1, bgColor: b().bg }}
              focusable
              onPress={() => props.onButton(b().id)}
            >
              <Text class="text-base text-center" style={{ textColor: b().fg }}>
                {b().text}
              </Text>
            </View>
          )}
        </For>
      </View>
    </View>
  );
}
