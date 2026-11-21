import { Text, View } from "@pocketjs/framework/solid/components";
import { lapText } from "./app";

// One text per face: the 16 px regular slot, the 24 px bold slot and the
// 14 px mono slot. ASCII only, so both compile modes bake the same charset.
export default function App() {
  return (
    <View class="w-full h-full bg-black flex-col items-center justify-center gap-2">
      <Text class="text-base text-white">Regular 0123456789</Text>
      <Text class="text-2xl font-bold text-white">{lapText()}</Text>
      <Text class="text-sm font-mono text-white">mono 00:00.00</Text>
    </View>
  );
}
