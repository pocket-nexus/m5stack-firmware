import { Image, Text, View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";

// UsbApp: artwork usb.jpg with the battery level right-aligned at x 121,
// y 6, in front of the "%" the artwork draws.
export default function Usb(props: { battery: i32 }) {
  return (
    <View debugName="Usb" class="absolute left-0 top-0 w-[135] h-[240] overflow-hidden">
      <Image class="absolute left-0 top-0 w-[256] h-[256]" src="images/usb.png" />
      <View class="absolute left-[74] top-[5] w-[47] h-[18] flex-col items-end">
        <Text class="text-sm text-black">{props.battery}</Text>
      </View>
    </View>
  );
}
