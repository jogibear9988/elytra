/** Certificate rejection must happen before any HTTP application request is sent. */
try {
  await fetch(`https://127.0.0.1:${process.argv[2]}/`);
  throw new Error('untrusted TLS succeeded');
} catch (error) {
  if (!(error instanceof TypeError) || error.cause?.code !== 'DEPTH_ZERO_SELF_SIGNED_CERT') {
    throw error;
  }
  console.log('PASS Fetch TLS certificate rejection', error.cause.code);
}
