/** Explicit CommonJS compatibility fixture; ordinary samples remain ESM. */
const vscode = require('vscode');

module.exports = function getApi() {
  if (require('vscode') !== vscode) {
    throw Error('Nested CommonJS require lost the original extension API identity');
  }
  return vscode;
};
