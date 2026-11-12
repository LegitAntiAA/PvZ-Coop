# Endless T/ST weather — approved contract

Implementation was isolated in `C:\PvZ\weather16-agent`, branch `codex/endless-weather-20261112` from core checkpoint `0a5eb9f`. It is now integrated in main at `048ba60`; the old worktree is frozen. Final acceptance corrections belong to main under Root coordination. Never activate windows or capture desktop.

## Behavior and authority

- Applies to `IsEndlessT()` (T and ST); other native modes keep their behavior. Host advances weather only while playing, unpaused and nonterminal.
- Existing `mNetDayPhase` is the target0/1. New `mNetNightBlend`0..1000 approaches it by4 per tick (2.5s full transition). `StageIsNight()` changes at midpoint500; nocturnal sleep changes on that edge. Initial day is blend0. Phase interval remains6000.
- Fog and storm use two independent random rolls at phase edge (25% fog,20% storm), durations3000/2000. Overlap is valid. No exclusive reset/validator. Native fog must provide Plantern/Blover behavior; check wind countdown also expires correctly while fog inactive. User correction: Plantern clears its cell and eight adjacent cells (3×3), with no horizontal stretching or extra bottom-row clearing; Torchwood behavior is unchanged.
- Storm uses separate new kind0..3/counter0..900, preserving native three flash patterns/schedule without borrowing `Challenge::mChallengeState`. Outside native flash windows the field is opaque black; native UI remains usable. Rain and fog are below storm. One thunder emission per actual edge, no duplicate host+client playback or latejoin replay.
- New authoritative fields are already declared before `mDrawOnlyCamera`/`mPaused`; preserve that adjacency and native save range. UI-only presentation fields also belong before that pair and do not need saving.

## Presentation/audio

- Backdrop blends native day/night images using visual blend; no abrupt image switch or old rectangle weather overlay. Snapshot bootstrap starts at received blend; smooth ordinary updates without predicting game rules.
- `Board::UpdateEndlessWeatherPresentation()` owns local interpolation/music presentation. Root adds its common update call after reviewing actual Board update flow. UI owns its implementation in a new CPP.
- Use native storm render item/DrawRain/DrawStormNight/DrawStormFlash via explicit kind/counter overload; do not cover all UI by the old post-world overlay. Flash seed uses synchronized weather state for T/ST, preserving other native modes.
- Music gain is a separate0..1 envelope (`abs(2*visualBlend/1000-1)`), old tune fades out, switch day/night at silent midpoint, new tune fades in. Keep user volume and native burst/layer balance; multiply once (no compounding), apply on initial playback to avoid a pop, restore gain1 when leaving T/ST. T/ST music choice follows day/night, independent of fog episodes. Verify actual music channel volume state; no system/desktop audio capture.
- Preload background1/2/4 fog resources and native rain for dynamic T/ST; no missing late resource groups.

## Snapshot33

- Entity owns a bounded transactional portable `NetEndlessWeather.h` codec replacing the old7-byte weather header with existing day/timers + new blend/kind/counter + the actual native fog alpha grid asU8. Derive grid size from native declaration and static_assert at integration; do not synthesize85 fog update passes on clients.
- Grid wire covers native fog too; clients apply authoritative native fog grid/offset/blown countdown and draw it. Do not run gameplay plant updates. No fake warmup or blind divergence. Existing fog offset/blown fields remain reused.
- Bounds: phase0/1, timer<=6000, fog<=3000, storm<=2000, blend<=1000, kind<=3, counter<=900, kind0=>counter0, storm0=>kind/counter0; both fog and storm allowed. Non-T weather-only fields encode0. Validate candidate before mutation and restore read position/state on failure.
- Root bumps protocol32→33 and wires tests/projects. Never weaken existing mystery/seed/camera validation.

## Ownership

- Modes: Board.cpp weather update, StageIsNight/StageHasFog and related native fog gameplay/resources; authoritative Board fields only; new header may be read but Entity owns codec. CutScene weather preload guards only after notifying Root. No Bungee/PvP edits in weather branch unless a new confirmed problem.
- UI: Board::DrawBackdrop and removal of old end overlay call, Challenge.cpp/.h storm rendering overloads/DrawWeather, Music.cpp/.h gain/fade integration; new `NetEndlessPresentation.cpp`; UI-only Board state block. No host simulation/codec or core chooser/browser changes.
- Entity: NetBoardSync.cpp header/snapshot decoding/encoding before IsCoopEndless, new codec/unit files; native fog grid/cache validity. No custom-tail rendering or gameplay changes.
- Root: common Board update hook, projects/protocol, runtime helper/runner, review/integration/docs/git. Shared files have function/block ownership; use narrow patches and notify owners before crossing it.

## Acceptance

Build/unit, actual hidden host/client day/mid/night and reverse transition, native sleep/sun behavior, Plantern hole and real Blover clearing/return, storm black/brief native flash/rain and fog overlap, pause/bootstrap, actual music layer gain values and no stuck gain on menu exit. Use controlled phase/boundary timers explicitly; do not claim full natural60s cycles or audible output without observing them. Core15 publication and weather16 completion remain separate milestones.
