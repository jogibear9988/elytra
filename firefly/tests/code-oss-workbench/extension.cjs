/** Exercise CodeOSS's original Module._load interceptor with actual CJS activation. */
const vscode = require('vscode');
const getNestedApi = require('./commonjs-helper.cjs');
if (require('vscode') !== vscode || getNestedApi() !== vscode) {
  throw Error('CommonJS extension API identity across repeated and nested requires');
}
let activations = 0;

exports.activate = function activate(context) {
  if (getNestedApi() !== vscode) {
    throw Error('CommonJS extension API identity during activation');
  }
  activations++;
  context.subscriptions.push(
    vscode.commands.registerCommand('elytra.workbenchAcceptance', (nonce) => ({
      nonce,
      activations,
      extensionId: context.extension.id
    }))
  );
  return {
    extensionFormat: 'commonjs',
    commands: vscode.commands,

    get activations() {
      return activations;
    }
  };
};

exports.deactivate = function deactivate() {};
