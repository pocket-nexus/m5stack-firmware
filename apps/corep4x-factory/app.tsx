// The CoreP4X factory demo views: 480x480 logical pixels at raster density 1,
// the factory's LVGL screen coordinates. Each screen is its factory picture
// (padded to a 512x512 texture and clipped to the screen); the function
// screen adds its 14 touch areas, and a function window draws over it.
import { Show } from "solid-js";
import { For, Image, View } from "@pocketjs/framework/solid/components";
import { onMount } from "@pocketjs/framework/solid/lifecycle";
import Dialog from "./Dialog.tsx";
import GpioPopup from "./GpioPopup.tsx";
import I2cView from "./I2cView.tsx";
import IrView from "./IrView.tsx";
import ImuView from "./ImuView.tsx";
import MusicView from "./MusicView.tsx";
import OffView from "./OffView.tsx";
import PingView from "./PingView.tsx";
import PowerView from "./PowerView.tsx";
import RecSetDialog from "./RecSetDialog.tsx";
import RecView from "./RecView.tsx";
import SdView from "./SdView.tsx";
import TimerView from "./TimerView.tsx";
import TouchCanvas from "./TouchCanvas.tsx";
import TouchView from "./TouchView.tsx";
import UsbView from "./UsbView.tsx";
import WifiView from "./WifiView.tsx";
import SleepView from "./SleepView.tsx";
import WindowFrame from "./WindowFrame.tsx";
import {
  actionBg,
  actionEnabled,
  actionIcon,
  actionFg,
  actionPlaceholder,
  actionShown,
  actionText,
  dialogButton,
  dialogButtons,
  dialogHeight,
  dialogOn,
  dialogTop,
  dialogText,
  dialogTitle,
  exitWindow,
  gpioApply,
  gpioClose,
  gpioHigh,
  gpioOpen,
  gpioOpenDrain,
  gpioOutput,
  gpioPull,
  gpioSetDrive,
  gpioSetLevel,
  gpioSetMode,
  gpioSetPull,
  gpioStatus,
  gpioTitle,
  powerBacklight,
  powerCharge,
  powerIndicator,
  powerKnob,
  powerPin,
  powerPins,
  powerPinDots,
  powerPinOutputs,
  powerRails,
  powerSetCharge,
  powerToggle,
  powerVbat,
  functionOn,
  functionX,
  i2cBubbles,
  i2cScan,
  i2cSummary,
  i2cSummaryColor,
  imuHidden,
  imuMarker0X,
  imuMarker0Y,
  imuMarker1X,
  imuMarker1Y,
  imuMarkerCount,
  imuOverlay,
  imuOverlayError,
  imuOverlayText,
  imuTextureRow,
  indicatorOn,
  indicatorX,
  indicatorY,
  infoOn,
  irAddressText,
  irAuto,
  irAutoEnabled,
  irBytes,
  irClear,
  irCommandText,
  irFramesText,
  irPressed,
  irRaw,
  irRepeatOn,
  irRepeatText,
  irResultColor,
  irResultText,
  irSend,
  irSendEnabled,
  irShown,
  irStatus,
  irStatusColor,
  irToggleAuto,
  irToggleRepeat,
  musicBarOpacity,
  musicBars,
  musicHeights,
  musicButtonBg,
  musicButtonEnabled,
  musicButtonFg,
  musicButtonText,
  musicDuration,
  musicElapsed,
  musicIndicator,
  musicKnob,
  musicPlaying,
  musicPress,
  musicProgress,
  musicSliderEnabled,
  musicStatus,
  musicStatusColor,
  musicVolume,
  infoX,
  offAction,
  offActionColor,
  offBar,
  offBarColor,
  offPower,
  offPowerBg,
  offPowerEnabled,
  offPowerText,
  offRestart,
  offRestartBg,
  offRestartEnabled,
  offRestartText,
  offSound,
  offSoundOn,
  offStatus,
  offVersion,
  pingEnabled,
  pingEthDetail,
  pingEthDot,
  pingEthText,
  pingSummary,
  pingSummaryColor,
  pingTargets,
  pingToggle,
  pressArea,
  recBg,
  recCloseSet,
  recDb,
  recEnabled,
  recFg,
  recPlayBg,
  recPlayEnabled,
  recPlayFg,
  recPlayText,
  recPressPlay,
  recPressRec,
  recSetOn,
  recSetText,
  recShowRef,
  recSpectrum,
  recSpectrumMode,
  recText,
  recTextureRow,
  recToggleRef,
  recWaveform,
  sdBack,
  sdGoBack,
  sdPath,
  sdRow,
  sdRows,
  sdSize,
  sdStatusText,
  selectTab,
  sleepButton,
  sleepDeep,
  sleepEnabled,
  sleepImu,
  sleepResult,
  sleepResultColor,
  sleepRtc,
  sleepSeconds,
  sleepSetSeconds,
  sleepStatus,
  sleepTouch,
  sleepToggleButton,
  sleepToggleImu,
  sleepToggleRtc,
  sleepToggleTouch,
  start,
  startupOn,
  startupX,
  timerEnabled,
  usbResult,
  touchCanvasOn,
  touchClean,
  touchComplete,
  touchExit,
  touchIndicatorOn,
  touchInfo,
  touchPill,
  touchPillDarkOn,
  touchStatusText,
  touchTextureRow,
  touchToggleIndicator,
  timerSave,
  timerValues,
  win,
  winHeight,
  winLeft,
  winOpacity,
  winOutline,
  winPages,
  winPlaceholder,
  winTab,
  winTabEdges,
  winTabs,
  winTop,
  winWidth,
  windowAction,
} from "./app";

export default function App() {
  onMount(start);
  return (
    <View debugName="CoreP4XFactory" class="w-full h-full overflow-hidden bg-[#F5F5F5]">
      <Show when={infoOn()}>
        <View class="absolute top-0 w-[480] h-[480] overflow-hidden" style={{ insetL: infoX() }}>
          <Image class="absolute left-0 top-0 w-[512] h-[512]" src="assets/info.png" />
        </View>
      </Show>
      <Show when={startupOn()}>
        <View class="absolute top-0 w-[480] h-[480] overflow-hidden" style={{ insetL: startupX() }}>
          <Image class="absolute left-0 top-0 w-[512] h-[512]" src="assets/startup.png" />
        </View>
      </Show>
      <Show when={functionOn()}>
        <View debugName="Function" class="absolute top-0 w-[480] h-[480] overflow-hidden" style={{ insetL: functionX() }}>
          <Image class="absolute left-0 top-0 w-[512] h-[512]" src="assets/function.png" />
          <View class="absolute left-[290] top-[2] w-[80] h-[55]" focusable onPress={() => pressArea(12)} />
          <View class="absolute left-[398] top-[2] w-[80] h-[55]" focusable onPress={() => pressArea(13)} />
          <View class="absolute left-[15] top-[60] w-[100] h-[100]" focusable onPress={() => pressArea(0)} />
          <View class="absolute left-[130] top-[60] w-[100] h-[100]" focusable onPress={() => pressArea(1)} />
          <View class="absolute left-[245] top-[60] w-[100] h-[100]" focusable onPress={() => pressArea(2)} />
          <View class="absolute left-[360] top-[60] w-[100] h-[100]" focusable onPress={() => pressArea(3)} />
          <View class="absolute left-[15] top-[195] w-[100] h-[100]" focusable onPress={() => pressArea(4)} />
          <View class="absolute left-[130] top-[195] w-[100] h-[100]" focusable onPress={() => pressArea(5)} />
          <View class="absolute left-[245] top-[195] w-[100] h-[100]" focusable onPress={() => pressArea(6)} />
          <View class="absolute left-[360] top-[195] w-[100] h-[100]" focusable onPress={() => pressArea(7)} />
          <View class="absolute left-[15] top-[330] w-[100] h-[100]" focusable onPress={() => pressArea(8)} />
          <View class="absolute left-[130] top-[330] w-[100] h-[100]" focusable onPress={() => pressArea(9)} />
          <View class="absolute left-[245] top-[330] w-[100] h-[100]" focusable onPress={() => pressArea(10)} />
          <View class="absolute left-[360] top-[330] w-[100] h-[100]" focusable onPress={() => pressArea(11)} />
        </View>
      </Show>
      <Show when={win() >= 0}>
        {/* The backdrop is transparent and takes every touch: the function
            screen under it stays visible and inert. */}
        <View class="absolute left-0 top-0 w-[480] h-[480]" focusable onPress={() => {}} />
        <WindowFrame
          left={winLeft()}
          top={winTop()}
          width={winWidth()}
          height={winHeight()}
          opacity={winOpacity()}
          pages={winPages()}
          tabs={winTabs()}
          tab={winTab()}
          outline={winOutline()}
          tabEdges={winTabEdges()}
          placeholder={winPlaceholder()}
          actionShown={actionShown()}
          actionPlaceholder={actionPlaceholder()}
          actionText={actionText()}
          actionIcon={actionIcon()}
          actionBg={actionBg()}
          actionFg={actionFg()}
          actionEnabled={actionEnabled()}
          onAction={windowAction}
          onExit={exitWindow}
          onTab={selectTab}
        >
          <Show when={win() === 13}>
            <OffView
              status={offStatus()}
              version={offVersion()}
              action={offAction()}
              actionColor={offActionColor()}
              soundOn={offSoundOn()}
              soundEnabled={true}
              powerText={offPowerText()}
              powerBg={offPowerBg()}
              powerEnabled={offPowerEnabled()}
              restartText={offRestartText()}
              restartBg={offRestartBg()}
              restartEnabled={offRestartEnabled()}
              bar={offBar()}
              barColor={offBarColor()}
              onSound={offSound}
              onPowerOff={offPower}
              onRestart={offRestart}
            />
          </Show>
          <Show when={win() === 0}>
            <PowerView
              tab={winTab()}
              rails={powerRails()}
              backlight={powerBacklight()}
              knob={powerKnob()}
              indicator={powerIndicator()}
              vbat={powerVbat()}
              charge={powerCharge()}
              pins={powerPins()}
              pinDots={powerPinDots()}
              pinOutputs={powerPinOutputs()}
              onToggle={powerToggle}
              onCharge={powerSetCharge}
              onPin={powerPin}
            />
          </Show>
          <Show when={win() === 1}>
            <TouchView status={touchStatusText()} indicator={touchIndicatorOn()} onIndicator={touchToggleIndicator} />
          </Show>
          <Show when={win() === 4}>
            <ImuView
              hidden={imuHidden()}
              texture={imuTextureRow()}
              overlay={imuOverlay()}
              overlayText={imuOverlayText()}
              overlayError={imuOverlayError()}
              markers={imuMarkerCount()}
              marker0X={imuMarker0X()}
              marker0Y={imuMarker0Y()}
              marker1X={imuMarker1X()}
              marker1Y={imuMarker1Y()}
            />
          </Show>
          <Show when={win() === 2}>
            <SdView path={sdPath()} rows={sdRows()} backEnabled={sdBack()} size={sdSize()} status={sdStatusText()} onRow={sdRow} onBack={sdGoBack} />
          </Show>
          <Show when={win() === 3}>
            <I2cView summary={i2cSummary()} summaryColor={i2cSummaryColor()} bubbles={i2cBubbles()} onScan={i2cScan} />
          </Show>
          <Show when={win() === 5}>
            <RecView
              texture={recTextureRow()}
              spectrum={recSpectrum()}
              showRef={recShowRef()}
              db={recDb()}
              recText={recText()}
              recBg={recBg()}
              recFg={recFg()}
              recEnabled={recEnabled()}
              playText={recPlayText()}
              playBg={recPlayBg()}
              playFg={recPlayFg()}
              playEnabled={recPlayEnabled()}
              onRec={recPressRec}
              onPlay={recPressPlay}
            />
          </Show>
          <Show when={win() === 11}>
            <MusicView
              bars={musicBars()}
              heights={musicHeights()}
              playing={musicPlaying()}
              progress={musicProgress()}
              elapsed={musicElapsed()}
              duration={musicDuration()}
              status={musicStatus()}
              statusColor={musicStatusColor()}
              buttonText={musicButtonText()}
              buttonBg={musicButtonBg()}
              buttonFg={musicButtonFg()}
              buttonEnabled={musicButtonEnabled()}
              volume={musicVolume()}
              knob={musicKnob()}
              indicator={musicIndicator()}
              sliderEnabled={musicSliderEnabled()}
              barOpacity={musicBarOpacity()}
              onPlay={musicPress}
            />
          </Show>
          <Show when={win() === 6}>
            <PingView
              ethText={pingEthText()}
              ethDot={pingEthDot()}
              ethDetail={pingEthDetail()}
              targets={pingTargets()}
              enabled={pingEnabled()}
              summary={pingSummary()}
              summaryColor={pingSummaryColor()}
              onToggle={pingToggle}
            />
          </Show>
          <Show when={win() === 8}>
            <WifiView />
          </Show>
          <Show when={win() === 9}>
            <UsbView result={usbResult()} />
          </Show>
          <Show when={win() === 10}>
            <IrView
              status={irStatus()}
              statusColor={irStatusColor()}
              result={irResultText()}
              resultColor={irResultColor()}
              bytes={irBytes()}
              pressed={irPressed()}
              autoInc={irAuto()}
              repeat={irRepeatOn()}
              sendEnabled={irSendEnabled()}
              autoEnabled={irAutoEnabled()}
              frames={irFramesText()}
              shown={irShown()}
              repeatText={irRepeatText()}
              address={irAddressText()}
              command={irCommandText()}
              raw={irRaw()}
              onAuto={irToggleAuto}
              onRepeat={irToggleRepeat}
              onSend={irSend}
              onClear={irClear}
            />
          </Show>
          <Show when={win() === 7}>
            <TimerView
              year={timerValues()[0] ?? ""}
              month={timerValues()[1] ?? ""}
              day={timerValues()[2] ?? ""}
              hour={timerValues()[3] ?? ""}
              minute={timerValues()[4] ?? ""}
              second={timerValues()[5] ?? ""}
              enabled={timerEnabled()}
              onSaveDate={timerSave}
              onSaveTime={timerSave}
            />
          </Show>
          <Show when={win() === 12}>
            <SleepView
              status={sleepStatus()}
              result={sleepResult()}
              resultColor={sleepResultColor()}
              buttonSleep={sleepButton()}
              rtcWake={sleepRtc()}
              rtcSeconds={sleepSeconds()}
              touchWake={sleepTouch()}
              imuWake={sleepImu()}
              enabled={sleepEnabled()}
              onButtonSleep={sleepToggleButton}
              onRtc={sleepToggleRtc}
              onSeconds={sleepSetSeconds}
              onTouch={sleepToggleTouch}
              onImu={sleepToggleImu}
              onDeepSleep={sleepDeep}
            />
          </Show>
        </WindowFrame>
      </Show>
      <Show when={touchCanvasOn()}>
        <TouchCanvas
          texture={touchTextureRow()}
          pill={touchPill()}
          pillDark={touchPillDarkOn()}
          complete={touchComplete()}
          onInfo={touchInfo}
          onClean={touchClean}
          onExit={touchExit}
        />
      </Show>
      <Show when={recSetOn()}>
        <RecSetDialog
          text={recSetText()}
          spectrum={recSpectrum()}
          showRef={recShowRef()}
          onRef={recToggleRef}
          onWaveform={recWaveform}
          onSpectrum={recSpectrumMode}
          onClose={recCloseSet}
        />
      </Show>
      <Show when={gpioOpen()}>
        <GpioPopup
          title={gpioTitle()}
          status={gpioStatus()}
          output={gpioOutput()}
          level={gpioHigh()}
          openDrain={gpioOpenDrain()}
          pull={gpioPull()}
          onMode={gpioSetMode}
          onLevel={gpioSetLevel}
          onDrive={gpioSetDrive}
          onPull={gpioSetPull}
          onApply={gpioApply}
          onClose={gpioClose}
        />
      </Show>
      <Show when={dialogOn()}>
        <Dialog title={dialogTitle()} text={dialogText()} top={dialogTop()} height={dialogHeight()} buttons={dialogButtons()} onButton={dialogButton} />
      </Show>
      <Show when={indicatorOn()}>
        <View class="absolute w-[8] h-[8] rounded-[4] border-[1] border-[#0000FF]" style={{ insetL: indicatorX(), insetT: indicatorY() }} />
      </Show>
    </View>
  );
}
