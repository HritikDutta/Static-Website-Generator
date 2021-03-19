@echo off

cl /Zi /c generator/*.c /I ..\swg
cl /Zi main.c *.obj /Fe:swg