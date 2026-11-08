/** Shell strings are fixtures, with explicit quoting for the host's genuine command interpreter. */
export function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

export function quote(value) {
  return process.platform === 'win32'
    ? '"' + value.replaceAll('"', '\\"') + '"'
    : "'" + value.replaceAll("'", "'\\''") + "'";
}

export function python(code, replaceShell = false) {
  return (
    (replaceShell && process.platform !== 'win32' ? 'exec ' : '') +
    quote(process.argv[2]) +
    ' -c ' +
    quote(code)
  );
}
