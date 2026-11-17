/** Run through CodeOSS's ordinary --extensionTestsPath control, never a replacement API. */
import * as vscode from 'vscode';
import { readFile, writeFile, rename } from 'node:fs/promises';
import process from 'node:process';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function shellQuote(value) {
  return "'" + value.replace(/'/g, "'\\''") + "'";
}

async function terminalWorkflow(config) {
  let terminal;
  let subscription;
  let timer;
  const deadline = new Promise((_, reject) => {
    timer = setTimeout(() => reject(Error('Owned terminal workflow timed out')), 30000);
  });

  const work = async () => {
    const closed = new Promise((done) => {
      subscription = vscode.window.onDidCloseTerminal((value) => {
        if (value === terminal) {
          done(value);
        }
      });
    });
    terminal = vscode.window.createTerminal({
      name: 'Owned terminal acceptance',
      shellPath: '/bin/sh',
      shellArgs: ['-f'],
      cwd: vscode.Uri.file(config.terminalWorkspace),
      env: { ENV: '/dev/null', BASH_ENV: '/dev/null', HISTFILE: '/dev/null' },
      isTransient: true
    });
    const pid = await terminal.processId;
    check(Number.isInteger(pid) && pid > 0, 'Real terminal shell PID');
    await writeFile(
      config.terminalReady + '.tmp',
      JSON.stringify({ schema: 1, nonce: config.nonce, pid }) + '\n',
      { flag: 'wx' }
    );
    await rename(config.terminalReady + '.tmp', config.terminalReady);
    let authenticated = false;
    for (let attempt = 0; attempt < 150; attempt++) {
      try {
        const gate = JSON.parse(await readFile(config.terminalGate, 'utf8'));
        check(
          gate.schema === 1 && gate.nonce === config.nonce && gate.pid === pid,
          'Terminal PID gate identity'
        );
        authenticated = true;
        break;
      } catch (error) {
        if (error.code !== 'ENOENT') {
          throw error;
        }
      }
      await new Promise((done) => setTimeout(done, 100));
    }
    check(authenticated, 'Native terminal ownership verified before commands');
    check(config.terminalText.endsWith('\n'), 'Owned terminal expected output');
    terminal.sendText(
      "printf '%s\\n' " +
        shellQuote(config.terminalText.slice(0, -1)) +
        ' > ' +
        shellQuote(config.terminalOutput) +
        '; exit 17',
      true
    );
    const ended = await closed;
    check(ended.exitStatus?.code === 17, 'Actual terminal close reports exit status 17');
    return pid;
  };

  try {
    return await Promise.race([work(), deadline]);
  } finally {
    clearTimeout(timer);
    subscription?.dispose();
    terminal?.dispose();
  }
}

export async function run() {
  const config = JSON.parse(await readFile(new URL('./acceptance.json', import.meta.url), 'utf8'));
  check(
    config.schema === 1 && typeof config.nonce === 'string' && config.nonce.length === 36,
    'Owned acceptance configuration'
  );
  const extension = vscode.extensions.getExtension('elytra-owned.workbench-acceptance');
  check(extension, 'Development extension discovered by the original scanner');
  const api = await extension.activate();
  check(extension.isActive && api.activations === 1, 'Genuine extension activation and export');
  check(
    api.extensionFormat === config.extensionFormat,
    'Requested extension module format activated'
  );
  if (config.extensionFormat === 'commonjs') {
    check(
      api.commands === vscode.commands,
      'CJS interceptor and ESM hook expose the same commands API'
    );
  }
  const reply = await vscode.commands.executeCommand('elytra.workbenchAcceptance', config.nonce);
  check(
    reply.nonce === config.nonce && reply.activations === 1 && reply.extensionId === extension.id,
    'Registered command crosses the actual API'
  );
  const uri = vscode.Uri.file(config.document);
  const document = await vscode.workspace.openTextDocument(uri);
  check(document.getText() === config.initial, 'Original owned file opened through workspace API');
  const editor = await vscode.window.showTextDocument(document, {
    preview: false,
    preserveFocus: true
  });
  check(editor.document === document, 'Actual editor owns the opened document');
  const edit = new vscode.WorkspaceEdit();
  edit.insert(uri, new vscode.Position(0, 0), config.prefix);
  check(await vscode.workspace.applyEdit(edit), 'WorkspaceEdit accepted');
  check(document.isDirty && document.getText() === config.expected, 'Real document model changed');
  check(await document.save(), 'Real editor document saved');
  check(
    !document.isDirty && document.getText() === config.expected,
    'Saved model remains coherent'
  );
  // This native read is useful within the host, but the Python parent separately
  // verifies physical bytes after shutdown; the result file alone is not a pass.
  check(
    (await readFile(config.document, 'utf8')) === config.expected,
    'Native extension-host file readback'
  );
  const result = {
    schema: 1,
    extensionFormat: config.extensionFormat,
    nonce: config.nonce,
    extensionId: extension.id,
    pid: process.pid,
    runtimeMite: typeof process.versions.mite === 'string',
    activated: true,
    command: true,
    opened: true,
    editor: true,
    edited: true,
    saved: true
  };
  if (config.terminal === true) {
    result.terminalPid = await terminalWorkflow(config);
    result.terminal = true;
  }
  await writeFile(config.result + '.tmp', JSON.stringify(result) + '\n', {
    encoding: 'utf8',
    flag: 'wx'
  });
  await rename(config.result + '.tmp', config.result);
  process.stdout.write('ELYTRA_OWNED_WORKBENCH_EDIT_SAVE_OK\n');
}
