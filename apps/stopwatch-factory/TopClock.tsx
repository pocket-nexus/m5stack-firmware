import { Text, View } from "@pocketjs/framework/solid/components";

// Factory ArcTopClock: five Maple Mono 28 characters along the top of the
// round panel. Each character keeps the factory rotation; the boxes sit where
// the factory pivot (233 px below the label) places them.
export default function TopClock(props: { c0: string; c1: string; c2: string; c3: string; c4: string; tint: boolean }) {
  return (
    <View class="absolute left-0 top-0 w-[466] h-[60]">
      <View class="absolute left-[188] top-[7] w-[20] h-[34]" style={{ rotate: -10 }}>
        <Text class={props.tint ? "w-[20] h-[34] text-2xl text-center text-[#CDEBF9]" : "w-[20] h-[34] text-2xl text-center text-white"}>{props.c0}</Text>
      </View>
      <View class="absolute left-[209] top-[5] w-[20] h-[34]" style={{ rotate: -4.6 }}>
        <Text class={props.tint ? "w-[20] h-[34] text-2xl text-center text-[#CDEBF9]" : "w-[20] h-[34] text-2xl text-center text-white"}>{props.c1}</Text>
      </View>
      <View class="absolute left-[224] top-[3] w-[20] h-[34]">
        <Text class={props.tint ? "w-[20] h-[34] text-2xl text-center text-[#CDEBF9]" : "w-[20] h-[34] text-2xl text-center text-white"}>{props.c2}</Text>
      </View>
      <View class="absolute left-[239] top-[5] w-[20] h-[34]" style={{ rotate: 3.2 }}>
        <Text class={props.tint ? "w-[20] h-[34] text-2xl text-center text-[#CDEBF9]" : "w-[20] h-[34] text-2xl text-center text-white"}>{props.c3}</Text>
      </View>
      <View class="absolute left-[259] top-[8] w-[20] h-[34]" style={{ rotate: 8.4 }}>
        <Text class={props.tint ? "w-[20] h-[34] text-2xl text-center text-[#CDEBF9]" : "w-[20] h-[34] text-2xl text-center text-white"}>{props.c4}</Text>
      </View>
    </View>
  );
}
