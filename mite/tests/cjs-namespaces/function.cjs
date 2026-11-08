/** Function exports can carry statically discoverable properties too. */
module.exports = function callable() {
  return 'callable';
};

module.exports.answer = 42;
