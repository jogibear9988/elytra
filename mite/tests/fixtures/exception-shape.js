for (const ErrorType of [Error, TypeError, RangeError]) {
  try {
    throw new ErrorType('explicit message');
  } catch (error) {
    console.log(error.name + ': ' + error.message);
  }
}
