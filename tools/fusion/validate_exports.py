"""Reproduce source exports and check identity, provenance, and rejection contracts."""

import argparse
import hashlib
import json
import pathlib
import re
import sys

sys.dont_write_bytecode = True

import export_animation
import export_catalog


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_catalog_asset_issues(manifest, tables, assets, bindings):
    expected_issues = {}
    verified = []

    def source_path(value):
        path = pathlib.PurePosixPath(value)
        assert not path.is_absolute() and ".." not in path.parts and "\\" not in value and ":" not in value, value
        return assets / path

    for group, rows in tables.items():
        export_catalog.unique_index(rows, "id", group + " catalog ID")
        for row in rows:
            key = group, row["id"]
            item = row["assets"]
            binding = bindings.get(key, {})
            roles = {role: binding[role] for role in ("runtime", "preview") if role in binding}
            assert item["loaderBinding"] == roles, "loader reference mismatch: " + str(key)
            paths = sorted({record["prefab"] for record in roles.values()})
            assert item["paths"] == paths, "non-loader asset paths: " + str(key)
            missing = [path for path in paths if not source_path(path).is_file()]
            runtime = roles.get("runtime")
            evidence = item["classEvidence"]
            class_name = item["className"]
            status = "sourceRuntimeBindingUnknown"
            if runtime and runtime["prefab"] not in missing:
                assert evidence and evidence["prefabSha256"] == digest(source_path(runtime["prefab"])), str(key)
                status = "sourceLoaderVerified" if class_name else "sourceLoaderVerifiedClassUnresolved"
                assert evidence["status"] == ("rootEntityScriptVerified" if class_name else "rootEntityScriptUnresolved"), str(key)
                if class_name:
                    verified.append((key, runtime["prefab"], class_name, evidence))
            else:
                assert class_name is None and evidence is None, "class without runtime binding: " + str(key)
                if roles.get("preview") and not missing:
                    status = "sourcePreviewOnly"
            assert item["mappingStatus"] == status, "incorrect mapping status: " + str(key)
            assert item["scriptGuidVerifiedPrefabs"] == ([runtime["prefab"]] if class_name else []), str(key)
            if row["id"] >= 0 and (not paths or missing or status != "sourceLoaderVerified"):
                expected_issues[key] = {"group": group, "id": row["id"], "enumName": row["enumName"],
                                        "mappingStatus": status, "missingAssets": missing}

    issues = {}
    for issue in manifest["issues"]:
        key = issue["group"], issue["id"]
        assert issue["id"] >= 0 and key not in issues, "duplicate/negative catalog issue: " + str(key)
        issues[key] = issue
    assert issues == expected_issues, "catalog issue correspondence differs: missing=" + str(sorted(expected_issues.keys()-issues.keys())) + \
        " extra=" + str(sorted(issues.keys()-expected_issues.keys())) + \
        " changed=" + str(sorted(key for key in issues.keys() & expected_issues.keys() if issues[key] != expected_issues[key]))

    for key, prefab, class_name, evidence in verified:
        candidates = evidence["rootEntityComponents"]
        assert len(candidates) == 1, "ambiguous runtime class: " + str(key)
        candidate = candidates[0]
        assert candidate["declaredClassName"] == class_name, "class provenance mismatch: " + str(key)
        script = source_path(candidate["script"])
        meta = script.with_suffix(script.suffix + ".meta")
        assert digest(script) == candidate["scriptSha256"] and digest(meta) == candidate["scriptMetaSha256"], str(key)
        assert script.stem == class_name and re.search(r"\bclass\s+" + re.escape(class_name) + r"\b", script.read_text(encoding="utf-8-sig")), str(key)
        assert re.search(r"^guid: " + re.escape(candidate["scriptGuid"]) + r"$", meta.read_text(encoding="utf-8-sig"), re.M), str(key)
        docs = {int(ident): (int(kind), body) for kind, ident, body in re.findall(
            r"^--- !u!(\d+) &(-?\d+)\s*\n(.*?)(?=^--- !u!|\Z)", source_path(prefab).read_text(encoding="utf-8-sig"), re.M | re.S)}
        root_id, component_id = candidate["rootGameObjectFileId"], candidate["componentFileId"]
        roots = [int(re.search(r"m_GameObject: \{fileID: (-?\d+)\}", body)[1]) for kind, body in docs.values()
                 if kind in (4, 224) and re.search(r"^  m_Father: \{fileID: 0\}", body, re.M)]
        assert roots == [root_id] and docs[root_id][0] == 1, "wrong prefab root: " + str(key)
        assert component_id in {int(value) for value in re.findall(r"component: \{fileID: (-?\d+)\}", docs[root_id][1])}, str(key)
        component_kind, body = docs[component_id]
        assert component_kind == 114 and re.search(r"m_GameObject: \{fileID: " + str(root_id) + r"\}", body), str(key)
        assert re.search(r"m_Script: \{fileID: -?\d+, guid: " + re.escape(candidate["scriptGuid"]) + r",", body), str(key)
        kind_field = "thePlantType" if key[0] == "PlantType" else "theZombieType"
        assert re.search(r"^  " + kind_field + r": " + str(candidate["serializedKind"]) + r"\s*$", body, re.M), str(key)


def run(source, repository):
    scratch = repository / "work/fusion/asset-validation"
    scratch.mkdir(parents=True, exist_ok=True)
    results = []

    def check(name, operation):
        try:
            operation()
            results.append({"name": name, "status": "Passed"})
        except Exception as error:
            results.append({"name": name, "status": "Failed", "error": str(error)})

    def equal_files(left, right, names):
        for name in names:
            if digest(left / name) != digest(right / name):
                raise AssertionError("different regenerated file: " + name)

    catalog_output = scratch / "catalog"
    catalog_output.mkdir(exist_ok=True)
    export_catalog.export(source, catalog_output)
    catalog = repository / "assets/fusion/catalog"
    check("catalog deterministic regeneration", lambda: equal_files(catalog_output, catalog,
        ["plants.json", "zombies.json", "bullets.json", "almanac.json", "modes.json", "manifest.json", "FusionCatalogData.inc"]))

    def counts_and_unknowns():
        manifest = export_catalog.load_json(catalog / "manifest.json")
        assert manifest["counts"] == {"plantEnumValues": 287, "plantStats": 283, "plantAlmanac": 281,
            "zombieEnumValues": 95, "zombieStats": 94, "zombieAlmanac": 88, "bulletEnumValues": 100}
        assert manifest["missingPlantStats"] == [-1, 257, 258, 800]
        plants = export_catalog.load_json(catalog / "plants.json")
        assert max(row["id"] for row in plants) == 1192
        assert all(row["chooserEligibility"] is None and row["unlockRule"] is None for row in plants)
        assert all(not row["runtimeImplemented"] for row in plants)
    check("sparse source IDs and explicit unknown support", counts_and_unknowns)

    assets = source / "assetripper/extracted/AssetRipper_export_20261112_215914/ExportedProject/Assets"
    manifest = export_catalog.load_json(catalog / "manifest.json")
    tables = {group: export_catalog.load_json(catalog / (name + ".json"))
              for group, name in (("PlantType", "plants"), ("ZombieType", "zombies"), ("BulletType", "bullets"))}
    binding_reference = export_catalog.load_json(repository / "tools/fusion/reference/loader-bindings-2.3.1.json")
    bindings = {(entry["group"], entry["id"]): entry for entry in binding_reference["entries"]}
    check("catalog issues exactly cover unknown/missing bindings and verified root-script provenance",
          lambda: validate_catalog_asset_issues(manifest, tables, assets, bindings))

    def issue_and_provenance_rejections():
        def rejected(changed_manifest, changed_tables):
            try:
                validate_catalog_asset_issues(changed_manifest, changed_tables, assets, bindings)
            except AssertionError:
                return
            raise AssertionError("inconsistent catalog evidence accepted")
        if manifest["issues"]:
            rejected(dict(manifest, issues=manifest["issues"][1:]), tables)
            rejected(dict(manifest, issues=manifest["issues"] + manifest["issues"][:1]), tables)
        known = next(row for row in tables["PlantType"] if row["id"] >= 0 and row["assets"]["mappingStatus"] == "sourceLoaderVerified")
        spurious = {"group": "PlantType", "id": known["id"], "enumName": known["enumName"],
                    "mappingStatus": "sourceLoaderVerified", "missingAssets": []}
        rejected(dict(manifest, issues=manifest["issues"] + [spurious]), tables)
        changed = json.loads(json.dumps(tables))
        row = next(row for row in changed["PlantType"] if row["assets"]["mappingStatus"] == "sourceLoaderVerified")
        row["assets"]["classEvidence"]["rootEntityComponents"][0]["declaredClassName"] += "Invalid"
        rejected(manifest, changed)
    check("omitted/duplicate/spurious issues and altered runtime class provenance rejected", issue_and_provenance_rejections)

    def zombie_store_proofs():
        table = export_catalog.load_json(repository / "tools/fusion/reference/zombie-stats-2.3.1.json")
        assert digest(source / "GameAssembly.dll") == table["source_sha256"]
        assert len(table["entries"]) == 94
        for row in table["entries"]:
            decoded = {}
            for store in row["packed_native_stores"]:
                index, offset = divmod(store["array_byte_offset"]-0x20, 16)
                assert index == row["id"] and offset in (0, 8)
                packed = int(store["u64_hex"], 0)
                decoded[offset] = (packed & 0xffffffff, packed >> 32)
            assert decoded[0] == (row["attack_damage"], row["max_health"])
            assert decoded[8] == (row["first_armor_max_health"], row["second_armor_max_health"])
    check("94 zombie packed-store proofs and binary identity", zombie_store_proofs)

    profiles = {
        "peashooter": "Resources/plants/peashooter/PeashooterPrefab.prefab",
        "sunflower": "Resources/plants/sunflower/SunflowerPrefab.prefab",
        "peasunflower": "Resources/plants/_mixer/peasunflower/PeaSunFlowerPrefab.prefab",
        "cherryshooter": "Resources/plants/_mixer/cherryshooter/CherryshooterPrefab.prefab",
        "doubleshooter": "Resources/plants/doublepea/DoubleShooterPrefab.prefab",
        "doublecherry": "Resources/plants/_mixer/doublecherry/DoubleCherryPrefab.prefab",
        "nutshooter": "Resources/plants/_mixer/nutshooter/NutShooterPrefab.prefab",
        "sunnut": "Resources/plants/_mixer/sunnut/SunNutPrefab.prefab",
        "twinflower": "Resources/plants/twinflower/TwinFlowerPrefab.prefab",
        "gatlingpea": "Resources/plants/gatlingpea/GatlingPeaPrefab.prefab",
        "cherrygatling": "Resources/plants/_mixer/cherrygatlingpea/CherryGatlingPrefab.prefab",
        "peapuff": "Resources/plants/_mixer/peasmallpuff/PeaSmallPuffPrefab.prefab",
        "doublepuff": "Resources/plants/_mixer/doublepuff/DoublePuffPrefab.prefab",
        "gatlingpuff": "Resources/plants/_mixer/doublepuff/GatlingPuffPrefab.prefab",
        "zombie": "Resources/zombies/_prefab/Zombie.prefab",
        "projectile-pea": "Resources/bullet/prefabs/ProjectilePea.prefab",
        "projectile-sun": "Resources/bullet/prefabs/BulletSun.prefab",
        "projectile-cherry": "Resources/bullet/prefabs/CherryBullet.prefab",
        "projectile-nut": "Resources/bullet/prefabs/NutBullet.prefab",
        "projectile-puffpea": "Resources/bullet/prefabs/PuffPea.prefab",
        "sun": "Resources/items/sun/Sun.prefab",
        "little-sun": "Resources/items/sun/BulletSun.prefab",
        "splitpea": "Resources/plants/peashooter/splitpea/SplitPeaPrefab.prefab",
        "cherrysplit": "Resources/plants/peashooter/splitpea/cherrysplit/CherrySplitPrefab.prefab",
        "splitpuff": "Resources/plants/smallpuff/splitpuff/SplitPuffPrefab.prefab",
        "puffnut": "Resources/plants/_mixer/puffnut/PuffNutPrefab.prefab",
        "snowpeashooter": "Resources/plants/showpeashooter/SnowPeashooterPrefab.prefab",
        "doublesnow": "Resources/plants/showpeashooter/doublesnow/DoubleSnowPrefab.prefab",
        "snowgatling": "Resources/plants/showpeashooter/snowgatling/SnowGatlingPrefab.prefab",
        "icepuff": "Resources/plants/_mixer/icepuff/IcePuffPrefab.prefab",
        "snowgatlingpuff": "Resources/plants/_mixer/doublepuff/snowgatlingpuff/SnowGatlingPuffPrefab.prefab",
        "projectile-snowpea": "Resources/bullet/prefabs/ProjectileSnowPea.prefab",
        "projectile-icicle-small": "Resources/bullet/prefabs/ProjectileIcicleSmall.prefab",
        "portrait-peashooter": "Resources/plants/peashooter/PeaShooterPreview.prefab",
        "portrait-sunflower": "Resources/plants/sunflower/SunflowerPreview.prefab",
        "portrait-peasunflower": "Resources/plants/_mixer/peasunflower/PeaSunFlowerPreview.prefab",
        "portrait-cherryshooter": "Resources/plants/_mixer/cherryshooter/CherryShooterPreview.prefab",
        "portrait-doubleshooter": "Resources/plants/doublepea/DoubleShooterPreview.prefab",
        "portrait-doublecherry": "Resources/plants/_mixer/doublecherry/DoubleCherryPreview.prefab",
        "portrait-nutshooter": "Resources/plants/_mixer/nutshooter/NutShooterPreview.prefab",
        "portrait-sunnut": "Resources/plants/_mixer/sunnut/SunNutPreview.prefab",
        "portrait-twinflower": "Resources/plants/twinflower/TwinFlowerPreview.prefab",
        "portrait-gatlingpea": "Resources/plants/gatlingpea/GatlingPeaPreview.prefab",
        "portrait-cherrygatling": "Resources/plants/_mixer/cherrygatlingpea/CherryGatlingPreview.prefab",
        "portrait-peapuff": "Resources/plants/_mixer/peasmallpuff/PeaSmallPuffPreview.prefab",
        "portrait-doublepuff": "Resources/plants/_mixer/doublepuff/DoublePuffPreview.prefab",
        "portrait-gatlingpuff": "Resources/plants/_mixer/doublepuff/GatlingPuffPreview.prefab",
        "portrait-splitpea": "Resources/plants/peashooter/splitpea/SplitPeaPreview.prefab",
        "portrait-cherrysplit": "Resources/plants/peashooter/splitpea/cherrysplit/CherrySplitPreview.prefab",
        "portrait-splitpuff": "Resources/plants/smallpuff/splitpuff/SplitPuffPreview.prefab",
        "portrait-puffnut": "Resources/plants/_mixer/puffnut/PuffNutPreview.prefab",
        "portrait-snowpeashooter": "Resources/plants/showpeashooter/SnowPeaShooterPreview.prefab",
        "portrait-doublesnow": "Resources/plants/showpeashooter/doublesnow/DoubleSnowPreview.prefab",
        "portrait-snowgatling": "Resources/plants/showpeashooter/snowgatling/SnowGatlingPreview.prefab",
        "portrait-icepuff": "Resources/plants/_mixer/icepuff/IcePuffPreview.prefab",
        "portrait-snowgatlingpuff": "Resources/plants/_mixer/doublepuff/snowgatlingpuff/SnowGatlingPuffPreview.prefab",
        "cherrynut": "Resources/plants/_mixer/cherrynut/CherryNutPrefab.prefab",
        "portrait-cherrynut": "Resources/plants/_mixer/cherrynut/CherryNutPreview.prefab",
        "potatonut": "Resources/plants/_mixer/potatonut/PotatoNutPrefab.prefab",
        "portrait-potatonut": "Resources/plants/_mixer/potatonut/PotatoNutPreview.prefab",
        "cherrymine": "Resources/plants/potatomine/cherrymine/CherryMinePrefab.prefab",
        "portrait-cherrymine": "Resources/plants/potatomine/cherrymine/CherryMinePreview.prefab",
        "nutchomper": "Resources/plants/_mixer/nutchomper/NutChomperPrefab.prefab",
        "portrait-nutchomper": "Resources/plants/_mixer/nutchomper/NutChomperPreview.prefab",
        "sunchomper": "Resources/plants/_mixer/sunchomper/SunChomperPrefab.prefab",
        "portrait-sunchomper": "Resources/plants/_mixer/sunchomper/SunChomperPreview.prefab",
        "cherrychomper": "Resources/plants/_mixer/cherrychomper/CherryChomperPrefab.prefab",
        "portrait-cherrychomper": "Resources/plants/_mixer/cherrychomper/CherryChomperPreview.prefab",
        "cabbage": "Resources/plants/cabbagepult/CabbagepultPrefab.prefab",
        "portrait-cabbage": "Resources/plants/cabbagepult/CabbagepultPreview.prefab",
        "melonpult": "Resources/plants/melonpult/MelonpultPrefab.prefab",
        "portrait-melonpult": "Resources/plants/melonpult/MelonpultPreview.prefab",
        "wintermelon": "Resources/plants/melonpult/wintermelon/WinterMelonPrefab.prefab",
        "portrait-wintermelon": "Resources/plants/melonpult/wintermelon/WinterMelonPreview.prefab",
        "cornpult": "Resources/plants/cornpult/CornpultPrefab.prefab",
        "portrait-cornpult": "Resources/plants/cornpult/CornpultPreview.prefab",
        "corncabbage": "Resources/plants/cabbagepult/corncabbage/CornCabbagePrefab.prefab",
        "portrait-corncabbage": "Resources/plants/cabbagepult/corncabbage/CornCabbagePreview.prefab",
        "cornmelon": "Resources/plants/melonpult/cornmelon/CornMelonPrefab.prefab",
        "portrait-cornmelon": "Resources/plants/melonpult/cornmelon/CornMelonPreview.prefab",
        "suncabbage": "Resources/plants/cabbagepult/suncabbage/SunCabbagePrefab.prefab",
        "portrait-suncabbage": "Resources/plants/cabbagepult/suncabbage/SunCabbagePreview.prefab",
        "cabbagemelon": "Resources/plants/melonpult/cabbagemelon/CabbageMelonPrefab.prefab",
        "portrait-cabbagemelon": "Resources/plants/melonpult/cabbagemelon/CabbageMelonPreview.prefab",
        "projectile-suncabbage": "Resources/bullet/prefabs/SunCabbageBullet.prefab",
        "projectile-kernel": "Resources/bullet/prefabs/ProjectileKernal.prefab",
        "projectile-butter": "Resources/bullet/prefabs/ProjectileButter.prefab",
        "projectile-bigkernel": "Resources/bullet/prefabs/BigKernalBullet.prefab",
        "projectile-bigbutter": "Resources/bullet/prefabs/BigButterBullet.prefab",
        "projectile-cornmelon": "Resources/bullet/prefabs/MelonBullet_kernal.prefab",
        "projectile-buttermelon": "Resources/bullet/prefabs/MelonBullet_butter.prefab",
        "projectile-cabbagemelon": "Resources/bullet/prefabs/MelonBullet_cabbage.prefab",
        "background1": "Resources/background/background1.prefab"}
    for name, prefab in profiles.items():
        original = repository / "assets/fusion/pilot" / name
        generated = scratch / name
        extra = ["Resources/zombies/image/reanim/Zombie_outerarm_upper2.asset"] if name == "zombie" else []
        if name == "nutchomper":
            extra = ["Resources/plants/_mixer/nutchomper/cracked" + str(stage) + "_" + part + ".asset"
                     for stage in (1, 2) for part in ("back", "head")]
        export_animation.export(assets, prefab, generated, name == "background1", "bg" if name == "background1" else None, name == "background1", extra)
        def identity(original=original, generated=generated):
            equal_files(original, generated, [path.relative_to(generated).as_posix() for path in generated.rglob("*") if path.is_file()])
            package = export_catalog.load_json(generated / "animation.json")
            assert len({node["path"] for node in package["nodes"]}) == len(package["nodes"])
            source_png_hashes = {item["sha256"] for item in package["sources"] if item["path"].endswith(".png")}
            for texture in (generated / "textures").glob("*.png"):
                assert digest(texture) in source_png_hashes
            for state in package["controller"]["states"]:
                assert isinstance(state["id"], int)
            for layer in package["controller"]["layers"]:
                assert layer["defaultState"] in {state["id"] for state in package["controller"]["states"]}
            for variant in package["spriteVariants"].values():
                mesh = variant["mesh"]
                assert len(mesh["indices"]) % 3 == 0
                assert all(0 <= index < len(mesh["vertices"]) for index in mesh["indices"])
        check(name + " deterministic package, exact atlas bytes, controller IDs", identity)

    def nullable_blink_renderers():
        package = export_catalog.load_json(catalog.parent / "pilot/cherryshooter/animation.json")
        empty = [sprite for sprite in package["sprites"] if sprite["guid"] is None]
        assert len(empty) == 2
        for sprite in empty:
            node = package["nodes"][sprite["node"]]
            path = node["path"]
            assert not node["active"] and "mesh" not in sprite
            curves = [curve for clip in package["clips"].values() for curve in clip["curves"]
                      if curve["path"] == path and curve.get("attribute") == "m_IsActive"]
            assert curves and {key["value"] for curve in curves for key in curve["keys"]} == {0.0, 1.0}
    check("initially empty renderers preserve active curves without invented sprites", nullable_blink_renderers)

    def unresolved_nonzero_sprite():
        prefab = assets / profiles["cherryshooter"]
        mutated = scratch / "unresolved-nonzero-sprite.prefab"
        original = prefab.read_text(encoding="utf-8")
        assert "m_Sprite: {fileID: 0}" in original
        mutated.write_text(original.replace("m_Sprite: {fileID: 0}", "m_Sprite: {fileID: 21300000}", 1), encoding="utf-8")
        try:
            export_animation.export(assets, str(mutated.resolve()), scratch / "unresolved-nonzero-output")
        except ValueError as error:
            assert "unresolved external sprite GUID" in str(error)
        else:
            raise AssertionError("nonzero unresolved SpriteRenderer reference accepted")
    check("nonzero unresolved sprite references remain rejected", unresolved_nonzero_sprite)

    def unsupported_weighted_curve():
        source_clip = assets / "Resources/plants/peashooter/shoot.anim"
        mutated = scratch / "unsupported-weighted.anim"
        mutated.write_text(source_clip.read_text(encoding="utf-8").replace("weightedMode: 0", "weightedMode: 1", 1), encoding="utf-8")
        try:
            export_animation.read_clip(mutated)
        except ValueError as error:
            assert "weighted" in str(error)
        else:
            raise AssertionError("unsupported weighted curve was accepted")
    check("unsupported weighted curve rejected", unsupported_weighted_curve)
    report = {"passed": sum(item["status"] == "Passed" for item in results),
              "failed": sum(item["status"] == "Failed" for item in results), "checks": results,
              "limits": "Source export checks only; no native rendering/source-runtime comparison or cooperative gameplay acceptance."}
    (scratch / "summary.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return report["failed"]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=pathlib.Path, required=True)
    args = parser.parse_args()
    sys.exit(1 if run(args.source.resolve(), pathlib.Path(__file__).resolve().parents[2]) else 0)
