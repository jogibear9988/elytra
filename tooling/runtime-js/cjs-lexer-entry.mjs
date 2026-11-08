/** Private synchronous source analysis; the pinned JS provider needs no Wasm or Node host. */
import lexer from './node_modules/cjs-module-lexer/lexer.js';

export default function createCommonJSLexer() {
  return lexer.parse;
}
