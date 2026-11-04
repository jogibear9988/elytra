setTimeout(() => console.log('WRONG pending timer'), 1);
console.log('before');
process.exit(6);
console.log('WRONG continued');
