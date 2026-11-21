import { mount } from "@pocketjs/framework/solid";
import { connectDevice } from "../../platforms/esp-idf/m5_pocket_app/js/m5-bridge.ts";
import App from "./app.tsx";
import * as host from "./app.ts";

// JS mode only. connectDevice also binds the textures the board sets with
// m5_pocket_app_set_texture (the badge picture, images/badge_slot.png).
mount(() => {
  connectDevice(host);
  return <App />;
});
