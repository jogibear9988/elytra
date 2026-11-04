globalThis.cjsLoads = (globalThis.cjsLoads || 0) + 1;
exports.instance = globalThis.cjsLoads;
exports.parentKnowsChild = module.parent.children.includes(module);
exports.parentName = module.parent.filename;
