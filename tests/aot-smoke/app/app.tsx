import { ActionHandler, Text, View } from "@pocketjs/framework/solid/components";
import { BTN } from "@pocketjs/framework/input";
import { count, increment, pointer, status, tap, taps } from "./app";

export default function App() {
  return (
    <View class="w-full h-full bg-slate-950 flex-col items-center justify-center gap-2">
      <ActionHandler button={BTN.CROSS} onPress={() => increment()} />
      <Text class="text-sm text-white font-bold">Count {count()}</Text>
      <View debugName="TapTarget" class="w-40 h-10 rounded-lg bg-blue-600 items-center justify-center focus:bg-blue-500 active:bg-blue-700" focusable onPress={() => tap()}>
        <Text class="text-sm text-white">Taps {taps()}</Text>
      </View>
      <Text class="text-xs text-slate-300">{status()}</Text>
      <Text class="text-xs text-slate-400">{pointer()}</Text>
    </View>
  );
}
