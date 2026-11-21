import { Show } from "solid-js";
import { View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";
import GlyphsClockBlue from "./GlyphsClockBlue.tsx";
import GlyphsClockWhite from "./GlyphsClockWhite.tsx";

// Factory ArcTopClock: five Maple Mono Medium 28 px characters (glyph
// images) along the top of the round panel. Each character keeps the factory
// rotation; the boxes sit where the factory pivot (233 px below the label)
// places them. ids holds the five codepoints of "HH:MM" (0 drawn as O).
export default function TopClock(props: { ids: i32[]; pens: i32[]; tint: boolean }) {
  return (
    <View class="absolute left-0 top-0 w-[466] h-[60]">
      <View class="absolute left-[188] top-[7] w-[20] h-[34]" style={{ rotate: -10 }}>
        <Show when={props.tint}>
          <GlyphsClockBlue ids={props.ids} base={0} xs={props.pens} />
        </Show>
        <Show when={!props.tint}>
          <GlyphsClockWhite ids={props.ids} base={0} xs={props.pens} />
        </Show>
      </View>
      <View class="absolute left-[209] top-[5] w-[20] h-[34]" style={{ rotate: -4.6 }}>
        <Show when={props.tint}>
          <GlyphsClockBlue ids={props.ids} base={1} xs={props.pens} />
        </Show>
        <Show when={!props.tint}>
          <GlyphsClockWhite ids={props.ids} base={1} xs={props.pens} />
        </Show>
      </View>
      <View class="absolute left-[224] top-[3] w-[20] h-[34]">
        <Show when={props.tint}>
          <GlyphsClockBlue ids={props.ids} base={2} xs={props.pens} />
        </Show>
        <Show when={!props.tint}>
          <GlyphsClockWhite ids={props.ids} base={2} xs={props.pens} />
        </Show>
      </View>
      <View class="absolute left-[239] top-[5] w-[20] h-[34]" style={{ rotate: 3.2 }}>
        <Show when={props.tint}>
          <GlyphsClockBlue ids={props.ids} base={3} xs={props.pens} />
        </Show>
        <Show when={!props.tint}>
          <GlyphsClockWhite ids={props.ids} base={3} xs={props.pens} />
        </Show>
      </View>
      <View class="absolute left-[259] top-[8] w-[20] h-[34]" style={{ rotate: 8.4 }}>
        <Show when={props.tint}>
          <GlyphsClockBlue ids={props.ids} base={4} xs={props.pens} />
        </Show>
        <Show when={!props.tint}>
          <GlyphsClockWhite ids={props.ids} base={4} xs={props.pens} />
        </Show>
      </View>
    </View>
  );
}
