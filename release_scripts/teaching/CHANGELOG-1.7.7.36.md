# MathScrap 1.7.7.36 — uncommitted Function Calculator

Base: graphdesk-teaching-fork at 58e1012e3234826f1009a1c3e653bc5a658a0272.
No commits, pushes, workflow dispatches, or releases were made for this change.

Functions → Function Calculator inserts an editable, offline calculator on the
lesson page. Uses the Desmos MathQuill fork for live notation and a restricted
expression reader with Nerdamer for bounded exact output. Supports fractions,
radicals/indexed roots, powers, logarithms with a base, trig/inverse trig,
absolute value, pi/e, factorials, and evaluation of functions in x.
Degrees/radians and x values are saved per calculator through the existing
widget preferences in the lesson document. No cloud calculator or API key.

Input safeguards:
- Explicitly disarm shape placement and close its tray before insertion, even
  if Selector was already selected; close active cover placement too.
- Use the existing Functions popup and pen routing rather than adding a new tray.
- Calculator canvas gestures reach the web editor; tablet packets use global
  coordinates and are consumed to prevent duplicate clicks and ink dots.
- Single click activation for keypad, cursor-preserving press handling,
  proximity-loss cancellation, and proportional sizing through widget delegate.
- Incomplete or invalid edits immediately clear the previous answer.
- Exact output runs in a bounded Blob worker, keeping expensive CAS work away
  from the UI; the numerical result remains available if exact work times out.

Validation:
- 79 numerical, domain/error, exact-result, and worker-integrity checks passed.
- 41 Chromium UI checks passed (including pen pointer packets, single insertion,
  keypad cursor placement, mode changes, preference restore, independent instances,
  stale-output clearing, worker sequencing, and proportional viewport scaling).
- Four changed C++ implementation files compile to objects against Qt 5.15.13.
- Seven Qt WebEngine host checks passed: offline editor/worker, function value,
  native preference bridge save/reload, and a graphics-proxy keypad click.
- Full Windows application build, native popup regression suite, document-level
  save/reopen, and physical Huion input have NOT been run for this iteration.
  The isolated Qt host used a software-rendering backend; it is not a complete
  MathScrap application or a Windows device test.

Not included: graphing, equation solving, symbolic functions of an unassigned
variable, calculus input/evaluation, or changes to existing guided formula cards.
