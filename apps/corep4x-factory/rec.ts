// REC window (ui_function_rec_window.cpp). The board captures the ES7210
// (MIC1, MIC2 and MIC3 as "REF"), records 10 s of raw 16 kHz TDM frames to
// SPIFFS /webspiffs/REC.PCM, plays the last recording back, and draws the
// display card (waveform charts or the scrolling spectrograms) into the
// texture "assets/rec-canvas.png"; the views add the labels and buttons.
// "Set" switches Waveform/Spectrum and shows or hides the REF row.
import type { Color, i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";

export const IDLE: i32 = 0;
export const RECORDING: i32 = 1;
export const RECORDED: i32 = 2;
export const PLAYING: i32 = 3;

export let spectrum: boolean = false;
export let showRef: boolean = false;
export let setOpen: boolean = false;

export function open(): void {
  spectrum = false;
  showRef = false;
  setOpen = false;
  device.request("rec 1");
}

export function close(): void {
  setOpen = false;
  device.request("rec 0");
}

function sendView(): void {
  device.request(`recView ${spectrum ? 1 : 0} ${showRef ? 1 : 0}`);
}

export function openSet(): void {
  setOpen = true;
}

export function closeSet(): void {
  setOpen = false;
}

export function setMode(toSpectrum: boolean): void {
  spectrum = toSpectrum;
  setOpen = false;
  sendView();
}

export function toggleRef(): void {
  showRef = !showRef;
  sendView();
}

export function setText(): string {
  return `Current: ${spectrum ? "Spectrum" : "Waveform"}\nWaveform is the default REC view.\nSpectrum stays blank until REC starts.\nREF is MIC3 and is hidden by default.`;
}

/** The board forces Waveform when a recording ends or playback starts. */
export function boardView(boardSpectrum: boolean): void {
  spectrum = boardSpectrum;
}

export function pressRec(): void {
  const state = device.recState;
  if (state === IDLE) {
    if (!device.recBusy) {
      device.request("recStart");
    }
  } else if (!device.recStopping) {
    // END REC: discard a recording in progress, delete the saved one.
    device.request("recEnd");
  }
}

export function pressPlay(): void {
  const state = device.recState;
  if (state === PLAYING || (state === IDLE && device.recPlayBusy)) {
    if (!device.recPlayStopping) {
      device.request("recPlayStop");
    }
  } else if (state === RECORDED && !device.recBusy) {
    device.request("recPlay");
  }
}

export interface Buttons {
  recText: string;
  recBg: Color;
  recFg: Color;
  recEnabled: boolean;
  playText: string;
  playBg: Color;
  playFg: Color;
  playEnabled: boolean;
}

/** set_record_buttons_enabled. */
export function buttons(): Buttons {
  const state = device.recState;
  const stopping = device.recStopping;
  const playStopping = device.recPlayStopping;
  if (state === RECORDING) {
    return {
      recText: stopping ? "ENDING..." : "END REC",
      recBg: stopping ? "#D97706" : "#DC2626",
      recFg: "#FFFFFF",
      recEnabled: !stopping,
      playText: "PLAY LAST",
      playBg: "#F3F4F6",
      playFg: "#111827",
      playEnabled: false,
    };
  }
  if (state === RECORDED) {
    return { recText: "END REC", recBg: "#DC2626", recFg: "#FFFFFF", recEnabled: true, playText: "PLAY LAST", playBg: "#2563EB", playFg: "#FFFFFF", playEnabled: !device.recBusy };
  }
  if (state === PLAYING) {
    return {
      recText: playStopping ? "ENDING..." : "END REC",
      recBg: playStopping ? "#D97706" : "#DC2626",
      recFg: "#FFFFFF",
      recEnabled: !playStopping,
      playText: "END PLAY",
      playBg: "#F59E0B",
      playFg: "#FFFFFF",
      playEnabled: !playStopping,
    };
  }
  if (device.recPlayBusy) {
    return { recText: "REC", recBg: "#F3F4F6", recFg: "#111827", recEnabled: false, playText: "END PLAY", playBg: "#F59E0B", playFg: "#FFFFFF", playEnabled: !playStopping };
  }
  return { recText: "REC", recBg: "#2563EB", recFg: "#FFFFFF", recEnabled: !device.recBusy, playText: "PLAY LAST", playBg: "#F3F4F6", playFg: "#111827", playEnabled: false };
}
