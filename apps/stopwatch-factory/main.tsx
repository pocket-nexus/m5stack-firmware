import { mount } from "@pocketjs/framework/solid";
import { registerTexture } from "@pocketjs/framework";
import { connectDevice } from "../../platforms/esp-idf/m5_pocket_app/js/m5-bridge.ts";
import App from "./app.tsx";
import * as model from "./app.ts";

// JS mode only. The board uploads the active badge JPEG as a texture and
// publishes its handle as `badgeTexture`; the guest binds it to the image
// name the badge view uses before the model sees the new handle. The AOT
// crate does the same through Ui::register_image.
const host = {
  hostNumber(name: string, value: number): void {
    if (name === "badgeTexture" && value >= 0) {
      registerTexture("images/badge_slot.png", value);
    }
    model.hostNumber(name, value);
  },
  hostText: model.hostText,
  hostArray: model.hostArray,
  hostEvent: model.hostEvent,
  takeRequests: model.takeRequests,
};

mount(() => {
  connectDevice(host);
  return <App />;
});
