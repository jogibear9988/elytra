const before = process.cwd();
process.chdir(__dirname);
console.log(process.cwd().replaceAll('\\', '/') === __dirname.replaceAll('\\', '/'));
process.chdir(before);
console.log(process.cwd() === before);
try { process.chdir('mite-working-directory-missing-fixture'); }
catch (error) { console.log(error.code, error.errno < 0, error.syscall, error.path === before, error.dest); }
try { process.chdir(42); }
catch (error) { console.log(error.name, error.code); }
