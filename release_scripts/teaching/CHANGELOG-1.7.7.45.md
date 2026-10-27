# MathScrap 1.7.7.45

Includes the Shapes redesign and tray/selection corrections.
Explicitly route pen selection gestures through the board’s mouse interaction
path, keeping press, movement and release together. Accept the original tablet
packets to prevent duplicate platform-generated clicks. Release selection
capture on pen proximity loss. Handwriting retains its pressure-sensitive path.

The Windows native test confirmed all mouse body-drag cases passed before
identifying the missing tablet-to-mouse fallback. Full Windows validation is
rerun for this correction. About and installer versions both report 1.7.7.45.
