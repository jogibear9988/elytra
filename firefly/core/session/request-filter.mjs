/** Compile bounded Electron-style URL filters without interpreting patterns as regex code. */
export default function requestFilter(value, error, unsupported) {
  const bad = (message) => error('ERR_INVALID_ARG_VALUE', message);

  const types = new Set([
    'mainFrame',
    'subFrame',
    'stylesheet',
    'script',
    'image',
    'font',
    'object',
    'xhr',
    'ping',
    'cspReport',
    'media',
    'webSocket'
  ]);

  function pattern(source) {
    if (typeof source !== 'string' || source.length > 8192) {
      throw bad('Invalid webRequest URL pattern');
    }
    if (source === '<all_urls>') {
      return () => true;
    }
    const match = /^(\*|[a-z][a-z0-9+.-]*):\/\/([^/]*)(\/.*)$/i.exec(source);
    if (!match || /[\r\n\0?#@]/.test(match[2])) {
      throw bad('Invalid webRequest URL pattern: ' + source);
    }
    const scheme = match[1].toLowerCase();
    let authority = match[2].toLowerCase();
    let port = null;
    // IPv6 literal patterns are accepted exactly. Wildcards apply only to DNS
    // suffixes, never to a fragment of a hostname or an address literal.
    const separator = authority.lastIndexOf(':');
    if (separator >= 0 && (!authority.startsWith('[') || separator > authority.indexOf(']'))) {
      port = authority.slice(separator + 1);
      authority = authority.slice(0, separator);
      if (port !== '*' && (!/^\d{1,5}$/.test(port) || Number(port) > 65535)) {
        throw bad('Invalid webRequest pattern port');
      }
    }
    const suffix = authority.startsWith('*.') ? authority.slice(2) : null;
    if ((authority.includes('*') && authority !== '*' && !suffix) || suffix?.includes('*')) {
      throw bad('Invalid webRequest pattern hostname');
    }
    if (!authority && scheme !== 'file') {
      throw bad('Missing webRequest pattern hostname');
    }
    const path = new RegExp(
      '^' +
        match[3]
          .split('*')
          .map((part) => part.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'))
          .join('.*') +
        '$'
    );
    return (url) => {
      const protocol = url.protocol.slice(0, -1);
      if (scheme === '*' ? !['http', 'https'].includes(protocol) : protocol !== scheme) {
        return false;
      }
      const host = url.hostname.toLowerCase();
      if (
        authority !== '*' &&
        (suffix ? host !== suffix && !host.endsWith('.' + suffix) : host !== authority)
      ) {
        return false;
      }
      const effectivePort =
        url.port ||
        ({ 'http:': '80', 'https:': '443', 'ws:': '80', 'wss:': '443' }[url.protocol] ?? '');
      return (
        (port === null || port === '*' || effectivePort === String(Number(port))) &&
        path.test(url.pathname + url.search)
      );
    };
  }

  if (value === undefined) {
    return () => true;
  }
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    throw bad('webRequest filter must be an object');
  }
  for (const key of Object.keys(value)) {
    if (!['urls', 'excludeUrls', 'types'].includes(key)) {
      unsupported('webRequest filter ' + key);
    }
  }

  function list(items, required) {
    if (items === undefined && !required) {
      return [];
    }
    if (!Array.isArray(items) || items.length > 256) {
      throw bad('webRequest URL filters must be arrays of at most 256 patterns');
    }
    return items.map(pattern);
  }

  const include = list(value.urls, true);
  const exclude = list(value.excludeUrls, false);
  const selected = value.types;
  if (
    selected !== undefined &&
    (!Array.isArray(selected) ||
      selected.length > types.size ||
      selected.some((item) => !types.has(item)))
  ) {
    throw bad('Invalid webRequest resource types');
  }
  const allowed = selected === undefined || selected.length === 0 ? null : new Set(selected);
  return (details) => {
    let url;
    try {
      url = new URL(details.url);
    } catch {
      return false;
    }
    return (
      (!allowed || allowed.has(details.resourceType)) &&
      (!include.length || include.some((test) => test(url))) &&
      !exclude.some((test) => test(url))
    );
  };
}
