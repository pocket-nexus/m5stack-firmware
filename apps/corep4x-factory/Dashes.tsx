import { For, Image, View } from "@pocketjs/framework/solid/components";
import type { Edge } from "./frame";

/** Dashed edges (frame.ts): each a view clipped to the edge that holds the
 *  dash strip, shifted onto the edge's dashes. */
export default function Dashes(props: { edges: Edge[] }) {
  return (
    <For each={props.edges} by={edge => edge.id}>
      {edge => (
        <View class="absolute overflow-hidden" style={{ insetL: edge().x, insetT: edge().y, width: edge().w, height: edge().h }}>
          <View class={edge().vertical ? "absolute w-[2] h-[512]" : "absolute w-[512] h-[2]"} style={{ insetL: edge().sx, insetT: edge().sy }}>
            <Image
              class={edge().vertical ? "absolute left-0 top-0 w-[2] h-[512]" : "absolute left-0 top-0 w-[512] h-[2]"}
              src={edge().vertical ? "assets/dash-v.png" : "assets/dash-h.png"}
            />
          </View>
        </View>
      )}
    </For>
  );
}
