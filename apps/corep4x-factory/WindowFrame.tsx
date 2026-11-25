import { Show, type JSX } from "solid-js";
import { For, Image, Text, View } from "@pocketjs/framework/solid/components";
import type { Color, f32, i32, Px } from "@pocketjs/framework/solid/std";
import Dashes from "./Dashes.tsx";
import type { Edge } from "./frame";

/** A function window (ui_function_window.cpp). The backdrop's corner, size
 *  and opacity are animated; every part keeps its screen position relative
 *  to the backdrop's corner and is clipped by it. Single windows have a
 *  460x418 white content panel at (10,52); paged windows a 460x370 panel and
 *  110x50 tabs along y 421. The header at (300,4) holds the action button
 *  (or the dashed placeholder) and Exit. */
export default function WindowFrame(props: {
  left: Px;
  top: Px;
  width: Px;
  height: Px;
  opacity: f32;
  pages: i32;
  tabs: string[];
  tab: i32;
  outline: Edge[];
  tabEdges: Edge[];
  placeholder: Edge[];
  actionShown: boolean;
  actionPlaceholder: boolean;
  actionText: string;
  actionIcon: boolean;
  actionBg: Color;
  actionFg: Color;
  actionEnabled: boolean;
  onAction: () => void;
  onExit: () => void;
  onTab: (index: i32) => void;
  children?: JSX.Element;
}) {
  return (
    <View
      class="absolute overflow-hidden"
      style={{ insetL: props.left, insetT: props.top, width: props.width, height: props.height, opacity: props.opacity }}
    >
      <Show when={props.pages === 0}>
        <View class="absolute left-[10] top-[52] w-[460] h-[418] bg-white overflow-hidden">{props.children}</View>
      </Show>
      <Show when={props.pages > 0}>
        <View class="absolute left-[10] top-[52] w-[460] h-[370] bg-white overflow-hidden">{props.children}</View>
        {/* The strip right of the last tab, white while the last tab is active. */}
        <View
          class={props.tab === props.pages - 1 ? "absolute top-[422] h-[48] bg-white" : "absolute top-[422] h-[48] bg-[#E2E8F0]"}
          style={{ insetL: 10 + props.pages * 110, width: 460 - props.pages * 110 }}
        />
        <For each={props.tabs} by={name => name}>
          {(name, index) => (
            <View
              class={index() === props.tab ? "absolute top-[421] w-[110] h-[50] bg-white items-center justify-center" : "absolute top-[421] w-[110] h-[50] bg-[#E2E8F0] items-center justify-center"}
              style={{ insetL: 10 + index() * 110 }}
              focusable
              onPress={() => props.onTab(index())}
            >
              <Text class={index() === props.tab ? "text-base text-center text-[#2563EB]" : "text-base text-center text-[#64748B]"}>{name()}</Text>
            </View>
          )}
        </For>
      </Show>
      {/* Header: white box with the action button and Exit. */}
      <View class="absolute left-[300] top-[4] w-[170] h-[48] bg-white">
        <Show when={props.actionPlaceholder}>
          <View class="absolute left-[6] top-[5] w-[76] h-[38] rounded-[6] bg-[#F3F4F6]">
            <Dashes edges={props.placeholder} />
          </View>
        </Show>
        <Show when={!props.actionPlaceholder && props.actionShown}>
          <View
            class="absolute left-[6] top-[5] w-[76] h-[38] rounded-[6] items-center justify-center"
            style={{ bgColor: props.actionBg, opacity: props.actionEnabled ? 1 : 0.5 }}
            focusable
            onPress={() => {
              if (props.actionEnabled) {
                props.onAction();
              }
            }}
          >
            <Show when={props.actionIcon}>
              <Image class="w-[16] h-[16]" src="assets/refresh.png" />
            </Show>
            <Show when={!props.actionIcon}>
              <Text class="text-sm text-center" style={{ textColor: props.actionFg }}>
                {props.actionText}
              </Text>
            </Show>
          </View>
        </Show>
        <View class="absolute left-[88] top-[5] w-[76] h-[38] rounded-[6] bg-[#D32F2F] items-center justify-center" focusable onPress={() => props.onExit()}>
          <Text class="text-sm text-center text-white">Exit</Text>
        </View>
      </View>
      <Dashes edges={props.outline} />
      <Dashes edges={props.tabEdges} />
    </View>
  );
}
