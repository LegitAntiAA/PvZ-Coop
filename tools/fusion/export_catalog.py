"""Export original Fusion 2.3.1 data without treating catalog entries as playable.

Usage: python tools/fusion/export_catalog.py --source C:/PvZ/fusion/mod
No Unity, game execution, network access, or third-party Python packages required.
"""

import argparse
import csv
import hashlib
import json
import pathlib
import re


def load_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")


def unique_index(rows, field, label):
    result = {}
    for row in rows:
        key = row[field]
        if key in result:
            raise ValueError(f"duplicate {label}: {key}")
        result[key] = row
    return result


def cpp_string(value):
    # ASCII source avoids dependence on MSVC's current source-code page.
    # Escape decimal digits too: MSVC warns when any digit follows an octal
    # escape, even when its three digits are already complete (C4125).
    return '"' + "".join(f"\\{b:03o}" if b >= 128 else
                         "\\" + chr(b) if chr(b) in '\\"' else
                         f"\\{b:03o}" if b < 32 or ord("0") <= b <= ord("9") else chr(b)
                         for b in value.encode("utf-8")) + '"'


def cpp_float(value):
    value = format(value, ".9g")
    return value + ("f" if "." in value or "e" in value else ".0f")


def almanac_index(rows, field, enum_rows, expected_count, label):
    if not isinstance(rows, list) or len(rows) != expected_count:
        raise ValueError(f"{label}: expected {expected_count} source records")
    known_ids = {row["value"] for row in enum_rows if row["value"] >= 0}
    for row in rows:
        if not isinstance(row, dict) or type(row.get(field)) is not int:
            raise ValueError(f"{label}: expected an integer source ID")
        item_id = row[field]
        if item_id < 0 or item_id >= 65535 or item_id not in known_ids:
            raise ValueError(f"{label}: unknown source ID {item_id}")
        for name in ("name", "info", "introduce"):
            if not isinstance(row.get(name), str):
                raise ValueError(f"{label} {item_id}: expected string field {name}")
        if not row["name"] or not isinstance(row.get("cost", ""), str):
            raise ValueError(f"{label} {item_id}: invalid name/cost")
    return unique_index(rows, field, label + " ID")


def almanac_fields_sha256(row):
    fields = {key: row.get(key, "") for key in ("name", "info", "introduce", "cost")}
    encoded = json.dumps(fields, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def export(source, output, bindings_path=None):
    analysis = source / "analysis"
    export_root = source / "assetripper/extracted/AssetRipper_export_20261112_215914/ExportedProject/Assets"
    resources = export_root / "Resources"
    enum_path = analysis / "game_enums.json"
    enums = load_json(enum_path)
    almanac_plants = load_json(resources / "LawnStrings.json")["plants"]
    almanac_zombies = load_json(resources / "ZombieStrings.json")["zombies"]
    plant_text = almanac_index(almanac_plants, "seedType", enums["PlantType"], 281, "plant almanac")
    zombie_text = almanac_index(almanac_zombies, "theZombieType", enums["ZombieType"], 88, "zombie almanac")
    zombie_reference_path = pathlib.Path(__file__).parent / "reference/zombie-stats-2.3.1.json"
    zombie_reference = load_json(zombie_reference_path)
    binary_sha256 = sha256(source / "GameAssembly.dll")
    if binary_sha256 != zombie_reference["source_sha256"]:
        raise ValueError("zombie base stats were verified against a different GameAssembly.dll")
    if bindings_path is None:
        bindings_path = pathlib.Path(__file__).parent / "reference/loader-bindings-2.3.1.json"
    binding_reference = load_json(bindings_path)
    if binding_reference.get("schema") != 1 or binding_reference.get("sourceBinarySha256") != binary_sha256:
        raise ValueError("loader bindings were recovered from a different source/schema")
    bindings = {}
    for entry in binding_reference["entries"]:
        key = entry["group"], entry["id"]
        if key[0] not in ("PlantType", "ZombieType") or type(key[1]) is not int or key[1] < 0 or key in bindings:
            raise ValueError("invalid/duplicate source loader identity: " + str(key))
        for role in ("runtime", "preview"):
            record = entry.get(role)
            if record is None:
                continue
            path = pathlib.PurePosixPath(record["prefab"])
            if path.is_absolute() or ".." in path.parts or "\\" in record["prefab"] or ":" in record["prefab"]:
                raise ValueError("nonportable loader prefab: " + str(path))
            resource = path.relative_to("Resources").with_suffix("").as_posix()
            if record["id"] != key[1] or record["arrayByteOffset"] != 0x20 + key[1]*8 or resource.casefold() != record["resourcePath"].casefold():
                raise ValueError("inconsistent source loader slot/path: " + str(key))
            for field in ("functionVA", "loadVA", "storeVA", "stringReferenceVA"):
                if not re.fullmatch(r"0x[0-9a-fA-F]+", record[field]):
                    raise ValueError("missing source instruction evidence: " + str(key))
        bindings[key] = entry
    zombie_stats = unique_index(zombie_reference["entries"], "id", "zombie stats ID")
    stats = {}
    rows = csv.reader((resources / "plant_data.txt").read_text(encoding="gb18030").splitlines())
    next(rows)
    for line, row in enumerate(rows, 2):
        if len(row) != 8:
            raise ValueError(f"plant_data.txt:{line}: expected eight columns")
        plant_id = int(row[0])
        if plant_id in stats:
            raise ValueError(f"duplicate plant stat ID {plant_id}")
        item = dict(zip(("attackIntervalSeconds", "productionIntervalSeconds", "damage",
                         "maxHealth", "rechargeSeconds", "sunCost"),
                        (float(row[1]), float(row[2]), int(row[3]), int(row[4]), float(row[5]), int(row[6]))))
        if any(item[k] < 0 for k in ("attackIntervalSeconds", "productionIntervalSeconds", "damage", "rechargeSeconds")) or item["maxHealth"] <= 0:
            raise ValueError(f"invalid stats for plant {plant_id}")
        item["nameZh"] = row[7]
        stats[plant_id] = item

    script_guids = {}
    for path in sorted((export_root / "Scripts/Assembly-CSharp").glob("*.cs.meta")):
        match = re.search(r"^guid: ([0-9a-f]{32})$", path.read_text(encoding="utf-8"), re.M)
        if match:
            if match.group(1) in script_guids:
                raise ValueError("duplicate source MonoScript GUID")
            script_guids[match.group(1)] = path
    prefab_cache = {}
    issues = []

    def root_entity_class(prefab, group):
        key = prefab, group
        if key in prefab_cache:
            return prefab_cache[key]
        path = export_root / prefab
        text = path.read_text(encoding="utf-8-sig")
        docs = {int(ident): (int(kind), body) for kind, ident, body in re.findall(
            r"^--- !u!(\d+) &(-?\d+)\s*\n(.*?)(?=^--- !u!|\Z)", text, re.M | re.S)}
        roots = [int(re.search(r"m_GameObject: \{fileID: (-?\d+)\}", body)[1])
                 for kind, body in docs.values() if kind in (4, 224) and re.search(r"^  m_Father: \{fileID: 0\}", body, re.M)]
        candidates = []
        if len(roots) == 1 and roots[0] in docs and docs[roots[0]][0] == 1:
            components = {int(value) for value in re.findall(r"component: \{fileID: (-?\d+)\}", docs[roots[0]][1])}
            kind_field = "thePlantType" if group == "PlantType" else "theZombieType"
            for ident, (kind, body) in docs.items():
                if kind != 114 or ident not in components or not re.search(r"m_GameObject: \{fileID: " + str(roots[0]) + r"\}", body):
                    continue
                serialized_kind = re.search(r"^  " + kind_field + r": (-?\d+)\s*$", body, re.M)
                if not serialized_kind:
                    continue
                guid = re.search(r"m_Script: \{fileID: -?\d+, guid: ([0-9a-f]{32}),", body)
                script = script_guids.get(guid[1]) if guid else None
                script_source = script.with_suffix("") if script else None
                class_name = script_source.stem if script_source else None
                if script_source and (not script_source.is_file() or not re.search(
                        r"\bclass\s+" + re.escape(class_name) + r"\b", script_source.read_text(encoding="utf-8-sig"))):
                    class_name = None
                candidates.append({"componentFileId": ident, "rootGameObjectFileId": roots[0],
                    "serializedKind": int(serialized_kind[1]), "scriptGuid": guid[1] if guid else None,
                    "script": script_source.relative_to(export_root).as_posix() if script_source else None,
                    "declaredClassName": class_name,
                    "scriptSha256": sha256(script_source) if script_source and script_source.is_file() else None,
                    "scriptMetaSha256": sha256(script) if script else None})
        resolved = len(candidates) == 1 and candidates[0]["declaredClassName"] is not None
        result = (candidates[0]["declaredClassName"] if resolved else None,
                  {"prefabSha256": sha256(path), "rootEntityComponents": candidates,
                   "status": "rootEntityScriptVerified" if resolved else "rootEntityScriptUnresolved"})
        prefab_cache[key] = result
        return result

    def assets_for(group, item_id, name):
        binding = bindings.get((group, item_id), {})
        paths = sorted({record["prefab"] for role in ("runtime", "preview") if (record := binding.get(role))})
        missing = [path for path in paths if not (export_root / path).is_file()]
        runtime = binding.get("runtime")
        class_name, class_evidence = (None, None)
        status = "sourceRuntimeBindingUnknown"
        if runtime and runtime["prefab"] not in missing:
            class_name, class_evidence = root_entity_class(runtime["prefab"], group)
            status = "sourceLoaderVerified" if class_name else "sourceLoaderVerifiedClassUnresolved"
        elif binding.get("preview") and not missing:
            status = "sourcePreviewOnly"
        if missing or (item_id >= 0 and status != "sourceLoaderVerified"):
            issues.append({"group": group, "id": item_id, "enumName": name,
                           "mappingStatus": status, "missingAssets": missing})
        return {"className": class_name, "mappingStatus": status, "paths": paths,
                "scriptGuidVerifiedPrefabs": [runtime["prefab"]] if class_name else [],
                "animationConverted": False,
                "loaderBinding": {role: binding[role] for role in ("runtime", "preview") if role in binding},
                "classEvidence": class_evidence}

    catalog = {}
    for group, filename, text_by_id in (("PlantType", "plants", plant_text), ("ZombieType", "zombies", zombie_text), ("BulletType", "bullets", {})):
        unique_index(enums[group], "value", group + " ID")
        unique_index(enums[group], "name", group + " name")
        entries = []
        for value in sorted(enums[group], key=lambda entry: entry["value"]):
            item_id = value["value"]
            if item_id < -1 or item_id >= 65535:
                raise ValueError(f"out-of-range {group} ID {item_id}")
            name = value["name"]
            item = {"id": item_id, "enumName": name, "almanac": text_by_id.get(item_id),
                    "assets": assets_for(group, item_id, name), "runtimeImplemented": False}
            if group == "PlantType":
                item["stats"] = stats.get(item_id)
                item["chooserEligibility"] = None
                item["unlockRule"] = None
            elif group == "ZombieType":
                item["stats"] = zombie_stats.get(item_id)
            entries.append(item)
        catalog[filename] = entries
        write_json(output / (filename + ".json"), entries)
    known = {item["id"] for item in catalog["plants"]}
    if set(stats) - known or set(plant_text) - known:
        raise ValueError("plant stats/almanac reference an unknown PlantType")
    known_zombies = {item["id"] for item in catalog["zombies"]}
    if set(zombie_text) - known_zombies:
        raise ValueError("zombie almanac references an unknown ZombieType")
    if set(zombie_stats) != known_zombies - {-1}:
        raise ValueError("zombie stat table does not match the named source enum")
    write_json(output / "almanac.json", {"plants": almanac_plants, "zombies": almanac_zombies})
    write_json(output / "modes.json", {key: enums[key] for key in ("LevelType", "SceneType", "ChallengeLevel", "ExploreLevel", "TravelAdvanture")})
    source_files = [enum_path,
                    resources / "plant_data.txt", resources / "LawnStrings.json", resources / "ZombieStrings.json", source / "GameAssembly.dll"]
    try:
        binding_label = bindings_path.resolve().relative_to(pathlib.Path(__file__).resolve().parents[2]).as_posix()
    except ValueError:
        binding_label = bindings_path.name
    manifest = {"schema": 1, "sourceVersion": "Fusion 2.3.1", "assetPathRoot": "ExportedProject/Assets",
                "sources": [{"path": path.relative_to(source).as_posix(), "sha256": sha256(path)} for path in source_files],
                "counts": {"plantEnumValues": len(catalog["plants"]), "plantStats": len(stats), "plantAlmanac": len(plant_text),
                           "zombieEnumValues": len(catalog["zombies"]), "zombieStats": len(zombie_stats), "zombieAlmanac": len(zombie_text), "bulletEnumValues": len(catalog["bullets"])},
                "recoveredTables": [{"path": "tools/fusion/reference/zombie-stats-2.3.1.json", "sha256": sha256(zombie_reference_path),
                                     "originalBinarySha256": zombie_reference["source_sha256"], "sourceFunctions": zombie_reference["source_functions"]},
                                    {"path": binding_label, "sha256": sha256(bindings_path),
                                     "originalBinarySha256": binding_reference["sourceBinarySha256"], "proof": binding_reference["proof"]}],
                "missingPlantStats": [item["id"] for item in catalog["plants"] if item["stats"] is None],
                "issues": issues,
                "limitations": ["Catalog data is not an implemented native gameplay system.",
                                "Plant InitPlant overrides execute after CSV base stats; per-ability overrides remain authoritative.",
                                "Card eligibility, unlock rules and generated animation availability are intentionally unknown.",
                                "Runtime prefab identity follows original loader result stores; a MonoScript GUID alone does not identify a source ID.",
                                "Bullet loader bindings and unassigned plant runtime slots remain unknown rather than inferred from names/classes.",
                                "Asset paths contain exact loaded runtime/preview prefabs, not all prefabs sharing a class or guessed related art.",
                                "Paths identify source assets; bulk source textures are not bundled by this exporter."]}
    write_json(output / "manifest.json", manifest)
    native = ["// Generated by tools/fusion/export_catalog.py. Original IDs; base CSV stats only.\n", "static const FusionPlantDefinition kFusionPlants[] = {\n"]
    for item in catalog["plants"]:
        if item["id"] < 0:
            continue
        st = item["stats"] or {key: 0 for key in ("attackIntervalSeconds", "productionIntervalSeconds", "damage", "maxHealth", "rechargeSeconds", "sunCost")}
        name = (item["almanac"] or {}).get("name") or st.get("nameZh") or item["enumName"]
        values = [str(item["id"]), cpp_string(item["enumName"]), cpp_string(name), cpp_float(st["attackIntervalSeconds"]),
                  cpp_float(st["productionIntervalSeconds"]), str(st["damage"]), str(st["maxHealth"]), cpp_float(st["rechargeSeconds"]),
                  str(st["sunCost"]), "true" if item["stats"] else "false", "true" if item["almanac"] else "false"]
        native.append("    {" + ", ".join(values) + "},\n")
    native.append("};\n")
    for key, array_name in (("zombies", "kFusionZombies"), ("bullets", "kFusionBullets")):
        native.append("static const FusionEntityName " + array_name + "[] = {\n")
        for item in catalog[key]:
            if item["id"] >= 0:
                name = (item["almanac"] or {}).get("name") or item["enumName"]
                native.append("    {" + str(item["id"]) + ", " + cpp_string(item["enumName"]) + ", " + cpp_string(name) + "},\n")
        native.append("};\n")
    native.append("static const FusionZombieStats kFusionZombieStats[] = {\n")
    for item in sorted(zombie_stats.values(), key=lambda row: row["id"]):
        native.append("    {" + ", ".join(str(item[key]) for key in ("id", "attack_damage", "max_health", "first_armor_max_health", "second_armor_max_health")) + "},\n")
    native.append("};\n")
    for rows, id_field, array_name in ((almanac_plants, "seedType", "kFusionPlantAlmanac"),
                                       (almanac_zombies, "theZombieType", "kFusionZombieAlmanac")):
        native.append("static const FusionAlmanacEntry " + array_name + "[] = {\n")
        for row in sorted(rows, key=lambda entry: entry[id_field]):
            fields = [str(row[id_field])] + [cpp_string(row.get(key, ""))
                for key in ("name", "info", "introduce", "cost")]
            fields.append(cpp_string(almanac_fields_sha256(row)))
            native.append("    {" + ", ".join(fields) + "},\n")
        native.append("};\n")
    (output / "FusionCatalogData.inc").write_text("".join(native), encoding="ascii", newline="\n")
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[2] / "assets/fusion/catalog")
    parser.add_argument("--bindings", type=pathlib.Path, default=None, help="audited source-ID loader reference (default: bundled Fusion2.3.1 reference)")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    result = export(args.source.resolve(), args.output.resolve(), args.bindings.resolve() if args.bindings else None)
    print(json.dumps({"counts": result["counts"], "missingPlantStats": result["missingPlantStats"], "assetIssues": len(result["issues"])}, sort_keys=True))
