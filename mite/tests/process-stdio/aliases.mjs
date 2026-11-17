/** Both borrowed descriptors refer to one real pipe open-file description. */
setImmediate(() => process.stdout.write('unref-alias-immediate-ran\n')).unref();
process.stdout.write('aliased stdout\n');
process.stderr.write('aliased stderr\n');
