# Function Calculator

On-page MathScrap widget. Fully offline, including fonts and expression evaluation.

Type a numerical expression, `f(x)=…`, `y=…`, or an expression involving x.
The x field appears when x is used. Every unfamiliar input has a visible keypad
button; the Desmos MathQuill fork also accepts sqrt, nthroot, pi, trig/log names,
`/`, `^`, and `_`. Arrow buttons and Next slot move the math cursor.

The expression, x value, and angle mode belong to this widget instance and use
MathScrap's existing WebChannel `sankore.setPreference` API. The native widget
manifest id identifies calculator input after its folder is renamed to a document
UUID. The normal widget delegate scales proportionally. Drawing outside it is
unchanged; inside it input is routed to the math editor instead of creating ink.

The reader in engine.js accepts only a bounded mathematical grammar. It never
evaluates user text as JavaScript or forwards arbitrary CAS commands. Exact
simplification uses a cancellable worker with a 1.5 second deadline. Inputs are
limited to 700 LaTeX characters, 24 nested groups, and finite real numeric results.
Numerical output has 12 significant digits. Exact output is shown when available;
unsupported notation/domain errors are explicit, with old results cleared.

To regenerate the offline Blob worker after an engine/vendor change:

    node tests/teaching/build-calculator-worker.cjs

From repository root, run:

    node tests/teaching/calculator-engine.cjs
    node tests/teaching/calculator-browser.cjs

Browser tests require the existing Playwright dev dependency and a Chromium
installation. CALCULATOR_CHROMIUM may select an explicit executable;
CALCULATOR_REPORT_DIR sets the screenshot/result output directory.

To try just the editor/calculator outside MathScrap, open index.html in a browser.
Lesson save preferences are only available inside MathScrap.

See vendor/README.md for pinned dependency versions and licences.
