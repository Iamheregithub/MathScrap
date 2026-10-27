# MathScrap 1.7.7.48

- Screen shades stay fixed. Only the centred top reveal handle can move; side and bottom edges no longer resize the shade.
- Newly drawn regular covers start locked. Their top-right circular lock button toggles position locking. Locked covers cannot move, resize or rotate, but the reveal handle remains usable.
- Cover buttons retain a 48-pixel tap target at different zoom levels. The shade close button stays inside the viewport when the shade is almost fully pulled down.
- Cover controls are interactive overlays rather than edits to the saved SVG, preserving untouched legacy lesson assets and keeping controls out of exported pages.
- Customize workspace → Covers contains only Cover colour. Remove the reveal/reset/lock commands and the global reveal shortcut.
- Remove double-click reveal to prevent accidental exposure of solutions.
- Page erasing preserves covers and shades, including their lock, position and reveal state.
- No new Undo/Redo support is added for the cover lock, reveal, move or shade-close controls.
- About and installer version: 1.7.7.48.

Windows validation includes locked/unlocked mouse and pen interactions, fixed shade geometry, on-object button priority while drawing shapes, save/reload state, close-button accessibility, and Erase All preservation, followed by the existing native, recovery and original OpenBoard compatibility gates.
