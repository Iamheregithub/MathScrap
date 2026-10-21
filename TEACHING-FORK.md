# OpenBoard Teaching

Status: the first Windows x64 test installer is built. Automated native checks, installation on a runner without the Qt SDK, and uninstallation passed. Physical Huion/projector and full PDF/document regression testing remain outstanding.

Based on OpenBoard master commit `30d120cec100e97e6675d1b9be0c67eafb4c502b`. Upstream:
https://github.com/OpenBoard-org/OpenBoard

## Windows build and installation

The Windows workflow builds x64 with MSVC 2022 and Qt 6.8.3, runs native checks,
and creates a per-user Inno Setup installer. Successful builds publish
`OpenBoard-Teaching-Windows-x64` in the repository's Actions artifacts.
The separate installer-validation workflow installs the package on a runner
without the Qt SDK, repeats native checks, then uninstalls it.

OpenBoard Teaching uses its own executable, shortcuts, settings and lesson
folder. Existing OpenBoard installations are kept separate. Export/import UBZ
files to transfer lessons. The installer is currently unsigned.

For HDMI projection of both Lesson and GraphDesk, use Windows' Duplicate display
mode. OpenBoard's separate extended-display board view shows the lesson; it does
not yet mirror the GraphDesk tab.

## Teaching controls

Right-click any colour swatch to change it. Right-click the top toolbar, or use
Customize Toolbars, to choose visible controls and reorder them. Defaults are
P for pen, E for eraser, L for line, Ctrl++ / Ctrl+- for zoom, and Ctrl+0 for
reset zoom. Change these in the existing keyboard shortcut editor and bind the
same keys in a Huion driver profile for `OpenBoardTeaching.exe`.

Use Shapes to insert a shape; select it with the selection tool to move, resize
or rotate it. Use Shapes again to edit its outline/fill. Drag a selected
triangle's three circular handles to move its vertices.

GraphDesk opens inside its own tab. Insert adds the current graph to the lesson.
To revise an inserted graph, select it on the Lesson tab, open GraphDesk, use
Load selected graph for editing, make changes, then Update loaded graph on lesson.

## Implemented features

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

## Test installer provenance

Compiled source: `e1518aabdee4bbf068fa5581ab9f4ab2b361927d`.

- Build and native checks: https://github.com/Iamheregithub/OpenBoard/actions/runs/39648974503
- Installation/native checks/uninstallation: https://github.com/Iamheregithub/OpenBoard/actions/runs/39649077368
- Setup filename: `OpenBoard-Teaching-Windows-x64-Setup.exe`
- Setup SHA-256: `eb3ac30a4f270b88311c985a950f5d5ea46c3d5e88d81030321019517c11d74a`

The package is unsigned and is intended for initial testing. Test on copies of
lessons before using it in class. Hardware pressure, projector behaviour and
all document import/export paths have not been verified by these automated checks.

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

Windows compilation and native startup, toolbar, ink/highlight layering and
pixels, triangle undo/redo, scene switching, ten colours, and GraphDesk export/restore
checks passed on GitHub Actions. Installation, installed native checks and
uninstallation also passed on a separate Windows runner without the Qt SDK. Local DOM tests do not replace Windows checks or
physical tablet/projector testing.

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
