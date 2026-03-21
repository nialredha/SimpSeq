@echo off

if not exist .\build mkdir .\build
pushd build 

REM https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-alphabetically?view=msvc-170
set cl_flags=/MT /nologo /FC /W4 /Zi /Od

set cl_includes=/I..\src /I..\base /I..\os

REM WAV Dump
cl  %cl_includes% ..\wav_dump.c %cl_flags% /link /out:"wav_dump.exe"

REM WAV Dump With Data
cl %cl_includes% ..\wav_dump_with_data.c %cl_flags% /link /out:"wav_dump_with_data.exe"

REM WAV Dump Raw
cl %cl_includes% ..\wav_dump_raw.c %cl_flags% /link /out:"wav_dump_raw.exe"

REM WAV Play
cl %cl_includes% ..\wav_play.c %cl_flags% /link ole32.lib /out:"wav_play.exe"
