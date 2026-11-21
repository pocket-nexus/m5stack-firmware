import { Match, Show, Switch } from "solid-js";
import { ActionHandler, Image, Text, View } from "@pocketjs/framework/solid/components";
import { onMount } from "@pocketjs/framework/solid/lifecycle";
import { BTN } from "@pocketjs/framework/input";
import AlarmClock from "./AlarmClock.tsx";
import Badge from "./BadgeView.tsx";
import Fft from "./FftView.tsx";
import Fluid from "./FluidView.tsx";
import Imu from "./ImuView.tsx";
import Launcher from "./LauncherView.tsx";
import Setup from "./SetupView.tsx";
import Wheel from "./WheelView.tsx";
import StatusBar from "./StatusBarView.tsx";
import Stopwatch from "./StopwatchView.tsx";
import TopClock from "./TopClock.tsx";
import WatchFace from "./WatchFace.tsx";
import {
  aboutProgress,
  anchorDX,
  anchorDY,
  alarmAdd,
  alarmAddY,
  alarmCancel,
  alarmConfirmAdd,
  alarmDelete,
  alarmDialog,
  alarmDismiss,
  alarmRows,
  alarmScreen,
  alarmScroll,
  alarmToggle,
  badgeCancel,
  badgeDialog,
  badgeEdit,
  badgeEditing,
  badgeImage,
  badgeCanvas,
  badgeMessage,
  fftBars,
  fftHz,
  fftLabels,
  fftScale,
  fftTap,
  flowOffsets,
  fluidDots,
  imuBallSize,
  imuBallX,
  imuBallY,
  imuLabels,
  imuOrbitX,
  imuOrbitY,
  imuTap,
  imuTextX,
  imuTextY,
  imuTextZ,
  ringTime,
  ringing,
  rollerRows,
  rollerShift,
  setupConfirm,
  setupDateStage,
  setupLevel,
  setupOpen,
  setupScreen,
  setupScroll,
  setupSummary,
  setupToggleSfx,
  setupToggleVibrate,
  setupToggleWifi,
  setupVersion,
  sfxOn,
  vibrateOn,
  wheelConfirm,
  wheelLabels,
  wheelLeft,
  wheelPointer,
  wheelRight,
  wheelScreen,
  wheelTap,
  wifiOn,
  wifiStatus,
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
        <Match when={page() === "boot"}>
          <View class="absolute left-0 top-[201] w-[466] h-[36] items-center justify-center">
            <Text class="text-2xl text-white">StopWatch</Text>
          </View>
          <View class="absolute left-0 top-[237] w-[466] h-[22] items-center justify-center">
            <Text class="text-base text-[#BFBFBF]">Starting up ...</Text>
          </View>
          <View class="absolute left-0 top-[434] w-[466] h-[20] items-center justify-center">
            <Text class="text-sm text-[#8B8B8B]">V0.5</Text>
          </View>
        </Match>
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
            flow={flowOffsets()}
            dx={anchorDX()}
            dy={anchorDY()}
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
        <Match when={page() === "alarm"}>
          <AlarmClock
            screen={alarmScreen()}
            rows={alarmRows()}
            scroll={alarmScroll()}
            addY={alarmAddY()}
            dialog={alarmDialog()}
            rollerRows={rollerRows()}
            rollerShift={rollerShift()}
            onToggle={alarmToggle}
            onAdd={alarmAdd}
            onConfirmAdd={alarmConfirmAdd}
            onDelete={alarmDelete}
            onCancel={alarmCancel}
          />
        </Match>
        <Match when={page() === "setup"}>
          <Setup
            screen={setupScreen()}
            scroll={setupScroll()}
            level={setupLevel()}
            summary={setupSummary()}
            dateStage={setupDateStage()}
            sfx={sfxOn()}
            vibrate={vibrateOn()}
            wifi={wifiOn()}
            wifiStatus={wifiStatus()}
            progress={aboutProgress()}
            rollerRows={rollerRows()}
            rollerShift={rollerShift()}
            onOpen={setupOpen}
            onVersion={setupVersion}
            onConfirm={setupConfirm}
            onSfx={setupToggleSfx}
            onVibrate={setupToggleVibrate}
            onWifi={setupToggleWifi}
          />
        </Match>
        <Match when={page() === "wheel"}>
          <Wheel
            screen={wheelScreen()}
            rightHalf={wheelRight()}
            leftHalf={wheelLeft()}
            labels={wheelLabels()}
            pointer={wheelPointer()}
            rollerRows={rollerRows()}
            rollerShift={rollerShift()}
            onConfirm={wheelConfirm}
            onSpin={wheelTap}
          />
        </Match>
        <Match when={page() === "imu"}>
          <Imu
            ballLeft={imuBallX()}
            ballTop={imuBallY()}
            ballSize={imuBallSize()}
            orbitX={imuOrbitX()}
            orbitY={imuOrbitY()}
            labels={imuLabels()}
            x={imuTextX()}
            y={imuTextY()}
            z={imuTextZ()}
            onTap={imuTap}
          />
        </Match>
        <Match when={page() === "fft"}>
          <Fft bars={fftBars()} scale={fftScale()} labels={fftLabels()} hz={fftHz()} onTap={fftTap} />
        </Match>
        <Match when={page() === "badge"}>
          <Badge dialog={badgeDialog()} editing={badgeEditing()} image={badgeImage()} canvas={badgeCanvas()} message={badgeMessage()} onEdit={badgeEdit} onCancel={badgeCancel} />
        </Match>
        <Match when={page() === "fluid"}>
          <Fluid dots={fluidDots()} />
        </Match>
      </Switch>
      <Show when={ringing()}>
        <View class="absolute left-0 top-0 w-[466] h-[466] bg-black">
          <Image class="absolute left-[169] top-[90] w-[128] h-[32]" src="images/alarm_icon.png" />
          <View class="absolute left-0 top-[130] w-[466] h-[110] items-center justify-center">
            <Text class="text-5xl text-white">{ringTime()}</Text>
          </View>
          <View class="absolute left-[67] top-[283] w-[333] h-[103] rounded-[52] bg-[#4AD78C] items-center justify-center" focusable onPress={alarmDismiss}>
            <Text class="text-2xl text-[#0F5831]">OK</Text>
          </View>
        </View>
      </Show>
    </View>
  );
}
