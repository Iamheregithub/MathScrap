# MathScrap development and validation guide

MathScrap is the mathematics teaching fork in this repository. It was previously called LessonBoard. The active application branch is `graphdesk-teaching-fork`; `master` receives shared documentation and security fixes but does not yet contain every recent application change.

See [README.md](README.md) for features, controls and the current download.

## Current release

**MathScrap 1.7.7.33**

- [Release and installer](https://github.com/Iamheregithub/MathScrap/releases/tag/mathscrap-v1.7.7.33)
- Compiled source: [`0d953c8d00d5d12775566401931bc63804de9eec`](https://github.com/Iamheregithub/MathScrap/commit/0d953c8d00d5d12775566401931bc63804de9eec)
- [Windows build, native/exploratory checks, recovery, compatibility and packaging](https://github.com/Iamheregithub/MathScrap/actions/runs/40196547276)
- [Development-branch DOM and Chromium security regression checks](https://github.com/Iamheregithub/MathScrap/actions/runs/40196547253)
- [Development-branch CodeQL scan](https://github.com/Iamheregithub/MathScrap/actions/runs/40196547369)
- Setup filename: `MathScrap-Windows-x64-Setup.exe`
- SHA-256 checksum: use `SHA256SUMS.txt` attached to the release.

This release removes stale stylus-contact state that could block later Functions or shape-control taps after popup hover and dismissal. Each new press starts a fresh routing decision; popup release state is cleared before dispatching actions, including actions that open dialogs. Windows regression checks reproduced the Functions lockout on the preceding code and passed with the correction. Additional menu-action checks verify actual hover, press and release delivery to a modal dialog. Expanded checks tap the actual Border and Fill swatches on a selected shape, verify its SVG colours, and draw another shape with the chosen colours. The compact three-row Shapes tray, 80% opacity, prior tablet-coordinate alignment, Undo/Redo and save/reopen behavior remain included. Physical Huion hardware validation remains a manual check.

The installer is unsigned. Its per-user installation directory and internal profile retain the `OpenBoardTeaching` identifier so upgrades preserve settings and lessons. Existing OpenBoard installations use a separate profile. Exchange UBZ files to transfer lessons between the applications.

## Build the Windows application

Check out `graphdesk-teaching-fork`. The [.github/workflows/windows-teaching.yml](https://github.com/Iamheregithub/MathScrap/blob/graphdesk-teaching-fork/.github/workflows/windows-teaching.yml) workflow is the reproducible build recipe. It uses Windows 2022, MSVC 2022, Qt 6.8.3 and the pinned upstream third-party dependency revision declared in that workflow.

The workflow:

1. Runs JavaScript DOM tests and Chromium sandbox checks.
2. Builds and stages the native application using [build-windows.ps1](https://github.com/Iamheregithub/MathScrap/blob/graphdesk-teaching-fork/release_scripts/teaching/build-windows.ps1).
3. Runs native startup, drawing, formula, GraphDesk, document/PDF and workspace checks.
4. Runs per-colour thickness, covers, saved-version and crash-recovery checks.
5. Runs exploratory live-canvas checks across Pages states and zoom levels using [test-exploratory.ps1](https://github.com/Iamheregithub/MathScrap/blob/graphdesk-teaching-fork/release_scripts/teaching/test-exploratory.ps1).
6. Builds an instrumented original OpenBoard 1.7.7 and round-trips generated lesson files.
7. Creates the Inno Setup installer, computes its checksum, and publishes the release and validation artifacts.

The installer version is set in [OpenBoardTeaching.iss](https://github.com/Iamheregithub/MathScrap/blob/graphdesk-teaching-fork/release_scripts/teaching/OpenBoardTeaching.iss). Increase it for a new application release.

## Main implementation areas

| Area | Location |
| --- | --- |
| Preferences sections, live settings and shortcut editor | `src/core/UBPreferencesController.*` |
| Pointer input and selection outlines | `src/board/UBBoardView.*` |
| Paper & Ink workspace and floating tools | `src/board/UBPaperInk.*` |
| Formula generation and intermediate steps | `src/domain/UBMathFormula.*` |
| Editable SVG formula and graph objects | `src/domain/UBGraphicsSvgItem.*` |
| Teaching controls and tool integration | `src/board/UBTeachingTools.*` |
| Covers, versions and recovery | `src/board/UBLessonTools.*` |
| Bundled GraphDesk application | `resources/graphdesk/GraphDesk.html` |
| Native application and formula regressions | `src/board/UBTeachingSmokeTest.cpp`, `src/board/UBMathFormulaTest.cpp` |
| Browser and DOM regressions | `tests/teaching/` |

MathScrap's current interface includes the floating pen tray, combined Line and Zoom controls, a categorized Settings & more cog menu, and a visual workspace-customization preview. Older instructions describing separate permanent Solid Line and Dashed Line buttons as the default layout are obsolete.

## Preferences

Open Preferences from the application menu. The sidebar search finds sections by their names and related settings.

| Section | Options |
| --- | --- |
| Workspace | Touch-sized controls, floating-toolbar labels, Pages sidebar, startup view, and workspace customization. |
| Writing & colours | Separate Pen and Highlighter palettes, pressure response and starting widths. |
| Page & PDF | Grid colours and opacity, and whether PDF exports include the grid and background colour. |
| Documents | Lesson storage and migration, document-list options, Trash retention, and access to saved versions. |
| Keyboard | On-screen keyboard options and searchable commands with editable keyboard shortcuts. Conflicting bindings are rejected; Clear removes a binding. |
| Projector | Connected-screen selection and presentation options. |
| Web & network | System-browser choice, browser home page and proxy credentials. |
| About | MathScrap version and release downloads, upstream project information, licences and credits. |

Workspace changes take effect immediately. Done, the window close button and Escape retain edited text fields. Document-folder changes take effect on the next launch using the existing migration procedure.

Reset applies to the selected section; Writing has separate Pen and Highlighter resets. It preserves lesson files, saved versions, the document folder, toolbar order and settings in other sections. Keyboard reset affects on-screen keyboard options; edit shortcut bindings individually. Browser reset preserves proxy credentials. About has no reset.

The release-download action opens MathScrap releases rather than the upstream OpenBoard update feed.

## JavaScript checks

From `tests/teaching`:

```sh
npm ci --ignore-scripts
npm test
npx playwright install chromium
node security-browser.cjs
```

- `graphdesk-dom.cjs`: four-function plotting, SVG export, project/metadata round trips, HTML-shaped labels, and rejection of malformed project and worksheet attributes without modifying the current project.
- `security-dom.cjs`: widget edit/display/drop operations, text/attribute injection, legacy-library merge behaviour and embedding configuration.
- `help-templates.cjs`: default, French, Slovak, regional and unsupported-language fallback loads.
- `security-browser.cjs`: Chromium verifies that AnyEmbed, HTML preview and oEmbed retain formatted content inside opaque sandboxes while denying parent DOM and mocked parent API access.
- `graphdesk-smoke.cjs`: additional browser startup/export/restore smoke test, available with `npm run browser`.

The [security-regression workflow](.github/workflows/security-regression.yml) runs DOM and sandbox checks independently on both branches. Chromium tests do not exercise Qt's actual WebChannel bridge; native tests cover the application separately.

## Lessons, recovery and compatibility

The lesson format remains OpenBoard's UBZ format, version 4.8.0. Formula objects, GraphDesk graphs, shapes and covers carry metadata in SVG assets. Original OpenBoard can display these objects; MathScrap supplies the extra editing, triangle-vertex and cover-reveal controls.

Recovery runs every 20 seconds when the application is not drawing or displaying a modal dialog. Changed lessons also receive retained versions. Restores create separate lessons rather than replacing the current lesson. Recovery history is separate from the configurable Documents folder; moving Documents does not move that history.

The Windows compatibility checks create, import, edit, export and reopen generated lessons through both MathScrap and the original OpenBoard 1.7.7 source. They check page counts, PDF assets, text, drawing geometry/colours, SVG references and transforms, archive integrity and rendered pages. Original OpenBoard receives only a test entry point and isolated test profile; its readers and writers are unchanged.

These tests use generated fixtures. They do not validate every existing lesson or every older OpenBoard release.

## Security maintenance

See [SECURITY.md](SECURITY.md) for private reporting and [the October 2026 review](docs/security/review-2026-10-24.md) for the findings, fixes, evidence and validation limits.

CodeQL scans both branches and runs weekly. Dependabot security updates propose changes for review; they do not automatically merge or release them. Avoid committing credentials, student information or personal lesson files.

## Manual checks still needed

- Physical Huion/tablet pressure, pen/eraser switching and hardware shortcuts.
- Projector behaviour: Windows Duplicate display mirrors Lesson and GraphDesk. The separate extended-display board view has not been adapted to mirror GraphDesk.
- Existing classroom lessons, unusual asset combinations, and broader import/export cases.
- Classroom usability of touch sizes, narrow windows, tool customization and formula presentation.

## Upstream and licence

MathScrap is an independent OpenBoard fork. Its original fork base was OpenBoard commit `30d120cec100e97e6675d1b9be0c67eafb4c502b`. Upstream: https://github.com/OpenBoard-org/OpenBoard.

Retain the [GPLv3 licence](LICENSE), [COPYRIGHT](COPYRIGHT), and applicable third-party notices when modifying or distributing the application.
