import { defineHostConfig } from "@pocketjs/framework/host-config";

export default defineHostConfig({
  host: "../pocket.host.json",
  app: {
    id: "dev.m5stack-firmware.aot-smoke",
    title: "M5 AOT smoke",
    version: "0.1.0",
    entry: "main.tsx",
    output: "aot-smoke",
    framework: "solid",
    viewport: { logical: [240, 135], presentation: "native" },
    features: ["input.buttons", "input.touch", "text.glyphs.baked"],
    aot: true,
    model: "compiled",
  },
});
