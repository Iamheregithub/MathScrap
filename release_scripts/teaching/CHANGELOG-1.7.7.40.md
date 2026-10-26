# MathScrap 1.7.7.40

Windows installer build corrections for the cumulative 1.7.7.39 update.

- Replace the recovery fixture's `private` macro with friend test access. MSVC
  encodes method access in linker symbols, so the previous fixture failed to
  link on Windows even though its Linux build passed.
- Keep the Calculator worker and its embedded input files at LF line endings
  on every Git checkout. This preserves the exact-source consistency check on
  Windows without weakening the assertion or changing calculator behaviour.
- Stop the Windows workflow immediately if any JavaScript validation command
  fails; a later successful command can no longer hide an earlier failure.
- About MathScrap and installer version updated to 1.7.7.40.

All features from 1.7.7.39 remain included: Function Calculator, optional hover
trays (off by default), Erase All with stylus double-tap mode switching, and
idle scheduling/background recovery processing with performance diagnostics.
