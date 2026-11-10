/** Local diagnostic CSS discovery; actual stylesheet/module loading stays upstream. */
import { promises as fs } from 'node:fs';
import { join } from 'node:path';
import { isLocalDesktop } from '../common/elytraLocalDesktop.js';

const revision = '6531648f5ace8b77ae799262bdc97bf048461938';

function refuse(message: string, cause?: unknown): never {
  throw Object.assign(new Error(`Code-OSS staged CSS inventory: ${message}`), {
    code: 'ERR_CODE_OSS_CSS_INVENTORY',
    cause
  });
}

async function readInventory(basePath: string): Promise<string[]> {
  let value: unknown;
  try {
    const bytes = await fs.readFile(join(basePath, 'elytra-css-modules.json'), 'utf8');
    if (bytes.length > 1024 * 1024) refuse('metadata exceeds budget');
    value = JSON.parse(bytes);
  } catch (error) {
    refuse('missing or malformed; refresh compiled renderer assets', error);
  }
  const record = value as {
    schema?: unknown;
    kind?: unknown;
    revision?: unknown;
    cssModules?: unknown;
  };
  if (
    !record ||
    typeof record !== 'object' ||
    Object.keys(record).length !== 4 ||
    record.schema !== 1 ||
    record.kind !== 'code-oss-staged-dev-css' ||
    record.revision !== revision ||
    !Array.isArray(record.cssModules) ||
    record.cssModules.length === 0 ||
    record.cssModules.length > 2048
  ) {
    refuse('invalid schema, source revision or file count');
  }
  let previous = '';
  const modules: string[] = [];
  for (const item of record.cssModules) {
    if (
      typeof item !== 'string' ||
      item.length > 4096 ||
      !item.startsWith('vs/') ||
      !item.endsWith('.css') ||
      /[\\\0?#]/.test(item) ||
      item.split('/').some((part) => !part || part === '.' || part === '..') ||
      item <= previous
    ) {
      refuse('paths must be sorted unique relative vs/*.css outputs');
    }
    previous = item;
    modules.push(item);
  }
  return modules;
}

/** Preserve ordinary Node/Electron's original ripgrep discovery without I/O. */
export function libJSDevelopmentCSSModules(basePath: string): Promise<string[]> | undefined {
  const versions = process.versions as Record<string, string | undefined>;
  if (
    typeof versions.node === 'string' ||
    typeof versions.electron === 'string' ||
    !isLocalDesktop()
  )
    return undefined;
  if (
    typeof versions.mite !== 'string' ||
    typeof versions.libjs !== 'string' ||
    process.env.ELYTRA_CODE_OSS_LOCAL_DESKTOP !== '1' ||
    typeof basePath !== 'string' ||
    !basePath ||
    basePath.includes('\0')
  ) {
    refuse('requires the verified local LibJS diagnostic runtime');
  }
  return readInventory(basePath);
}
