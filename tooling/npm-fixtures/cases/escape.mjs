import escape from 'escape-string-regexp';
import once from 'once';
const sample = '[a-z]+(foo)? $5 ^.*\\';
const escaped = escape(sample);
console.log(new RegExp('^' + escaped + '$', 'u').test(sample), escaped.includes('\\x2d'));
try {
  escape(42);
} catch (error) {
  console.log(error.name, error.message);
}
console.log(once(() => 'CJS imported by ESM')());
