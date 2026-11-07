console.log('child output');
console.error('child error');
if (process.argv[2] === 'fail') {
  process.exitCode = 7;
}
