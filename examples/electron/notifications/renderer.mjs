/** Manual request status; native acceptance does not certify an OS banner was visible. */
const result = document.querySelector('#result');
const button = document.querySelector('#notify');

button.onclick = async () => {
  button.disabled = true;
  result.textContent = 'Waiting for native notification acknowledgement…';
  try {
    const value = await desktop.notify();
    if (value?.nativeRequestAccepted !== true || value.osVisibility !== 'unverified') {
      throw Error('Unexpected notification acknowledgement');
    }
    result.textContent = 'Native request accepted. OS visibility remains unverified.';
    document.title = 'Notification request accepted; visibility unverified';
  } catch (error) {
    result.textContent = error.message;
    document.title = 'EXAMPLE_ERROR: ' + error.message;
  } finally {
    button.disabled = false;
  }
};
