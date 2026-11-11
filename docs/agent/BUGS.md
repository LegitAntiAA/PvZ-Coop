# Confirmed findings during play-test delivery

`FIXED` means implemented; `VERIFIED` requires executed evidence. Full original requirements remain in REQUEST.txt.

| ID | Cause / impact | State and evidence |
|---|---|---|
| B-CHALLENGE-BOARD-LIFETIME | SHOW_CHALLENGE_PAGE entered a full scene without destroying the client's previous Board; BringToBack placed the menu underneath it | VERIFIED: universal ShowChallengeScreen teardown; Promote9b Passed clean boundary, three-player chooser and slot2 planting mirrored on all peers |
| B-SNAIL-BODY-RATE | Reanimation::PlayReanim(rate0) preserves the old rate; native StinkyFinish explicitly sets0 but the network sleeping pose did not, so client body could emerge while Zzz slept | VERIFIED: explicit sleeping body rate0; Presentation7 Passed9stages/18frames and independent matching closed-shell/Zzz image review |
| B-BROWSER-PENDING | A failed pre-open RTC connection left directoryConnected true with no structured error, so JOINING could remain pending | FIXED in5b source: direct-connection-failed, immediate local membership cleanup, idempotent leave, and no redundant directory drop. Native PUBLIC22/22 and Worker9/9 Passed including pending failure→retry→realRTC; NativeUI6 real browser password/capacity errors, Cancel and retry Passed |
| B-CONFIG-CWD | NetLoadConfig ran before SexyApp changed CWD; relative saves then targeted a different coop.ini, losing tint/unlock on another launch path | VERIFIED: default executable-path child-process and CWD tests Passed in665/0 suite; NativeUI5b actual SDL tint64/128/191 survived Close/Reopen and full host/client restart, and config override enabled actual minigames clicks with zero progress |
| B-SPECTATOR-CURSOR | OnClientCursor explicitly discarded observers and PvP overlay discarded every remote cursor | FIXED: observer pointer accepted with neutral host-sanitized cursor, PvP draws neutral observer cursors. Kick5b neutral cursor/communication Passed |
| B-SPECTATOR-PROMOTE | Observers only became active at main menu, so next-level/Endless transitions could leave them read-only indefinitely | FIXED: common completed-level/start/retry boundaries invalidate old input, promote and balance retained owned cards. VERIFIED by Promote9b: observer becomes slot2, bank10/quota4-3-3 and actual free Puff placement agreed on all3 peers |
| B-LATE-START | Custom TD board used internallevel0 while its original START descriptor usedlevel1; reconstructing START from board lost the source descriptor | FIXED: keep exact START payload and replace roster count only. Kick5b and Promote9b Passed exact TD descriptor on late join |
| B-READY-REENTRY | A committed chooser remained eligible for another ready/start tick, continually resetting intro to4260; TD emitted5136 CHOOSER_START frames | FIXED: active chooser lifecycle guard and bounded auto-ready. EXE4b crossed the formerly failing TD intro; Kick5b/Promote9b complete runs Passed |
| B-ADVICE-OVERFLOW | A translated153-byte Russian advice copied into128-byte arrays, corrupting adjacent fields and UTF-8 | VERIFIED: dynamic SexyString storage; Fresh4b complete153-byte UTF-8 equality plus actual readable host/client pickup renders; full Fresh4b Passed |
| B-CHAT-LITERAL | TodDrawString treated `[nickname] message` as a localization key, stripped the final character and rendered Missing/`>` | FIXED: network text uses the literal font renderer. Dedicated Unicode/input regression pending |
| B-CHAT-DRAFT | Trimming a draft rejected trailing spaces; byte backspace removed half a Cyrillic code point | FIXED: untrimmed draft validation and code-point backspace. Unit whitespace cases Passed; incremental runtime fixture pending |
| B-PVP-SHOVEL | Zombie players could take/use plant tools; confirmed by the user's own play-test | VERIFIED: PvP5b BF47BFE0 Passed, SDL/raw/forged cursor rejected for Zombies and legitimate Plants shovel accepted; Root independently viewed private zombie/plant HUD frames |
| B-PVP-GOAL | Stock level award could end PvP early; death polling missed zombies killed outside their update pass; repeated fade initialization | VERIFIED: PvP5b actual native deaths98→99 held without award/fade, then100 began decreasing fade once; both99/100 frames and matching post-action hashes observed |
| B-GITHUB-ICON | ImageLib appends an extension, so an explicit `.png` caller searched for `.png.png` | VERIFIED: basename caller; Fresh4d Passed560x560 source asset and8,8,32,32 icon bounds; actual client menu frame independently viewed |
| B-RTC-LIFETIME | Deleting a peer connection did not delete its separately-owned data channels / quiesce their callbacks | FIXED: delete peer then channels outside mutex before freeing callback owner. Native15 checks and actual game WebRTC pair Passed |
| B-FX-MISSING | Client simulation is intentionally disabled; assigning flags does not produce stock limb/effect/attachment side effects | FIXED: presentation-only transitions, reliable standalone effects and intro snapshots. Fresh demo/sod/reward rays Passed; Presentation7 Passed dedicated limb/jack/snail/avatar fixture with actual renderer frames |

## Test infrastructure findings

- The original background capture read an unused widget texture. Old black BMPs are not visual evidence. The current capture reads only this game's SDL renderer before Present and keeps an owned surface.
- EXE3 interaction helper was compiled31seconds before its capture correction. Logic passed, eight helper images were black; repeat on current binary is required.
- Fresh3 stopped after a fixture ACK/scene-transition race. It is Failed as a whole. Fresh4b includes the race and rate-limit fixes and Passed.
- No Computer Use, desktop capture, global input injection or forced foreground activation is permitted by this task.


- B-CHOOSER-FINAL-CARD — VERIFIED9b: ready counted flying cards while final serializer found only settled cards, producing9/10 and allowing later intro snapshots to erase relative order. Settled-card gate, exact quotas and ordered final barrier now validated by Promote9b across3peers (bank10/hash7AC708CE/actual planting). Modal-ready recheck and shared pick/unpick FIFO included; dedicated rapid-swap/modal-race test Not executed.
