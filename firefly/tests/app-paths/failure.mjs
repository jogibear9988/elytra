/** A failed real profile open must preserve user files and never emit readiness. */
import { app } from 'firefly';
app.setPath('sessionData', process.argv[2]);

app.whenReady().then(() => console.log('UNEXPECTED_PROFILE_READY'));
