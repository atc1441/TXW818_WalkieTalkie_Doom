cd /d %~dp0

set dirname=%1
set dirname=%dirname:~0,-4%

del /q/f loader.bin	
del /q/f code512.bin
del /q/f param.bin
del /q/f program.bin
del /q/f project_merge.bin

