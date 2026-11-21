"""Recover source campaign configuration; never label native levels as Fusion levels.

Input is the saved, bounded Ghidra evidence, not a live game or guessed data.
Run from the repository root. Source NPC selection/AI/runtime are separate work.
"""
import hashlib
import json
import operator
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
RECOVERED = ROOT / "work/fusion/recovered"
STAGE = ROOT / "work/fusion/stage272/modes"


def load(path):
    value = json.loads(path.read_text(encoding="utf-8"))
    return json.loads(value["content"][0]["text"]) if "content" in value else value


def functions(path):
    return {f["name"].replace("$$", "__"): f["decompiled_code"].replace("\r", "")
            for f in load(path)["functions"].values()}


def switch_body(code, parameter):
    start = code.index("switch(" + parameter + ")")
    begin = code.index("{", start)
    depth = 1
    for index in range(begin + 1, len(code)):
        depth += (code[index] == "{") - (code[index] == "}")
        if depth == 0:
            return code[begin + 1:index]
    raise ValueError("unbalanced source switch")


def cases(code):
    labels = list(re.finditer(r"^\s*case (0x[0-9a-f]+|\d+):", code, re.M))
    result = {}
    for index, label in enumerate(labels):
        stop = labels[index + 1].start() if index + 1 < len(labels) else len(code)
        body = code[label.end():stop]
        next_index = index + 1
        while not body.strip() and next_index < len(labels):
            stop = labels[next_index + 1].start() if next_index + 1 < len(labels) else len(code)
            body = code[labels[next_index].end():stop]
            next_index += 1
        result[int(label[1], 0)] = body
    return result


def allowed_adventure(code, level):
    """Trace only fixed allow-list writes for a valid initialized source bool array.

    The complete function has no RNG/game-state tests or loops. Every accepted
    condition is either class-init, null, or array bounds; unknown forms abort.
    Gotos are followed, including targets shared by different switch cases.
    """
    lines = code.splitlines()
    labels = {m[1]: index for index, line in enumerate(lines)
              if (m := re.fullmatch(r"\s*(LAB_[0-9a-f]+):\s*", line))}
    starts = {int(m[1], 0): index + 1 for index, line in enumerate(lines)
              if (m := re.fullmatch(r"\s*case (0x[0-9a-f]+|\d+):\s*", line))}
    if set(starts) != set(range(1, 46)):
        raise ValueError("source campaign case coverage changed")

    def condition(value):
        value = re.sub(r"\*\((?:u?int) \*\)\(lVar1 \+ 0x18\)", "4096", value)
        value = re.sub(r"\*\(int \*\)\(InitZombieList_TypeInfo \+ 0xe0\)", "1", value)
        value = value.replace("lVar1", "1").strip()
        match = re.fullmatch(r"(0x[0-9a-f]+|\d+)\s*(==|!=|<=|>=|<|>)\s*(0x[0-9a-f]+|\d+)", value)
        if not match:
            raise ValueError("unsupported source condition: " + value)
        return {"==": operator.eq, "!=": operator.ne, "<=": operator.le,
                ">=": operator.ge, "<": operator.lt, ">": operator.gt}[match[2]](
                    int(match[1], 0), int(match[3], 0))

    result = []
    index = starts[level]
    for _ in range(10000):
        line = lines[index].strip()
        if line.startswith("if ("):
            depth, end = 0, None
            for i in range(line.index("("), len(line)):
                depth += (line[i] == "(") - (line[i] == ")")
                if depth == 0:
                    end = i
                    break
            if end is None:
                raise ValueError("multiline source condition not supported")
            accepted = condition(line[line.index("(") + 1:end])
            tail = line[end + 1:].strip()
            if not accepted:
                if tail == "{":
                    depth = 1
                    while depth:
                        index += 1
                        depth += lines[index].count("{") - lines[index].count("}")
                index += 1
                continue
            line = tail
        write = re.fullmatch(r"\*\(undefined1 \*\)\(lVar1 \+ (0x[0-9a-f]+|\d+)\) = 1;", line)
        if write:
            result.append(int(write[1], 0) - 0x20)
        jump = re.fullmatch(r"goto (LAB_[0-9a-f]+);", line)
        if jump:
            index = labels[jump[1]] + 1
            continue
        if line == "return;":
            return sorted(set(result))
        if line == "break;" or "FUN_180281cb0();" in line or "FUN_180281ca0();" in line:
            raise ValueError("source valid-array path entered an exception")
        if "InitZombieList__Allow" in line:
            raise ValueError("source allow-list helper now requires independent extraction")
        index += 1
    raise ValueError("source allow-list trace exceeded finite bound")


def export():
    primary_path = STAGE / "source-level-mode-primary.json"
    primary = functions(primary_path)
    old = functions(RECOVERED / "levels-ghidra-batch1.json")
    scene_code = primary["UIMgr__GetSceneType"]
    adventure_wave_code = primary["InitZombieList__InitAdvWave"]
    challenge_wave_code = primary["InitZombieList__InitChallengeWave"]
    adventure_wave_cases = cases(switch_body(adventure_wave_code, "param_1"))
    challenge_wave_cases = cases(switch_body(challenge_wave_code, "param_1"))
    challenge_start = re.search(r"\n  case 1:\s*switch\(param_2\)", scene_code)
    if not challenge_start:
        raise ValueError("source Challenge scene dispatcher changed")
    challenge_scene_cases = cases(switch_body(scene_code[challenge_start.start():], "param_2"))
    label_scenes = {m[1]: int(m[2], 0) for m in re.finditer(
        r"(switchD_[A-Za-z0-9_]+):\s*return (0x[0-9a-f]+|\d+);", scene_code)}
    adventure_start = re.search(r"\n  case 0:\s*switch\(param_2\)", scene_code)
    adventure_scene_cases = cases(switch_body(scene_code[adventure_start.start():], "param_2"))
    normal_scene_code = functions(RECOVERED / "levels-ghidra-batch3.json")["Board__NormalSceneSettings"]
    normal_scene_cases = cases(switch_body(normal_scene_code, "param_2"))
    for scene, sun, rows in ((1, 150, 5), (2, 300, 6), (3, 150, 6), (4, 300, 5)):
        body = normal_scene_cases[scene]
        if scene == 1:
            label = re.search(r"goto (switchD_[A-Za-z0-9_]+);", body)[1]
            body = normal_scene_code[normal_scene_code.index(label + ":"):]
        match = re.search(r"\(param_1 \+ 0xcc\) = (0x[0-9a-f]+|\d+);", body)
        assert match and int(match[1], 0) == sun
        if rows == 6:
            assert "(param_1 + 0x10c) = 6;" in body
    catalog_path = ROOT / "assets/fusion/catalog/modes.json"
    enum_rows = load(catalog_path)["ChallengeLevel"]
    scalar_assignment = re.compile(r"\+ 4\)\s*=\s*(0x[0-9a-f]+|\d+);")

    def wave_count(body, default):
        match = scalar_assignment.search(body)
        return int(match[1], 0) if match else default

    adventure = []
    for level in range(1, 46):
        scene = (level - 1) // 9
        scene_body = adventure_scene_cases.get(level, "return 0;")
        assert int(re.search(r"return (0x[0-9a-f]+|\d+);", scene_body)[1], 0) == scene
        allowed = allowed_adventure(old["InitZombieList__AdvantureZombieTypeSpawn"], level)
        if not allowed or any(k < 0 or k > 255 for k in allowed):
            raise ValueError("invalid source campaign allow list")
        adventure.append({"level": level, "scene": scene, "columns": 10,
            "rows": 6 if scene in (2, 3) else 5,
            "initialSun": (500, 150, 300, 150, 300)[scene], "initialMoney": 3000,
            "maximumWavesFirstCycle": wave_count(adventure_wave_cases[level], None),
            "laterCycleWaveBonus": 10, "allowedZombieKinds": allowed,
            "nativeProjection": {"background": scene,
                "layoutLevel": scene * 10 + (level - 1) % 9 + 1,
                "meaning": "native layout carrier only; not progress/unlock or source gameplay parity"}})
    assert adventure[0]["allowedZombieKinds"] == [0]
    assert adventure[1]["allowedZombieKinds"] == [0, 2]
    assert adventure[0]["maximumWavesFirstCycle"] == 10
    assert all(r["maximumWavesFirstCycle"] in (10, 20, 30, 40) for r in adventure)

    challenges = []
    for row in enum_rows:
        ident = row["value"]
        body = challenge_scene_cases.get(ident, "return 0;")
        result = re.search(r"return (0x[0-9a-f]+|\d+);", body)
        jump = re.search(r"goto (switchD_[A-Za-z0-9_]+);", body)
        scene = int(result[1], 0) if result else label_scenes[jump[1]] if jump else None
        if ident != 53 and scene is None:
            raise ValueError("unresolved challenge scene " + str(ident))
        challenges.append({"id": ident, "enumName": row["name"], "scene": None if ident == 53 else scene,
            "maximumWaves": None if ident == 53 else wave_count(challenge_wave_cases.get(ident, ""), 20),
            "requiresCustomMapData": ident == 53, "runtimeAdmission": False})

    unlock_code = (STAGE / "unlock-primary.c").read_text(encoding="utf-8")
    unlock_cases = cases(switch_body(unlock_code, "param_1"))
    unlocks = [{"kind": k, "requiredAdventureCompleted": 0} for k in (0, 1)]
    for kind in range(2, 35):
        body = unlock_cases[kind]
        match = re.search(r"return \*\(char \*\)\(lVar2 \+ (0x[0-9a-f]+|\d+)\)", body)
        if not match or "+ 0xd0)" not in body:
            raise ValueError("unresolved adventure base unlock " + str(kind))
        unlocks.append({"kind": kind, "requiredAdventureCompleted": int(match[1], 0) - 0x20})
    assert unlocks[2]["requiredAdventureCompleted"] == 1
    assert unlocks[3]["requiredAdventureCompleted"] == 2
    # Source special cards have sparse plant IDs. Keep their actual IDs rather
    # than treating the dense base-card array index as a universal identity.
    special_unlocks = []
    for kind, body in sorted(unlock_cases.items()):
        if kind < 35 or "+ 0xd0)" not in body:
            continue
        matches = re.findall(r"return \*\(char \*\)\(lVar2 \+ (0x[0-9a-f]+|\d+)\)", body)
        if len(matches) != 1:
            raise ValueError("unresolved adventure special-card unlock " + str(kind))
        completed = int(matches[0], 0) - 0x20
        if not 1 <= completed <= 45:
            raise ValueError("invalid adventure special-card completion " + str(kind))
        special_unlocks.append({"kind": kind, "requiredAdventureCompleted": completed})

    selection_path = STAGE / "source-zombie-selection-primary.json"
    selection = load(selection_path)["decompiled_code"].replace("\r", "")
    weights = [(int(kind, 0), int(weight, 0)) for kind, weight in re.findall(
        r"System_Collections_Generic_Dictionary<Int32Enum,_int>__Add\s*"
        r"\(lVar4,(0x[0-9a-f]+|\d+),(0x[0-9a-f]+|\d+),"
        r"Method_System_Collections_Generic_Dictionary<ZombieType,_int>_Add__\);", selection)]
    assert len(weights) == 49 and len({kind for kind, _ in weights}) == 49
    assert weights[:2] == [(0, 4000), (2, 3000)]
    add_code = functions(RECOVERED / "levels-ghidra-batch2.json")["InitZombieList__AddZombieToList"]
    cost_cases = cases(switch_body(add_code, "param_1"))
    cost_labels = {m[1]: int(m[2], 0) for m in re.finditer(
        r"(switchD_[A-Za-z0-9_]+):\s*iVar9 = (0x[0-9a-f]+|\d+);", add_code)}
    weighted = []
    for kind, weight in weights:
        body = cost_cases.get(kind, "")
        direct = re.search(r"iVar9 = (0x[0-9a-f]+|\d+);", body)
        jump = re.search(r"goto (switchD_[A-Za-z0-9_]+);", body)
        cost = int(direct[1], 0) if direct else cost_labels[jump[1]] if jump else 1
        weighted.append({"kind": kind, "weight": weight, "cost": cost})
    weighted_kinds = {r["kind"] for r in weighted}
    for row in adventure:
        row["finalWaveOnlyKinds"] = [kind for kind in row["allowedZombieKinds"] if kind not in weighted_kinds]
        assert any(kind in weighted_kinds for kind in row["allowedZombieKinds"])

    source_paths = [primary_path, RECOVERED / "levels-ghidra-batch1.json",
        RECOVERED / "levels-ghidra-batch2.json", RECOVERED / "levels-ghidra-batch3.json",
        RECOVERED / "levels-ghidra-batch7.json", RECOVERED / "levels-ghidra-batch8.json",
        RECOVERED / "waves-contract.json", selection_path, STAGE / "source-boss-insertion-primary.json", catalog_path,
        STAGE / "unlock-primary.c"]
    report = {"schema": 1, "sourceVersion": "Fusion2.3.1", "runtimeVerified": False,
        "sourceBinarySha256": load(RECOVERED / "waves-contract.json")["sources"][0]["sha256"],
        "sourceEvidence": [{"path": p.relative_to(ROOT).as_posix(),
            "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for p in source_paths],
        "adventure": adventure, "challenges": challenges, "baseUnlocks": unlocks,
        "adventureSpecialUnlocks": special_unlocks,
        "zombieSelectionInSourceDictionaryOrder": weighted,
        "waveGeneration": {"source": "recovered/waves-contract.json + InitZombieList.InitZombie3E3B60",
            "difficultyMultipliers": [1, 3, 5, 7, 10, 10],
            "initialBudget": "max(1,floor(wave*multiplier/3)); every tenth doubled",
            "weightedDraw": "RandomRangeInt(1,totalWeight+1), first cumulativeWeight>=draw in source Dictionary insertion order",
            "insufficientBudget": "emit source kind0 but subtract the originally chosen kind cost (Adventure)",
            "finalWave": "AFTER weighted fill, append each allowed type once with temporary budget20; excludes212 and218..224",
            "bossAdditions": "UltimateBoss calls TryAddBoss only for already-allowed boss IDs. None of these IDs are in45 Adventure allowlists.",
            "timing": "initial15s; tutorialOver5s; after spawn30s; strict countdown<0; source health early-wave rules"},
        "nativeComparisons": [
            {"sourceId": 38, "nativeMode": "GAMEMODE_CHALLENGE_COLUMN", "status": "shared primitive candidate; source placement/waves still required"},
            {"sourceId": 42, "nativeMode": "GAMEMODE_CHALLENGE_RAINING_SEEDS", "status": "shared drop/card primitive candidate; source deck/waves still required"},
            {"sourceId": 59, "nativeMode": "GAMEMODE_CHALLENGE_POGO_PARTY", "status": "native jumping AI candidate; exact Fusion NPC/rules still required"},
            {"sourceId": 100, "nativeMode": "GAMEMODE_CHALLENGE_BEGHOULED", "status": "shared match primitive candidate; source palette/matches still required"},
            {"sourceId": 102, "nativeMode": "GAMEMODE_SCARY_POTTER_1", "status": "shared vase primitive candidate; source vase contents/win logic still required"}],
        "limits": "Complete static45 Adventure configurations and122 challenge scene/wave-count entries; not complete generated waves, source NPC AI, special-level scripts, rewards or runtime acceptance."}
    output = ROOT / "assets/fusion/catalog/source-levels272.json"
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    generated = ["// Generated by tools/fusion/export_source_levels272.py from saved primary evidence.\n"]
    for row in adventure:
        generated.append("constexpr uint16_t zombies%d[] = {%s};\n" % (row["level"], ",".join(map(str, row["allowedZombieKinds"]))))
    generated.append("constexpr FusionSourceAdventure272 adventure272[] = {\n")
    for r in adventure:
        generated.append("    {%d,%d,%d,%d,%d,%d,%d,zombies%d},\n" % (r["level"], r["scene"], r["rows"], r["columns"], r["maximumWavesFirstCycle"], r["initialSun"], r["initialMoney"], r["level"]))
    generated.append("};\nconstexpr FusionSourceChallenge272 challenges272[] = {\n")
    for r in challenges:
        generated.append('    {%d,"%s",%d,%d,%s},\n' % (r["id"], r["enumName"], r["scene"] if r["scene"] is not None else -1,
            r["maximumWaves"] or 0, "true" if r["requiresCustomMapData"] else "false"))
    generated.append("};\nconstexpr uint8_t baseUnlocks272[] = {%s};\n" % ",".join(str(r["requiredAdventureCompleted"]) for r in unlocks))
    generated.append("constexpr struct { uint16_t kind; uint8_t completed; } adventureSpecialUnlocks272[] = {\n")
    for row in special_unlocks:
        generated.append("    {%d,%d},\n" % (row["kind"], row["requiredAdventureCompleted"]))
    generated.append("};\n")
    generated.append("constexpr FusionSourceZombieWeight272 zombieWeights272[] = {\n")
    for row in weighted:
        generated.append("    {%d,%d,%d},\n" % (row["kind"], row["weight"], row["cost"]))
    generated.append("};\n")
    (ROOT / "Lawn/Fusion/generated/FusionSourceLevels272.inc").write_text("".join(generated), encoding="ascii")
    print(json.dumps({"adventure": len(adventure), "challenges": len(challenges), "baseUnlocks": len(unlocks),
        "adventureSpecialUnlocks": len(special_unlocks),
        "uniqueSourceZombieDependencies": len({z for r in adventure for z in r["allowedZombieKinds"]}), "runtimeVerified": False}))


if __name__ == "__main__":
    export()
