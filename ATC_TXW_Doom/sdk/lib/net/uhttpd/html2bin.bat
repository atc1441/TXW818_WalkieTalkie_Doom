@echo off
del html_bytes.c
for %%i in (*.gz) do ( html2bin.exe STEP1 %%i)
::html2bin.exe STEP2
::for %%i in (*.gz) do ( html2bin.exe STEP3 %%i)
::html2bin.exe STEP4
