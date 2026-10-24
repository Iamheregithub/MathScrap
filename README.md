# MathScrap

A classroom whiteboard for mathematics, built on [OpenBoard](https://github.com/OpenBoard-org/OpenBoard).

MathScrap combines handwriting, editable graphs, step-by-step formula tools, shapes, and reveal covers in one teaching workspace.

[**Download MathScrap for Windows x64**](https://github.com/Iamheregithub/MathScrap/releases/latest/download/MathScrap-Windows-x64-Setup.exe) · [Release notes and checksums](https://github.com/Iamheregithub/MathScrap/releases/latest) · [Development guide](TEACHING-FORK.md)

## What you can do

- **Write and draw:** ten editable colour swatches, pen and highlighter, solid or dashed lines, and shapes with separate border and fill controls. Each colour remembers its thickness for each drawing tool.
- **Teach formulas step by step:** quadratic formula, discriminant, Pythagorean theorem, distance, slope, sine law, cosine law, polynomial derivatives, and difference quotients. Editable inputs show substitution and intermediate steps where applicable. Coordinate tools group inputs as points.
- **Graph with GraphDesk:** plot multiple functions and inequalities, add points and geometric constructions, prepare student copies and answer keys, and export SVG or PNG. Insert graphs into lessons and load them again for editing.
- **Reveal work gradually:** place independent covers over lesson content and reveal or hide them as you teach.
- **Make the workspace yours:** customize toolbar visibility and order, use the floating pen tray and More tools menu, and find workspace, writing, document, keyboard and display options in searchable Preferences.
- **Manage lessons:** organize pages and documents, export selected pages to PDF, and restore recovery snapshots or saved versions as separate lessons.

## Quick controls

| Control | Behaviour |
| --- | --- |
| Line | Click to select the remembered line style; double-click to switch solid/dashed. |
| Zoom | Click to activate; click again to return to Select. Double-click to switch zoom in/out. |
| Shapes | Open the shape controls, choose a shape, and drag to draw. |
| Functions | Insert an editable math formula object. |
| Customize Toolbars | Open from the application menu or toolbar context menu. |
| Preferences | Search settings, change keyboard shortcuts, and reset options within the selected section. |

GraphDesk has its own tab. To revise an inserted graph, select it in the lesson, open GraphDesk, load the selected graph, edit it, and update the graph on the lesson.

## Download and platform status

The current published release is **MathScrap 1.7.7.25**. Windows x64 installers and SHA-256 checksums are available on the [Releases page](https://github.com/Iamheregithub/MathScrap/releases). The installer is currently unsigned.

MathScrap's automated build and application checks target Windows with Qt 6.8.3 and MSVC 2022. OpenBoard is cross-platform, but MathScrap-specific macOS and Linux packages have not been validated or published here.

The active MathScrap development branch is [`graphdesk-teaching-fork`](https://github.com/Iamheregithub/MathScrap/tree/graphdesk-teaching-fork). Use that branch when building the latest application; `master` also receives shared documentation and security fixes.

## Testing and security

Release 1.7.7.25 passed:

- Native Windows drawing, formula, Zoom, GraphDesk, document/PDF and workspace checks.
- Preferences navigation, shortcut editing, scoped resets and saving on close; selection-box alignment with Pages open/closed, three zoom levels and both drag directions.
- Exploratory live-canvas checks at three zoom levels with Pages open and closed: object drags, shape and text previews, pixel drag threshold, tool switching, Undo/Redo, lesson save/reopen, and formula insertion after zoom. The floating zoom control no longer covers the bottom tool tray.
- Recovery and saved-version checks, including interrupted-session recovery.
- File round trips through original OpenBoard 1.7.7.
- JavaScript DOM and Chromium sandbox regression tests.

[Windows validation run](https://github.com/Iamheregithub/MathScrap/actions/runs/40110176247) · [Security review](docs/security/review-2026-10-24.md) · [Current security and quality status](https://github.com/Iamheregithub/MathScrap/security)

CodeQL scans the default and development branches on pushes and pull requests, with scheduled scans as well. Secret scanning, push protection, Dependabot alerts and security updates, and private vulnerability reporting are enabled. Report vulnerabilities privately using the instructions in [SECURITY.md](SECURITY.md).

Physical tablet pressure, projector behaviour, and the full range of existing classroom documents still need manual testing. Automated checks use generated fixtures.

## OpenBoard compatibility and attribution

MathScrap retains OpenBoard's UBZ lesson format. Graphs, formula cards, shapes and covers use SVG objects; MathScrap supplies their additional editing and teaching controls.

This is an independent fork, formerly called LessonBoard, based on the work of OpenBoard, Open-Sankoré and Uniboard contributors. See [COPYRIGHT](COPYRIGHT) and the [GPLv3 licence](LICENSE). Upstream project information and cross-platform build guidance are available from [OpenBoard](https://github.com/OpenBoard-org/OpenBoard).
