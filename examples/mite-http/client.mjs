import { get } from 'node:http';

const url = process.argv[2] ?? 'http://127.0.0.1:3000/hello';
const response = await new Promise((resolve, reject) => {
  get(url, { agent: false }, resolve).on('error', reject);
});
console.log(`HTTP ${response.statusCode} ${response.statusMessage}`);
response.setEncoding('utf8');
for await (const chunk of response) {
  console.log(chunk);
}
