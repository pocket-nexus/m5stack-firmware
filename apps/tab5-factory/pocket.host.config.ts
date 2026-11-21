import { defineHostConfig } from "@pocketjs/framework/host-config";

export default defineHostConfig({
  host: "../../programs/tab5/pocket.host.json",
  app: {
    id: "com.m5stack.tab5.factory",
    title: "M5Stack Tab5 Factory Demo",
    version: "0.1.0",
    entry: "main.tsx",
    output: "tab5-factory",
    framework: "solid",
    viewport: { fixed: { logical: [640, 360], presentation: "native" } },
    features: ["input.touch", "text.glyphs.baked"],
    tickHz: 30,
    aot: true,
    model: "compiled",
  },
});
