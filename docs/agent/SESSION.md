# Active delivery

Task: fix and implement the full user play-test list in REQUEST.txt, including online room browser/WebRTC, while preserving existing work. See WORKBOARD.md for owners and acceptance.

Root currently implements network interactions/readiness, background verification and integration. Three agents work in non-overlapping areas. Source changes are uncommitted. Initial four dirty files predate this task and are preserved.

Build: `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe PlantsVsZombies.sln -p:Configuration=ReleaseGOTY -p:Platform=Win32 -m:4 -v:m -nologo`.

User prohibits Computer Use/foreground launches. All new game runs must use the Background smoke option after verifying hidden-window launch code is built. Capture game render target only. Never capture desktop, activate windows or inject user input.

Routine Russian Telegram development reports are authorized. Initial reporting succeeded. Last major evidence is baseline build/tests/Adventure1, not the new implementation.
