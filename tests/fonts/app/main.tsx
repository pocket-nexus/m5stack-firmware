// JS mode entry. AOT compiles App, not this file.
import { mount } from "@pocketjs/framework/solid";
import App from "./app.tsx";

mount(() => <App />);
