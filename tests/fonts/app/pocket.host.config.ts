import { defineHostConfig } from "@pocketjs/framework/host-config";

export default defineHostConfig({
  host: "../pocket.host.json",
  app: {
    id: "dev.m5stack-firmware.fonts-test",
    title: "M5 fonts test",
    version: "0.1.0",
    entry: "main.tsx",
    output: "fonts-test",
    framework: "solid",
    viewport: { logical: [240, 135], presentation: "native" },
    features: ["input.buttons", "text.glyphs.baked"],
    aot: true,
    model: "compiled",
  },
});
