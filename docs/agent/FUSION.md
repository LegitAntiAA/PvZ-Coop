# Fusion 2.3.1 content and gameplay integration — active work

Requested 2026-11-13. Owner: Root. Source baseline: clean `main` `5bfba06`; working branch `codex/fusion-coop-20261113`. Integrate Fusion content and its distinctive gameplay into the existing native PvZ-Coop game. The user's clarified objective is not an exact Unity engine/UI recreation. A completed infrastructure task does not mean Fusion gameplay is complete.

## Requirements and acceptance

- Latest explicit steering09:31 Moscow: user went to sleep and did NOT pause/cancel the task. Repeated correction must persist: inspect and extend actual native mechanisms before adding code; preserve existing behavior as well as visuals (own-first cards, countedReady, tools, mystery/difficulty). User explicitly requested remembering this; one authorized memory update note was saved. Fusion almanac, Dave shop and other distinctive content remain required; use supplied translated strings in C:/PvZ/fusion/mod/analysis (initial candidates strings_map.tsv/strings_idx.json), verify actual language/fields first.

- Main menu `Gamemodes` → `Classic` / `Fusion`, using the existing native PvZ/mod UI. Classic behavior and save layouts remain compatible.
- Latest field steering: a separate button in the upper-right corner during live gameplay, explicitly outside Options; each player independently keeps their own skin, persisted locally. The host must not select the client's background. Both skins share the same real10×5 cells/entities and switching must not restart the level or reset cards.
- User clarification (2026-11-13): reuse Classic's native chooser, top seed bank and shovel. Unity UI duplication is not an acceptance requirement; Fusion gameplay, prices, cooldowns and co-op features remain required.
- Broader clarification: reuse existing PvZ systems and artwork wherever their behavior fits (explicit example: suns). Implement Fusion-specific mechanics/content rather than rebuilding equivalent Unity systems. Exact Unity camera/hierarchy/shader/cosmetic timing is not an acceptance requirement. Keep meaningful Fusion differences such as the5-value small sun, recipes, unit abilities and level content; Classic's small-sun enum pays15 and must not silently change Fusion balance.
- Architectural correction (latest user steering): Fusion is an additional mode of the SAME Board/Plant/Zombie/Projectile/Coin/SeedBank/SeedChooser and network engine. Shared Classic code may and should be changed behind explicit content-mode checks. A separate production FusionSimulation/Runtime world is not accepted. The tested standalone runtime is a reference/validation prototype only; keep useful recovered data/recipes/assets/pure checks, replace its production direction with native entity integration.
- Reuse the actual native UI functions and widget, rather than a similar texture composition. Remove hotkey-number and P1/P2 captions from the Fusion hotbar; retain owner colors. The user reported a confirmed giant-leg defect in the prototype's imported basic-zombie rig. Fix the playable native path using the original Zombie renderer; do not declare the issue verified before native frames are inspected.
- Fusion uses the supplied local 2.3.1 source build: original plant/zombie/projectile behaviors, directional fusion recipes and placement layers, animation curves/events, campaign/waves, mini-games, Travel and almanac. Unrecovered or unimplemented behaviors must remain explicitly tracked.
- Preserve co-op features, including per-player seed selection/quotas, mystery selection and its provenance/discount, difficulty, ordered input, communication and late spectators. Fusion IDs must not be truncated into classic byte enums.
- The host commits gameplay mutations; clients apply complete validated state. Verify concurrent fusion intent, animation/event phases, removal/death and late joins on real native host/client processes.
- Fusion progress and field-based saves live separately from Classic profiles/raw board saves. A guest must not persist the host's progress into their local profile.
- Publish only through the user's existing authenticated gh/Git workflow. No release/tag is requested.

## Current tasks

### Integration31 — user restored disk space, 2026-11-13 16:00 Moscow

Integration31 now Passed: ordinary appbuild, unit12137/0/0, nativeManual armedfault601common, Classic1016common, ownedcleanup. User's newer FIELD-ART32 priority: replace copied ninth/tenth lawn join with actual ImageGen texture, align Fusion backdrop down12.874px; source done, normal32build/runtime pending. UI09 solo book andQA12 remote mode/book lifetime fixtures preparing freeze; Root added2-line lifecycle fix from independentreview, still unverified.

Disk/header baseline repaired and actual unit11762/0/0 Passed;37primary hashes unchanged.21 staged files integrated to C with SHA checks. Ordinary app31 MSBuild IN_PROGRESS. See SESSION.md current checkpoint for authoritative live status; old timestamps below are historical.

| ID | Owner | Status | Acceptance / evidence |
|---|---|---|---|
| RECOVERY31 | Root | DONE | Exact original header restored and baseline unit11762/0/0 Passed;12.3GiB freeC; backups retained |
| INTEGRATION31 | Root | IN_PROGRESS | Normal MSBuild+newunit+native/Classic checks on actual C project, not overlay obj package |
| FIELD02 / GRID-QA02 | Root | IN_TESTING | Z28 fullnative10x5 Passed586common, top-right direct native button visible/independent saved skins/heldshovel; fresh C verification pending |
| ALMANAC-UI08 | Content + Root | IN_TESTING | Existing book+typed sourceIDs+344 staticportraits/25explicitmissing+sourcefont and RU hashprovider integrated; native GUI acceptance pending |
| ALMANAC-QA09 | Content | IN_PROGRESS | New owned hidden SDL mainmenu/noBoard book/page/scroll/close/reopen fixture; Root launches |
| REVIEW10 | Architect | IN_REVIEW | Independent targeted FIELD/UI/Classic/lifetime review; no shared edits |
| BASE-PARITY11 | Mechanics | IN_PROGRESS | Source-derived exact next3–5 plant contracts/reuse proposals; no speculative production code |
| RU07 | Mechanics + Root | DONE for88texts |35base+53special plant translations; Root reran5data+13translation checks and164C++provider checks;193plant/88zombie texts still missing |
| RNG-QA04 | Root + Architect | IN_TESTING | Source integrated; Z29 whole D10 RngFailure Passed44common/82rx/freshchooser+epochs+secondfullintro; C normal build repeat and ordinary cases pending |

### Native10 implementation — 2026-11-13, 11:15 Moscow

| ID | Owner | Status | Acceptance / evidence |
|---|---|---|---|
| GRID02 | Root + Content entity hooks | IN_REVIEW | Same Board10×5/pitch72, native column9 terrain/offset sidecar, playable bounds and native wirev2/protocol37. Classic MAX9 unchanged. Actual rebuilt ABI helper Passed22599bytes/all149offsets/entitysizes; app26 not built yet |
| GRID-QA02 | Architect | IN_PROGRESS | Extend existing real native Manual for col9 SDL plant/fusion/shovel, col10/row5/gutter rejection, local malformed decoder rejection before mutation,1066/800 and independently held skins |
| FIELD02 | Root | IN_TESTING | User rejected Options placement. Root now uses separate native FIELD stonebutton below Menu plus local NetConfig persistence; no clientOptions permission change remains. Direct-button runtime pending27 |
| I18N02 | Mechanics / Root review | DONE for glossary import |300 source-exact existing RU entries,4 quality flags,13 exporter checks Passed; Root reran actual export and verified SHA. This is not a fully translated almanac |
| ALMANAC-DATA04 | Mechanics | IN_PROGRESS | Extend generated typed catalog with all281/88 source almanac entries/fields, no Classic enum casts |
| ART-INVENTORY04 | Content | IN_PROGRESS | Exact source array-store bindings for281plant/88zombie almanac IDs; Root independently checked IDsets and650 referenced prefab files,0missing. Rendering/export completeness still unverified |

Unit19 rebuilt/executed `Passed11762/0/0`, including all source almanac entries, local field config reload and old9-column native wire rejection. Unit17 compile Failed four char8_t literals; fixed without global compiler-flag changes. Unit18 passed but retained octal warnings; allASCII digits escaped and realunit19compile now has0C4125/C2664. Native26 wholeFailed at obsoleteclientOptions access after successfulcol9fusion/bounds; directbutton27/runtime pending. PortraitExport05 actual281plants/63zombies validated,25unresolved. Classic25 fullintro Adventure8Soak1066/800 `Passed` E7315972, owned cleanup. Native10 runtime/late spectators, full source abilities/campaign/minigames, native almanac/store UI and missing RU descriptions remain unfinished. Source-only concern to investigate after grid checkpoint: native mystery RNG-failure recovery still enters Classic RestartSeedChooser while native selection remains committed Intro; do not generalize Classic RNG-recovery evidence to this path.

Historical task states below are superseded by this checkpoint where they conflict.

### Verified shared native checkpoint — 2026-11-13, 10:25 Moscow

- App25 `Passed`. Whole native Manual25 `Passed`,635 common native snapshots and owned cleanup: work/fusion/native-world-build25/run_9cdf07cc8f8e4ad09a62d0f420bc7176. Both peers exercised actual shared1/2Ready and own-first chooser/bank, retained packet IDs/CD, cross-player fusion, native combat, isolated5/25sun IDs and250→255→280 ledger, retired-coin metadata through removal, native shovel.
- NativeMysteryD10 on24 `Passed`; NativeMysteryD1 on25 `Passed`,35 common hashes and owned cleanup: work/fusion/native-mystery-build25-d1/run_dd9f4707d97f47c5a10f71d8a2237db2. Actual On–Off–On/secret full Intro/atomic reveal/purchase. Common Board::GetSeedPacketCost now supplies authoritative extra-card price. Earlier failed23 price and24 retirement evidence remain preserved, not counted as passes.
- Last unit15 on23 `Passed`:8405checks/0failures/0skipped. Classic25 fullintro Adventure8Soak1066/800 is currently `IN_TESTING`. Native gameplay scope remains9×5 and plant kinds0/1/1000, not full Fusion.
- Grid10 implementation is next after this regression. FieldVIS02 cache helper compiled25 but not wired/called/runtimeverified. I18N02 found300 actual candidate RU glossary strings after excluding5 reference-only placeholders; full almanac/store adapters remain required.

Historical task states below are superseded by this checkpoint where they conflict.

### Reopened co-op UI acceptance — 2026-11-13, 09:22 Moscow

User correctly reported that native22 still bypassed two existing co-op mechanisms: literal READY rather than sharedreadycount, and forcedhostpacketorder rather than eachviewer'sowncardsfirst. Root acknowledged this as an implementation error. FUSION-UI-REUSE02/READY-REUSE03/UI-QA04 are IN_PROGRESS: reuse actual sharedCoopRefreshStartButton/NetManagerReadyMystery API, extract existingClassic cyclicownerpolicy, preserve native SeedPacket IDs/CD while sharedbank sorting/retargeting heldcursors. Root removes forcednativecanonical snapshot overwrite and prohibits aliasmatching normal/extra. New runtime must prove actual1/2frames forbothpeers, own-firstchooser/bank and subsequentrealpurchase/fusion; sourcechanges alone are not acceptance. Native22 whole pass below remains gameplay evidence only, not acceptance of these reopened UI requirements.

Classic22 fullintro Adventure8Soak at1066/800 Passed3314E17F with ownedcleanup. NewnativeMysteryD1/D10fixture+runner ready, Rootregistration added, notyetbuilt/run. Grid10/skintoggle planned only after this co-opcheckpoint.

### Latest steering / verification — 2026-11-13, 09:07 Moscow

- Update09:14: native22 whole `Passed`,350host/350client applied/350common native snapshot hashes, owned cleanup true: work/fusion/native-world-build22/run_73d69ed52e4a47c4aac068c93f70c9c4. Real native cross-player fusion20→21/kind1000, parent IDs/price/CD, stale-target rejection, normal zombie walk/headless/death, projectile8/5sun,25sun/double-click single-credit, shovel/Escape/RMB all covered. Root inspected actual chooser and zombie-walk frames: Mystery fits, topgap reduced, native zombie proportions normal. Scope remains three plant kinds/stock9x5 validation, not full campaign.
- Intro22 Complete `Passed`536frames/120prelude/PTS0/773queuedAAC, maxpumpgap67.67ms, maxFIFO6/8audioframes queued beforefirstpresentation. Actual BMP0.5/1.5/3/4.5. Escape22 `Passed`; both owned cleanup succeeded. Paths work/intro/startup22/run_ecf6d06afde04844863eed822728cda4 and escape22/run_de48c864a2bf4290ae92e83ea3211f31. Audible fidelity Not tested. Latest player now verified for supplied MP4 visual/completion/window/skip behavior.
- Root now runs Classic Adventure8Soak22 at1066/800/fullIntro to verify current shared-code regression before native10 extension. Do not build while this pair lives. Source NativeMystery QA pending registration/build23; no new world code meanwhile.

- User reports Mystery stone background too narrow and excessive chooser top gap. Build22 reuses the exact Classic co-op `NetUi::StoneButton` renderer for Mystery, and places native Fusion catalog at y123 (40px higher), with matching clip/hit/scroll viewport. Actual new frame review is pending native22; Classic layout unchanged.
- Grid truth: current native validation remains9x5, not an already extended10column campaign. User permits Fusion field artwork; asks for a remembered in-game Native/Fusion field-skin toggle only if native10column field can work correctly. Native10 requires safe mode-aware geometry/terrain access, not stretching a9column bitmap or blindly raising raw-saved MAX_GRID_SIZE_X. GRID01 read-only plan in progress; no skin toggle exists yet.
- Win32 MSVC ABI01 helpers executed: HEAD Board sizeof23084/pausedOffset485 versus current23088/489; saved raw tail22599bytes both, checked relative members unchanged. Plant368/Zombie456/Projectile228/Coin232/SeedPacket96/SeedBank3132 unchanged. This establishes layout compatibility, not an actual load-game/runtime result. Evidence work/fusion/abi.
- Build20 and21 Passed; unit14 Passed8370/0/0. Native20 Failed only the same-poll synthetic READY hover;21 reached native chooser/full Intro/Playing and host Plant20 on both peers, then Failed client fusion because fixture clicked field before authoritative held-card echo. Fixture22 waits the exact stable held packet before SDL planting. No complete native gameplay pass yet.
- Intro20 Complete Passed536frames/120prelude/PTS0,4realBMPs,77playback WM_NULL samples(max3ms), maxpumpgap61.155ms. Escape20 Passed61frames, real scoped Escape, skippedtrue/failedfalse. Supplied MP4 hashes all match. Audio listening Not tested. Additional offline decode-order review found only0–23ms audio lookahead; bounded6frame FIFO now built22, so Complete/Escape22 reruns are required before accepting the latest player. No source MP4/transcoding changes.
- Build22 Passed (UI/FIFO/fixture changes). Root now runs startup22; keep games serialized and do not rebuild until owned processes cleaned. Content builds a separate native MysteryD1/D10 fixture, Mechanics investigates GRID01, Architect finishes ABI evidence. No commit/push/release.

### Active native integration / intro checkpoint — 2026-11-13, 08:47 Moscow

- FUSION-NATIVE-01 `IN_TESTING`, Root: real Board/Plant/Zombie/Projectile/Coin/SeedPacket/SeedChooser, metadata-only NativeContext, normal NETMSG_SNAPSHOT with strict FNS1 metadata. New native modules are built in19; solo gameplay boundary, roster transfer and ordered native START_PLAYING/reveal are additional source changes in build20. No separate entity world is accepted for production. Old runtime remains a reference pending removal from obsolete paths.
- Supported native validation content is only plants0/1/1000, normal zombie and bullet8, stock9x5 grid/fresh4cards. Entire source campaign,10column migration, remaining plants/zombies/minigames/almanac and native mystery/late observer acceptance remain unfinished. Ordinary-user Fusion activities stay closed.
- Native19 Failed before client initialization: fixture nickname18characters exceeded16. Runner shortened names. Native19b connected both peers, then Failed stage1 because the shared native cutscene chose Classic replay Dave1601 and waited for input. Mechanics added a Fusion-only guard before the Classic dialogue chain; real pan/chooser/full ready intro remain shared. This fix is implemented, runtime pending.
- Build19 `Passed`, protocol36. Unit13 `Passed`:8354checks/0failures/0skipped. Build18 compiler failure (invalid hit-type enum) was corrected before19. Build20 is running; it includes16additional native codec checks for Choosing/committed flags/free-plant provenance. Do not count these as passed until execution.
- INTRO-01/02 `IN_TESTING`, Root: supplied C:/PvZ/intro.mp4 is now assets/videos/intro.mp4; MSBuild always copies that version to $(OutDir)/videos/intro.mp4. Source/assets/output SHA256 all4A325C5C1743F02D1923B4D6FCB561155E8ED1DF1E300ED8770E454F7DE5F0AD. Confirmed cause: old player drops pre-audio H264 IDR; custom next key3.866667s, independently reproduced by dropping only packet0. New StartupVideo.cpp feeds all packets, renders/pumps on caller window thread and drains decoders/audio.
- Intro startup19 `Failed`: runner applied WM_NULL during pre-video resource initialization and cleanup interrupted before frame0. Failed artifacts retained in work/intro/startup19/run_c8b35e7739c148b59b08dfc61d1a1f11. QA now uses atomic actual player-entry evidence/exact SDL HWND; pre-entry samples remain diagnostics, strict250ms playback bound and early real renderer BMP/PTS/completion/hash checks unchanged. Actual playback fix is not yet VERIFIED; audio listening and skip workflow Not executed.
- Ownership: Root shared app/Board/network/build/runtime/docs; Architect codec frozen/read-only review; Mechanics native CutScene frozen; Content intro runner frozen. Games only hidden owned processes, no CUA/global input; no build while games alive. Both native19 runs cleaned. No commit/push/release yet.

Next: finish20+unit, rerun intro with entry marker and inspect early renderer images; run native Fusion20, then cover native mystery/late joins and Classic regression on the resulting binary. Previous reference-world17 evidence below does not verify this new native path.

### Current architectural checkpoint — 2026-11-13, 07:30+ Moscow

- Previous standalone-world acceptance is reference evidence only, not acceptance of the now-clarified production architecture. Root acknowledged over-isolation; no further development of a second production engine is authorized by the clarified scope.
- Build17 Passed; unit12 Passed `checks=7802 failures=0 skipped=0`. Shared native Coin collection math/REANIM_SUN are implemented. Fusion schema2 rejects the old field semantics.
- Reference native17 Manual Passed (`work/fusion/fusion-native17/run_f768fb443f584666ad3d5ccc43eb4ba2`):239 host transmissions,236 client applications/common hashes. Reference Mystery17 Passed (`work/fusion/fusion-mystery17/run_bdd711267a24498da46e981e5a697188`),29 common hashes after actual newer epoch and native purchase. Both owned cleanups succeeded. Observer17 is Not executed and deferred while replacing the architecture.
- Native15 Failed its required Intro frame capture despite both own pass results; inspection showed the client DID observe Intro but the fixture waited300ms before capture. Capture now occurs when the phase is actually observed; subsequent Manual17 Passed all required frames.
- Classic regression app17 Passed: `work/fusion/classic-soak17/run_eb534ec996d64c469586a9f797341c51`, real hidden1066/800 peers, full intro, personal chooser, native plant/sun inputs, native loss/camera/dialog sequence and common snapshot hash03A2CE28. This verifies the current shared changes, not future integration.
- Latest user corrections reopen native UI integration: literal SeedBank/SeedPacket/SeedChooser function reuse is required; prototype captions removed in source after app17, unbuilt. The giant zombie leg remains a confirmed, unverified-fix prototype bug. Engineer2 has only investigated native Zombie/Plant draw anchors and has not edited production to claim a fix.
- Architect is defining the native context/UI/snapshot contracts. SeedBank storage is already32 but stock layout max10; preserve50x70 cards and original geometry for fresh4, design a native additional row/scroll later for14. Native MAX_GRID_SIZE_X is9 and raw-saved; source campaign10columns needs safe grid extension/accessors, not a blind bounds relaxation. The first native validation subset may use the stock grid, explicitly not the finished source campaign.

Immediate owners: Root = Board/LawnApp/native network integration and builds; Architect = contract then real native UI; Mechanics = native Plant/Projectile/Coin ability hooks after context API; Content = unique1000 asset draw inside real Plant, baseline Zombie stays native. No agent owns permission to create a second set of gameplay entities.

### Verified checkpoint — 2026-11-13, 06:44 Moscow

- Working tree contains the initial native port modules and is NOT committed/pushed. Source baseline remains5bfba06, protocol35 on development branch. Ordinary-user Fusion availability stays closed: source Adventure is not fully implemented; only the audited `fusion_validation/1` smoke activity can start the new runtime.
- App build14 Passed with Fusion simulation/bank/tutorial/world cards/presentation/runtime, explicit mode negotiation, START/ROOM/COMMAND wire, Board dispatch and raw-save isolation. Earlier build11 Failed on ambiguous fixture helper name; renamed. Native12 Failed image loading because the old image library appends extensions; adapter now passes image stems. Native13 exposed unsupported 3D hierarchy rotations; renderer now composes full quaternion/TRS before projection.
- Unit10 Passed `checks=7783 failures=0 skipped=0`. Includes source six-plant kernel (0/1/1000/1030/1032/1090), recipe table, projectile timers, transactional world/start/room/command wire, personal bank and tutorial/world cards. This does NOT establish full gameplay or animation parity.
- Asset validation Passed22/Failed0 for catalog and18 source packages; 900 Euler-key geometry checks Passed. Native host/client renderer frames were inspected for chooser and the actual fused PeaSun. Exact visual parity against the running Unity original remains unverified.
- Dedicated native14 Passed: `work/fusion/fusion-native14/run_b51edc603c1743438f9e8769df8757af/summary.json`, protocol35, host202 transmissions, client198 applications and198 common room hashes, identity-checked owned cleanup succeeded. Real SDL interactions covered personal normal/extra selection, ready/intro, host Peashooter→client Sunflower fusion into1000 with identical ID/parents/HP/sun, stale input with no charge, bullet8/5sun/headless/death, single-credit collection and shovel. Original Adventure, mystery runtime and late observer were not exercised by this run.
- Native menu attempts2/4 Failed on fixture transition/barrier races; corrected. Attempt8 reached both own pass results and captures, then the generic Adventure runner Failed because it expected unrelated adventure actor markers. Keep all artifacts; a dedicated runner is being written rather than weakening the existing runner.
- Dedicated GameModes native14 subsequently Passed at06:57 Moscow: `work/fusion/gamemodes-native14/run_0383adada8c14bc98356ad7ce6d7c953/summary.json`. Actual host/client mouse/keyboard modal/authority/availability checks reached Classic Playing, host15 snapshots/client14 applied, common hash1E73EEDD, owned cleanup succeeded. Root inspected the actual client modal renderer capture. The earlier generic-runner failure is retained as historical evidence, not the current acceptance result.
- Root owns integration/Net/Board/LawnApp/wire/test registration. Engineer1 now extends only `NetFusionSmoke.cpp` and `RunFusionSmoke.ps1` for independent mystery and late-observer cases. Engineer2 owns the Runtime UI sections for the user-requested Classic native chooser/bank/shovel replacement. Engineer3 completed source wave investigation; simulation/tutorial code is frozen.
- Reviewer findings implemented and included in build14: NEXT sequence counter reset1/successful-send handling, deferred transport failure, per-slot Fusion purge/apply-time authentication, roster transition rules, Classic gameplay isolation and raw save/load refusal. Native14 verified ordered command acceptance/rejection; transport-failure, roster-churn and malformed-network scenarios remain separately unexecuted.
- Complete reports/evidence: `work/fusion/{architecture,mechanics,content,levels,tutorial,waves,runtime,integration-review,network-review,wire-review}.md`; `work/fusion/recovered/` contains Ghidra evidence and exact source contracts.

### Immediate next actions

Native chooser/bank/shovel source changes are ready, unbuilt. Implement shared native Coin collection math and native REANIM_SUN rendering, then build and run actual host/client workflows because the UI/sun path changed. Inspect renderer captures and run mystery/late-observer acceptance; dedicated menu acceptance has Passed. Root added a strict validation-only spectator-intent sender and host rejection marker; this change is after build14 and is not yet built/tested. Keep the full content/gameplay integration active after this validated chain.

Source first Adventure still needs runtime composition of tutorial/dropped cards/glove, exact wave scheduler including dying-corpse health contributions, mowers/loss, trophy/progression and its own persistent save. It is10×5/500sun, fresh4 card identities, max14bank; source difficulty2 is the default at co-op difficulty1. Tutorial teaches1→1000→1032 and grants3free pea world cards; waves start after completion, first source D2 level has105normal zombies in10waves. Native validation must not be described as this completed campaign.

| ID | Priority / size | Owner | Deliverable / acceptance | Status / dependency |
|---|---|---|---|---|
| FUSION-S00 | P0 / small | Root | Fresh ReleaseGOTY Win32 + existing unit suite, then native Classic control | DONE; build Passed, tests1871/0/0 Passed, native Adventure1 control protocol34 hashBE29997A Passed |
| FUSION-A01 | P0 / medium | Architect | Actual-code mode/save/network/ID contracts | DONE; `work/fusion/architecture.md`; no gameplay acceptance implied |
| FUSION-I01 | P0 / medium | Engineer1 + Root | Separate content mode, strict WELCOME/LOBBY/START protocol35 codecs and negative tests | DONE for mode negotiation; pure tests and dedicated native menu/Classic transition Passed |
| FUSION-I02 | P0 / large | Engineer2 (fusion_content) | Deterministic original-data exporter/native catalog + faithful animation pilot | IN_PROGRESS; local original CSV/JSON/prefab evidence |
| FUSION-I03 | P0 / medium | Engineer3 (fusion_mechanics) | Complete verified directional recipe table, source rules and pure lookup | DONE for recipe data/lookup; independent417store checks and unit table tests Passed; runtime coverage remains separate |
| FUSION-UI01 | P1 / medium | Root | Native selector/modal, mode authority/lifetime and Classic regression | DONE; dedicated native menu14 Passed; ordinary Fusion remains explicitly unavailable until campaign implementation |
| FUSION-PLAY01 | P1 / large | Root + engineers | First complete source-verified Fusion planting/combat/fusion path with actual native animations and host/client state | IN_PROGRESS; native validation planting/fusion/combat/collection/shovel Passed on two processes; original tutorial/Adventure orchestration remains |
| FUSION-NATIVE-UI01 | P0 / medium | Engineer2 + Root | Classic chooser/card/bank/shovel elements, preserving Fusion pricing/cooldowns/ownership and actual host/client workflow | IN_PROGRESS; new user priority after native14 |
| FUSION-QA02 | P1 / medium | Engineer1 + Root | Actual mystery reveal/purchase and third-process late-observer state/authority validation | IN_PROGRESS; fixture/runner implementation, Root controls build/process slots |
| FUSION-NATIVE-SUN | P0 / medium | Engineer2/3 + Root | Reuse native REANIM_SUN and shared Coin collection math, preserve Fusion5/25 values and single host credit | IN_PROGRESS; new user reuse priority; source fall motion is not yet migrated |
| FUSION-CONTENT | P1 / very large | Root | All behavior families, source levels/mini-games/Travel/almanac, full chooser/mystery/difficulty support | BACKLOG; depends on verified PLAY01 mechanism; overall task remains incomplete |
| FUSION-ACCEPT | P1 / large | Root / independent review | Integrated native runtime, malformed-state tests, real interaction/latejoin/regression evidence | BACKLOG; depends on implemented systems |

## Evidence and constraints

- `work/fusion/baseline-build.log`: fresh baseline build exit0. `work/fusion/baseline-tests.log`: checks1871 failures0 skipped0.
- Original metadata has287 PlantType entries (including markers, max1192),95 ZombieType entries,100 BulletType entries. Enum counts are not verified playable counts.
- Supplied analysis sections cover46/248 plant behavior classes,29/89 zombie classes and0/80 bullet classes. Titles calling the document complete are not proof of coverage.
- Source `Resources/plant_data.txt` is GB18030 CSV; `PlantDataLoader.LoadPlantData` actually parses it. Original almanac JSON is separately available. Exporter records source hashes and missing data.
- Use owned hidden native smoke processes and renderer-only captures. Respect existing process identity checks and do not build while games run. Root serializes launch/build slots.
- No gameplay feature is verified by metadata extraction, source implementation or a passing compile alone. Record real checks and remaining gaps here as work progresses.

## Next actions

Finish mode/catalog/recipe contracts and native UI, inspect actual converted animation output, then integrate the first complete Fusion gameplay path. Extend through source behavior families only after the reusable authoritative mechanism works. Keep the full user objective active; do not report menu/catalog work as a completed Fusion port.
