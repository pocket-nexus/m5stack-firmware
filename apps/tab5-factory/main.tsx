import { mount } from "@pocketjs/framework/solid";
import { connectDevice } from "../../platforms/esp-idf/m5_pocket_app/js/m5-bridge.ts";
import App from "./app.tsx";
import * as host from "./app.ts";

mount(() => {
  connectDevice(host);
  return <App />;
});
