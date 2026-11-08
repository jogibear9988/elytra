/** Named namespace bindings are snapshots; the default object remains mutable. */
exports.count = 1;

exports.change = function change() {
  module.exports.count = 2;
  const later = 'addedLater';
  module.exports[later] = 3;
};
