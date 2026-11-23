import { defineHostConfig } from "@pocketjs/framework/host-config";

export default defineHostConfig({
  host: "../../programs/stopwatch/pocket.host.json",
  app: {
    id: "com.m5stack.stopwatch.factory",
    title: "StopWatch",
    version: "0.1.0",
    entry: "main.tsx",
    output: "stopwatch-factory",
    framework: "solid",
    viewport: { fixed: { logical: [466, 466], presentation: "native" } },
    features: ["input.buttons", "input.touch", "text.glyphs.baked"],
    // The faces the font atlases are baked from (docs/FONTS.md).
    fonts: {
      regular: "fonts/Montserrat-Medium.ttf",
      bold: "fonts/Montserrat-SemiBold.ttf",
    },
    tickHz: 30,
    aot: true,
    model: "compiled",
  },
});
