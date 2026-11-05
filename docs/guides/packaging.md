# Packaging and releases for app authors

Current output is experimental macOS ARM64 packaging. Native Windows/Linux desktop
and package acceptance remain unfinished. No complete cross-platform release has
been established; required release jobs fail until their native gates pass.

## Development tree versus package

A source-tree Firefly executable uses its companion Ladybird bundle. An experimental
packaged app carries its own runtime, native launcher, physical helpers/resources and
internal libraries. Packaging must preserve those files and their security metadata;
copying only the main executable is insufficient.

## Build and inspect the sample

Build Mite/Firefly and the pinned Designer as described in [getting started](../getting-started.md)
and the [sample README](../../examples/web-component-designer/README.md). Then use
[the native release guide](../development/releases.md) for the exact packaging command,
layout, checksums and driver. The staged output includes a Mite CLI, Firefly hello-world
app and Designer app. Paths with spaces and relocation have native smoke evidence.

The packager records pins, host, architecture, signing and source/build state. Notices
live inside apps and alongside archives. Treat those records as part of the artifact;
keep them with a shared build. Inspect actual BUILD.json rather than assuming a file
named macos or windows has passed the required native tests.

## Signing and installation status

macOS packages are ad-hoc signed and not notarized. The conservative minimum OS
version is the build host. Developer-ID signing, notarization, installer formats,
older-host acceptance and an end-user installation/update lifecycle remain required.
This guide does not provide a production installation command for unverified assets.

## GitHub release workflow

Version-tag builds and manual dispatch for an exact existing tag run the required
native jobs. Verified archives/checksums become Actions artifacts. Publication creates
a draft prerelease only after every required platform succeeds, with manual publish
opt-in where applicable; an existing published release is not overwritten.

The current Windows gaps block the complete requested macOS/Windows Designer release.
No release tag is created just to make documentation or CI appear complete.
See [release implementation](../development/releases.md), [roadmap](../roadmap.md)
and [security](security.md) before distributing an app to users.
