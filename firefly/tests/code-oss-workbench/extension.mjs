/** Ordinary development extension: activation owns a genuine command registration. */
import * as vscode from 'vscode';
let activations = 0;

export function activate(context) {
  activations++;
  context.subscriptions.push(
    vscode.commands.registerCommand('elytra.workbenchAcceptance', (nonce) => ({
      nonce,
      activations,
      extensionId: context.extension.id
    }))
  );
  if (context.extension.packageJSON.activationEvents.includes('onStartupFinished')) {
    setTimeout(() => {
      import('./tests.mjs')
        .then(({ run }) => run())
        .catch((error) => console.error('Owned workspace switch failed:', error));
    }, 0);
  }
  return {
    extensionFormat: 'esm',

    get activations() {
      return activations;
    }
  };
}

export function deactivate() {}
