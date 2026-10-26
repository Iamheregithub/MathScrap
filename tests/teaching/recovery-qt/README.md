# Recovery responsiveness regression

This Qt Widgets executable compiles the production `UBLessonHistory` method
bodies, `UBLessonBackupStore`, and `UBInputActivity`. Only the application
boundaries (board, scene serializer, document proxy, settings and persistence
queue) use fixtures; this is not a full application or hardware test.

Run `python prepare.py`, configure `test.pro` with qmake in `build/`, build, and
run `recovery-qt-test` with `QT_QPA_PLATFORM=offscreen`. The Windows installer
workflow runs `test-recovery-responsive.ps1` before compiling the application.

Checks cover a blocked persistence queue while GUI timers continue, the idle
and held-stroke gates, resumed input during queue draining, background backup
processing, concurrent source writes, serialization failure, corrupted blob
repair, restoration, Delete all during a running backup, deletion suppression,
explicit resumption, version pruning and the final clean shutdown checkpoint.
Existing full-application save/restart and recovery suites still run separately.
