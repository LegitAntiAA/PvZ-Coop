# Active delivery

Task: fix and implement the full user play-test list in REQUEST.txt, including online room browser/WebRTC, while preserving existing work. See WORKBOARD.md for owners and acceptance.

Root integrates and verifies the implementation. Three agents cover entity/presentation QA, native UI QA, and modes/online QA. Initial four dirty files predated this task and are preserved in checkpoint a991342. Checkpoint512ccdd merged remote history and was pushed main via the user's gh-auth; later changes are being checkpointed separately.

Build: `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe PlantsVsZombies.sln -p:Configuration=ReleaseGOTY -p:Platform=Win32 -m:4 -v:m -nologo`.

User prohibits Computer Use/foreground launches. All new game runs must use the Background smoke option after verifying hidden-window launch code is built. Capture game render target only. Never capture desktop, activate windows or inject user input.

Routine Russian Telegram development reports and regular commits/pushes to main via gh-auth are explicitly authorized. Reporting works.

Current executable: integration4d, Win32 ReleaseGOTY; builds Passed and650 protocol checks Passed. Fresh4b Passed full core UI flow (73s), actual WebRTC game pair Passed with8 common snapshot hashes. Old black interaction helper captures were invalid and need a current-binary repeat. See WORKBOARD.md and BUGS.md for actual failures/fixes.

Frozen source now includes bank mapping/ID fixes, PvP tool/score/trickle/HUD fixes, literal chat/draft, dynamic advice storage, spectator mutation policy and new presentation/UI fixtures. Active game queue: entity Spectator3 -> UI Fresh24frames -> modes PvP/TD -> entity Presentation18frames -> Root interactions/Unicode/legacy regression. At most3 game processes while the user works; no rebuild until owned game processes stop. Do not infer test success from source readiness.
