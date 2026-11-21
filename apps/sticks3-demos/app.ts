// Root model of the StickS3 demos. It owns every signal the views render;
// the state modules beside it hold each demo's logic in plain fields, and
// `frame` runs the visible page and publishes its results once per frame.
import { createSignal } from "solid-js";
import { copy, f32, f64, frames, idiv, round, type Deg, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";
import * as rng from "./rng";
import * as home from "./home";
import * as maze from "./maze";
import * as dice from "./dice";
import * as reaction from "./reaction";
import * as snake from "./snake";
import * as level from "./level";
import * as sound from "./sound";
import * as metronome from "./metronome";
import type { Box, Die, Page, Spot } from "./types";

/** Carousel order. */
const DEMOS: Page[] = ["maze", "dice", "reaction", "snake", "level", "sound", "metronome"];

const BRIGHT: i32 = 200;
const DIM: i32 = 24;
const DIM_AFTER_S: f64 = 30.0;
const VOLUME: i32 = 110;
/** The "hold FRONT for menu" hint shows this long after a demo opens. */
const HINT_MS: f64 = 2200.0;
const FRAME_MS: f64 = 33.333;

// ---------------------------------- shell -----------------------------------

export const [page, setPage] = createSignal<Page>("home");
export const [battery, setBattery] = createSignal<i32>(76);
export const [batteryFill, setBatteryFill] = createSignal<i32>(11);
export const [batteryLow, setBatteryLow] = createSignal<boolean>(false);
export const [charging, setCharging] = createSignal<boolean>(false);
/** Width of the front-key hold bar that leads home, px. */
export const [holdBar, setHoldBar] = createSignal<i32>(0);
export const [hint, setHint] = createSignal<boolean>(false);

// ---------------------------------- home ------------------------------------

export const [cardOffsets, setCardOffsets] = createSignal<i32[]>([0, 135, 270, 405, -405, -270, -135]);
export const [selected, setSelected] = createSignal<i32>(0);

// ---------------------------------- maze ------------------------------------

export const [mazeWalls, setMazeWalls] = createSignal<Box[]>([]);
export const [mazeTraps, setMazeTraps] = createSignal<Spot[]>([]);
export const [mazeBallX, setMazeBallX] = createSignal<i32>(0);
export const [mazeBallY, setMazeBallY] = createSignal<i32>(0);
export const [mazeBallScale, setMazeBallScale] = createSignal<f32>(1.0);
export const [mazeGoalX, setMazeGoalX] = createSignal<i32>(0);
export const [mazeGoalY, setMazeGoalY] = createSignal<i32>(0);
export const [mazeGoalScale, setMazeGoalScale] = createSignal<f32>(1.0);
export const [mazeTime, setMazeTime] = createSignal<string>("0.0");
export const [mazeLevel, setMazeLevel] = createSignal<string>("Level 1");
export const [mazeCleared, setMazeCleared] = createSignal<boolean>(false);
export const [mazeBest, setMazeBest] = createSignal<string>("--");
export const [mazeNewBest, setMazeNewBest] = createSignal<boolean>(false);

// ---------------------------------- dice ------------------------------------

export const [diceList, setDiceList] = createSignal<Die[]>([]);
export const [diceRolling, setDiceRolling] = createSignal<boolean>(false);
export const [diceTotal, setDiceTotal] = createSignal<string>("");
export const [diceDetail, setDiceDetail] = createSignal<string>("");
export const [diceCount, setDiceCount] = createSignal<string>("2 dice");

// -------------------------------- reaction ----------------------------------

export const [reactionState, setReactionState] = createSignal<i32>(0);
export const [reactionMs, setReactionMs] = createSignal<string>("0");
export const [reactionRating, setReactionRating] = createSignal<string>("");
export const [reactionBest, setReactionBest] = createSignal<string>("--");
export const [reactionAverage, setReactionAverage] = createSignal<string>("--");
export const [reactionNewBest, setReactionNewBest] = createSignal<boolean>(false);
export const [reactionDots, setReactionDots] = createSignal<i32>(0);
export const [reactionLong, setReactionLong] = createSignal<boolean>(false);

// --------------------------------- snake ------------------------------------

export const [snakeSegments, setSnakeSegments] = createSignal<Spot[]>([]);
export const [snakeFoodX, setSnakeFoodX] = createSignal<i32>(0);
export const [snakeFoodY, setSnakeFoodY] = createSignal<i32>(0);
export const [snakeFoodScale, setSnakeFoodScale] = createSignal<f32>(1.0);
export const [snakeState, setSnakeState] = createSignal<i32>(0);
export const [snakeScore, setSnakeScore] = createSignal<string>("4");
export const [snakeBest, setSnakeBest] = createSignal<string>("--");
export const [snakeVisible, setSnakeVisible] = createSignal<boolean>(true);
export const [snakeNewBest, setSnakeNewBest] = createSignal<boolean>(false);

// --------------------------------- level ------------------------------------

export const [levelFlat, setLevelFlat] = createSignal<boolean>(true);
export const [levelBubbleX, setLevelBubbleX] = createSignal<i32>(0);
export const [levelBubbleY, setLevelBubbleY] = createSignal<i32>(0);
export const [levelVialX, setLevelVialX] = createSignal<i32>(0);
export const [levelTextX, setLevelTextX] = createSignal<string>("+0.0°");
export const [levelTextY, setLevelTextY] = createSignal<string>("+0.0°");
export const [levelTextRoll, setLevelTextRoll] = createSignal<string>("+0.0°");
export const [levelRoll, setLevelRoll] = createSignal<Deg>(0.0);
export const [levelIsLevel, setLevelIsLevel] = createSignal<boolean>(false);
export const [levelZeroed, setLevelZeroed] = createSignal<boolean>(false);

// --------------------------------- sound ------------------------------------

export const [soundSegments, setSoundSegments] = createSignal<Box[]>([]);
export const [soundBars, setSoundBars] = createSignal<Box[]>([]);
export const [soundLevel, setSoundLevel] = createSignal<string>("--");
export const [soundPeak, setSoundPeak] = createSignal<string>("--");
export const [soundMax, setSoundMax] = createSignal<string>("--");
export const [soundSlow, setSoundSlow] = createSignal<boolean>(false);
export const [soundMicOn, setSoundMicOn] = createSignal<boolean>(false);

// ------------------------------- metronome ----------------------------------

export const [metroBpm, setMetroBpm] = createSignal<string>("100");
export const [metroTempo, setMetroTempo] = createSignal<string>("Andante");
export const [metroRunning, setMetroRunning] = createSignal<boolean>(false);
export const [metroBeat, setMetroBeat] = createSignal<i32>(-1);
export const [metroFlash, setMetroFlash] = createSignal<f32>(0.0);
export const [metroSwing, setMetroSwing] = createSignal<Deg>(0.0);
export const [metroWeight, setMetroWeight] = createSignal<i32>(76);

// --------------------------------- state ------------------------------------

let lastMs: f64 = 0.0;
let openedMs: f64 = 0.0;
let dimmed: boolean = false;
let wallsPublished: i32 = -1;
let snakePublished: i32 = -1;

function msText(ms: i32): string {
  return ms > 0 ? `${ms} ms` : "--";
}

function secondsText(ms: i32): string {
  return ms > 0 ? `${maze.seconds(f64(ms))} s` : "--";
}

// --------------------------------- pages ------------------------------------

function openDemo(target: Page, now: f64): void {
  if (target === "maze") {
    maze.open(now);
  } else if (target === "dice") {
    dice.open(now);
  } else if (target === "reaction") {
    reaction.open(now);
  } else if (target === "snake") {
    snake.open(now);
  } else if (target === "level") {
    level.open(now);
  } else if (target === "sound") {
    sound.open(now);
  } else {
    metronome.open(now);
  }
  wallsPublished = -1;
  snakePublished = -1;
  openedMs = now;
  setPage(target);
  input.swallow();
  device.tone(2637, 30);
}

function goHome(): void {
  const current = page();
  if (current === "sound") {
    sound.close();
  } else if (current === "metronome") {
    metronome.close();
  }
  home.wake();
  setPage("home");
  setHoldBar(0);
  input.swallow();
  device.tone(1319, 40);
}

function homeFrame(now: f64, dt: f64): void {
  if (input.anyKey) {
    home.wake();
  }
  if (input.sidePressed || input.sideRepeat) {
    home.next();
    device.tone(2093, 15);
  }
  home.update(dt);
  setCardOffsets(home.offsets());
  setSelected(home.selected);
  if (input.frontPressed) {
    openDemo(DEMOS[home.selected], now);
  }
}

// ------------------------------- publishing ---------------------------------

function publishMaze(): void {
  if (wallsPublished !== maze.generation) {
    wallsPublished = maze.generation;
    setMazeWalls(copy(maze.walls));
    setMazeTraps(copy(maze.traps));
    setMazeGoalX(round(maze.goalX));
    setMazeGoalY(round(maze.goalY));
  }
  setMazeBallX(round(maze.ballX));
  setMazeBallY(round(maze.ballY));
  setMazeBallScale(f32(maze.ballScale));
  setMazeGoalScale(f32(1.0 + 0.12 * (maze.pulse < 0.5 ? maze.pulse : 1.0 - maze.pulse)));
  setMazeTime(maze.seconds(maze.elapsedMs));
  setMazeLevel(`Level ${maze.level}`);
  setMazeCleared(maze.state === maze.CLEARED);
  setMazeBest(secondsText(maze.bestMs()));
  setMazeNewBest(maze.newBest);
}

function publishDice(): void {
  setDiceList(dice.dice());
  setDiceRolling(dice.rolling);
  setDiceCount(dice.count === 1 ? "1 die" : "2 dice");
  if (dice.rolls === 0) {
    setDiceTotal("");
    setDiceDetail("Shake or press");
  } else if (dice.rolling) {
    setDiceTotal("");
    setDiceDetail("");
  } else {
    setDiceTotal(`${dice.total}`);
    setDiceDetail(dice.count === 2 ? `${dice.values[0]} + ${dice.values[1]}` : "");
  }
}

function publishReaction(): void {
  setReactionState(reaction.state);
  setReactionMs(`${reaction.resultMs}`);
  setReactionLong(reaction.resultMs >= 1000);
  setReactionRating(reaction.rating(reaction.resultMs));
  setReactionBest(msText(reaction.bestMs()));
  setReactionAverage(msText(reaction.average()));
  setReactionNewBest(reaction.newBest);
  setReactionDots(reaction.dots);
}

function publishSnake(): void {
  if (snakePublished !== snake.version) {
    snakePublished = snake.version;
    setSnakeSegments(snake.segments());
  }
  setSnakeFoodX(snake.foodX());
  setSnakeFoodY(snake.foodY());
  setSnakeFoodScale(f32(0.8 + 0.25 * (snake.pulse < 0.5 ? snake.pulse : 1.0 - snake.pulse)));
  setSnakeState(snake.state);
  setSnakeScore(`${snake.length()}`);
  const best = snake.bestLength();
  setSnakeBest(best > 0 ? `${best}` : "--");
  setSnakeVisible(snake.visible);
  setSnakeNewBest(snake.newBest);
}

function publishLevel(): void {
  setLevelFlat(level.flat);
  setLevelBubbleX(level.bubbleX());
  setLevelBubbleY(level.bubbleY());
  setLevelVialX(level.vialX());
  setLevelTextX(level.degrees(level.angleX));
  setLevelTextY(level.degrees(level.angleY));
  setLevelTextRoll(level.degrees(level.roll));
  setLevelRoll(f32(level.roll) as Deg);
  setLevelIsLevel(level.isLevel);
  setLevelZeroed(level.zeroed);
}

function publishSound(): void {
  setSoundMicOn(device.micOn);
  setSoundSegments(sound.segments());
  setSoundBars(sound.bars());
  setSoundLevel(device.micOn ? sound.db(sound.levelDb) : "--");
  setSoundPeak(sound.db(sound.peakDb));
  setSoundMax(sound.db(sound.maxDb));
  setSoundSlow(sound.slow);
}

function publishMetronome(): void {
  setMetroBpm(`${metronome.bpm}`);
  setMetroTempo(metronome.tempoName());
  setMetroRunning(metronome.running);
  setMetroBeat(metronome.beat);
  setMetroFlash(f32(metronome.flash));
  setMetroSwing(f32(metronome.swing) as Deg);
  setMetroWeight(metronome.weightOffset());
}

function publishStatus(now: f64): void {
  const percent = device.batteryLevel < 0 ? 0 : device.batteryLevel;
  setBattery(percent);
  setBatteryFill(idiv(percent * 14 + 50, 100));
  setBatteryLow(percent <= 15 && !device.charging);
  setCharging(device.charging);
  setHoldBar(page() === "home" ? 0 : round(input.homeProgress() * 135.0));
  setHint(page() !== "home" && now - openedMs < HINT_MS);
}

// --------------------------------- frame ------------------------------------

function demoFrame(current: Page, now: f64, dt: f64): void {
  if (current === "maze") {
    maze.update(now, dt);
    publishMaze();
  } else if (current === "dice") {
    dice.update(now, dt);
    publishDice();
  } else if (current === "reaction") {
    reaction.update(now);
    publishReaction();
  } else if (current === "snake") {
    snake.update(now, dt);
    publishSnake();
  } else if (current === "level") {
    level.update(now, dt);
    publishLevel();
  } else if (current === "sound") {
    sound.update(now, dt);
    publishSound();
  } else {
    metronome.update(now, dt);
    publishMetronome();
  }
}

function frame(step: i32): void {
  if (!device.bridged) {
    device.advance(FRAME_MS);
  }
  const now = device.uptimeMs;
  let dt = (now - lastMs) / 1000.0;
  if (step === 0 || dt < 0.0 || dt > 0.1) {
    dt = FRAME_MS / 1000.0;
  }
  lastMs = now;
  if (step === 0) {
    rng.seed(device.seed);
  }
  input.update(now);
  if (input.anyKey) {
    rng.stir(now);
  }
  const current = page();
  if (current === "home") {
    homeFrame(now, dt);
  } else if (input.homeRequested) {
    goHome();
  } else {
    demoFrame(current, now, dt);
  }
  // Dim the backlight on an idle home screen; any key restores it.
  const idle = page() === "home" && home.idle > DIM_AFTER_S;
  if (idle !== dimmed) {
    dimmed = idle;
    device.setBrightness(idle ? DIM : BRIGHT);
  }
  publishStatus(now);
}

export async function run(): Promise<void> {
  for (let step: i32 = 0; step < 2147483647; step++) {
    // The board's values for a frame arrive before the frame boundary.
    await frames(1);
    frame(step);
  }
}

export function start(): void {
  device.setBrightness(BRIGHT);
  device.setVolume(VOLUME);
  device.tone(2093, 40);
  run();
}

// Root exports that main.tsx (JS mode) uses to reach device.ts.
export function hostNumber(name: string, value: f64): void {
  device.hostNumber(name, value);
}

export function hostText(name: string, value: string): void {
  device.hostText(name, value);
}

export function hostArray(name: string, values: f32[]): void {
  device.hostArray(name, values);
}

export function hostEvent(name: string, value: f64): void {
  device.hostEvent(name, value);
}

export function takeRequests(): string[] {
  return device.takeRequests();
}
