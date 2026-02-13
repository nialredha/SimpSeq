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


REM WAV Dump V2
cl /I..\src /I..\base ..\wav_dump_v2.c %cl_flags% /link /out:"wav_dump_v2.exe"

REM WAV Dump With Data V2
cl /I..\src /I..\base ..\wav_dump_with_data_v2.c %cl_flags% /link /out:"wav_dump_with_data_v2.exe"

REM WAV to WAV V2
cl /I..\src /I..\base ..\wav_to_wav_v2.c %cl_flags% /link /out:"wav_to_wav_v2.exe"
