import { Show } from "solid-js";
import { Text, View } from "@pocketjs/framework/solid/components";
import { idiv, type i32 } from "@pocketjs/framework/solid/std";

// Factory app_imu view: #FFB333 ball with a small cross, the fixed 289 px
// cross, the #CBFFB1 orbit dot at radius 214 and the acceleration labels.
export default function Imu(props: {
  ballLeft: i32;
  ballTop: i32;
  ballSize: i32;
  orbitX: i32;
  orbitY: i32;
  labels: boolean;
  x: string;
  y: string;
  z: string;
  onTap: () => void;
}) {
  return (
    <View class="absolute left-0 top-0 w-[466] h-[466] bg-black" focusable onPress={() => props.onTap()}>
      <View class="absolute left-0 top-0 rounded-[60] bg-[#FFB333]" style={{ translateX: props.ballLeft, translateY: props.ballTop, width: props.ballSize, height: props.ballSize }} />
      <Show when={props.labels}>
        <View class="absolute left-[224] top-[224] w-[18] h-[18] rounded-full bg-[#CBFFB1]" style={{ translateX: props.orbitX, translateY: props.orbitY }} />
      </Show>
      <View class="absolute left-[-18] top-[-1] w-[36] h-[2] rounded-[1] bg-white" style={{ translateX: props.ballLeft + idiv(props.ballSize, 2), translateY: props.ballTop + idiv(props.ballSize, 2) }} />
      <View class="absolute left-[-1] top-[-18] w-[2] h-[36] rounded-[1] bg-white" style={{ translateX: props.ballLeft + idiv(props.ballSize, 2), translateY: props.ballTop + idiv(props.ballSize, 2) }} />
      <View class="absolute left-[89] top-[232] w-[289] h-[2] bg-white" />
      <View class="absolute left-[232] top-[89] w-[2] h-[289] bg-white" />
      <Show when={props.labels}>
        <Text class="absolute left-[244] top-[94] w-[100] h-[20] text-base text-[#CBFFB1]">{props.x}</Text>
        <Text class="absolute left-[96] top-[241] w-[100] h-[20] text-base text-[#CBFFB1]">{props.y}</Text>
        <Text class="absolute left-[244] top-[352] w-[100] h-[20] text-base text-[#CBFFB1]">{props.z}</Text>
      </Show>
    </View>
  );
}
