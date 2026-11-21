import { Image, Text, View } from "@pocketjs/framework/solid/components";
import type { f32, i32 } from "@pocketjs/framework/solid/std";
import PageDot from "./PageDot.tsx";

// Factory launcher: one 200 × 200 icon per 466 px page, centred 15 px above
// the screen centre, side arrows at ±200 px, the app title at +155 px and
// the page dots at +200 px.
export default function Launcher(props: {
  offsets: i32[];
  index: i32;
  title: string;
  titleOpacity: f32;
  onOpen: () => void;
  onPrevious: () => void;
  onNext: () => void;
}) {
  return (
    <View class="absolute left-0 top-0 w-[466] h-[466] bg-black overflow-hidden">
      <View class="absolute left-[105] top-[90] w-[256] h-[256]" style={{ translateX: props.offsets[0] ?? 0 }} focusable onPress={() => props.onOpen()}>
        <Image class="w-[256] h-[256]" src="images/fluidbox_icon.png" />
      </View>
      <View class="absolute left-[105] top-[90] w-[256] h-[256]" style={{ translateX: props.offsets[1] ?? 0 }} focusable onPress={() => props.onOpen()}>
        <Image class="w-[256] h-[256]" src="images/icon_clock.png" />
      </View>
      <View class="absolute left-[105] top-[90] w-[256] h-[256]" style={{ translateX: props.offsets[2] ?? 0 }} focusable onPress={() => props.onOpen()}>
        <Image class="w-[256] h-[256]" src="images/icon_watch_face.png" />
      </View>
      <View class="absolute left-[105] top-[90] w-[256] h-[256]" style={{ translateX: props.offsets[3] ?? 0 }} focusable onPress={() => props.onOpen()}>
        <Image class="w-[256] h-[256]" src="images/icon_stopwatch.png" />
      </View>
      <View class="absolute left-[105] top-[90] w-[256] h-[256]" style={{ translateX: props.offsets[4] ?? 0 }} focusable onPress={() => props.onOpen()}>
        <Image class="w-[256] h-[256]" src="images/icon_badge.png" />
      </View>
      <View class="absolute left-[105] top-[90] w-[256] h-[256]" style={{ translateX: props.offsets[5] ?? 0 }} focusable onPress={() => props.onOpen()}>
        <Image class="w-[256] h-[256]" src="images/icon_imu.png" />
      </View>
      <View class="absolute left-[105] top-[90] w-[256] h-[256]" style={{ translateX: props.offsets[6] ?? 0 }} focusable onPress={() => props.onOpen()}>
        <Image class="w-[256] h-[256]" src="images/icon_fft.png" />
      </View>
      <View class="absolute left-[105] top-[90] w-[256] h-[256]" style={{ translateX: props.offsets[7] ?? 0 }} focusable onPress={() => props.onOpen()}>
        <Image class="w-[256] h-[256]" src="images/icon_lucky_wheel.png" />
      </View>
      <View class="absolute left-[105] top-[90] w-[256] h-[256]" style={{ translateX: props.offsets[8] ?? 0 }} focusable onPress={() => props.onOpen()}>
        <Image class="w-[256] h-[256]" src="images/icon_setup.png" />
      </View>
      <View class="absolute left-[7] top-[153] w-[52] h-[160] items-center justify-center" focusable onPress={() => props.onPrevious()}>
        <Image class="w-[32] h-[128]" src="images/icon_indicator_left.png" />
      </View>
      <View class="absolute left-[407] top-[153] w-[52] h-[160] items-center justify-center" focusable onPress={() => props.onNext()}>
        <Image class="w-[32] h-[128]" src="images/icon_indicator_right.png" />
      </View>
      <View class="absolute left-0 top-[372] w-[466] h-[34] items-center justify-center" style={{ opacity: props.titleOpacity }}>
        <Text class="text-2xl font-bold text-white">{props.title}</Text>
      </View>
      <PageDot x={162} selected={props.index === 0} />
      <PageDot x={178} selected={props.index === 1} />
      <PageDot x={194} selected={props.index === 2} />
      <PageDot x={210} selected={props.index === 3} />
      <PageDot x={226} selected={props.index === 4} />
      <PageDot x={242} selected={props.index === 5} />
      <PageDot x={258} selected={props.index === 6} />
      <PageDot x={274} selected={props.index === 7} />
      <PageDot x={290} selected={props.index === 8} />
    </View>
  );
}
