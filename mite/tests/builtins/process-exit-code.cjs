for (const value of [undefined, null, '7', ' ', 2147483648]) {
    process.exitCode = value;
    console.log(typeof process.exitCode, String(process.exitCode));
}
for (const value of ['', 'foo', 1.2, NaN, Infinity]) {
    try { process.exitCode = value; }
    catch (error) { console.log(error.name, error.code); }
}
process.exitCode = 0;
