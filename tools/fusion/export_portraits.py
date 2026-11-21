"""Export verified static Fusion almanac portraits through the existing sprite codec.

First export: --bindings work/fusion/art-inventory04.manifest.json
Reproduce later: --bindings assets/fusion/almanac/portraits/bindings.json
The portable output does not require a Board or a live gameplay actor.
"""

import argparse
import hashlib
import json
import math
import pathlib
import re
import shutil
import struct
import sys
import tempfile

sys.dont_write_bytecode = True
import export_animation


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True, allow_nan=False) + "\n", encoding="utf-8", newline="\n")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def portable(value):
    path = pathlib.PurePosixPath(value)
    return not path.is_absolute() and not pathlib.PureWindowsPath(value).drive and "\\" not in value


def load_bindings(path):
    data = read_json(path)
    if data.get("schema") == 1 and "entries" in data:
        result = data
    else:
        entries = []
        for plant in data["plants"]:
            entries.append({"group": "plants", "id": plant["seedType"], "prefab": plant["preview"]["prefab"],
                "spriteRendererFileId": None, "unsupportedReason": None, "sourceBinding": plant["preview"]})
        for zombie in data["zombies"]:
            card = zombie["cardPortrait"]
            selected = card["selected"] if card else None
            reason = None if selected else "composite_source_card_not_converted" if card else "source_almanac_card_absent"
            entries.append({"group": "zombies", "id": zombie["theZombieType"],
                "prefab": card["sourcePrefab"] if selected else None,
                "spriteRendererFileId": selected["renderer"]["fileId"] if selected else None,
                "unsupportedReason": reason, "sourceBinding": {"runtime": zombie["runtime"],
                    "cardComponentId": card["cardComponentId"] if card else None,
                    "cardGameObjectId": card["cardGameObjectId"] if card else None,
                    "spriteGuid": selected["renderer"]["spriteGuid"] if selected else None}})
        result = {"schema": 1, "sourceBinarySha256": data["sourceBinarySha256"], "entries": entries}
    seen = set()
    for row in result["entries"]:
        key = row["group"], row["id"]
        if row["group"] not in ("plants", "zombies") or type(row["id"]) is not int or not 0 <= row["id"] < 65535 or key in seen:
            raise ValueError("invalid/duplicate source portrait identity: " + str(key))
        if row["prefab"] is not None and (not portable(row["prefab"]) or ".." in pathlib.PurePosixPath(row["prefab"]).parts):
            raise ValueError("portrait source prefab must stay inside original Assets")
        seen.add(key)
    result["entries"] = sorted(result["entries"], key=lambda row: (row["group"], row["id"]))
    return result


def guid_index(assets):
    # Read only metadata headers once. Reusing this lookup avoids hundreds of
    # recursive GUID searches in the individual source conversion calls.
    result = {}
    for meta in sorted(assets.rglob("*.meta")):
        with meta.open(encoding="utf-8-sig") as file:
            header = file.read(256)
        match = re.search(r"^guid: ([0-9a-f]{32})$", header, re.M)
        if match:
            if match[1] in result:
                raise ValueError("ambiguous original GUID: " + match[1])
            result[match[1]] = meta.with_suffix("")
    return result


def check_package(package, folder, assets, source_hashes):
    if package["schema"] != 1 or not package["static"] or package["clips"] or package["controller"]["states"] or len(package["sprites"]) != 1:
        raise ValueError("portrait must be exactly one static source sprite")
    if package["unusedSpriteBonesExplicitlyAllowed"] or package["nonRenderingComponents"] or package["deferredComponents"]:
        raise ValueError("portrait cannot silently retain ignored rendering/gameplay components")
    for source in package["sources"]:
        relative = source["path"]
        if not portable(relative) or ".." in pathlib.PurePosixPath(relative).parts:
            raise ValueError("invalid source provenance path")
        if relative not in source_hashes:
            source_hashes[relative] = digest(assets / relative)
        if source_hashes[relative] != source["sha256"]:
            raise ValueError("source provenance SHA mismatch: " + relative)
    bounds = package["visualBounds"]
    if not all(math.isfinite(v) for v in bounds.values()) or bounds["minX"] >= bounds["maxX"] or bounds["minY"] >= bounds["maxY"]:
        raise ValueError("invalid actual transformed portrait bounds")
    nodes = package["nodes"]
    if len({node["path"] for node in nodes}) != len(nodes):
        raise ValueError("duplicate portrait Transform path")
    for index, node in enumerate(nodes):
        current = index
        visited = set()
        while current >= 0:
            if current >= len(nodes) or current in visited or not nodes[current]["active"]:
                raise ValueError("invalid/inactive selected portrait ancestry")
            visited.add(current)
            current = nodes[current]["parent"]
        q = node["rotation"]
        if q["x"] or q["y"] or not all(math.isfinite(v) for field in ("position", "scale", "rotation") for v in node[field].values()):
            raise ValueError("unsupported/nonfinite visible portrait Transform")
    for item in package["spriteVariants"].values():
        if item["hasBoneMetadata"] or not portable(item["texture"]):
            raise ValueError("portrait has unsupported bones or a nonportable texture")
        texture = (folder / item["texture"]).resolve()
        original_png_hashes = {row["sha256"] for row in package["sources"] if row["path"].lower().endswith(".png")}
        if digest(texture) not in original_png_hashes:
            raise ValueError("portrait PNG bytes differ from original source provenance")
        with texture.open("rb") as file:
            header = file.read(24)
        if header[:8] != b"\x89PNG\r\n\x1a\n":
            raise ValueError("portrait texture is not PNG")
        width, height = struct.unpack(">II", header[16:24])
        rect = item["textureRect"]
        if rect["width"] <= 0 or rect["height"] <= 0 or rect["x"] < -0.001 or rect["yTop"] < -0.001 or rect["x"] + rect["width"] > width+0.001 or rect["yTop"] + rect["height"] > height+0.001:
            raise ValueError("portrait sprite rectangle is outside the actual PNG")
        mesh = item["mesh"]
        if not mesh["indices"] or len(mesh["indices"]) % 3 or any(type(i) is not int or i < 0 or i >= len(mesh["vertices"]) for i in mesh["indices"]):
            raise ValueError("invalid portrait source triangle indices")
        if any(not all(math.isfinite(vertex[k]) for k in ("x", "y", "z", "u", "v")) or not -0.001 <= vertex["u"] <= 1.001 or not -0.001 <= vertex["v"] <= 1.001 for vertex in mesh["vertices"]):
            raise ValueError("invalid portrait vertex/UV")
    if not package["sprites"][0]["enabled"]:
        raise ValueError("selected UI portrait renderer is disabled")


def export(source, bindings_path, output, scratch):
    assets = source / "assetripper/extracted/AssetRipper_export_20261112_215914/ExportedProject/Assets"
    bindings = load_bindings(bindings_path)
    if digest(source / "GameAssembly.dll") != bindings["sourceBinarySha256"]:
        raise ValueError("portrait bindings refer to another original GameAssembly")
    if output == assets or assets in output.parents:
        raise ValueError("refusing to write portrait outputs inside original source Assets")
    original_almanac = {"plants": ("LawnStrings.json", "plants", "seedType"), "zombies": ("ZombieStrings.json", "zombies", "theZombieType")}
    for group, (filename, field, id_field) in original_almanac.items():
        expected = {row[id_field] for row in read_json(assets / "Resources" / filename)[field]}
        actual = {row["id"] for row in bindings["entries"] if row["group"] == group}
        if actual != expected:
            raise ValueError("portrait bindings differ from the complete original " + group + " almanac ID set")
    guids = guid_index(assets)
    output.mkdir(parents=True, exist_ok=True)
    (output / "textures").mkdir(exist_ok=True)
    scratch.mkdir(parents=True, exist_ok=True)
    manifest = {"schema": 1, "staticOnly": True, "sourceBinarySha256": bindings["sourceBinarySha256"],
        "animationSchema": 1, "groups": {"plants": {}, "zombies": {}}, "textures": {},
        "validation": "Source conversion/structure only. Native UI rendering is verified separately."}
    source_hashes = {}
    with tempfile.TemporaryDirectory(prefix="portrait-export05-", dir=scratch) as temporary:
        stage = pathlib.Path(temporary).resolve()
        if scratch.resolve() not in stage.parents:
            raise ValueError("portrait scratch path escaped its owned directory")
        for number, binding in enumerate(bindings["entries"]):
            group, ident = binding["group"], binding["id"]
            record = {"id": ident, "status": "unsupported", "publicAccepted": False,
                "package": None, "packageSha256": None, "reason": binding["unsupportedReason"]}
            manifest["groups"][group][str(ident)] = record
            if binding["unsupportedReason"]:
                continue
            relative = pathlib.PurePosixPath(group) / str(ident)
            staged = stage / relative
            export_animation.export(assets, binding["prefab"], staged, guid_lookup=guids,
                sprite_renderer_file_id=binding["spriteRendererFileId"])
            package = read_json(staged / "animation.json")
            check_package(package, staged, assets, source_hashes)
            texture_paths = {}
            for image in sorted((staged / "textures").glob("*.png")):
                sha = digest(image)
                destination = output / "textures" / (sha + ".png")
                if destination.exists():
                    if digest(destination) != sha:
                        raise ValueError("existing shared portrait texture SHA mismatch")
                else:
                    shutil.copyfile(image, destination)
                shared = "../../textures/" + destination.name
                texture_paths["textures/" + image.name] = shared
                manifest["textures"]["textures/" + destination.name] = {"sha256": sha, "bytes": image.stat().st_size}
            for sprite in list(package["spriteVariants"].values()) + package["sprites"]:
                sprite["texture"] = texture_paths[sprite["texture"]]
            package_path = output / relative / "animation.json"
            write_json(package_path, package)
            check_package(package, package_path.parent, assets, source_hashes)
            record.update(status="ready", publicAccepted=True, package=(relative / "animation.json").as_posix(),
                packageSha256=digest(package_path), reason=None, visualBounds=package["visualBounds"],
                sourcePrefab=binding["prefab"], sourceSpriteRendererFileId=binding["spriteRendererFileId"])
            if number % 50 == 0:
                print("portrait export", number, group, ident, flush=True)
    manifest["counts"] = {group: {"total": len(records), "ready": sum(r["publicAccepted"] for r in records.values()),
        "unsupported": sum(not r["publicAccepted"] for r in records.values())} for group, records in manifest["groups"].items()}
    manifest["textureCount"] = len(manifest["textures"])
    manifest["textureBytes"] = sum(row["bytes"] for row in manifest["textures"].values())
    write_json(output / "bindings.json", bindings)
    write_json(output / "manifest.json", manifest)
    validate(output, assets)
    return manifest


def validate(output, assets):
    manifest = read_json(output / "manifest.json")
    bindings = load_bindings(output / "bindings.json")
    expected_files = {"manifest.json", "bindings.json"}
    source_hashes = {}
    for group, records in manifest["groups"].items():
        expected_ids = {str(row["id"]) for row in bindings["entries"] if row["group"] == group}
        if set(records) != expected_ids:
            raise ValueError("portrait manifest dropped or reassigned a source ID")
        for key, record in records.items():
            binding = next(row for row in bindings["entries"] if row["group"] == group and row["id"] == record["id"])
            if str(record["id"]) != key or not 0 <= record["id"] < 65535:
                raise ValueError("noncanonical portrait ID")
            if record["status"] != "ready":
                if record["publicAccepted"] or record["package"] is not None or not record["reason"]:
                    raise ValueError("unsupported portrait was exposed as accepted")
                continue
            relative = record["package"]
            if not record["publicAccepted"] or not portable(relative) or ".." in pathlib.PurePosixPath(relative).parts:
                raise ValueError("invalid ready portrait package")
            package_path = output / relative
            if digest(package_path) != record["packageSha256"]:
                raise ValueError("portrait package SHA mismatch")
            package = read_json(package_path)
            if package["sourcePrefab"] != binding["prefab"] or binding["unsupportedReason"] is not None:
                raise ValueError("portrait package source does not match its verified binding")
            if binding["spriteRendererFileId"] is not None:
                if package.get("uiPortraitExtraction", {}).get("spriteRendererFileId") != binding["spriteRendererFileId"] or package["sprites"][0]["guid"] != binding["sourceBinding"]["spriteGuid"]:
                    raise ValueError("zombie portrait selected the wrong original renderer/sprite")
            check_package(package, package_path.parent, assets, source_hashes)
            for item in package["spriteVariants"].values():
                resolved = (package_path.parent / item["texture"]).resolve()
                if output.resolve() not in resolved.parents:
                    raise ValueError("portrait texture reference escaped the approved package root")
            expected_files.add(relative)
    for relative, data in manifest["textures"].items():
        if not portable(relative) or ".." in pathlib.PurePosixPath(relative).parts or digest(output / relative) != data["sha256"]:
            raise ValueError("invalid shared portrait texture reference/SHA")
        expected_files.add(relative)
    actual_files = {path.relative_to(output).as_posix() for path in output.rglob("*") if path.is_file()}
    if actual_files != expected_files:
        raise ValueError("portrait root contains missing/unexpected files")
    if sum(len(records) for records in manifest["groups"].values()) != len(bindings["entries"]):
        raise ValueError("portrait manifest dropped source bindings")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=pathlib.Path)
    parser.add_argument("--bindings", type=pathlib.Path, default=pathlib.Path("work/fusion/art-inventory04.manifest.json"))
    parser.add_argument("--output", type=pathlib.Path, default=pathlib.Path("assets/fusion/almanac/portraits"))
    parser.add_argument("--scratch", type=pathlib.Path, default=pathlib.Path("work/fusion/portrait-export05.stage"))
    parser.add_argument("--validate-only", action="store_true")
    args = parser.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    if args.validate_only:
        validate(output, source / "assetripper/extracted/AssetRipper_export_20261112_215914/ExportedProject/Assets")
        print("portrait validation Passed")
    else:
        result = export(source, args.bindings.resolve(), output, args.scratch.resolve())
        print(json.dumps({"status": "Passed", "counts": result["counts"], "textures": result["textureCount"], "bytes": result["textureBytes"]}, sort_keys=True))
