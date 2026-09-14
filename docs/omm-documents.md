# Open Mindmap documents (`.omm`)

An `.omm` document is the existing UTF-8 JSON document, saved with a dedicated extension. There is no archive, binary header, embedded screenshot, or platform-specific wrapper. The schema remains `format: "mindarchy"`, `version: 1`. Text, Task, Date, calendar entries, themes, layout, manual offsets, notes, folding and relationships retain their existing representation.

Save dialogs default to `.omm`. Open dialogs accept `.omm` and `.json` files containing the current `mindarchy` v1 document format. Older prototype format identifiers are not supported. The app still validates the content regardless of its extension and saves atomically.

```sh
jq . 'Project.omm'                 # inspect/format the JSON
cp 'Project.omm' 'Project.json'     # exact same content, another filename
mindarchy 'Project.omm'          # positional argument / file-manager opening
mindarchy --render-preview 'Project.omm' --preview-output preview.png
```

The preview command needs no display and accepts `--preview-size 512` (32–4096). It recomputes the saved document's layout with the same engine and shares shape, branch and calendar drawing with the canvas. It renders themes, mirrored manual branches, rich text, tasks, and Date totals, without editor controls or selection. Very large maps omit illegibly small labels. A preview shows the last saved file, not unsaved changes in an editor; Date comments retain their normal hidden presentation. The raster preview is read-only, so its calendar arrows and days are not interactive.

## macOS

The application exports UTI `org.mindarchy.omm`, conforming to `public.json`, with MIME type `application/x-omm+json`. It registers only `.omm` as an owned document; other apps' JSON associations are unaffected. Finder Open events and command-line documents are supported. Later Finder opens create another application instance so they cannot replace an unsaved map.

The CMake macOS build includes `Contents/PlugIns/OMMPreview.appex`, a modern, sandboxed Quick Look Preview extension. Finder selects this extension for `.omm` and pressing Space requests a new PNG from the saved JSON. The helper uses Qt Core/Gui with the offscreen platform plugin, not the app's QML interface. It reads the file granted by Quick Look and does not write the document. The release workflow deploys its Qt dependencies and signs the extension before sealing the enclosing application. Developer ID signing and notarization use the existing release credentials.

Install the generated app using the `.pkg` installer or drag it into Applications from the `.dmg`. Both contain the preview extension. If the extension is disabled, enable **Open Mindmap Preview** in System Settings → General → Login Items & Extensions → Quick Look. macOS may cache previews until the file changes or Quick Look is reopened. Legacy `.json` files keep the system's normal JSON preview; rename/save as `.omm` to request this map preview.

## Omarchy / Linux

The shared-mime-info definition maps `.omm` to `application/x-omm+json`, subclassing `application/json`. The desktop entry passes a filename to the app; the thumbnailer invokes the same display-independent renderer. Install for the current user after building:

```sh
python3 scripts/install-linux-filetype.py --binary build/mindarchy
gio info -a standard::content-type examples/Welcome.omm
xdg-mime query default application/x-omm+json
```

The installer puts the binary in `~/.local/bin`, registers the desktop entry/MIME/thumbnailer under `$XDG_DATA_HOME` (normally `~/.local/share`), and updates the caches. CMake/qmake installation also provides these metadata files for system packaging; package installation must refresh the MIME/desktop caches. Over SSH, `xdg-mime query filetype` may fall back to `file` and report `application/json`; `gio info` checks the registered MIME database used by Thunar. Nautilus's thumbnail sandbox cannot execute binaries under `~/.local/bin`. For its image thumbnails, install system-wide:

```sh
sudo python3 scripts/install-linux-filetype.py --system --binary build/mindarchy
```

If a user thumbnailer was previously installed, update its `Exec` and `TryExec` paths to `/usr/local/bin/mindarchy` (or remove that user override so the system thumbnailer is used). The current Omarchy installation is user-scoped: opening and standalone PNG rendering were verified, while Nautilus thumbnails require this system installation. Generic Linux thumbnailers depend on the file manager (for example Tumbler with Thunar); a universal Finder-style Space preview is not part of the Linux desktop standard. File opening and the standalone renderer do not depend on a particular file manager.

## Implementation references

- [Apple Quick Look UI](https://developer.apple.com/documentation/QuickLookUI): modern preview app extensions.
- [Apple preview provider](https://developer.apple.com/documentation/quicklookui/qlpreviewprovider): data-based preview replies.
- [Shared MIME-info specification](https://specifications.freedesktop.org/shared-mime-info/latest-single/): extensions and MIME inheritance.
