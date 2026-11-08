/** Public main-host export acceptance; no BrowserWindow or desktop renderer is created. */
import firefly, { app, protocol, session } from 'firefly';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

try {
  const alias = await import('electron');
  check(
    protocol === firefly.protocol &&
      alias.protocol === protocol &&
      require('electron').protocol === protocol,
    'ESM/CommonJS singleton exports'
  );
  check(alias.session === session && require('electron').session === session, 'session aliases');
  check(!app.isReady(), 'registration must run before ready');
  let early = false;
  try {
    protocol.handle('file', () => Response.error());
  } catch (error) {
    early = error.code === 'ERR_FIREFLY_NOT_READY';
  }
  check(early, 'modern handlers must wait for readiness');
  protocol.registerSchemesAsPrivileged([
    { scheme: 'app', privileges: { standard: true, secure: true } }
  ]);
  let repeated = false;
  try {
    protocol.registerSchemesAsPrivileged([]);
  } catch (error) {
    repeated = error.code === 'ERR_FIREFLY_PROTOCOL_ALREADY_REGISTERED';
  }
  check(repeated, 'second registration must fail');
  await app.whenReady();
  let late = false;
  try {
    protocol.registerSchemesAsPrivileged([]);
  } catch (error) {
    late = error.code === 'ERR_FIREFLY_PROTOCOL_TOO_LATE';
  }
  check(late, 'ready registration must fail');
  check(
    typeof protocol.handle === 'function' &&
      typeof protocol.unhandle === 'function' &&
      typeof protocol.registerFileProtocol === 'function',
    'modern and legacy callback APIs must be present'
  );

  protocol.handle('file', () => Response.error());

  check(session.defaultSession.protocol.isProtocolHandled('file'), 'shared file interception');
  protocol.unhandle('file');
  check(!protocol.isProtocolHandled('file'), 'interception removed through same singleton');
  console.log('PASS actual Firefly main protocol exports/registration/readiness without a window');
  app.exit(0);
} catch (error) {
  console.error(error);
  app.exit(1);
}
