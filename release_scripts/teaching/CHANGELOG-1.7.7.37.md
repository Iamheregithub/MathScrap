# MathScrap 1.7.7.37

- Pen tray colour/thickness panels no longer open on hover by default, including for existing users without a saved preference.
- Customize workspace > Layout & appearance now includes **Open pen trays on hover**. Apply and Done save the setting; Cancel discards unapplied edits; Restore Defaults turns it off.
- Clicking or tapping Pen, Highlight, Eraser or Line retains the existing tray interaction. Shapes and Functions menus retain their existing input routing.
- Applying customization cancels pending hover timers and closes the current pen panel before rebuilding buttons.
- Includes the uncommitted 1.7.7.36 Function Calculator work. About and Windows installer version are now 1.7.7.37.

Validation: UBPaperInk.cpp, UBTeachingTools.cpp and UBApplication.cpp compiled against Qt 5.15.13. Existing GraphDesk DOM, security (43), help-template (50), and calculator engine (79) checks passed. No full Windows application build or physical Huion interaction test was run for this change.
