# gomacc and gdi32.dll

TL;DR - gomacc.exe with gdi32.dll lead to micro UI hangs and slows builds.

Lock contention can cause UI hangs if many processes are destroyed at the same
time. This can easily happen on goma builds. This issue was discussed here:

https://randomascii.wordpress.com/2017/07/09/24-core-cpu-and-i-cant-move-my-mouse/

Microsoft has patched Windows to reduce this contention but the fix is
imperfect. And, if user32.dll or gdi32.dll are loaded then the lock contention
is made far worse - process destruction times get about 5-6x slower and the
user-crit lock is held for most of this time.

When gdi32.dll is imported in gomacc, that causes slow NtGdiCloseProcess
function calling when terminating process.

Note: NtGdiCloseProcess time increases when the machine has been running for a
long time so the discrepancy is likely to increase.

Hence, gomacc.exe would crash if it detects gdi32.dll is loaded.

However, some tools (e.g ConEmu) inject dlls, which pull in gdi32.dll,
and makes unable to use goma.

# For ConEmu users

There are several workaround with this issue.

1. `set GOMA_GOMACC_ALLOW_GDI32DLL=true` as escape hatch.
      Warning: performance degradation.
2. Try a different terminal, such as Cmder.
3. Try disabling ConEmuHk.dll injection, either gloablly or
   with -cur_console:i.
      Warning: potential consequences with ANSI color codes.
