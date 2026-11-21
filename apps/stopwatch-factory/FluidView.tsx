import { For, View } from "@pocketjs/framework/solid/components";
import type { Dot } from "./app";

// FluidBox: particles drawn as 22 px discs in the factory speed colours.
export default function Fluid(props: { dots: Dot[] }) {
  return (
    <View class="absolute left-0 top-0 w-[466] h-[466] bg-black">
      <For each={props.dots} by={dot => dot.id}>
        {dot => (
          <View
            class={dot().level === 0 ? "absolute left-0 top-0 w-[22] h-[22] rounded-full bg-[#0A2DA5]" : dot().level === 1 ? "absolute left-0 top-0 w-[22] h-[22] rounded-full bg-[#287DEB]" : dot().level === 2 ? "absolute left-0 top-0 w-[22] h-[22] rounded-full bg-[#96CDFA]" : "absolute left-0 top-0 w-[22] h-[22] rounded-full bg-white"}
            style={{ translateX: dot().x, translateY: dot().y }}
          />
        )}
      </For>
    </View>
  );
}
