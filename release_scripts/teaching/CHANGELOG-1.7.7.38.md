# MathScrap 1.7.7.38

- Double-click Eraser, or double-tap it with the stylus, to switch the same button to Erase All. Repeat to return to Eraser.
- Erase All uses OpenBoard's undoable erase-annotations operation for the current page's handwriting and highlighting. Shapes, inserted objects and page backgrounds remain.
- Switching modes does not erase. A subsequent single click/tap clears the ink after the system double-click interval. This delay lets a second click/tap cancel the pending clear when returning to Eraser.
- Escape, pressing another control, applying customization, or changing the active page before execution cancels the pending operation. The target scene is held weakly.
- Erase All has its own icon and tooltip and does not open the eraser-size tray. The normal Eraser and circle-and-tap erasing remain available.
- Includes the uncommitted Function Calculator and optional hover setting. About and installer version are 1.7.7.38.

Validation: 16 Qt control checks passed using the production button, event filter, tablet routing and delayed-clear methods in a stub board host. Tests cover mouse, direct stylus and canvas-addressed stylus double/single taps, cancellation and regular Eraser recovery. Three modified native implementation files compiled with Qt 5.15.13. Existing web regression tests passed.

A native smoke check now exercises real ink/highlighter removal, shape preservation and Undo/Redo. That full application check was added but not run here. No complete Windows build, installer, or physical Huion test was performed.
