"""Replace application boundaries with fixtures; retain all production history method bodies."""
from pathlib import Path
here=Path(__file__).resolve().parent
source=(here.parents[2]/'src/core/UBLessonHistory.cpp').read_text()
source='\n'.join(line for line in source.splitlines() if not (line.startswith('#include "') and 'UBLessonHistory.h' not in line and 'UBInputActivity.h' not in line))
(here/'production-history.cpp').write_text('#include "stubs.h"\n'+source+'\n')
