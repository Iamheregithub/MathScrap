# MathScrap 1.7.7.35

- Choosing **Not now** in the recovery prompt remembers the choice across restarts, while retaining the copies in **Versions** for later review.
- **Delete all…** is available in the recovery prompt and **Delete all history…** in Versions. Confirmation removes recovery copies and retained versions while preserving saved lessons in Documents.
- Deleted history stays cleared for unchanged content. Current edits are included in the deletion baseline; new lesson content or **Create saved version…** resumes recovery.
- Orderly shutdown records a backup failure separately from a crash, so the recovery prompt accurately reflects what happened.
- The Shapes tray replaces Diamond with **Pentagon**. Existing diamond objects retain their saved shape identity and remain editable.

Validation covers dismissal and restart, interruption after dismissal, review and restore, cancel/confirm deletion, deletion and restart, explicit versions, new edits, backup-write failures, empty recovery history, native drawing and pen interactions, GraphDesk, exploratory tools, and round trips with original OpenBoard 1.7.7.
