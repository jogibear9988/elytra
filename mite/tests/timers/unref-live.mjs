/** An unreferenced call still runs when other native work keeps the loop alive. */
let ran = false;
setImmediate(() => {
  ran = true;
}).unref();

setTimeout(() => console.log(ran), 30);
