import { View, Text } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";

// lv_roller as the factory styles it: 164 px tall, radius 52, Montserrat 28
// rows 46 px apart and a 47 px selection band in the middle.
export default function Roller(props: {
  left: i32;
  top: i32;
  width: i32;
  rows: string[];
  base: i32;
  shift: i32;
  wheel: boolean;
}) {
  return (
    <View
      class={props.wheel ? "absolute left-0 top-0 w-[466] h-[164] rounded-[52] bg-[#4C4C4C] overflow-hidden" : "absolute left-0 top-0 w-[466] h-[164] rounded-[52] bg-[#343434] overflow-hidden"}
      style={{ insetL: props.left, insetT: props.top, width: props.width }}
    >
      <View class={props.wheel ? "absolute left-0 top-[59] w-full h-[47] bg-[#737373]" : "absolute left-0 top-[59] w-full h-[47] bg-[#696969]"} />
      <View class="absolute left-0 top-[-33] w-full h-[230]" style={{ translateY: props.shift }}>
        <View class="absolute left-0 top-0 w-full h-[46] items-center justify-center">
          <Text class="text-2xl text-white">{props.rows[props.base] ?? ""}</Text>
        </View>
        <View class="absolute left-0 top-[46] w-full h-[46] items-center justify-center">
          <Text class="text-2xl text-white">{props.rows[props.base + 1] ?? ""}</Text>
        </View>
        <View class="absolute left-0 top-[92] w-full h-[46] items-center justify-center">
          <Text class="text-2xl text-white">{props.rows[props.base + 2] ?? ""}</Text>
        </View>
        <View class="absolute left-0 top-[138] w-full h-[46] items-center justify-center">
          <Text class="text-2xl text-white">{props.rows[props.base + 3] ?? ""}</Text>
        </View>
        <View class="absolute left-0 top-[184] w-full h-[46] items-center justify-center">
          <Text class="text-2xl text-white">{props.rows[props.base + 4] ?? ""}</Text>
        </View>
      </View>
    </View>
  );
}
