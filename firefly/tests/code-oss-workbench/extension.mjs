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
  return {
    extensionFormat: 'esm',

    get activations() {
      return activations;
    }
  };
}

export function deactivate() {}
