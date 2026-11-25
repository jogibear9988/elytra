"""Apply Elytra's ordered Ladybird patches in a verified, reusable build worktree."""

import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

from check_upstream import ROOT, check


def git(directory, *args, env=None):
    return subprocess.check_output(
        ["git", "-C", str(directory), *map(str, args)], text=True, env=env
    ).strip()


def expected_tree(upstream, revision, patches):
    """Compose the stack in a temporary index before touching any worktree."""
    with tempfile.TemporaryDirectory(prefix="elytra-ladybird-index-") as temporary:
        env = {**os.environ, "GIT_INDEX_FILE": str(Path(temporary) / "index")}
        git(upstream, "read-tree", revision, env=env)
        for patch in patches:
            git(upstream, "apply", "--cached", "--whitespace=error-all", patch, env=env)
        return git(upstream, "write-tree", env=env)


def validate(source, upstream, revision, tree):
    if Path(git(source, "rev-parse", "--show-toplevel")).resolve() != source.resolve():
        raise ValueError(f"Invalid Ladybird build checkout: {source}")
    source_common = git(source, "rev-parse", "--path-format=absolute", "--git-common-dir")
    upstream_common = git(upstream, "rev-parse", "--path-format=absolute", "--git-common-dir")
    if (
        Path(source_common).resolve() != Path(upstream_common).resolve()
        or git(source, "rev-parse", "HEAD") != revision
    ):
        raise ValueError(f"Ladybird build checkout has an unexpected base: {source}")
    if git(source, "write-tree") != tree:
        raise ValueError(f"Ladybird build checkout index differs from the patch stack: {source}")
    if git(source, "diff", "--name-only") or git(
        source, "ls-files", "--others", "--exclude-standard"
    ):
        raise ValueError(f"Ladybird build checkout has local changes: {source}")


def prepare(root=ROOT, verify_source=None):
    revision = check(root)
    upstream = root / "vendor/ladybird"
    patches = sorted((root / "patches/ladybird").glob("*.patch"))
    if not patches:
        if verify_source is not None and upstream.resolve() != verify_source.resolve():
            raise ValueError(
                "Ladybird patch stack was removed; configure a fresh renderer build directory"
            )
        return upstream
    tree = expected_tree(upstream, revision, patches)
    key = hashlib.sha256(f"{revision}:{tree}".encode()).hexdigest()[:20]
    source = root / "build/ladybird-patched" / key
    if verify_source is not None and source.resolve() != verify_source.resolve():
        raise ValueError("Ladybird patch stack changed; configure a fresh renderer build directory")
    if not source.exists():
        if verify_source is not None:
            raise ValueError(f"Missing patched Ladybird build checkout: {source}")
        source.parent.mkdir(parents=True, exist_ok=True)
        git(upstream, "worktree", "add", "--quiet", "--detach", source, revision)
        for patch in patches:
            git(source, "apply", "--index", "--whitespace=error-all", patch)
    validate(source, upstream, revision, tree)
    return source


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify-source", type=Path)
    args = parser.parse_args()
    print(prepare(verify_source=args.verify_source))
