# OpenBoard teaching fork — source checkpoint

Status: initial source implementation, not a tested release or Windows installer.

Based on OpenBoard master commit 30d120c. The complete base commit is recorded in
the source checkpoint's UPSTREAM.txt. Upstream:
https://github.com/OpenBoard-org/OpenBoard

## Implemented in source

- Ten pen/highlighter toolbar swatches. Existing short custom palettes are padded,
  preserving their saved colours. Right-click a swatch to edit its current
  light/dark-background palette without opening Preferences.
- Solid/dashed line toggle. Dashes become polygon geometry so the existing
  document/export path can retain them.
- Highlighter and ink use separate z-order bands above PDF/image content.
  Translucent marks on light backgrounds use multiply blending to preserve
  printed black text. Existing stroke groups are classified on load.
- Shapes button inserts rectangle, square, circle, or triangle, with separate
  outline/fill colours and optional fill. The existing object frame supplies
  resize/rotate/move. Selected triangles have draggable vertex handles. Select
  an existing shape and use Shapes again to recolour it. Shape content edits
  enter the undo stack and save as SVG assets.
- Customize Toolbars dialog, also accessible from a top-toolbar context menu:
  check visible controls, drag to reorder, restore defaults. Web and Backgrounds
  are included. Laser pointer starts hidden. Presentation proxies preserve
  shortcuts for hidden controls. The Customize control stays accessible.
- Defaults P/E/L select pen/eraser/line. Teaching zoom actions use Ctrl++,
  Ctrl+-, Ctrl+0, registered with the existing shortcut editor. Ctrl+0 resets
  zoom; it is not a separate fit-to-page calculation. Single-letter shortcuts
  are suppressed in text/equation inputs. Existing saved user bindings win.
- Internal Lesson/GraphDesk tabs. GraphDesk is bundled as a Qt resource and
  remains loaded while switching tabs. Insert adds an SVG graph to the active
  lesson. Select a previously inserted graph, open GraphDesk, and use Load
  selected graph for editing, then Update loaded graph on lesson. Graph data
  is embedded in SVG metadata and content edits enter the undo stack.

## Validation completed

- Git diff whitespace validation.
- Qt resource XML and UI form XML parse; all referenced resource files exist.
- All four shipped selected pen/highlighter palettes have ten entries.
- Embedded GraphDesk JavaScript syntax check with Node.
- GraphDesk DOM test: startup, four simultaneous functions, dashed SVG output,
  project restore, and base64 metadata round trip.

The DOM test uses jsdom with a ResizeObserver stub; it does not test layout,
handwriting, Qt WebEngine, native selection handles, PDF output, or pixels.

## Outstanding validation

No native build has completed. This environment has no Windows toolchain or Qt
SDK. Attempts to obtain Qt dependencies encountered failed/502 downloads.
The Playwright browser download also failed. Native C++ compile errors and
runtime issues may therefore remain. Do not treat this checkpoint as a working
Windows application.

Required next checks on a configured build host:

1. Compile using the upstream build prerequisites; both qmake and CMake source
   lists include the teaching controller. CMake is the upstream Linux path;
   use the upstream Windows build instructions for Windows packaging:
   https://github.com/OpenBoard-org/OpenBoard/wiki/Build-OpenBoard-on-Windows
2. Run with a separate test profile/document folder.
3. Verify Windows Huion pressure, pen/eraser switching, toolbar shortcuts, and
   HDMI duplicate/extended display behaviour. In extended-display mode the
   separate board display has not been adapted to mirror the GraphDesk tab.
4. Test highlights drawn before/after ink, printed PDF text, undo/redo, and
   reopening old and new documents, including overlapping coloured strokes.
5. Test dashed line preview, erasing, save/reload, and PDF export.
6. Test triangle handles, transformations, recolouring, undo/redo, page/object
   duplication, save/reload, and PDF export.
7. Test toolbar hiding/reordering/restoring across restarts, display text modes,
   and narrow window sizes. Confirm hidden-tool shortcut behaviour.
8. Test GraphDesk insert/re-edit/update, changed page during insertion, saved
   document reopening, all existing graph functions, and Qt SVG rendering.

## Tests

From tests/teaching, install its package.json dependencies and run npm test.
The optional npm run browser command additionally needs Playwright Chromium.
No tests here substitute for the Windows/Qt/tablet checks above.
