@echo off

if not exist .\build mkdir .\build
pushd build 

REM https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-alphabetically?view=msvc-170
set cl_flags=/MT /nologo /FC /W4 /Zi /Od

REM WAV Dump
cl /I..\src /I..\base ..\wav_dump.c %cl_flags% /link /out:"wav_dump.exe"

REM WAV Dump With Data
cl /I..\src /I..\base ..\wav_dump_with_data.c %cl_flags% /link /out:"wav_dump_with_data.exe"

REM WAV to WAV
cl /I..\src /I..\base ..\wav_to_wav.c %cl_flags% /link /out:"wav_to_wav.exe"

REM WAV Play WIN32
cl /I..\base ..\wav_play_win32.c %cl_flags% /TP /link Ole32.lib /out:"wav_play_win32.exe"
