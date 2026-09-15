# Publish a GitHub release

`scripts/release-github.py` uploads completed installers to `kangu/mindarchy`, generates download and installation notes, and adds `SHA256SUMS` and `release-assets.json`. It runs on macOS, Linux, and Windows with Python 3.9+ and the [GitHub CLI](https://cli.github.com).

It creates a **draft** by default. `--publish` makes it public only after downloading all uploaded assets and verifying their checksums. The script does not build, sign, notarize, or fetch remote builds.

## Prepare the binaries

Build every selected platform with the same version. The default selection requires macOS, Windows, and Omarchy; use `--platforms omarchy` for an Omarchy preview, or another explicit comma-separated subset.

| Platform | Default input | Included assets |
| --- | --- | --- |
| macOS | `dist/macos/<version>/<arch>/<timestamp>/` | Latest completed `release.json` per architecture, with its PKG and DMG; both must match manifest hashes. |
| Windows | `artifacts/windows/` | `Mindarchy-<version>-windows-x64-setup.exe` |
| Omarchy | `artifacts/omarchy/` | `mindarchy-<version>-<pkgrel>-<arch>.pkg.tar.zst` (also xz/gz), plus accompanying `.sig` files if present. |

At least one architecture must exist for each selected platform. All available matching architectures are included; missing architectures are not fabricated or required. For Omarchy, stage only one package revision per architecture. Incomplete macOS build directories without a manifest are ignored; a corrupt latest completed build fails validation rather than falling back to an older build.

The remote Windows and Omarchy build scripts leave output on their build machines. Copy the installers to the default folders before publishing, or provide `--windows-dir`, `--omarchy-dir`, and `--macos-dir`. The macOS override must contain the version/architecture/timestamp hierarchy. Windows and Omarchy overrides point directly to the directory containing their installers. Paths with spaces should be quoted.

Only installer files and optional package signatures are selected. App bundles, logs, test screenshots, source archives, and older versions are excluded. The public JSON records filenames, sizes, hashes, architectures and available signing metadata, with no local filesystem paths. Windows and Omarchy hashes are computed from the staged files; their build scripts do not provide a comparable macOS-style manifest, so check their build provenance before staging them.

## Preview locally

From the repository root:

```sh
python3 scripts/release-github.py --version 0.1.4 --dry-run
```

For the currently available macOS build:

```sh
python3 scripts/release-github.py --version 0.1.4 --platforms macos --prerelease --dry-run
```

Dry runs need neither `gh` nor authentication, and do not change GitHub or create local release output. They print the selected files, hashes and proposed notes. Remote tag existence and permissions are checked only during a real upload.

## Configure GitHub

Install GitHub CLI, then authenticate with an account that can create releases in the destination repository:

```sh
gh auth login --hostname github.com
```

On macOS, GitHub CLI can be installed with `brew install gh`. In automation, use `GH_TOKEN` with repository Contents write permission; do not put tokens into arguments or source files.

The release tag must already exist on GitHub. Tag the source commit actually used to build these binaries, then push that tag. For example, after replacing `BUILD_COMMIT_SHA` with that commit:

```sh
git tag -a v0.1.4 BUILD_COMMIT_SHA -m "Mindarchy 0.1.4"
git push origin refs/tags/v0.1.4
```

Do not repeat tag creation if the correct tag already exists. The publisher uses GitHub CLI's `--verify-tag`; it never silently creates a tag at the current default branch. Current package manifests do not record a source commit, so the script cannot prove that the binaries were built from the tagged commit.

## Create, review, and publish

Create an Omarchy preview draft:

```sh
python3 scripts/release-github.py --version 0.1.4 --platforms omarchy --prerelease
```

Or create a release with all three platforms:

```sh
python3 scripts/release-github.py --version 0.1.4
```

Add `--notes-file path/to/changes.md` to prepend your release highlights to the generated notes. Add `--repo OWNER/REPO` for a different GitHub repository.

After reviewing the draft, repeat the same version, platforms, artifact folders, notes file and pre-release setting with `--resume --publish`:

```sh
python3 scripts/release-github.py --version 0.1.4 --platforms omarchy --prerelease --resume --publish
```

To create and publish in one invocation, use `--publish` on the initial command. It still stages a draft and verifies every asset first. A draft is not downloadable by the general public; the printed release link becomes public after publication, provided the repository is public.

## Interrupted uploads

Rerun with `--resume` and the same inputs. The script verifies existing draft assets and uploads only missing files. It refuses differing files, duplicate or unexpected assets, and already published releases. A failure leaves the draft available for inspection; it does not delete it or overwrite assets. Fix any conflicting draft assets manually before resuming, or use a new version. Avoid concurrent publisher runs for the same tag.

Verification downloads all assets, so allow temporary disk space for both a staged copy and a downloaded copy, plus the original build files. macOS signing and notarization states are copied from the build manifest into the notes; uploading an unsigned installer does not make it signed or notarized.

## Script tests

```sh
python3 -m unittest discover -s tests -p test_release_github.py -v
```

Tests use temporary fixture artifacts and a mocked GitHub CLI; they do not create remote releases. CLI behavior follows the official [release create](https://cli.github.com/manual/gh_release_create), [upload](https://cli.github.com/manual/gh_release_upload), [download](https://cli.github.com/manual/gh_release_download), and [edit](https://cli.github.com/manual/gh_release_edit) commands.
