/** Unchanged Code-OSS classes, real protocol/session/net; controlled environment/log/remote peers. */
import { app, net, protocol, session } from 'electron';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

protocol.registerSchemesAsPrivileged([
  {
    scheme: 'vscode-file',
    privileges: { standard: true, secure: true, supportFetchAPI: true, corsEnabled: true }
  },
  {
    scheme: 'vscode-managed-remote-resource',
    privileges: { standard: true, secure: true, supportFetchAPI: true, corsEnabled: true }
  }
]);

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

app.whenReady().then(async () => {
  const timeout = setTimeout(() => {
    console.error('Code-OSS protocol consumer timed out');
    app.exit(1);
  }, 15000);
  let service;
  try {
    await import(process.env.ELYTRA_PROTOCOL_NLS);
    const { ProtocolMainService } = await import(process.env.ELYTRA_PROTOCOL_CONSUMER);
    const { URI } = await import(process.env.ELYTRA_PROTOCOL_URI);
    const directory = process.env.ELYTRA_PROTOCOL_FIXTURE;
    const logs = { errors: [], warnings: [], traces: [] };
    const log = {
      error: (...args) => logs.errors.push(args),

      warn: (...args) => logs.warnings.push(args),

      trace: (...args) => logs.traces.push(args)
    };

    const fileURI = (name) => URI.parse(pathToFileURL(path.join(directory, name)).href);

    service = new ProtocolMainService(
      {
        appRoot: path.join(directory, 'app'),
        extensionsPath: path.join(directory, 'extensions'),
        workspaceStorageHome: fileURI('workspace'),
        crossOriginIsolated: true,
        isBuilt: false
      },
      { defaultProfile: { globalStorageHome: fileURI('global') } },
      log
    );
    check(
      await session.defaultSession.protocol.isProtocolHandled('vscode-file'),
      'actual legacy route registration'
    );
    check(
      await session.defaultSession.protocol.isProtocolHandled('file'),
      'actual modern file route registration'
    );

    let refused = false;
    try {
      await net.fetch(process.env.ELYTRA_PROTOCOL_SECRET);
    } catch {
      refused = true;
    }
    check(
      refused &&
        logs.errors.length === 1 &&
        String(logs.errors[0][0]).includes('Refused to load resource'),
      'actual Code-OSS Response.error file denial must reject genuine net.fetch'
    );

    let calls = 0;
    const expected = new Uint8Array([0, 255, 128, 13, 10, 65, 0, 195, 188]);

    service.registerManagedRemoteResourceProtocol(async (uri) => {
      ++calls;
      check(
        uri.scheme === 'vscode-managed-remote-resource' && uri.authority === 'window:42',
        'actual parsed remote URI'
      );
      if (uri.path === '/failure') {
        throw Error('owned controlled remote service failure');
      }
      check(uri.path === '/binary', 'actual remote resource path');
      return {
        statusCode: 200,
        body: Buffer.from(expected).toString('base64'),
        mimeType: 'application/octet-stream'
      };
    });

    const invalid = await net.fetch('vscode-managed-remote-resource://outside/binary');
    check(
      invalid.status === 404 && (await invalid.text()) === 'Not found' && calls === 0,
      'actual invalid window authority 404 without remote dispatch'
    );
    const failed = await net.fetch('vscode-managed-remote-resource://window:42/failure');
    check(
      failed.status === 500 && failed.body === null && calls === 1 && logs.warnings.length === 1,
      'actual remote failure becomes null-body 500 and logs warning'
    );
    const response = await net.fetch('vscode-managed-remote-resource://window:42/binary');
    const actual = new Uint8Array(await response.arrayBuffer());
    check(
      response.status === 200 &&
        response.headers.get('content-type') === 'application/octet-stream' &&
        actual.length === expected.length &&
        actual.every((byte, index) => byte === expected[index]) &&
        calls === 2,
      'actual base64 remote response preserves binary bytes and MIME'
    );

    // Exercise genuine validatedIpcMain registration/removal, without inventing a renderer sender.
    const objectURL = service.createIPCObjectUrl();
    check(objectURL.resource.scheme === 'vscode', 'actual IPC object URL namespace');
    objectURL.update({ owned: true });
    objectURL.dispose();
    check(logs.traces.length === 2, 'actual IPC object URL cleanup');
    service.dispose();
    service = undefined;
    check(
      !(await session.defaultSession.protocol.isProtocolHandled('vscode-file')) &&
        !(await session.defaultSession.protocol.isProtocolHandled(
          'vscode-managed-remote-resource'
        )),
      'actual service disposal removes both custom route registrations'
    );
    console.log(
      'PASS actual pinned Code-OSS ProtocolMainService genuine main-process protocol consumers: file denial, remote authority 404, failure 500, base64 binary/MIME200, IPC registration and cleanup; legacy renderer file transport and workbench untested'
    );
    clearTimeout(timeout);
    app.exit(0);
  } catch (error) {
    service?.dispose();
    clearTimeout(timeout);
    console.error(error);
    app.exit(1);
  }
});
