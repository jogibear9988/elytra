/** Multipart serialization preserves immutable file parts; parsing collects explicit consumers. */
export default function createMultipart(host, blobs, forms, MIMEType, loadParser) {
  const { Blob, File } = blobs;
  const { FormData } = forms;

  function escape(value) {
    return value.replace(/\r/g, '%0D').replace(/\n/g, '%0A').replace(/"/g, '%22');
  }

  function lineBreaks(value) {
    return value.replace(/\r\n|\r|\n/g, '\r\n');
  }

  function formDataToBlob(form) {
    const boundary =
      '----mite-formdata-' + host.require('node:crypto').randomBytes(18).toString('hex');
    const parts = [];
    for (const [name, value] of form) {
      const prefix = `--${boundary}\r\nContent-Disposition: form-data; name="${escape(lineBreaks(name))}"`;
      if (typeof value === 'string') {
        parts.push(prefix + '\r\n\r\n' + lineBreaks(value) + '\r\n');
      } else {
        parts.push(
          prefix +
            `; filename="${escape(value.name)}"\r\nContent-Type: ${blobs.type(value) || 'application/octet-stream'}\r\n\r\n`,
          value,
          '\r\n'
        );
      }
    }
    parts.push(`--${boundary}--\r\n`);
    return new Blob(parts, { type: `multipart/form-data; boundary=${boundary}` });
  }

  function mimeType(value) {
    return value === null ? '' : (MIMEType.parse(value)?.toString() ?? '');
  }

  async function parseFormData(contentType, bytes) {
    const mime = contentType === null ? null : MIMEType.parse(contentType);
    if (mime?.essence === 'application/x-www-form-urlencoded') {
      // URL form decoding is UTF-8 irrespective of a charset parameter. Preserve
      // the leading BOM as form data, rather than TextDecoder's default stripping.
      const text = new host.TextDecoder('utf-8', { ignoreBOM: true }).decode(bytes);
      const form = new FormData();
      for (const [name, value] of new host.URLSearchParams(text)) {
        form.append(name, value);
      }
      return form;
    }
    if (mime?.essence !== 'multipart/form-data') {
      throw new TypeError('Body Content-Type is not form data');
    }
    const createParser = await loadParser();
    return new Promise((resolve, reject) => {
      let parser;
      let settled = false;
      const entries = [];

      function fail(cause) {
        if (settled) {
          return;
        }
        settled = true;
        reject(new TypeError('Failed to parse body as FormData', { cause }));
        parser?.destroy();
      }

      try {
        parser = createParser({
          headers: { 'content-type': mime.toString() },
          defCharset: 'utf8',
          defParamCharset: 'utf8',
          preservePath: true,
          // No silent truncation: convenience formData() explicitly collects its
          // result. The parser's fixed malformed/oversized-header checks remain.
          limits: { fieldSize: Infinity, fileSize: Infinity }
        });
        parser.on('error', fail);

        parser.on('field', (name, value, info) => {
          if (typeof value !== 'string' || info.nameTruncated || info.valueTruncated) {
            fail(new Error('Unsupported or truncated form field'));
          } else {
            entries.push([name, value]);
          }
        });

        parser.on('file', (name, file, info) => {
          const index = entries.length;
          entries.push([name, null]);
          const chunks = [];
          file.on('error', fail);

          file.on('data', (chunk) => chunks.push(chunk));

          file.on('end', () => {
            if (settled) {
              return;
            }
            try {
              const parts =
                info.encoding.toLowerCase() === 'base64'
                  ? [host.Buffer.from(host.Buffer.concat(chunks).toString('ascii'), 'base64')]
                  : chunks;
              entries[index][1] = new File(parts, info.filename ?? 'undefined', {
                type: info.mimeType
              });
            } catch (error) {
              fail(error);
            }
          });
        });

        parser.on('close', () => {
          if (settled) {
            return;
          }
          try {
            const result = new FormData();
            for (const [name, value] of entries) {
              result.append(name, value);
            }
            settled = true;
            resolve(result);
          } catch (error) {
            fail(error);
          }
        });

        parser.end(host.Buffer.from(bytes));
      } catch (error) {
        fail(error);
      }
    });
  }

  return { formDataToBlob, parseFormData, mimeType };
}
