globalThis.__ownedRetryLoads = (globalThis.__ownedRetryLoads || 0) + 1;
if (globalThis.__ownedRetryLoads === 1) {
  throw globalThis.__ownedLoadFailure;
}
module.exports = { loads: globalThis.__ownedRetryLoads };
