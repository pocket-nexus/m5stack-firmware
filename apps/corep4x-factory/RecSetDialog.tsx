import { Text, View } from "@pocketjs/framework/solid/components";
import Button from "./Button.tsx";

/** The "REC Set" dialog: a medium dialog (460x248) with the view text, the
 *  "Reference channel" REF switch (132x34) and Waveform / Spectrum / Close;
 *  the active mode's button is blue. */
export default function RecSetDialog(props: {
  text: string;
  spectrum: boolean;
  showRef: boolean;
  onRef: () => void;
  onWaveform: () => void;
  onSpectrum: () => void;
  onClose: () => void;
}) {
  return (
    <View class="absolute left-0 top-0 w-[480] h-[480] bg-[#11182780]" focusable onPress={() => {}}>
      <View class="absolute left-[10] top-[116] w-[460] h-[248] rounded-[8] bg-white border-[2] border-[#475569] overflow-hidden">
        <View class="absolute left-0 top-0 w-[456] h-[43] bg-[#E0E0E0]">
          <Text class="absolute left-[5] top-[12] text-base text-[#111827]">REC Set</Text>
        </View>
        <Text class="absolute left-[5] top-[48] w-[420] text-base text-[#111827]">{props.text}</Text>
        <Text class="absolute left-[5] top-[140] w-[190] text-base text-[#64748B]">Reference channel</Text>
        <Button
          left={203}
          top={132}
          width={132}
          height={34}
          text={props.showRef ? "REF: ON" : "REF: OFF"}
          bg={props.showRef ? "#2563EB" : "#F3F4F6"}
          fg={props.showRef ? "#FFFFFF" : "#111827"}
          border="#CBD5E1"
          radius={8}
          size={16}
          enabled={true}
          dim={0.5}
          onPress={() => props.onRef()}
        />
        <Button left={24} top={202} width={116} height={40} text="Waveform" bg={props.spectrum ? "#F3F4F6" : "#2563EB"} fg={props.spectrum ? "#111827" : "#FFFFFF"} border="#CBD5E1" radius={6} size={16} enabled={true} dim={0.5} onPress={() => props.onWaveform()} />
        <Button left={169} top={202} width={116} height={40} text="Spectrum" bg={props.spectrum ? "#2563EB" : "#F3F4F6"} fg={props.spectrum ? "#FFFFFF" : "#111827"} border="#CBD5E1" radius={6} size={16} enabled={true} dim={0.5} onPress={() => props.onSpectrum()} />
        <Button left={314} top={202} width={116} height={40} text="Close" bg="#F3F4F6" fg="#111827" border="#CBD5E1" radius={6} size={16} enabled={true} dim={0.5} onPress={() => props.onClose()} />
      </View>
    </View>
  );
}
