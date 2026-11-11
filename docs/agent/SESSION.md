# Active delivery

Task: fix and implement the full user play-test list in REQUEST.txt, including online room browser/WebRTC, while preserving existing work. See WORKBOARD.md for owners and acceptance.

Root integrates and verifies the implementation. Three agents cover entity/presentation QA, native UI and modes/online QA. Initial four dirty files predated this task and are preserved in checkpoint a991342. Latest published main03f28d4 passed Build and Release, Unit Tests and Pages CI. Subsequent changes are uncommitted pending the next integrated build.

Build: `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe PlantsVsZombies.sln -p:Configuration=ReleaseGOTY -p:Platform=Win32 -m:4 -v:m -nologo`.

User prohibits Computer Use/foreground launches. All new game runs must use the Background smoke option after verifying hidden-window launch code is built. Capture game render target only. Never capture desktop, activate windows or inject user input.

Routine Russian Telegram development reports and regular commits/pushes to main via gh-auth are explicitly authorized. Reporting works.

Current game executable: integration5b-final, Win32 ReleaseGOTY, build0.30.10-03f28d4130-1794426559827. Full5b and final leaf builds Passed; protocol665checks Passed, including a copied hidden EXE default-path/CWD regression. Public native WebRTC22checks and Worker9checks Passed, including real failed-join cleanup→retry. Earlier unexplained public WSS failure remains recorded; public game pair pending. Fresh4d Passed29markers/24frames per peer, TD4d Passed difficulty10/semantic cursors/five-card disconnect. Presentation5 Passed9stages/18frames, but independent visual review exposed snail body-rate;5b includes explicit0 and needs rerun. Kick5 and PvP5 whole runs Failed due fixture cooldown/motion guards;5b corrections compiled, repeat pending.

Latest user steering: native PvZ browser/lobby redesign NOW; larger appearance panel; verify settings after a real restart and unlock config main-menu button. Online endpoint/STUN removed from player settings. Owner deployed shared Worker at https://pvz-coop-signaling.rostov20172017.workers.dev; /health200/schema1 observed. NetServiceConfig.h embeds URL/STUN, ConnectOnlineDirectory() takes no args; smoke-only env override requires CoopSmokeActive.

Observers must communicate through chat/cursor/middle ping/Alt packet ping/Ctrl drawing and become players at a completed-level boundary. Root implemented common CheckForGameEnd/PreNewGame/EndLevel hooks, epoch invalidation and retained-bank ownership rebalance with stale held-card cancellation. UI engineer owns NetLobbyPanel/native dialogs and RunNativeUiSmoke (appearance/reopen/restart/zero-progress unlock plus real public browser/password/capacity). This dedicated hidden UI runner currently owns the game slot. Modes owns the following PvP/public-game pair slot; entity then Kick/Promote/updated Presentation. Root has interactions/Unicode and work/RunDeliveryRegression.ps1 for10 legacy scenarios (Soak includes opt-in native Dave3-fixed-card fairness). No game rebuild while owned processes remain.

Newest clarification: all personal host/client Zombatars saved before joining must participate in natural spawn selection. Existing audit and client-only natural spawn Passed; Root promised editor→save→restart→join→both-personal-union E2E. Entity is preparing separate NetAvatarEditorSmoke.cpp/new wrapper, not integrated into5b. Root must add its project/freefunction hooks only after agreeing the bounded contract and next source freeze. Never imply this new end-to-end path is already verified.
