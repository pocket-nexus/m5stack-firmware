import { defineHostConfig } from "@pocketjs/framework/host-config";

export default defineHostConfig({
  host: "../../programs/corep4x/pocket.host.json",
  app: {
    id: "com.m5stack.corep4x.factory",
    title: "M5Stack CoreP4X Factory Demo",
    version: "0.1.0",
    entry: "main.tsx",
    output: "corep4x-factory",
    framework: "solid",
    viewport: { fixed: { logical: [480, 480], presentation: "native" } },
    features: ["input.touch", "text.glyphs.baked"],
    // The faces the font atlases are baked from (docs/FONTS.md).
    fonts: {
      regular: "fonts/Montserrat-Medium.ttf",
      bold: "fonts/Montserrat-SemiBold.ttf",
    },
    tickHz: 60,
    aot: true,
    model: "compiled",
  },
});
