/** Rendered page code: document state must survive native view reparenting. */
const documentIdentity = document.title + ':' + Math.random();
let sampleCount = 0;

viewFixture.onSample(async (request) => {
  try {
    const copies = await Promise.all(
      Array.from({ length: 8 }, async (_value, index) => {
        const bytes = new Uint8Array(4096);
        bytes[0] = index;
        bytes[1] = 128;
        bytes[bytes.length - 1] = 255;
        const echoed = await viewFixture.echo({ index, bytes });
        return (
          echoed.index === index &&
          echoed.bytes instanceof Uint8Array &&
          echoed.bytes.length === 4096 &&
          echoed.bytes[0] === index &&
          echoed.bytes[1] === 128 &&
          echoed.bytes[4095] === 255
        );
      })
    );
    await viewFixture.report({
      token: request.token,
      identity: viewFixture.identity,
      pid: viewFixture.pid,
      secure: viewFixture.secure,
      title: document.title,
      documentIdentity,
      width: window.innerWidth,
      height: window.innerHeight,
      sampleCount: ++sampleCount,
      binary: copies.every(Boolean),
      nodeAbsent: typeof process === 'undefined' && typeof require === 'undefined'
    });
  } catch (error) {
    await viewFixture.report({ token: request.token, error: String(error) });
  }
});

viewFixture.ready();
