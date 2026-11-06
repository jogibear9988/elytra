for (const specifier of ['./missing.mjs', './', './dep', './identity.mjs%2fextra',
                         './unknown.xyz', 'https://example.invalid/test.mjs',
                         process.platform==='win32'?'file:///invalid-drive/test.mjs':'file://remote/test.mjs']) {
    try {
        await import(specifier);
        console.log('WRONG accepted');
    } catch (error) {
        console.log(error.code);
    }
}
