# MathScrap 1.7.7.39

Reduce automatic saving interruptions during handwriting.

- Recovery waits for at least 1.5 seconds without input and for the current
  stroke to finish. If writing resumes while earlier writes drain, preparation
  is deferred again.
- The automatic pending-write wait runs outside the GUI thread. Recovery scans,
  SHA-256 hashing, verified blob copies and history pruning run on a worker with
  value-only inputs. Live scenes and widgets remain on the GUI thread.
- A recovery checkpoint serializes the current page once rather than twice.
  It skips thumbnail rendering; normal persistence still updates thumbnails.
- Autosave retries after idle time rather than starting during a stroke. Page
  sidebar previews also wait for idle time when the sidebar is visible.
- Explicit checkpoints, restoration, Delete all and shutdown finish outstanding
  backup work. A changing source repository or failed serialization preserves
  the previously committed recovery manifest. Changed source files are retried.
- Slow recovery/autosave preparation, UI event-loop gaps and stylus delivery
  gaps are recorded in the application log without requiring verbose mode.
  Diagnostics contain timings, not handwriting or lesson contents.
- About MathScrap and the installer identify this version as 1.7.7.39.

This cumulative update also includes the previously uncommitted Function
Calculator (1.7.7.36), optional pen-tray hover with default off (1.7.7.37), and
stylus-accessible double-tap Erase All mode (1.7.7.38).

Validation before commit:
- 87 focused Qt assertions passed using production history method bodies,
  backup storage and idle scheduling, with fixtures at application boundaries.
  Coverage includes a deliberately blocked disk queue while GUI timers run,
  resumed writing during draining, worker execution, concurrent file changes,
  failed serialization, blob repair, restoration, Delete all during a running
  backup, suppression/resumption, fifty-version retention and clean shutdown.
- Changed application source files compiled with Qt 5.15.13.
- Existing GraphDesk DOM, security (43), help templates (50), and calculator
  engine (79) checks passed.
- The Windows workflow now runs the recovery responsiveness fixture before
  the full application build and existing save/restart/recovery suites.

Limitations: The focused fixture substitutes the board/serializer/queue boundary;
its results are not a full native application or Huion hardware test. Scene
serialization still requires the GUI thread and is timed for follow-up. The
reported physical Windows/Huion freeze is not yet confirmed resolved.
