// A real CommonJS module exposes its parent so synthetic context identity is observable.
module.exports = { parent: module.parent.filename, count: 0 };
