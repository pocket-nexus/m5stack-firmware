import { Match, Show, Switch } from "solid-js";
import { Text, View } from "@pocketjs/framework/solid/components";
import { onMount } from "@pocketjs/framework/solid/lifecycle";
import DiceView from "./DiceView.tsx";
import HomeView from "./HomeView.tsx";
import LevelView from "./LevelView.tsx";
import MazeView from "./MazeView.tsx";
import MetronomeView from "./MetronomeView.tsx";
import ReactionView from "./ReactionView.tsx";
import SnakeView from "./SnakeView.tsx";
import SoundView from "./SoundView.tsx";
import {
  battery,
  batteryFill,
  batteryLow,
  cardOffsets,
  charging,
  diceCount,
  diceDetail,
  diceList,
  diceRolling,
  diceTotal,
  hint,
  holdBar,
  levelBubbleX,
  levelBubbleY,
  levelFlat,
  levelIsLevel,
  levelRoll,
  levelTextRoll,
  levelTextX,
  levelTextY,
  levelVialX,
  levelZeroed,
  mazeBallScale,
  mazeBallX,
  mazeBallY,
  mazeBest,
  mazeCleared,
  mazeGoalScale,
  mazeGoalX,
  mazeGoalY,
  mazeLevel,
  mazeNewBest,
  mazeTime,
  mazeTraps,
  mazeWalls,
  metroBeat,
  metroBpm,
  metroFlash,
  metroRunning,
  metroSwing,
  metroTempo,
  metroWeight,
  page,
  reactionAverage,
  reactionBest,
  reactionDots,
  reactionLong,
  reactionMs,
  reactionNewBest,
  reactionRating,
  reactionState,
  selected,
  snakeBest,
  snakeFoodScale,
  snakeFoodX,
  snakeFoodY,
  snakeNewBest,
  snakeScore,
  snakeSegments,
  snakeState,
  snakeVisible,
  soundBars,
  soundLevel,
  soundMax,
  soundMicOn,
  soundPeak,
  soundSegments,
  soundSlow,
  start,
} from "./app";

// The keys reach the model through device.ts (keyFront/keySide and their
// interrupt timestamps), so the views bind no ActionHandler. The bar at the
// top fills while the front key is held towards the home screen.
export default function App() {
  onMount(start);
  return (
    <View debugName="StickS3Demos" class="w-full h-full bg-[#0B0E13] overflow-hidden">
      <Switch>
        <Match when={page() === "home"}>
          <HomeView offsets={cardOffsets()} selected={selected()} percent={battery()} fill={batteryFill()} low={batteryLow()} charging={charging()} />
        </Match>
        <Match when={page() === "maze"}>
          <MazeView
            walls={mazeWalls()}
            traps={mazeTraps()}
            ballX={mazeBallX()}
            ballY={mazeBallY()}
            ballScale={mazeBallScale()}
            goalX={mazeGoalX()}
            goalY={mazeGoalY()}
            goalScale={mazeGoalScale()}
            time={mazeTime()}
            level={mazeLevel()}
            cleared={mazeCleared()}
            best={mazeBest()}
            newBest={mazeNewBest()}
          />
        </Match>
        <Match when={page() === "dice"}>
          <DiceView dice={diceList()} total={diceTotal()} detail={diceDetail()} count={diceCount()} rolling={diceRolling()} />
        </Match>
        <Match when={page() === "reaction"}>
          <ReactionView
            state={reactionState()}
            ms={reactionMs()}
            long={reactionLong()}
            rating={reactionRating()}
            best={reactionBest()}
            average={reactionAverage()}
            newBest={reactionNewBest()}
            dots={reactionDots()}
          />
        </Match>
        <Match when={page() === "snake"}>
          <SnakeView
            segments={snakeSegments()}
            foodX={snakeFoodX()}
            foodY={snakeFoodY()}
            foodScale={snakeFoodScale()}
            state={snakeState()}
            score={snakeScore()}
            best={snakeBest()}
            visible={snakeVisible()}
            newBest={snakeNewBest()}
          />
        </Match>
        <Match when={page() === "level"}>
          <LevelView
            flat={levelFlat()}
            bubbleX={levelBubbleX()}
            bubbleY={levelBubbleY()}
            vialX={levelVialX()}
            textX={levelTextX()}
            textY={levelTextY()}
            textRoll={levelTextRoll()}
            roll={levelRoll()}
            level={levelIsLevel()}
            zeroed={levelZeroed()}
          />
        </Match>
        <Match when={page() === "sound"}>
          <SoundView
            segments={soundSegments()}
            bars={soundBars()}
            level={soundLevel()}
            peak={soundPeak()}
            max={soundMax()}
            slow={soundSlow()}
            micOn={soundMicOn()}
          />
        </Match>
        <Match when={page() === "metronome"}>
          <MetronomeView
            bpm={metroBpm()}
            tempo={metroTempo()}
            running={metroRunning()}
            beat={metroBeat()}
            flash={metroFlash()}
            swing={metroSwing()}
            weight={metroWeight()}
          />
        </Match>
      </Switch>
      <View class="absolute left-0 top-0 h-[3] bg-[#F8FAFC]" style={{ width: holdBar() }} />
      <Show when={hint()}>
        <View class="absolute left-[12] top-[216] w-[111] h-[18] rounded-full bg-[#1E2530] items-center justify-center">
          <Text class="text-2xs text-[#C7CDD6]">Hold FRONT for menu</Text>
        </View>
      </Show>
    </View>
  );
}
