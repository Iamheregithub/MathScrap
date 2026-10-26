# Isolated Qt host test

Build test.pro with qmake and make (or the matching Windows build tool).
Run calculator-qt-test with two absolute arguments:
1. resources/library/applications/FunctionCalculator.wgt/index.html
2. An existing report directory for qt-calculator-window.png

The host embeds the actual offline widget in a QGraphicsProxyWidget and exposes
native WebChannel preferences. It tests radical input/exact work, evaluation at
x, preference storage/reload, and one native mouse click through the proxy.
This does not replace the complete MathScrap native pen/menu regression suite.
For headless Linux, QT_QPA_PLATFORM=offscreen and QT_QUICK_BACKEND=software may
be needed. QTWEBENGINE_DISABLE_SANDBOX=1 is needed only in a root test container.
