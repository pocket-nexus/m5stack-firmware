import { defineHostConfig } from "@pocketjs/framework/host-config";

// The host contract belongs to the StickS3 program. The config sits beside the
// application because PocketJS resolves app.entry relative to the config file
// without `..` segments and registers the config's directory as the
// application directory for AOT model admission.
export default defineHostConfig({
  host: "../../programs/sticks3/pocket.host.json",
  app: {
    id: "com.m5stack.sticks3.demos",
    title: "StickS3 Demos",
    version: "0.2.0",
    entry: "main.tsx",
    output: "sticks3-demos",
    framework: "solid",
    viewport: { fixed: { logical: [135, 240], presentation: "native" } },
    features: ["input.buttons", "text.glyphs.baked"],
    tickHz: 30,
    aot: true,
    model: "compiled",
  },
});
