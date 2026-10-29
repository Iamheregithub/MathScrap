# MathScrap development and validation guide

MathScrap is the mathematics teaching fork in this repository. It was previously called LessonBoard. The active application branch is `graphdesk-teaching-fork`; `master` receives shared documentation and security fixes but does not yet contain every recent application change.

See [README.md](README.md) for features, controls and the current download.

## Current release

**MathScrap 1.7.7.35**

- [Release and installer](https://github.com/Iamheregithub/MathScrap/releases/tag/mathscrap-v1.7.7.35)
- Compiled source: [`72d9b7c215e0d3272d7fffab109a6f6ef3fb7d5c`](https://github.com/Iamheregithub/MathScrap/commit/72d9b7c215e0d3272d7fffab109a6f6ef3fb7d5c)
- [Windows build, native/exploratory checks, recovery, compatibility and packaging](https://github.com/Iamheregithub/MathScrap/actions/runs/40264631974)
- [Development-branch DOM and Chromium security regression checks](https://github.com/Iamheregithub/MathScrap/actions/runs/40264631888)
- [Development-branch CodeQL scan](https://github.com/Iamheregithub/MathScrap/actions/runs/40264631923)
- Setup filename: `MathScrap-Windows-x64-Setup.exe`
- SHA-256 checksum: use `SHA256SUMS.txt` attached to the release.

This release remembers recovery-prompt dismissal across restarts, adds confirmed Delete all controls to the prompt and Versions, and replaces Diamond with Pentagon in the Shapes tray. Deleted history stays empty for unchanged content, including edits present when deletion was confirmed. New content or an explicit saved version resumes recovery; returning to earlier content updates the recovery copy too. Deletion also works when recovery storage is damaged. Saved lessons in Documents remain intact. An orderly exit with a failed backup records its error separately from a crash.

The Windows pen-input correction from 1.7.7.34 remains included and is retested in this release. The compact three-row Shapes tray, 80% opacity pen tray, toolbar, drawing, Undo/Redo, document and file-compatibility checks remain in the release gate. Physical Huion hardware validation remains a manual check.

The installer is unsigned. Its per-user installation directory and internal profile retain the `OpenBoardTeaching` identifier so upgrades preserve settings and lessons. Existing OpenBoard installations use a separate profile. Exchange UBZ files to transfer lessons between the applications.

## Build the Windows application

Check out `graphdesk-teaching-fork`. The [.github/workflows/windows-teaching.yml](https://github.com/Iamheregithub/MathScrap/blob/graphdesk-teaching-fork/.github/workflows/windows-teaching.yml) workflow is the reproducible build recipe. It uses Windows 2022, MSVC 2022, Qt 6.8.3 and the pinned upstream third-party dependency revision declared in that workflow.

The workflow:

1. Runs JavaScript DOM tests and Chromium sandbox checks.
2. Builds and stages the native application using [build-windows.ps1](https://github.com/Iamheregithub/MathScrap/blob/graphdesk-teaching-fork/release_scripts/teaching/build-windows.ps1).
3. Runs native startup, drawing, formula, GraphDesk, document/PDF and workspace checks.
4. Runs per-colour thickness, covers, saved-version and crash-recovery checks.
5. Runs normal-close/restart, recovery dismissal, review, deletion, new-work, damaged-storage and backup-error checks using [test-save-lifecycle.ps1](https://github.com/Iamheregithub/MathScrap/blob/graphdesk-teaching-fork/release_scripts/teaching/test-save-lifecycle.ps1).
6. Runs exploratory live-canvas checks across Pages states and zoom levels using [test-exploratory.ps1](https://github.com/Iamheregithub/MathScrap/blob/graphdesk-teaching-fork/release_scripts/teaching/test-exploratory.ps1).
7. Builds an instrumented original OpenBoard 1.7.7 and round-trips generated lesson files.
8. Creates the Inno Setup installer, computes its checksum, and publishes the release and validation artifacts.

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

**Not now** remembers dismissal for the available recovery copies, which remain accessible in Versions. **Delete all…** in the prompt and **Delete all history…** in Versions ask for confirmation, remove recovery copies and retained versions, and preserve saved lessons in Documents. Unchanged content stays cleared across restarts; new content or **Create saved version…** resumes recovery. Existing diamond objects retain their original shape identity and remain editable, while the tray offers Pentagon.

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

## Repeated-cycle Windows memory screen

`release_scripts/teaching/test-memory.ps1` runs 40 cycles in one native process,
using one fixed three-page PDF lesson. Each cycle imports the PDF, writes 30
strokes per page, exercises Undo/Redo and erasing, saves and independently
reopens pages, clears content/background and undo history, and returns to the
anchor lesson. Five warm-up cycles are excluded. Samples follow 500 ms of
event processing and deferred-delete delivery; production scene caches are
not forcibly purged. The fixed fixture remains in the disposable CI profile.
Run this script only in a disposable Windows account/profile, not a classroom
installation containing personal lessons.

The analyzer compares first/last five-sample medians and a least-squares trend.
It flags private-byte growth above 32 MiB AND 0.5 MiB/cycle; handle growth above
100 AND 2/cycle; GDI or USER object growth above 50 AND 1/cycle. These are initial
regression-screen budgets, not calibrated leak guarantees. Both conditions
must hold for a counter. Missing cycles, restarts, workload errors and timeouts
fail validation separately. Working set is recorded for context. JSONL samples,
JSON assessment and stdout/stderr logs are uploaded even on failure.

Scope is the main native process, not QtWebEngine child processes, GPU memory,
GraphDesk interactions or long classroom sessions. Scene-cache and allocator
retention can trigger investigation without being an allocation leak. A clean
run does not prove leak freedom. Suspicious growth needs allocation profiling
on Windows. `windows-memory.yml` runs diagnostics without publishing installers;
release-workflow integration is kept local pending diagnostic validation.

### Memory-test cleanup correction

The initial cycling harness called `QUndoStack::clear()` directly, bypassing
`UBBoardController::ClearUndoStack()`, which enumerates history items and
releases removed graphics while preserving clipboard sources. Its measured
retention therefore cannot be attributed directly to normal page switching.
The corrected workload uses the controller cleanup path before switching
lessons. Compare its results before changing application ownership behavior.

### Isolation and heap recording

The diagnostic workflow runs baseline, ink, PDF, save/reopen and combined lanes
in fresh native processes. All retain page switching and its automatic saves;
ink excludes explicit serialization/reload, PDF excludes ink and explicit
serialization/reload, and save/reopen uses one bounded SVG object per page.
Each lane records its mode and 40 measurements under `dist/memory/<mode>`.
Growth in baseline would implicate common infrastructure, not a specific lane.

A separate combined run enables Windows Performance Recorder heap tracing
for MathScrap.exe and captures `heap.etl`, with staged application PDBs.
Trace availability, start and stop status are recorded explicitly. The trace
run is instrumented and must not be used as the uninstrumented memory baseline.
The ETL requires heap analysis with matching symbols; merely recording it is
not identification of a leak. WPR configuration is removed in finally.
