# MathScrap 1.7.7.41

Correct the Erase All native smoke test to compare two shared scene pointers.
The SVG item's scene accessor returns a shared pointer; comparing it with a raw
pointer prevented the full Windows application from compiling. The test still
checks that clearing handwriting preserves the inserted shape and supports Undo.

Includes all 1.7.7.39 features and the Windows build corrections in 1.7.7.40.
About MathScrap and installer version updated to 1.7.7.41.
