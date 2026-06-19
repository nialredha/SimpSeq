@echo off

if not exist .\build mkdir .\build
pushd build 

REM https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-alphabetically?view=msvc-170
set c_flags=/MT /nologo /FC /W4 /Zi /Od

set cl_includes=/I..\src

REM WAV Dump
REM cl  %cl_includes% ..\src\wav_dump.c %c_flags% /link /out:"wav_dump.exe" ole32.lib winmm.lib

REM WAV Dump With Data
REM cl %cl_includes% ..\src\wav_dump_with_data.c %c_flags% /link /out:"wav_dump_with_data.exe" ole32.lib winmm.lib

REM WAV Dump Raw
REM cl %cl_includes% ..\src\wav_dump_raw.c %c_flags% /link /out:"wav_dump_raw.exe" ole32.lib winmm.lib

REM WAV Play
REM cl %cl_includes% ..\src\wav_play.c %c_flags% /link ole32.lib /out:"wav_play.exe" ole32.lib winmm.lib

REM WAV Sequencer
set l_flags=-incremental:no -opt:ref
set export_funcs=/EXPORT:trk_module_post_load /EXPORT:trk_module_post_reload /EXPORT:trk_module_update 

cl %cl_includes% ..\src\tracker.c %c_flags% /LD /link %l_flags% /DLL %export_funcs% /out:"trk_module.dll" ole32.lib winmm.lib
cl %cl_includes% ..\src\tracker_runtime.c %c_flags% /link /out:"tracker.exe" ole32.lib winmm.lib
