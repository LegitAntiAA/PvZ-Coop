"""Export every almanac plant with the existing strict source-model converter."""

import argparse
import hashlib
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True

import export_animation


def export_batch(assets, catalog, output, kinds=None):
    plants = [row for row in json.loads(catalog.read_text(encoding="utf-8"))
              if row["almanac"] is not None]
    ids = [row["id"] for row in plants]
    if len(ids) != len(set(ids)):
        raise ValueError("duplicate plant IDs in the source catalog")
    if kinds is not None:
        requested = set(kinds)
        if not requested.issubset(ids):
            raise ValueError("requested plant IDs are absent from the almanac catalog")
        plants = [row for row in plants if row["id"] in requested]
    output.mkdir(parents=True, exist_ok=True)
    records, hashes = [], {}

    def digest(path):
        if path not in hashes:
            hashes[path] = hashlib.sha256(path.read_bytes()).hexdigest()
        return hashes[path]

    def save():
        report = {
            "plantCount": len(plants),
            "exporterSha256": digest(Path(export_animation.__file__)),
            "passed": sum(row["status"] == "Passed" for row in records),
            "failed": sum(row["status"] == "Failed" for row in records),
            "packages": records,
            "limits": "Source exports only. Gameplay, native rendering and particle parity require separate acceptance.",
        }
        temporary = output / "summary.json.tmp"
        temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        temporary.replace(output / "summary.json")
        return report

    for plant in sorted(plants, key=lambda row: row["id"]):
        for role in ("runtime", "preview"):
            record = {"kind": plant["id"], "role": role}
            try:
                prefab = plant["assets"]["loaderBinding"][role]["prefab"]
                destination = output / str(plant["id"]) / role
                result = export_animation.export(assets, prefab, destination)
                package = json.loads((destination / "animation.json").read_text(encoding="utf-8"))
                if package["ignoredBindings"] or package["omittedInactiveNodes"] or package["unusedSpriteBonesExplicitlyAllowed"]:
                    raise ValueError("export omitted source data or bypassed the strict bone policy")
                texture_hashes = set()
                for source in package["sources"]:
                    actual = digest(assets / source["path"])
                    if actual != source["sha256"]:
                        raise ValueError("source hash mismatch: " + source["path"])
                    if Path(source["path"]).suffix.lower() == ".png":
                        texture_hashes.add(actual)
                for texture in (destination / "textures").iterdir():
                    if digest(texture) not in texture_hashes:
                        raise ValueError("copied texture differs from the source: " + texture.name)
                if role == "runtime" and not package.get("anchors", {}).get("shadow", {}).get("relativeToRuntimeRoot"):
                    raise ValueError("runtime model lacks its native ground anchor")
                record.update(status="Passed", prefab=prefab,
                              nodes=result["nodes"], sprites=result["sprites"], textures=result["textures"],
                              deferredComponents=package["deferredComponents"])
            except (ValueError, OSError, KeyError, AssertionError) as error:
                record.update(status="Failed", error=str(error))
            records.append(record)
            if len(records) % 20 == 0:
                progress = save()
                print(json.dumps({key: progress[key] for key in ("plantCount", "passed", "failed")}), flush=True)
    report = save()
    print(json.dumps({key: report[key] for key in ("plantCount", "passed", "failed")}), flush=True)
    return int(report["failed"] != 0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--kinds", type=int, nargs="+")
    args = parser.parse_args()
    sys.exit(export_batch(args.assets.resolve(), args.catalog.resolve(), args.output.resolve(), args.kinds))
