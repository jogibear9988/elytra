// Owned embedding shim for Electron's CSS spelling. LibWeb stays unmodified.
// No native functions are installed in the page. The host reads bounded rectangles.
/** Document-side CSS drag-region geometry with bounded native observations. */
export default function installAppRegions() {
  const property = '--firefly-app-region';
  const mapped = new WeakMap();
  const external = new WeakMap();
  const reset = document.createElement('style');
  reset.dataset.fireflyRegionAdapter = '';
  reset.textContent = `* { ${property}: none; }`;
  document.head.prepend(reset);
  // Recognize declaration names, respecting strings/comments. Preserve CSS grammar
  // and let LibWeb handle selectors, media queries, cascade and layout afterwards.
  function translate(source) {
    let out = '';
    let i = 0;
    let start = true;
    let quote = '';
    while (i < source.length) {
      const c = source[i];
      if (quote) {
        out += c;
        i++;
        if (c === '\\' && i < source.length) {
          out += source[i++];
        } else if (c === quote) {
          quote = '';
        }
        continue;
      }
      if (c === '"' || c === "'") {
        quote = c;
        out += c;
        i++;
        continue;
      }
      if (c === '/' && source[i + 1] === '*') {
        const end = source.indexOf('*/', i + 2);
        if (end < 0) {
          return source;
        }
        out += source.slice(i, end + 2);
        i = end + 2;
        continue;
      }
      if (start) {
        const match = /^(\s*)(-webkit-app-region|app-region)(\s*:)/i.exec(source.slice(i));
        if (match) {
          out += match[1] + property + match[3];
          i += match[0].length;
          start = false;
          continue;
        }
      }
      out += c;
      i++;
      if (c === '{' || c === ';') {
        start = true;
      } else if (!/\s/.test(c)) {
        start = false;
      }
    }
    return out;
  }

  function install(owner, source) {
    const previous = mapped.get(owner);
    if (previous?.source === source) {
      previous.style.media = owner.media || '';
      previous.style.disabled = owner.disabled;
      return;
    }
    previous?.style.remove();
    if (!/(?:-webkit-)?app-region\s*:/.test(source)) {
      mapped.delete(owner);
      return;
    }
    const style = document.createElement('style');
    style.dataset.fireflyRegionAdapter = '';
    style.__fireflyOwner = owner;
    style.textContent = translate(source);
    style.media = owner.media || '';
    owner.after(style);
    style.disabled = owner.disabled;
    mapped.set(owner, { source, style });
  }

  function update() {
    for (const style of document.querySelectorAll('style[data-firefly-region-adapter]')) {
      if (style !== reset && !style.__fireflyOwner?.isConnected) {
        style.remove();
      }
    }
    for (const style of document.querySelectorAll('style:not([data-firefly-region-adapter])')) {
      install(style, style.textContent);
    }
    for (const link of document.querySelectorAll('link[rel="stylesheet"]')) {
      const url = new URL(link.href, location.href);
      if (url.origin !== location.origin || external.get(link) === url.href) {
        continue;
      }
      external.set(link, url.href);

      fetch(url.href)
        .then((response) => (response.ok ? response.text() : Promise.reject()))
        .then((source) => {
          if (link.isConnected && link.href === url.href) {
            install(link, source);
          }
        })
        .catch(() => {});
    }
  }

  function selectors(rules, result) {
    for (const rule of rules) {
      if (rule.style?.getPropertyValue(property) && rule.selectorText) {
        result.add(rule.selectorText);
      }
      if (rule.cssRules) {
        selectors(rule.cssRules, result);
      }
    }
  }

  globalThis.__fireflyRegionSnapshot = () => {
    update();
    const matches = new Set();
    for (const style of document.querySelectorAll('style[data-firefly-region-adapter]')) {
      if (style === reset) {
        continue;
      }
      const names = new Set();
      selectors(style.sheet?.cssRules || [], names);
      for (const selector of names) {
        try {
          for (const element of document.querySelectorAll(selector)) {
            matches.add(element);
            if (matches.size > 256) {
              return '[]';
            }
          }
        } catch {}
      }
    }
    const regions = [];
    for (const element of matches) {
      const css = getComputedStyle(element);
      const value = css.getPropertyValue(property).trim();
      if (
        !['drag', 'no-drag'].includes(value) ||
        css.visibility !== 'visible' ||
        css.display === 'none'
      ) {
        continue;
      }
      const r = element.getBoundingClientRect();
      const x = Math.max(0, r.left);
      const y = Math.max(0, r.top);
      const right = Math.min(innerWidth, r.right);
      const bottom = Math.min(innerHeight, r.bottom);
      if (right > x && bottom > y) {
        regions.push([
          Math.ceil(x),
          Math.ceil(y),
          Math.floor(right - x),
          Math.floor(bottom - y),
          value === 'drag'
        ]);
      }
    }
    return JSON.stringify(regions);
  };
}
