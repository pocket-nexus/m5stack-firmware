import { Show } from "solid-js";
import { For, Text, View } from "@pocketjs/framework/solid/components";
import FooterView from "./FooterView.tsx";
import HeaderView from "./HeaderView.tsx";
import type { Die } from "./types";

// Dice: each die is a 46 px cube in its own perspective root, under a fixed
// tray tilt (rotate-x 24, rotate-y 30: seen from above and the left), turned
// by the model's rx/ry and lifted above a shadow that shrinks with the
// height. Faces are coloured with the model's shades; opposite faces add up
// to seven. Each face is rotated so its local +z points out of the cube and
// its pips sit 2 px out along it: the 3D pipeline painter-sorts triangles,
// and pips coplanar with their face would sort behind it in part. The 3D
// pipeline draws no corner radius, so faces and pips are square.
export default function DiceView(props: { dice: Die[]; total: string; detail: string; count: string; rolling: boolean; hint: boolean }) {
  return (
    <View debugName="Dice" class="absolute left-0 top-0 w-[135] h-[240] bg-[#0B0E13] overflow-hidden">
      <HeaderView title="DICE" info={props.count} accent={1} />
      <View class="absolute left-[6] top-[150] w-[123] h-[2] rounded-[1] bg-[#1C232D]" />
      <For each={props.dice} by={die => die.id}>
        {die => (
          <View class="absolute left-[-40] top-[-40] w-[80] h-[80]" style={{ translateX: die().x, translateY: die().y }}>
            <View class="absolute left-[14] top-[60] w-[52] h-[12] rounded-full bg-[#000000] opacity-70" style={{ scale: die().shadow }} />
            <View class="absolute left-0 top-0 w-[80] h-[80] perspective-[320]" style={{ translateY: -die().lift }}>
              <View class="absolute left-[17] top-[17] w-[46] h-[46] rotate-x-[24] rotate-y-[30]">
                <View class="absolute left-0 top-0 w-[46] h-[46]" style={{ rotateX: die().rx, rotateY: die().ry }}>
                  <View class="absolute left-0 top-0 w-[46] h-[46] rounded-[7] translate-z-[23]" style={{ bgColor: die().shades[0] ?? "#FAFAFA" }}>
                    <View class="absolute left-[17] top-[17] w-[12] h-[12] rounded-full bg-[#E11D48] translate-z-[2]" />
                  </View>
                  <View class="absolute left-0 top-0 w-[46] h-[46] rounded-[7] rotate-x-[-90] translate-y-[-23]" style={{ bgColor: die().shades[1] ?? "#FAFAFA" }}>
                    <View class="absolute left-[9] top-[9] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                    <View class="absolute left-[29] top-[29] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                  </View>
                  <View class="absolute left-0 top-0 w-[46] h-[46] rounded-[7] rotate-y-[90] translate-x-[23]" style={{ bgColor: die().shades[2] ?? "#FAFAFA" }}>
                    <View class="absolute left-[9] top-[9] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                    <View class="absolute left-[19] top-[19] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                    <View class="absolute left-[29] top-[29] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                  </View>
                  <View class="absolute left-0 top-0 w-[46] h-[46] rounded-[7] rotate-y-[-90] translate-x-[-23]" style={{ bgColor: die().shades[3] ?? "#FAFAFA" }}>
                    <View class="absolute left-[9] top-[9] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                    <View class="absolute left-[29] top-[9] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                    <View class="absolute left-[9] top-[29] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                    <View class="absolute left-[29] top-[29] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                  </View>
                  <View class="absolute left-0 top-0 w-[46] h-[46] rounded-[7] rotate-x-[90] translate-y-[23]" style={{ bgColor: die().shades[4] ?? "#FAFAFA" }}>
                    <View class="absolute left-[9] top-[9] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                    <View class="absolute left-[29] top-[9] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                    <View class="absolute left-[19] top-[19] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                    <View class="absolute left-[9] top-[29] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                    <View class="absolute left-[29] top-[29] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                  </View>
                  <View class="absolute left-0 top-0 w-[46] h-[46] rounded-[7] rotate-y-[180] translate-z-[-23]" style={{ bgColor: die().shades[5] ?? "#FAFAFA" }}>
                    <View class="absolute left-[9] top-[9] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                    <View class="absolute left-[29] top-[9] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                    <View class="absolute left-[9] top-[19] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                    <View class="absolute left-[29] top-[19] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                    <View class="absolute left-[9] top-[29] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                    <View class="absolute left-[29] top-[29] w-[8] h-[8] rounded-full bg-[#1F2430] translate-z-[2]" />
                  </View>
                </View>
              </View>
            </View>
          </View>
        )}
      </For>
      <Show when={!props.rolling}>
        <View class="absolute left-0 top-[160] w-[135] h-[44] flex-col items-center">
          <Text class="text-4xl font-bold text-[#F8FAFC]">{props.total}</Text>
        </View>
        <View class="absolute left-0 top-[200] w-[135] h-[16] flex-col items-center">
          <Text class="text-xs text-[#94A3B8]">{props.detail}</Text>
        </View>
      </Show>
      <FooterView text="FRONT throw · SIDE 1 or 2" hidden={props.hint} />
    </View>
  );
}
