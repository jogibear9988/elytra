const children = module.children;
try {
  require('./retry');
} catch (error) {
  console.log(error.message, children.length, module.children === children);
}
console.log(require('./retry'), children.length);
