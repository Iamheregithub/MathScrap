# MathScrap 1.7.7.43 — Shapes and object selection

- Redesigned Shapes tray with separate 2D and 3D rows; colour, fill and width controls.
- Rectangle, Ellipse, Triangle and Right Triangle replace redundant square/circle entries.
- Ctrl + Shift constrains drawing/resizing to a square, circle, equilateral triangle or rhombus.
- Parallelogram and 3–20-sided Regular Polygon tools; polygons always resize proportionally.
- Cylinder, rectangular prism, square-based pyramid, cone, sphere and triangular prism.
- Solid visible edges, dashed hidden edges; removed border-style and opacity selectors.
- Right triangles display a square right-angle marker, including after resizing/rotation.
- Only the active tool is highlighted; insertion initializes selection controls immediately.
- Shape/formula bodies use the board's movement and undo path rather than a MIME drag.
- Existing lesson shape IDs are retained for compatibility.

About and installer versions are both 1.7.7.43.

Validation: native Qt geometry/rendering checks and changed-source syntax checks.
The updated full application regressions and Windows installer still require a
Windows build; no physical Huion/stylus test is claimed by these local checks.
