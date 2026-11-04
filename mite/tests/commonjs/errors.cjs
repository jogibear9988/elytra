for (const request of [undefined, null, {}, 42]) {
    try { require(request); }
    catch (error) { console.log(error.name, error.code); }
}
for (const load of [require, require.resolve]) {
    try { load('./missing'); }
    catch (error) {
        console.log(error.code, error.requireStack.length, error.requireStack[0] === __filename);
    }
}
