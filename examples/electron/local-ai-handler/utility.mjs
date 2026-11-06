import process from 'node:process';
import { pathToFileURL } from 'node:url';
import { isAbsolute } from 'node:path';
const { localAIHandler, LanguageModelUtility } = await import(
  process.env.ELYTRA_EXAMPLE_RUNTIME === 'electron' ? 'electron/utility' : 'firefly/utility'
);
const file = process.env.ELYTRA_LOCAL_AI_MODULE;
if (!file || !isAbsolute(file)) throw Error('ELYTRA_LOCAL_AI_MODULE must be an absolute path');
const backend = await import(pathToFileURL(file).href);
if (
  typeof backend.LanguageModel !== 'function' ||
  !(backend.LanguageModel.prototype instanceof LanguageModelUtility)
)
  throw Error('Backend must export a real LanguageModelUtility subclass');
localAIHandler.setPromptAPIHandler((details) => {
  if (details.securityOrigin !== process.env.ELYTRA_EXAMPLE_ORIGIN)
    throw Error('Unexpected model caller origin');
  return backend.LanguageModel;
});
