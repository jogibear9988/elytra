import process from 'node:process';
const original = process.title;
try {
  process.title = 'kid';
  console.log(JSON.stringify({ pid: process.pid, title: process.title }));
} finally {
  process.title = original;
}
