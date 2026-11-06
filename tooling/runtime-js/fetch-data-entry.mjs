/** Pinned Blob core and lazy multipart parser, adapted to the standalone LibJS host. */
import SourceBlob from 'fetch-blob';
import MIMEType from 'whatwg-mimetype';
import createBlobs from '../../mite/runtime/bootstrap/fetch/data/blob.mjs';
import createFormData from '../../mite/runtime/bootstrap/fetch/data/form-data.mjs';
import createMultipart from '../../mite/runtime/bootstrap/fetch/data/multipart.mjs';

export default function createFetchData(host) {
  const blobs = createBlobs(SourceBlob, host);
  const forms = createFormData(blobs);
  const multipart = createMultipart(
    host,
    blobs,
    forms,
    MIMEType,
    async () => (await import('busboy')).default
  );
  return { ...blobs, ...forms, ...multipart };
}
