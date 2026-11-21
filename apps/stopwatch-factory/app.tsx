import { Match, Show, Switch } from "solid-js";
import { ActionHandler, Image, View } from "@pocketjs/framework/solid/components";
import { onMount } from "@pocketjs/framework/solid/lifecycle";
import { BTN } from "@pocketjs/framework/input";
import Launcher from "./Launcher.tsx";
import StatusBar from "./StatusBar.tsx";
import Stopwatch from "./Stopwatch.tsx";
import TopClock from "./TopClock.tsx";
import WatchFace from "./WatchFace.tsx";
import {
  appIndex,
  appTitle,
  batteryLevel,
  bigDigits,
  batteryText,
  bridged,
  charging,
  classicMode,
  clock0,
  clock1,
  clock2,
  clock3,
  clock4,
  dotX,
  dotY,
  face,
  faceBackground,
  facePanel,
  faceRotation,
  hourAngle,
  iconOffsets,
  keyAClick,
  keyBClick,
  launcherNext,
  launcherPrevious,
  minuteAngle,
  openCurrentApp,
  page,
  plainTime,
  run,
  secondAngle,
  simpleDot,
  simpleTips,
  statusShown,
  statusTap,
  statusY,
  stopwatchLeft,
  stopwatchRight,
  swLapScroll,
  swLaps,
  swLeftOffset,
  swRightOffset,
  swState,
  swTime,
  themeIndex,
  ticks,
  titleOpacity,
  watchDate,
  watchDay,
  watchHH,
  watchMM,
  watchSS,
  watchTap,
  watchTime,
  watchWeekday,
} from "./app";

export default function App() {
  onMount(run);
  return (
    <View class="w-[466] h-[466] bg-black overflow-hidden">
      <ActionHandler button={BTN.LTRIGGER} active={!bridged()} onPress={keyAClick} />
      <ActionHandler button={BTN.RTRIGGER} active={!bridged()} onPress={keyBClick} />
      <Switch>
        <Match when={page() === "guide"}>
          <View class="absolute left-0 top-0 w-[466] h-[466] bg-black">
            <Image class="absolute left-[46] top-[94] w-[256] h-[256]" src="images/go_home_guide_0.png" />
            <Image class="absolute left-[302] top-[94] w-[128] h-[256]" src="images/go_home_guide_1.png" />
            <Image class="absolute left-[46] top-[350] w-[256] h-[32]" src="images/go_home_guide_2.png" />
            <Image class="absolute left-[302] top-[350] w-[128] h-[32]" src="images/go_home_guide_3.png" />
          </View>
        </Match>
        <Match when={page() === "launcher"}>
          <Launcher
            offsets={iconOffsets()}
            index={appIndex()}
            title={appTitle()}
            titleOpacity={titleOpacity()}
            onOpen={openCurrentApp}
            onPrevious={launcherPrevious}
            onNext={launcherNext}
          />
          <TopClock c0={clock0()} c1={clock1()} c2={clock2()} c3={clock3()} c4={clock4()} tint={false} />
          <Show when={statusShown()}>
            <StatusBar y={statusY()} level={batteryLevel()} text={batteryText()} charging={charging()} onTap={statusTap} />
          </Show>
        </Match>
        <Match when={page() === "watch"}>
          <WatchFace
            face={face()}
            rotation={faceRotation()}
            mode={classicMode()}
            hourAngle={hourAngle()}
            minuteAngle={minuteAngle()}
            secondAngle={secondAngle()}
            time={watchTime()}
            plainTime={plainTime()}
            weekday={watchWeekday()}
            day={watchDay()}
            date={watchDate()}
            hh={watchHH()}
            mm={watchMM()}
            ss={watchSS()}
            background={faceBackground()}
            panel={facePanel()}
            theme={themeIndex()}
            dot={simpleDot()}
            tips={simpleTips()}
            dotX={dotX()}
            dotY={dotY()}
            ticks={ticks()}
            digits={bigDigits()}
            onTap={watchTap}
          />
        </Match>
        <Match when={page() === "stopwatch"}>
          <Stopwatch
            state={swState()}
            time={swTime()}
            laps={swLaps()}
            scroll={swLapScroll()}
            leftOffset={swLeftOffset()}
            rightOffset={swRightOffset()}
            onLeft={stopwatchLeft}
            onRight={stopwatchRight}
          />
          <TopClock c0={clock0()} c1={clock1()} c2={clock2()} c3={clock3()} c4={clock4()} tint={true} />
        </Match>
      </Switch>
    </View>
  );
}
