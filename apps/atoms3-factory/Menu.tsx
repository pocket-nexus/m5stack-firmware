import { Match, Switch } from "solid-js";
import { Image } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";

// func_img_list: the full-screen menu artwork of each function, drawn with
// M5.Display.drawPng(func_img_list[func_index], ~0u, 0, 0).
export default function Menu(props: { index: i32 }) {
  return (
    <Switch>
      <Match when={props.index === 0}>
        <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/wifi_scan.png" />
      </Match>
      <Match when={props.index === 1}>
        <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/i2c_scan.png" />
      </Match>
      <Match when={props.index === 2}>
        <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/uart_mon.png" />
      </Match>
      <Match when={props.index === 3}>
        <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/io_pwm.png" />
      </Match>
      <Match when={props.index === 4}>
        <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/io_adc.png" />
      </Match>
      <Match when={props.index === 5}>
        <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/ir_send.png" />
      </Match>
      <Match when={props.index === 6}>
        <Image class="absolute left-0 top-0 w-[128] h-[128]" src="img/imu_test.png" />
      </Match>
    </Switch>
  );
}
