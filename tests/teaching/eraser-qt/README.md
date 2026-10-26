# Pen tray Erase All control test

Run with an existing extracted Qt 5 Linux tree (or equivalent prefix):

```sh
python3 tests/teaching/eraser-qt/run.py /path/to/qt-prefix/usr /tmp/eraser-qt-test
```

The runner compiles the current production `button`, `routeTabletEvent`,
`eventFilter` and `requestEraseAll` methods directly from UBPaperInk.cpp.
Only the surrounding window, board, settings and canvas are test doubles.
It sends QMouseEvents and QTabletEvents through Qt's actual event dispatch.
The 16 checks verify mode changes, single clears, double-tap cancellation,
canvas-addressed tablet routing, page/Escape/other-control cancellation,
and regular Eraser recovery. It does not validate real ink removal or Undo.

`checkEraseAll` in UBTeachingSmokeTest.cpp provides the full application
ink/highlighter/shape/Undo/Redo regression for the native smoke suite.
