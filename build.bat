@echo off

if not exist .\build mkdir .\build
pushd build 

REM https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-alphabetically?view=msvc-170
set c_flags=/MT /nologo /FC /W4 /Zi /Od

set cl_includes=/I..\src /I..\base /I..\os

REM WAV Dump
REM cl  %cl_includes% ..\wav_dump.c %c_flags% /link /out:"wav_dump.exe" ole32.lib winmm.lib

REM WAV Dump With Data
REM cl %cl_includes% ..\wav_dump_with_data.c %c_flags% /link /out:"wav_dump_with_data.exe" ole32.lib winmm.lib

REM WAV Dump Raw
REM cl %cl_includes% ..\wav_dump_raw.c %c_flags% /link /out:"wav_dump_raw.exe" ole32.lib winmm.lib

REM WAV Play
REM cl %cl_includes% ..\wav_play.c %c_flags% /link ole32.lib /out:"wav_play.exe" ole32.lib winmm.lib

REM WAV Sound Mixer
set l_flags=-incremental:no -opt:ref
set export_funcs=/EXPORT:ss_update

cl %cl_includes% ..\simp_seq.c %c_flags% /LD /link %l_flags% /DLL %export_funcs% /out:"ss.dll" ole32.lib winmm.lib
cl %cl_includes% ..\simp_seq_entry_point.c %c_flags% /link /out:"simp_seq.exe" ole32.lib winmm.lib
