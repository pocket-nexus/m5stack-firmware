import { For, View } from "@pocketjs/framework/solid/components";
import type { ChartColumn } from "./app";

/** The LVGL line chart of the audio windows (243x120, #383838, #40FFA1
 *  series), drawn as 60 peak columns. */
export default function Chart(props: { columns: ChartColumn[] }) {
  return (
    <View class="w-[121.5] h-[60] rounded-[6] bg-[#383838] flex-row items-center justify-center overflow-hidden">
      <For each={props.columns} by={(column) => column.id}>
        {(column) => <View class="w-[2] bg-[#40FFA1]" style={{ height: column().height }} />}
      </For>
    </View>
  );
}
