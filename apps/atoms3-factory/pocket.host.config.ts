import { defineHostConfig } from "@pocketjs/framework/host-config";

// The AtomS3 factory demo (M5AtomS3-UserDemo). The host contract is the
// program's; this config sits beside the sources so the entry and the AOT
// model check stay inside the application directory (docs/APP-HOST.md).
export default defineHostConfig({
  host: "../../programs/atoms3/pocket.host.json",
  app: {
    id: "com.m5stack.atoms3.factory",
    title: "AtomS3 Factory Demo",
    version: "0.1.0",
    entry: "main.tsx",
    output: "atoms3-factory",
    framework: "solid",
    viewport: { logical: [128, 128], presentation: "native" },
    features: ["input.buttons", "text.glyphs.baked"],
    aot: true,
    model: "compiled",
    tickHz: 30,
  },
});
