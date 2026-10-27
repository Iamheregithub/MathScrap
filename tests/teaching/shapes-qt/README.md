# Native shape geometry checks

Build with `qmake test.pro` and your platform's make tool. Run
`shape-geometry-test [output-directory]` (or the `.exe` on Windows).
The optional existing output directory receives all twelve SVGs and a PNG gallery.
Use `QT_QPA_PLATFORM=offscreen` on a headless system.

Covers both modifier keys versus either alone, four drawing quadrants,
equilateral-triangle and rhombus side lengths, all 3–20-sided regular polygons,
Qt SVG parsing/rendering for every shape, hidden edges, square right-angle markers
after non-uniform scaling, and stable persisted SVG outlines across resizing and rotation.
The application smoke tests additionally exercise insertion, body dragging,
resize handlers, selection state, styling and persistence.
