# Confirmed findings during play-test delivery

`FIXED` means implemented; `VERIFIED` requires executed evidence. Full original requirements remain in REQUEST.txt.

| ID | Cause / impact | State and evidence |
|---|---|---|
| B-READY-REENTRY | A committed chooser remained eligible for another ready/start tick, continually resetting intro to4260; TD emitted5136 CHOOSER_START frames | FIXED: active chooser lifecycle guard and bounded auto-ready. EXE4b crossed the formerly failing TD intro; complete spectator run pending |
| B-ADVICE-OVERFLOW | A translated153-byte Russian advice copied into128-byte arrays, corrupting adjacent fields and UTF-8 | VERIFIED: dynamic SexyString storage; Fresh4b complete153-byte UTF-8 equality plus actual readable host/client pickup renders; full Fresh4b Passed |
| B-CHAT-LITERAL | TodDrawString treated `[nickname] message` as a localization key, stripped the final character and rendered Missing/`>` | FIXED: network text uses the literal font renderer. Dedicated Unicode/input regression pending |
| B-CHAT-DRAFT | Trimming a draft rejected trailing spaces; byte backspace removed half a Cyrillic code point | FIXED: untrimmed draft validation and code-point backspace. Unit whitespace cases Passed; incremental runtime fixture pending |
| B-PVP-SHOVEL | Zombie players could take/use plant tools; confirmed by the user's own play-test | FIXED: host-authoritative acting-team checks at hit, pickup and use, plus viewer-specific draw. PvP regression pending |
| B-PVP-GOAL | Stock level award could end PvP early; death polling missed zombies killed outside their update pass; repeated fade initialization | FIXED: custom-mode award guard, once-only death-edge count, one-time objective fade. Real death fixture98→99→100 pending |
| B-GITHUB-ICON | ImageLib appends an extension, so an explicit `.png` caller searched for `.png.png` | FIXED: basename caller. Found in actual Fresh menu review; new image assertion pending |
| B-RTC-LIFETIME | Deleting a peer connection did not delete its separately-owned data channels / quiesce their callbacks | FIXED: delete peer then channels outside mutex before freeing callback owner. Native15 checks and actual game WebRTC pair Passed |
| B-FX-MISSING | Client simulation is intentionally disabled; assigning flags does not produce stock limb/effect/attachment side effects | FIXED: presentation-only transitions, reliable standalone effects and intro snapshots. Fresh demo/sod/reward rays Passed; dedicated limb/jack/snail/avatar fixture pending |

## Test infrastructure findings

- The original background capture read an unused widget texture. Old black BMPs are not visual evidence. The current capture reads only this game's SDL renderer before Present and keeps an owned surface.
- EXE3 interaction helper was compiled31seconds before its capture correction. Logic passed, eight helper images were black; repeat on current binary is required.
- Fresh3 stopped after a fixture ACK/scene-transition race. It is Failed as a whole. Fresh4b includes the race and rate-limit fixes and Passed.
- No Computer Use, desktop capture, global input injection or forced foreground activation is permitted by this task.
