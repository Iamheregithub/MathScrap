"""Add only a test entry point to the official OpenBoard release checkout.
The original readers, writers, scene and graphics implementations stay untouched.
"""
from pathlib import Path
import sys,shutil
fork=Path(__file__).resolve().parents[2]
original=Path(sys.argv[1]).resolve()
for name in ['UBFileCompatibilityTest.cpp','UBFileCompatibilityTest.h']:
    shutil.copy2(fork/'src/board'/name,original/'src/board'/name)
p=original/'src/board/board.pri'
p.write_text(p.read_text()+'\nHEADERS += src/board/UBFileCompatibilityTest.h\nSOURCES += src/board/UBFileCompatibilityTest.cpp\n')
p=original/'OpenBoard.pro'
p.write_text(p.read_text().replace('TARGET = "OpenBoard"','TARGET = "OpenBoardTeaching"',1))
p=original/'src/core/main.cpp';s=p.read_text().replace('#include "UBSettings.h"','#include "UBSettings.h"\n#include "board/UBFileCompatibilityTest.h"')
s=s.replace('    QStringList args = app.arguments();','    QStringList args = app.arguments();\n    if (args.contains("--file-compatibility-test")) scheduleFileCompatibilityTest(&app);');p.write_text(s)
p=original/'src/core/UBApplication.cpp'
p.write_text(p.read_text().replace('setApplicationName("OpenBoard");','setApplicationName("OpenBoardCompatibilityTest");'))
(original/'release_scripts/teaching').mkdir(parents=True,exist_ok=True)
shutil.copy2(fork/'release_scripts/teaching/build-windows.ps1',original/'release_scripts/teaching/build-windows.ps1')

# Test-only startup preferences; application file readers/writers remain original.
p=original/'resources/etc/OpenBoard.config'
s=p.read_text().replace('EnableStartupHints=true','EnableStartupHints=false').replace('EnableAutomaticSoftwareUpdates=true','EnableAutomaticSoftwareUpdates=false')
p.write_text(s)
