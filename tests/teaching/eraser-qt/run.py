"""Run real pen-tray input methods with a stub board, using an existing Qt 5 tree.
Usage: python3 run.py /path/to/qt-prefix/usr /path/to/build-directory
This is a control-layer test; use the native smoke test for actual ink/Undo.
"""
import os, pathlib, subprocess, sys
qt=pathlib.Path(sys.argv[1]).resolve();build=pathlib.Path(sys.argv[2]).resolve();build.mkdir(parents=True,exist_ok=True)
here=pathlib.Path(__file__).resolve().parent;repo=here.parents[2]
source=(repo/'src/board/UBPaperInk.cpp').read_text()
chunks=[]
for start,end in [('QToolButton *UBPaperInk::button(', 'UBPaperInk::UBPaperInk('),('void UBPaperInk::requestEraseAll()', 'void UBPaperInk::selectTrayTool()'),('bool UBPaperInk::routeTabletEvent(', 'bool UBPaperInk::eventFilter(')]:
 chunks.append(source[source.index(start):source.index(end)])
chunks.append(source[source.index('bool UBPaperInk::eventFilter('):])
(build/'production-methods.inc').write_text('\n'.join(chunks));(build/'host.cpp').write_text((here/'host.cpp').read_text())
bin=qt/'lib/x86_64-linux-gnu/qt5/bin';inc=qt/'include/x86_64-linux-gnu/qt5';lib=qt/'lib/x86_64-linux-gnu'
env=os.environ.copy();env['LD_LIBRARY_PATH']=str(lib)+':'+env.get('LD_LIBRARY_PATH','');env['QT_QPA_PLATFORM']='offscreen';env['QT_QPA_PLATFORM_PLUGIN_PATH']=str(lib/'qt5/plugins')
subprocess.run([str(bin/'moc'),str(repo/'src/board/UBPaperInk.h'),'-o',str(build/'moc_paper.cpp')],check=True,env=env)
subprocess.run([str(bin/'moc'),str(build/'host.cpp'),'-o',str(build/'host.moc')],check=True,env=env)
cmd=['g++','-std=c++17','-fPIC','-I'+str(repo/'src/board'),'-I'+str(inc),'-I'+str(qt/'include')]+['-I'+str(inc/m) for m in ['QtWidgets','QtGui','QtCore']]+[str(build/'host.cpp'),str(build/'moc_paper.cpp'),'-L'+str(lib),'-Wl,-rpath,'+str(lib),'-lQt5Widgets','-lQt5Gui','-lQt5Core','-o',str(build/'eraser-qt-test')]
subprocess.run(cmd,check=True,env=env);subprocess.run([str(build/'eraser-qt-test')],check=True,env=env)
