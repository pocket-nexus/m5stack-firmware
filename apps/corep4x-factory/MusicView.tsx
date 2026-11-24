import { For, Text, View } from "@pocketjs/framework/solid/components";
import type { Color, f32, Px } from "@pocketjs/framework/solid/std";
import Button from "./Button.tsx";
import type { Bar } from "./music";

/** MUSIC window content, in screen coordinates: the spectrum panel with the
 *  title (the factory's Montserrat 28 at the 24 px slot) and 24 bars, the
 *  progress bar, the times, the status line, PLAY/PAUSE and the volume box. */
export default function MusicView(props: {
  bars: Bar[];
  playing: boolean;
  progress: Px;
  elapsed: string;
  duration: string;
  status: string;
  statusColor: Color;
  buttonText: string;
  buttonBg: Color;
  buttonFg: Color;
  buttonEnabled: boolean;
  volume: string;
  knob: Px;
  indicator: Px;
  sliderEnabled: boolean;
  barOpacity: f32;
  onPlay: () => void;
}) {
  return (
    <View class="absolute left-[-10] top-[-52] w-[480] h-[480]">
      <View class="absolute left-[10] top-[52] w-[460] h-[418] bg-[#F8FAFC]" />
      <View class="absolute left-[31] top-[66] w-[418] h-[230] rounded-[22] bg-[#EFF6FF] border-[1] border-[#BFDBFE]" />
      <Text class="absolute left-[50] top-[81] text-2xl text-[#111827]">CandyWind</Text>
      <Text class="absolute left-[52] top-[115] text-base text-[#64748B]">Mono 44.1 kHz 45 s</Text>
      <For each={props.bars} by={bar => bar.id}>
        {bar => (
          <View
            class={bar().sky ? "absolute w-[8] rounded-[4] bg-[#0EA5E9]" : "absolute w-[8] rounded-[4] bg-[#2563EB]"}
            style={{ insetL: bar().left, insetT: bar().top, height: bar().height, opacity: props.barOpacity }}
          />
        )}
      </For>
      <View class="absolute left-[24] top-[300] w-[432] h-[8] rounded-[5] bg-[#CBD5E1] overflow-hidden">
        <View class="absolute left-0 top-0 h-[8] rounded-[5] bg-[#2563EB]" style={{ width: props.progress }} />
      </View>
      <Text class="absolute left-[24] top-[313] w-[80] text-base text-[#64748B]">{props.elapsed}</Text>
      <View class="absolute left-[376] top-[313] w-[80] h-[18] flex-col items-end">
        <Text class="text-base text-[#64748B]">{props.duration}</Text>
      </View>
      <Text class="absolute left-[24] top-[342] w-[160] text-base" style={{ textColor: props.statusColor }}>
        {props.status}
      </Text>
      <View class="absolute left-[266] top-[342] w-[190] h-[18] flex-col items-end">
        <Text class="text-base text-[#64748B]">128 FFT  44.1 kHz</Text>
      </View>
      <Button left={24} top={381} width={184} height={60} text={props.buttonText} bg={props.buttonBg} fg={props.buttonFg} border="#CBD5E1" radius={30} size={16} enabled={props.buttonEnabled} dim={0.5} onPress={() => props.onPlay()} />
      <View class="absolute left-[228] top-[377] w-[228] h-[68] rounded-[14] bg-white border-[1] border-[#CBD5E1]" style={{ opacity: props.sliderEnabled ? 1 : 0.5 }}>
        <Text class="absolute left-[22] top-[10] text-base text-[#64748B]">{props.volume}</Text>
        <View class="absolute left-[22] top-[34] w-[176] h-[18] rounded-[9] bg-[#E5E7EB]" />
        <View class="absolute left-[22] top-[34] h-[18] rounded-[9] bg-[#2563EB]" style={{ width: props.indicator }} />
        <View class="absolute top-[28] w-[30] h-[30] rounded-[15] bg-white border-[2] border-[#2563EB]" style={{ insetL: props.knob }} />
      </View>
    </View>
  );
}
