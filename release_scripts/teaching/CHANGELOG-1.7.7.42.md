# MathScrap 1.7.7.42

Correct scene-item ownership in the recovery-new-work native regression.
`addText` registers each item in the scene's deletion set. The fixture manually
deleted an item after removing it through QGraphicsScene, leaving a dangling
entry that could be deleted again at shutdown. It now uses scene removal and
retains scene ownership until cleanup. The regression still checks removal,
returning to pre-change content, and restorable recovery data, and still
requires a successful process exit.

Includes all 1.7.7.39 features and subsequent Windows test/build corrections.
About MathScrap and installer version updated to 1.7.7.42.
