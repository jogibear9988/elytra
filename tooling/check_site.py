#!/usr/bin/env python3
"""Check built local links/resources, including repository-relative Pages paths."""

import argparse
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links = []
        self.ids = set()

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if "id" in attrs:
            if attrs["id"] in self.ids:
                raise AssertionError("Duplicate id: " + attrs["id"])
            self.ids.add(attrs["id"])
        for key in ["href", "src"]:
            if attrs.get(key):
                self.links.append(attrs[key])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("site", type=Path)
    args = parser.parse_args()
    root = args.site.resolve()
    pages = {}
    for file in root.rglob("*.html"):
        page = Page()
        page.feed(file.read_text())
        pages[file] = page
    assert pages, "Build the site first"
    checked = 0
    for file, page in pages.items():
        for link in page.links:
            parsed = urlsplit(link)
            if parsed.scheme or parsed.netloc:
                continue
            assert not parsed.path.startswith("/"), (
                file,
                link,
                "Absolute path breaks GitHub project Pages",
            )
            target = (file.parent / unquote(parsed.path)).resolve() if parsed.path else file
            if target.is_dir():
                target = target / "index.html"
            assert target.is_relative_to(root) and target.is_file(), (
                file,
                link,
                "Missing local resource",
            )
            if parsed.fragment and target in pages:
                assert unquote(parsed.fragment) in pages[target].ids, (
                    file,
                    link,
                    "Missing fragment",
                )
            checked += 1
    print(f"PASS {len(pages)} generated pages and {checked} local links/resources/fragments")


if __name__ == "__main__":
    main()
