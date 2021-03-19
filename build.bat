@echo off

cl /c generator/*.c /I ..\swg
cl main.c *.obj /Fe:swg

del *.obj