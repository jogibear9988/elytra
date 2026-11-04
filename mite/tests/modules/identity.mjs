globalThis.moduleInstances = (globalThis.moduleInstances || 0) + 1;
export const instance = globalThis.moduleInstances;
