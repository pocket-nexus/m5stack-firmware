import { Text, View } from "@pocketjs/framework/solid/components";
import type { Deg, i32 } from "@pocketjs/framework/solid/std";

// One number-flow digit (smooth_ui_toolkit DigitFlow): the strip
// 9,0,1,...,9,0 of 66 px rows behind a 66 px window, moved by the spring
// offset of the root model. The window stays upright so it clips the
// strip; with a turned face the glyphs turn and the window only moves.
export default function DigitStrip(props: { left: i32; dx: i32; dy: i32; offset: i32; theme: i32; rotation: Deg }) {
  return (
    <View class="absolute left-0 top-[33] w-[50] h-[66] overflow-hidden" style={{ translateX: props.left + props.dx, translateY: props.dy }}>
      <View class="absolute left-0 top-0 w-[50] h-[792]" style={{ translateY: -props.offset }}>
        <View class="absolute left-0 top-[0] w-[50] h-[66] items-center justify-center" style={{ rotate: props.rotation }}>
          <Text class={props.theme === 0 ? "text-5xl text-[#FFFFFF]" : props.theme === 1 ? "text-5xl text-[#484848]" : props.theme === 2 ? "text-5xl text-[#566E6C]" : props.theme === 3 ? "text-5xl text-[#6A5E4D]" : props.theme === 4 ? "text-5xl text-[#4B5E20]" : props.theme === 5 ? "text-5xl text-[#DFDEFF]" : props.theme === 6 ? "text-5xl text-[#FFEFE6]" : props.theme === 7 ? "text-5xl text-[#83627E]" : props.theme === 8 ? "text-5xl text-[#B1FFFA]" : "text-5xl text-[#ECF8FF]"}>9</Text>
        </View>
        <View class="absolute left-0 top-[66] w-[50] h-[66] items-center justify-center" style={{ rotate: props.rotation }}>
          <Text class={props.theme === 0 ? "text-5xl text-[#FFFFFF]" : props.theme === 1 ? "text-5xl text-[#484848]" : props.theme === 2 ? "text-5xl text-[#566E6C]" : props.theme === 3 ? "text-5xl text-[#6A5E4D]" : props.theme === 4 ? "text-5xl text-[#4B5E20]" : props.theme === 5 ? "text-5xl text-[#DFDEFF]" : props.theme === 6 ? "text-5xl text-[#FFEFE6]" : props.theme === 7 ? "text-5xl text-[#83627E]" : props.theme === 8 ? "text-5xl text-[#B1FFFA]" : "text-5xl text-[#ECF8FF]"}>0</Text>
        </View>
        <View class="absolute left-0 top-[132] w-[50] h-[66] items-center justify-center" style={{ rotate: props.rotation }}>
          <Text class={props.theme === 0 ? "text-5xl text-[#FFFFFF]" : props.theme === 1 ? "text-5xl text-[#484848]" : props.theme === 2 ? "text-5xl text-[#566E6C]" : props.theme === 3 ? "text-5xl text-[#6A5E4D]" : props.theme === 4 ? "text-5xl text-[#4B5E20]" : props.theme === 5 ? "text-5xl text-[#DFDEFF]" : props.theme === 6 ? "text-5xl text-[#FFEFE6]" : props.theme === 7 ? "text-5xl text-[#83627E]" : props.theme === 8 ? "text-5xl text-[#B1FFFA]" : "text-5xl text-[#ECF8FF]"}>1</Text>
        </View>
        <View class="absolute left-0 top-[198] w-[50] h-[66] items-center justify-center" style={{ rotate: props.rotation }}>
          <Text class={props.theme === 0 ? "text-5xl text-[#FFFFFF]" : props.theme === 1 ? "text-5xl text-[#484848]" : props.theme === 2 ? "text-5xl text-[#566E6C]" : props.theme === 3 ? "text-5xl text-[#6A5E4D]" : props.theme === 4 ? "text-5xl text-[#4B5E20]" : props.theme === 5 ? "text-5xl text-[#DFDEFF]" : props.theme === 6 ? "text-5xl text-[#FFEFE6]" : props.theme === 7 ? "text-5xl text-[#83627E]" : props.theme === 8 ? "text-5xl text-[#B1FFFA]" : "text-5xl text-[#ECF8FF]"}>2</Text>
        </View>
        <View class="absolute left-0 top-[264] w-[50] h-[66] items-center justify-center" style={{ rotate: props.rotation }}>
          <Text class={props.theme === 0 ? "text-5xl text-[#FFFFFF]" : props.theme === 1 ? "text-5xl text-[#484848]" : props.theme === 2 ? "text-5xl text-[#566E6C]" : props.theme === 3 ? "text-5xl text-[#6A5E4D]" : props.theme === 4 ? "text-5xl text-[#4B5E20]" : props.theme === 5 ? "text-5xl text-[#DFDEFF]" : props.theme === 6 ? "text-5xl text-[#FFEFE6]" : props.theme === 7 ? "text-5xl text-[#83627E]" : props.theme === 8 ? "text-5xl text-[#B1FFFA]" : "text-5xl text-[#ECF8FF]"}>3</Text>
        </View>
        <View class="absolute left-0 top-[330] w-[50] h-[66] items-center justify-center" style={{ rotate: props.rotation }}>
          <Text class={props.theme === 0 ? "text-5xl text-[#FFFFFF]" : props.theme === 1 ? "text-5xl text-[#484848]" : props.theme === 2 ? "text-5xl text-[#566E6C]" : props.theme === 3 ? "text-5xl text-[#6A5E4D]" : props.theme === 4 ? "text-5xl text-[#4B5E20]" : props.theme === 5 ? "text-5xl text-[#DFDEFF]" : props.theme === 6 ? "text-5xl text-[#FFEFE6]" : props.theme === 7 ? "text-5xl text-[#83627E]" : props.theme === 8 ? "text-5xl text-[#B1FFFA]" : "text-5xl text-[#ECF8FF]"}>4</Text>
        </View>
        <View class="absolute left-0 top-[396] w-[50] h-[66] items-center justify-center" style={{ rotate: props.rotation }}>
          <Text class={props.theme === 0 ? "text-5xl text-[#FFFFFF]" : props.theme === 1 ? "text-5xl text-[#484848]" : props.theme === 2 ? "text-5xl text-[#566E6C]" : props.theme === 3 ? "text-5xl text-[#6A5E4D]" : props.theme === 4 ? "text-5xl text-[#4B5E20]" : props.theme === 5 ? "text-5xl text-[#DFDEFF]" : props.theme === 6 ? "text-5xl text-[#FFEFE6]" : props.theme === 7 ? "text-5xl text-[#83627E]" : props.theme === 8 ? "text-5xl text-[#B1FFFA]" : "text-5xl text-[#ECF8FF]"}>5</Text>
        </View>
        <View class="absolute left-0 top-[462] w-[50] h-[66] items-center justify-center" style={{ rotate: props.rotation }}>
          <Text class={props.theme === 0 ? "text-5xl text-[#FFFFFF]" : props.theme === 1 ? "text-5xl text-[#484848]" : props.theme === 2 ? "text-5xl text-[#566E6C]" : props.theme === 3 ? "text-5xl text-[#6A5E4D]" : props.theme === 4 ? "text-5xl text-[#4B5E20]" : props.theme === 5 ? "text-5xl text-[#DFDEFF]" : props.theme === 6 ? "text-5xl text-[#FFEFE6]" : props.theme === 7 ? "text-5xl text-[#83627E]" : props.theme === 8 ? "text-5xl text-[#B1FFFA]" : "text-5xl text-[#ECF8FF]"}>6</Text>
        </View>
        <View class="absolute left-0 top-[528] w-[50] h-[66] items-center justify-center" style={{ rotate: props.rotation }}>
          <Text class={props.theme === 0 ? "text-5xl text-[#FFFFFF]" : props.theme === 1 ? "text-5xl text-[#484848]" : props.theme === 2 ? "text-5xl text-[#566E6C]" : props.theme === 3 ? "text-5xl text-[#6A5E4D]" : props.theme === 4 ? "text-5xl text-[#4B5E20]" : props.theme === 5 ? "text-5xl text-[#DFDEFF]" : props.theme === 6 ? "text-5xl text-[#FFEFE6]" : props.theme === 7 ? "text-5xl text-[#83627E]" : props.theme === 8 ? "text-5xl text-[#B1FFFA]" : "text-5xl text-[#ECF8FF]"}>7</Text>
        </View>
        <View class="absolute left-0 top-[594] w-[50] h-[66] items-center justify-center" style={{ rotate: props.rotation }}>
          <Text class={props.theme === 0 ? "text-5xl text-[#FFFFFF]" : props.theme === 1 ? "text-5xl text-[#484848]" : props.theme === 2 ? "text-5xl text-[#566E6C]" : props.theme === 3 ? "text-5xl text-[#6A5E4D]" : props.theme === 4 ? "text-5xl text-[#4B5E20]" : props.theme === 5 ? "text-5xl text-[#DFDEFF]" : props.theme === 6 ? "text-5xl text-[#FFEFE6]" : props.theme === 7 ? "text-5xl text-[#83627E]" : props.theme === 8 ? "text-5xl text-[#B1FFFA]" : "text-5xl text-[#ECF8FF]"}>8</Text>
        </View>
        <View class="absolute left-0 top-[660] w-[50] h-[66] items-center justify-center" style={{ rotate: props.rotation }}>
          <Text class={props.theme === 0 ? "text-5xl text-[#FFFFFF]" : props.theme === 1 ? "text-5xl text-[#484848]" : props.theme === 2 ? "text-5xl text-[#566E6C]" : props.theme === 3 ? "text-5xl text-[#6A5E4D]" : props.theme === 4 ? "text-5xl text-[#4B5E20]" : props.theme === 5 ? "text-5xl text-[#DFDEFF]" : props.theme === 6 ? "text-5xl text-[#FFEFE6]" : props.theme === 7 ? "text-5xl text-[#83627E]" : props.theme === 8 ? "text-5xl text-[#B1FFFA]" : "text-5xl text-[#ECF8FF]"}>9</Text>
        </View>
        <View class="absolute left-0 top-[726] w-[50] h-[66] items-center justify-center" style={{ rotate: props.rotation }}>
          <Text class={props.theme === 0 ? "text-5xl text-[#FFFFFF]" : props.theme === 1 ? "text-5xl text-[#484848]" : props.theme === 2 ? "text-5xl text-[#566E6C]" : props.theme === 3 ? "text-5xl text-[#6A5E4D]" : props.theme === 4 ? "text-5xl text-[#4B5E20]" : props.theme === 5 ? "text-5xl text-[#DFDEFF]" : props.theme === 6 ? "text-5xl text-[#FFEFE6]" : props.theme === 7 ? "text-5xl text-[#83627E]" : props.theme === 8 ? "text-5xl text-[#B1FFFA]" : "text-5xl text-[#ECF8FF]"}>0</Text>
        </View>
      </View>
    </View>
  );
}
