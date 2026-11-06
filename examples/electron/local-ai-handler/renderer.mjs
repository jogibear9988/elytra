const result = document.querySelector('#result');
const ready = () => {
  document.title = 'EXAMPLE_READY';
};
const fail = (error) => {
  result.textContent = error.message;
  document.title = 'EXAMPLE_ERROR: ' + error.message;
};
document.querySelector('#ask').onclick = async () => {
  try {
    if (typeof LanguageModel === 'undefined') throw Error('Prompt API unavailable');
    const model = await LanguageModel.create();
    try {
      result.textContent = await model.prompt(document.querySelector('#prompt').value);
    } finally {
      model.destroy();
    }
  } catch (error) {
    fail(error);
  }
};
