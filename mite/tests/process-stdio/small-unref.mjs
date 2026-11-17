/** A completed small pipe write must not create a loop reference for unrelated work. */
setImmediate(() => process.stdout.write('unref-immediate-ran\n')).unref();
process.stdout.write('small native pipe output\n');
