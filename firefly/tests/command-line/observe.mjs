/** Same observation runs on actual Electron and opt-in Firefly; no window is created. */
import { app } from 'electron';
const names = ['elytra-one', 'elytra-flag', 'elytra-empty', 'elytra-after', 'elytra-missing'];
console.log(
  JSON.stringify(
    names.map((name) => [
      name,
      app.commandLine.hasSwitch(name),
      app.commandLine.getSwitchValue(name)
    ])
  )
);
app.exit(0);
