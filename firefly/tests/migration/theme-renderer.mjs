/** Native appearance must reach LibWeb's CSS media state, including later updates. */
const dark = matchMedia('(prefers-color-scheme: dark)');

const report = () => {
  document.title = matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
};

dark.addEventListener('change', report);
// Hidden views can defer rendering-driven change events; query current CSS state
// independently so this fixture distinguishes preference delivery from painting.
setInterval(report, 20);
report();
