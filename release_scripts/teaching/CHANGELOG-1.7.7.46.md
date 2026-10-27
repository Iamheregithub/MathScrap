# MathScrap 1.7.7.46

- Drawn covers have a top reveal grip: pull down to expose writing, pull up to conceal it, keeping the bottom fixed. Existing native selection handles still move and resize covers.
- Screen shades have a circular close button at the top right. Mouse and pen presses delete only the shade on release; Undo restores it.
- The Cover tray contains only Draw cover, Screen shade and Close. The other seven controls are available in Customize workspace → Covers.
- Normalize visible and saved cover layer values during insertion, restore and z-order assignment so ordinary object levels cannot put covers beneath ink.
- Remove Shapes, 2D and 3D headings and their empty rows from the Shapes tray.
- Update the About and Windows installer version to 1.7.7.46.

Native regression checks cover tray contents, customization controls, mouse and tablet reveal gestures, anchored bottoms, shade deletion/Undo, ink occlusion during drawing, saved layers and lesson reopen. The Windows build runs the existing stylus, recovery, exploratory and original OpenBoard compatibility checks before installer packaging.
