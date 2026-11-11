# Menu and MenuItem

Status: partial main-process application menus. Local macOS Release acceptance
covers actual Qt native menu bars/actions with queued LibJS callbacks and the
public Firefly main host. Linux uses per-window Qt menu bars; fresh hosted native
acceptance is required. Native Windows desktop remains unavailable.

```js
import {Menu, MenuItem} from 'firefly';

const showDetails = new MenuItem({
  id: 'details', label: 'Show details', type: 'checkbox',
  click(item, window, event) {
    console.log(item.checked, window?.id, event.metaKey);
  }
});
const menu = Menu.buildFromTemplate([
  {label: 'Example', submenu: [showDetails, {type: 'separator'}, {role: 'quit'}]}
]);
Menu.setApplicationMenu(menu);
showDetails.enabled = false;
```

The explicit Electron alias and CommonJS expose the same main constructors.
Renderer/preload and standalone Mite do not expose these privileged APIs.
There is no synthesized default application menu. Menu.setApplicationMenu(null)
works before readiness, removes Firefly-owned bars and returns undefined. This
also supports Code-OSS's early menu suppression; it does not make its complete
menubar service compatible. OS-provided macOS application entries are not a full
Electron menu reproduction.

## Menu

| Member | Current contract |
| --- | --- |
| new Menu() | Empty menu; construct its items before installation |
| Menu.buildFromTemplate(template) | Array of MenuItem instances or option objects; nested submenu arrays are converted recursively |
| Menu.setApplicationMenu(menuOrNull) | Install/replace the application menu or remove owned bars; returns undefined |
| Menu.getApplicationMenu() | Currently installed Menu identity, initially null |
| menu.append(item) | Append a MenuItem before installation; returns undefined |
| menu.insert(position, item) | Integer index from zero through items.length; before installation only |
| menu.items | Frozen snapshot array of the same MenuItem objects |
| menu.getMenuItemById(id) | Recursive lookup of a nonempty string id, or null |

Application-menu top-level entries must be submenus. Each tree has at most 256
items, depth eight and unique nonempty ids. Shared/cyclic submenus and repeated
MenuItem ownership within a tree are rejected. Structural mutation of an installed
menu or its submenus is explicitly unsupported; build a replacement menu instead.
Property changes on installed items republish the native tree. Invalid/rejected
native updates preserve the previous JavaScript property and installed menu.

## MenuItem

new MenuItem(options) accepts only the following options. Unknown options throw
ERR_FIREFLY_UNSUPPORTED_API instead of being silently ignored.

| Option / property | Current contract |
| --- | --- |
| id | Optional string, default empty; read-only |
| label | Mutable string; default empty or a supported role label |
| type | Read-only normal, checkbox, separator or submenu; inferred from submenu when omitted |
| submenu | Read-only Menu or template array; required exactly for submenu type |
| enabled, visible | Mutable booleans, default true; ancestors also gate activation |
| checked | Mutable boolean for checkbox only, default false |
| click | Optional mutable callback; normal/checkbox items only |
| role | Read-only action role, or window/help/services submenu role; see below. Action roles take precedence over click |
| accelerator | Mutable bounded single-key Electron accelerator, or null; real QAction shortcut |
| userSettingsLabel | Mutable bounded JS-only metadata; not sent as a native command |
| commandId | Read-only positive process-local command identifier, assigned automatically |

A native selection queues click(menuItem, focusedBrowserWindowOrUndefined, event)
on the main VM's event loop. event contains shiftKey, ctrlKey, altKey and metaKey
booleans, plus triggeredByAccelerator for actual shortcut activation. Checkbox state changes before the callback and is synchronized across
owned native bars and newly attached windows. A throwing callback uses the existing
main callback error path. Native actions retain IDs and generations, never JS roots.
Old selections queued before replacement/removal/property republishing are discarded.

The quit role calls the ordinary cancellable app.quit() path. close calls close()
on the focused live owned BrowserWindow if one exists. No window is fabricated when
focus belongs elsewhere. Focused-window routing and physical keyboard modifiers
still need dedicated OS interaction acceptance; current native automation invokes
real QAction signals without physical clicks.

## Limits and unsupported features

Labels are bounded to 1,024 UTF-16 code units in JavaScript and independently to
1,024 UTF-8 bytes by the native host, without NUL. The native serialized tree is
limited to 64 KiB; positive signed-int32 generations/command IDs cannot wrap.
IDs use the same JavaScript text bound. Type/value failures use ERR_INVALID_ARG_TYPE,
ERR_INVALID_ARG_VALUE or ERR_OUT_OF_RANGE in the JS policy; native schema/UTF-8
budget errors throw TypeError/RangeError. These limits are explicit differences.

Single-key accelerators accept ASCII letters/digits/punctuation, F1–F24 and
navigation/editing keys, with Shift/Control/Ctrl/Alt/Meta/Super and CmdOrCtrl aliases.
Command/Cmd/Option are macOS aliases. Plus and Ctrl++ forms are supported.
The ASCII accelerator text is at most 256 characters. Chord sequences, media keys,
AltGr/lock/keypad keys and accelerators on submenu/separator items fail explicitly.
Physical keyboard-layout, hidden-item and Cocoa shortcut behavior still need
interaction acceptance; parsing and native construction are separate evidence.

Action roles are quit, close, minimize, zoom, hide, hideOthers, unhide, front,
cut, copy, paste, pasteAndMatchStyle, undo, redo and selectAll. Minimize/zoom use
the live focused window. Cocoa owns actual hide/show/front application actions and
Windows/Help/Services menu registration. Those macOS roles require the real Cocoa
provider; they fail explicitly under offscreen/Linux providers. Duplicate native
submenu roles are rejected before replacing the old menu. Editing scope is below.

Popup/context menus (popup/closePopup), global shortcuts, radio items, icons,
other roles, positioning options, native menu lifecycle events/EventEmitter behavior
and per-window setMenu remain unimplemented. Unknown roles/options still fail
explicitly. The combined [menus example](../examples/menus.md) remains blocked on
context-menu support and is not counted as a passing sample.

## Validation

```sh
cmake --build build/renderer --target check-firefly-menu check-firefly-menu-host --parallel 3
```

The policy runs against Node/Mite. The native probe uses real Qt actions and actual
LibJS callbacks, including disabled/hidden items, Unicode labels, checkbox state,
property changes, roles, new-window attachment and stale-event teardown. The public
host test checks actual ESM/CommonJS aliases and before/after-ready install/update/
removal without a BrowserWindow. Linux CI runs both native gates under Xvfb. These
are scoped component tests, not a Code-OSS workbench acceptance or physical menu
interaction suite. See [ADR 0128](../architecture/0128-application-menus.md).
API baseline: [Electron Menu](https://www.electronjs.org/docs/latest/api/menu) and
[MenuItem](https://www.electronjs.org/docs/latest/api/menu-item).

## Native editing scope

Editing roles target the actual focused owned renderer, including a child
WebContentsView; they do not fall back to an arbitrary window when no renderer
has focus. Undo, redo and selectAll use native engine editing commands. Copy/cut
read genuine DOM selection text and publish plaintext to the Cocoa/X11 system
clipboard. Paste and pasteAndMatchStyle currently both deliver plaintext from
that clipboard through native focused-frame paste IPC. Rich HTML, images and
Monaco's custom clipboard/event behavior are not accepted by this implementation.
A Code-OSS menu constructing successfully does not establish working editor edits.

Selection callbacks revalidate the view, document, renderer process and epoch
before writing the clipboard. There are at most 32 pending selection operations
per webContents, with a 16-MiB plaintext ceiling. Empty selections leave the
clipboard unchanged. Native cut removes the selection before its asynchronous
text response; a stale or oversized response is discarded rather than copying
unauthorized data, so cut does not promise transactional clipboard rollback.
Offscreen clipboard storage is not advertised as an OS clipboard provider.
Fresh native clipboard/edit workflow acceptance remains pending.

The expanded accelerator/role/editing source passes strict native compilation and
the existing controlled Node menu policy. The native Release host/helper build
passes. A single actual Cocoa Code-OSS launch stops earlier at app.dock.setMenu
and a renderer splash-copy mutation, so it does not yet accept the new native role
registration, physical shortcuts or editing actions. No additional GUI suite is run.

## macOS Dock menu

`app.dock.setMenu(menuOrNull)` and `app.dock.getMenu()` now have an owned native
implementation. Dock roots may contain normal leaf items; role/accelerator Dock
items remain refused. The Dock menu has separate native generations and event
routing from the application menu. Sharing installed item ownership between both
menus is rejected. Property updates republish the current tree; failed replacement
preserves its previous native/JS state. Clearing/shutdown restores a preceding
native registration only while the installed Dock menu is still Firefly-owned.

The provider calls genuine QMenu.setAsDockMenu on Cocoa. It validates the Qt native
delegate registration interface and does not leave Qt's retained native menu behind
after teardown. Offscreen/Linux providers refuse explicitly. Bounce, badge, icon,
show/hide and other Dock methods remain unsupported. Controlled policy and native
compilation are separate from native Dock click/construction acceptance, which
remains pending; no additional Cocoa session is run in this slice.
