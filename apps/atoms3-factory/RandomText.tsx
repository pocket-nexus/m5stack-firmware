import { Text } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";

// Text in one of 16 colours. The factory draws I2C addresses and UART bytes
// in random 24-bit colours (random(0, 255) per channel); the view selects a
// colour from this fixed palette by the model's random index.
export default function RandomText(props: { color: i32; text: string; small: boolean }) {
  return (
    <Text
      class={
        props.small
          ? props.color === 0
          ? "text-2xs leading-[8px] text-[#e04a2f]"
          : props.color === 1
          ? "text-2xs leading-[8px] text-[#f5c518]"
          : props.color === 2
          ? "text-2xs leading-[8px] text-[#4fd1c5]"
          : props.color === 3
          ? "text-2xs leading-[8px] text-[#9f7aea]"
          : props.color === 4
          ? "text-2xs leading-[8px] text-[#68d391]"
          : props.color === 5
          ? "text-2xs leading-[8px] text-[#f687b3]"
          : props.color === 6
          ? "text-2xs leading-[8px] text-[#63b3ed]"
          : props.color === 7
          ? "text-2xs leading-[8px] text-[#ed8936]"
          : props.color === 8
          ? "text-2xs leading-[8px] text-[#c6f6d5]"
          : props.color === 9
          ? "text-2xs leading-[8px] text-[#fbd38d]"
          : props.color === 10
          ? "text-2xs leading-[8px] text-[#b794f4]"
          : props.color === 11
          ? "text-2xs leading-[8px] text-[#90cdf4]"
          : props.color === 12
          ? "text-2xs leading-[8px] text-[#fc8181]"
          : props.color === 13
          ? "text-2xs leading-[8px] text-[#81e6d9]"
          : props.color === 14
          ? "text-2xs leading-[8px] text-[#faf089]"
          : "text-2xs leading-[8px] text-[#d6bcfa]"
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
