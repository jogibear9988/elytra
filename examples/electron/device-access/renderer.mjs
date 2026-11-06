/**
 * Device access browser interaction. Native setup stays in main; this script uses
 * document/browser APIs only.
 */
const result = document.querySelector('#result');

const ready = () => {
  document.title = 'EXAMPLE_READY';
};

const fail = (error) => {
  result.textContent = error.message;
  document.title = 'EXAMPLE_ERROR: ' + error.message;
};

document.querySelector('#choose').onclick = async () => {
  try {
    if (!navigator.hid) {
      throw Error('WebHID unavailable');
    }
    const devices = await navigator.hid.requestDevice({ filters: [] });
    result.textContent =
      devices.map((device) => device.productName).join(', ') || 'No device selected';
  } catch (error) {
    fail(error);
  }
};
