# LessonBoard

Status: LessonBoard Windows x64 test installer 1.7.7.8 is built. Automated native checks, selected-page PDF export, document-folder relocation across restarts, installation on a runner without the Qt SDK, and uninstallation passed. Native round trips with original OpenBoard 1.7.7, per-colour thickness, reveal covers, automatic timed crash recovery, version restoration, damaged-history rejection and upgrade preservation checks passed. Physical Huion/projector and broader document regression testing remain outstanding.

Based on OpenBoard master commit `30d120cec100e97e6675d1b9be0c67eafb4c502b`. Upstream:
https://github.com/OpenBoard-org/OpenBoard

## Windows build and installation

The Windows workflow builds x64 with MSVC 2022 and Qt 6.8.3, runs native checks,
and creates a per-user Inno Setup installer. Successful builds publish
`LessonBoard-Windows-x64` in the repository's Actions artifacts.
The separate installer-validation workflow upgrades the previously delivered package
on a runner without the Qt SDK, verifies the retained lesson location and data,
repeats native checks plus colour-thickness/covers/versions and automatic timed crash recovery, then uninstalls it. The previous package for this release is LessonBoard 1.7.7.7.

LessonBoard uses its own executable, shortcuts, settings and lesson
folder. Existing OpenBoard installations are kept separate. Export/import UBZ
files to transfer lessons. The stable internal OpenBoardTeaching profile identifier
is retained so upgrading to LessonBoard preserves existing settings and lessons.
The installer is currently unsigned.

For HDMI projection of both Lesson and GraphDesk, use Windows' Duplicate display
mode. OpenBoard's separate extended-display board view shows the lesson; it does
not yet mirror the GraphDesk tab.

## Teaching controls

On Windows, LessonBoard starts maximized with a normal title bar, system menu,
and minimize/restore/maximize/close buttons. Existing frameless profiles are
updated automatically; no text-file change is needed. Separate projector views
keep their full-screen display behaviour. The fixed upstream Board-toolbar
separators are removed because they otherwise collect before Stylus after the
customizable controls are reordered.

The ten colour swatches occupy two rows of five without a Color label.
Right-click any swatch to change it. Right-click the top toolbar, or choose
Customize Toolbars from the application menu, to choose visible controls and
reorder them. There is no Customize button on the toolbar. Solid Line and
Dashed Line are separate one-click drawing choices, each available on the top
and pen toolbars. Clicking Dashed Line immediately selects dashed drawing,
including when Pen was active. Clicking it again keeps dashed drawing active.
Defaults are P for pen, E for eraser, L for Solid Line, Ctrl+Shift+L for Dashed
Line, Ctrl++ / Ctrl+- for zoom, and Ctrl+0 for reset zoom. Change these in the existing keyboard shortcut editor and bind the
same keys in a Huion driver profile for `LessonBoard.exe`.

Press Shapes to reveal a second toolbar row. Choose a rectangle, square,
circle, triangle, ellipse, right triangle or diamond, then drag on the lesson
to draw it. Squares/circles constrain their proportions. Release selects the
new shape for movement, resizing, rotation and immediate appearance editing.
Choose another shape icon or Draw next to draw another. Escape cancels an
unfinished preview. Pen/eraser/line, GraphDesk and other application modes
leave shape drawing and dismiss the row.

Border and Fill have separate ten-colour palettes, custom-colour pickers,
and No border / No fill. Width offers 1, 2, 3, 5, 8 and 12 page units. Solid or
dashed borders are independent of the line tool's style. Fill opacity runs
from 0 to 100% while the border remains opaque. The last chosen appearance
is remembered. Selecting an existing shape loads its appearance into the row;
changes support undo/redo. Triangle and right-triangle vertices have three
draggable handles constrained to the shape's current SVG bounds.

All ten main-toolbar swatches paint identical 18-by-18 colour areas, including
selected and unselected states. The empty expanding spacer is removed from
both the toolbar and customization options, including older saved orders.

Shape outlines keep their page-space thickness when stretched horizontally or
vertically, including rotated and dashed shapes. Zoom still enlarges the whole
page normally. The outline is saved as an ordinary filled SVG path so PDF
exports and original OpenBoard can display the result. Further stretching in
original OpenBoard uses its own scaling behaviour; uniform outline regeneration
requires LessonBoard.

GraphDesk opens inside its own tab. Insert adds the current graph to the lesson.
To revise an inserted graph, select it on the Lesson tab, open GraphDesk, use
Load selected graph for editing, make changes, then Update loaded graph on lesson.

## Remembered thickness, reveal covers and recovery

Thickness is remembered automatically for each visible colour, independently
for Pen, Highlighter, Solid Line and Dashed Line. For example, select red with
Pen and choose thin, then select purple and choose thick. Switching back to
red recalls thin. Red can have different widths for each of the four tools.
Switching between solid and dashed lines recalls each style's own width even
though both use OpenBoard's underlying Line tool. The selected thickness
button updates when the width is recalled.

The memory follows the RGB colour, rather than its palette slot. Moving a
colour or placing it in two swatches shares that colour's remembered width
within the same tool. Editing a swatch recalls its new colour's width. Light
and dark palettes use their visible colours; highlighter opacity does not
create a different colour preference. New colours initially use stable widths
seeded from the user's settings when upgrading. Thickness changes save immediately; memory survives restarts.
The six numbered Favourite actions, their setup dialog and Ctrl+Alt+1–6
bindings are removed. Old favourite settings are left unused for rollback;
no lessons or existing palette colours are removed.

Add cover (Ctrl+Alt+C) places an opaque rectangle above handwriting. Select it
to move, resize or rotate it. Reveal / cover (Ctrl+Alt+R) toggles selected covers;
with none selected it opens the list. Covers… controls each cover independently.
Revealed covers retain a dashed frame for selection. Cover changes support undo
and persist as ordinary SVG objects. Original OpenBoard can display/move/resize
these objects; LessonBoard supplies the reveal controls.

Versions… (Ctrl+Alt+H) lists recovery copies and saved versions. Recovery saves
run every 20 seconds when no drawing stroke or modal dialog is active. Changed
lessons receive a retained version at most once every five minutes automatically,
on normal exit, or with Save version (Ctrl+Alt+S). The latest recovery plus up to
50 versions per lesson are retained; identical content does not create redundant
versions. Each restore creates a new lesson and never replaces the current one.
An interrupted session offers recovery review on the next launch.

History is stored under the application's user-data directory in lesson-history,
separately from the configurable Documents folder. Changing Documents does not
move history. Content-addressed blobs share unchanged assets within a lesson;
restores verify every blob's SHA-256 before importing a separate copy. Recovery
covers the last completed snapshot; work after it may be lost. Page, metadata,
and editable SVG writes use atomic replacement. This does not make the whole
multi-file lesson a single filesystem transaction or replace external backups.

The new controls are included in Customize Toolbars and the keyboard shortcut
editor, so frequently used actions can remain visible and other buttons hidden.

## PDF export and document storage

In Documents, select page thumbnails before choosing Export to PDF to export only
those pages, in document order. With no selected page thumbnails, the whole
document is exported. This applies to both PDF export engines, including original
PDF backgrounds and annotations.

Preferences → Documents → Browse selects the My Documents directory. Choose
Move existing documents or Use folder without moving; the change takes effect
on the next launch. Cancel pending change keeps the current directory. Moving
requires an empty destination, verifies copied file contents before activation,
and retains the original documents if migration fails. Using a folder without
moving leaves the old documents in place and accepts an empty folder or an
existing document repository. Parent/child directory choices are rejected.

## Implemented features

- Ten pen/highlighter toolbar swatches in two rows of five, without a Color label. Existing short custom palettes are padded,
  preserving their saved colours. Right-click a swatch to edit its current
  light/dark-background palette without opening Preferences.
- Separate Solid Line and Dashed Line actions, independently configurable on
  both toolbars. Dashes become polygon geometry so the existing document/export
  path can retain them.
- Highlighter and ink use separate z-order bands above PDF/image content.
  Translucent marks on light backgrounds use multiply blending to preserve
  printed black text. Existing stroke groups are classified on load.
- Expandable Shapes row draws rectangle, square, circle, triangle, ellipse,
  right triangle and diamond. Separate border/fill palettes include no border
  and no fill, with width, solid/dashed border and independent fill opacity. The existing object frame supplies
  resize/rotate/move. Selected triangles have draggable vertex handles. Select
  an existing shape and use Shapes again to recolour it. Shape content edits
  enter the undo stack and save as SVG assets. Uniform-width outlines survive
  nonuniform resizing, rotation, undo/redo, reload and both PDF exporters.
  Changed shared SVG assets receive new IDs so other pages retain their copies.
- Customize Toolbars dialog in the application menu and top-toolbar context menu:
  check visible controls, drag to reorder, restore defaults. Web and Backgrounds
  are included. Laser pointer starts hidden. Presentation proxies preserve
  shortcuts for hidden controls. The menu command stays accessible.
- Defaults P/E/L select pen/eraser/solid line; Ctrl+Shift+L selects dashed line. Teaching zoom actions use Ctrl++,
  Ctrl+-, Ctrl+0, registered with the existing shortcut editor. Ctrl+0 resets
  zoom; it is not a separate fit-to-page calculation. Single-letter shortcuts
  are suppressed in text/equation inputs. Existing saved user bindings win.
- Internal Lesson/GraphDesk tabs. GraphDesk is bundled as a Qt resource and
  remains loaded while switching tabs. Insert adds an SVG graph to the active
  lesson. Select a previously inserted graph, open GraphDesk, and use Load
  selected graph for editing, then Update loaded graph on lesson. Graph data
  is embedded in SVG metadata and content edits enter the undo stack.

## Compatibility with original OpenBoard

The document format remains OpenBoard's UBZ format and file version 4.8.0.
Native Windows round-trip tests against the official v1.7.7 source passed:

1. Create and export a three-page lesson using original OpenBoard.
2. Import into LessonBoard, add a dashed line, filled/outlined shapes and an SVG
   graph carrying four-function GraphDesk project metadata, and a reveal cover; save and export.
3. Import into original OpenBoard, add editable text, move/rotate/scale the graph,
   then save and export again.
4. Reopen and save in LessonBoard, including recognition of shape, graph and reveal-cover metadata.

The checks compare freshly read files rather than cached scenes: page counts,
PDF page indexes and file hashes, editable text, handwriting/dashed-line geometry
and colours, SVG file hashes and object transforms. Every source UBZ archive and
the original lesson directory remain unchanged during cross-application import.
Archive CRC checks and nine cross-application rendered-page comparisons also pass.
Every SVG referenced by a page must exist; shape/graph/cover counts are checked
against those live references. Unreferenced older SVGs can remain intentionally
to protect assets shared by other pages.

The original test build adds only an automated test entry point and isolated
executable/profile identity (plus startup preferences). Its document readers,
writers, graphics and persistence implementations are unchanged. This is a test
against original 1.7.7 source with the same compiler/Qt runtime, not an automated
UI test of the official distributed Windows installer.

These are generated fixtures, not the user's existing lessons. Older OpenBoard
versions, every possible asset type, and unusual/damaged documents are untested.
Original OpenBoard handles new shapes, graphs and covers as ordinary SVG objects;
triangle-vertex controls, GraphDesk editing and cover reveal controls require
LessonBoard. Highlighter compositing remains application-specific. The nine
cross-application scene renders include their PDF backgrounds synchronously:
the largest changed-pixel fraction is 0.001014 (about 0.102%) above a 12-level
threshold. LessonBoard-to-original and original-resave-to-LessonBoard comparisons
have zero changed pixels in these fixtures; exact identity for other lessons is
not claimed. Full fractions are recorded in compatibility-summary.json.

## Test installer provenance

Compiled source: `04203d83498a8a057660ed3a9d463badf98bd859`.

- Build and native checks: https://github.com/Iamheregithub/OpenBoard/actions/runs/39831373914
- Installation/native checks/uninstallation: https://github.com/Iamheregithub/OpenBoard/actions/runs/39831433058
- Setup filename: `LessonBoard-Windows-x64-Setup.exe`
- Setup SHA-256: `efdda0279f50224c43c86202585f01b4d83892cdd9244f982229ddbd24940ef5`

The package is unsigned and is intended for initial testing. Test on copies of
lessons before using it in class. Hardware pressure, projector behaviour and
all document import/export paths have not been verified by these automated checks.

## Validation completed

- Shape toolbar opens from Shapes and hides on switching tools. All seven
  shape choices are drawn through mouse events; tablet events draw a diamond.
  Triangle/right-triangle handles, cancelled previews without undo changes,
  border/fill palette edits, no border, opacity pixels, style undo/redo and
  fresh SVG reload are checked. Synthetic tablet events verify event handling,
  not physical Huion hardware or pressure.
- Actual colour-pixel counts verify identical swatch sizes in checked states;
  customization is opened and must contain no unnamed spacer entry. The
  contextual row must remain below the main toolbar with compact height; process
  exit is required to succeed after all native checks.

- The compact two-row palette, all ten colour choices, absence of a toolbar
  Customize button and presence of its menu command are checked natively.
- Dashed Line activates immediately from Pen; repeated selection stays dashed.
  Solid Line restores solid drawing. Actual rendered strokes confirm gaps.
  Top and pen-toolbar visibility settings are exercised independently.
- Shape outlines are measured after horizontal/vertical scaling and rotation,
  through scene rendering and ordinary SVG rendering. Undo/redo, fresh reload,
  copied transforms, shared-page asset protection and both PDF exporters pass.

- Native Windows caption/system-menu/minimize/maximize styles and enabled Close
  command are checked. Minimize, restore and maximize work; switching between
  Documents and the lesson retains the window controls and maximized state.
- Toolbar customization is opened, controls are reordered and restored, and
  leading dividers remain absent. These checks run across native process
  restarts and again in the installed deployment.

- Per-colour thickness is independently assigned and recalled for Pen,
  Highlighter, Solid Line and Dashed Line through their actual toolbar actions.
  Repeated colour/tool switches update the thickness button and preserve both
  palettes. Duplicate/recoloured swatches follow their actual RGB colours;
  background palette changes and highlighter alpha are checked. The memory is
  verified in two further processes, including after forced termination/recovery.
  Numbered Favourite controls must be absent.
- Covers hide existing handwriting, reveal independently, support undo/redo,
  retain transformations and reveal state after reloading, and stay above ink.
- Versions deduplicate unchanged content. Restoring an earlier version creates
  a separate readable lesson while the current one retains its newer content.
- Corrupted history blobs are rejected without changing the current lesson.
- The actual 20-second automatic timer saves a draft before forced process
  termination (exit 89). The next process detects an unclean session and restores
  the draft as a separate readable lesson. The installed deployment repeats this.

- Documents Export to PDF through its actual menu action: selected pages 3–6,
  no selected pages (whole document), and one selected page, with both PDF engines.
  Verified page counts, document order, original PDF backgrounds and annotation pixels.
- Noncontiguous and duplicate page-selection indexes through the simple exporter.
- Preferences Documents tab is visible and its current folder is displayed.
- Moving documents from C: to D: across restart, with a file-content hash check.
- Switching to a new folder without moving across restart; old documents retained.
- Unsafe occupied-destination and same-folder migration choices rejected.

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

Manual checks remaining:

1. Verify Windows Huion pressure, pen/eraser switching, toolbar shortcuts, and
   HDMI duplicate/extended display behaviour. In extended-display mode the
   separate board display has not been adapted to mirror the GraphDesk tab.
2. Test highlights drawn before/after ink, printed PDF text, undo/redo, and
   reopening old and new documents, including overlapping coloured strokes.
3. Test dashed line preview, erasing, save/reload, and PDF export.
4. Test triangle handles, transformations, recolouring, undo/redo, page/object
   duplication, save/reload, and PDF export.
5. Test toolbar hiding/reordering/restoring across restarts, display text modes,
   and narrow window sizes. Confirm hidden-tool shortcut behaviour.
6. Test GraphDesk insert/re-edit/update, changed page during insertion, saved
   document reopening, all existing graph functions, and Qt SVG rendering.

## Tests

From tests/teaching, install its package.json dependencies and run npm test.
The optional npm run browser command additionally needs Playwright Chromium.
No tests here substitute for the Windows/Qt/tablet checks above.
