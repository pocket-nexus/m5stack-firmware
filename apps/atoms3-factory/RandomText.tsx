import { Text } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";

// Text in one of 16 colours (small: the 12 px efont face, standing in for
// the factory's 6x8 GLCD Font0). The factory draws I2C addresses and UART bytes
// in random 24-bit colours (random(0, 255) per channel); the view selects a
// colour from this fixed palette by the model's random index.
export default function RandomText(props: { color: i32; text: string; small: boolean }) {
  return (
    <Text
      class={
        props.small
          ? props.color === 0
          ? "text-xs font-mono leading-[12px] text-[#e04a2f]"
          : props.color === 1
          ? "text-xs font-mono leading-[12px] text-[#f5c518]"
          : props.color === 2
          ? "text-xs font-mono leading-[12px] text-[#4fd1c5]"
          : props.color === 3
          ? "text-xs font-mono leading-[12px] text-[#9f7aea]"
          : props.color === 4
          ? "text-xs font-mono leading-[12px] text-[#68d391]"
          : props.color === 5
          ? "text-xs font-mono leading-[12px] text-[#f687b3]"
          : props.color === 6
          ? "text-xs font-mono leading-[12px] text-[#63b3ed]"
          : props.color === 7
          ? "text-xs font-mono leading-[12px] text-[#ed8936]"
          : props.color === 8
          ? "text-xs font-mono leading-[12px] text-[#c6f6d5]"
          : props.color === 9
          ? "text-xs font-mono leading-[12px] text-[#fbd38d]"
          : props.color === 10
          ? "text-xs font-mono leading-[12px] text-[#b794f4]"
          : props.color === 11
          ? "text-xs font-mono leading-[12px] text-[#90cdf4]"
          : props.color === 12
          ? "text-xs font-mono leading-[12px] text-[#fc8181]"
          : props.color === 13
          ? "text-xs font-mono leading-[12px] text-[#81e6d9]"
          : props.color === 14
          ? "text-xs font-mono leading-[12px] text-[#faf089]"
          : "text-xs font-mono leading-[12px] text-[#d6bcfa]"
          : props.color === 0
          ? "text-base leading-[16px] text-[#e04a2f]"
          : props.color === 1
          ? "text-base leading-[16px] text-[#f5c518]"
          : props.color === 2
          ? "text-base leading-[16px] text-[#4fd1c5]"
          : props.color === 3
          ? "text-base leading-[16px] text-[#9f7aea]"
          : props.color === 4
          ? "text-base leading-[16px] text-[#68d391]"
          : props.color === 5
          ? "text-base leading-[16px] text-[#f687b3]"
          : props.color === 6
          ? "text-base leading-[16px] text-[#63b3ed]"
          : props.color === 7
          ? "text-base leading-[16px] text-[#ed8936]"
          : props.color === 8
          ? "text-base leading-[16px] text-[#c6f6d5]"
          : props.color === 9
          ? "text-base leading-[16px] text-[#fbd38d]"
          : props.color === 10
          ? "text-base leading-[16px] text-[#b794f4]"
          : props.color === 11
          ? "text-base leading-[16px] text-[#90cdf4]"
          : props.color === 12
          ? "text-base leading-[16px] text-[#fc8181]"
          : props.color === 13
          ? "text-base leading-[16px] text-[#81e6d9]"
          : props.color === 14
          ? "text-base leading-[16px] text-[#faf089]"
          : "text-base leading-[16px] text-[#d6bcfa]"
      }
    >
      {props.text}
    </Text>
  );
}
