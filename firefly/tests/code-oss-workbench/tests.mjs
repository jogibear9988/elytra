/** Run through CodeOSS's ordinary --extensionTestsPath control, never a replacement API. */
import * as vscode from 'vscode';
import { readFile, writeFile, rename } from 'node:fs/promises';
import process from 'node:process';

function check(value, message) {
  if (!value) {
    throw Error(message);
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
  await writeFile(config.result + '.tmp', JSON.stringify(result) + '\n', {
    encoding: 'utf8',
    flag: 'wx'
  });
  await rename(config.result + '.tmp', config.result);
  process.stdout.write('ELYTRA_OWNED_WORKBENCH_EDIT_SAVE_OK\n');
}
