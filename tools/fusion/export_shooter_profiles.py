"""Extract only audited shooter timing bindings; this does not enable gameplay."""

import argparse
import csv
import hashlib
import json
import math
import pathlib
import re
import struct
import subprocess
import sys

sys.dont_write_bytecode = True

import export_animation


ASSET_ROOT = "assetripper/extracted/AssetRipper_export_20261112_215914/ExportedProject/Assets"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def relative(root, value):
    path = pathlib.PurePosixPath(value)
    if path.is_absolute() or ".." in path.parts or "\\" in value or ":" in value:
        raise ValueError("nonportable source path: " + value)
    return root / path


def float32(value):
    return struct.unpack("<f", struct.pack("<f", value))[0]


def number(value, policy):
    if not math.isfinite(value):
        raise ValueError("nonfinite timing value")
    if policy == "decimal-double":
        return value
    if policy == "float32-promoted":
        return float32(value)
    raise ValueError("unknown numeric policy: " + str(policy))


def cpp_number(value, policy):
    number(value, policy)
    literal = repr(value)
    return "static_cast<double>(" + literal + "f)" if policy == "float32-promoted" else literal


def event_array_name(kind, state, used_names):
    # Keep every pre-alternate identifier unchanged. A kind can now have a
    # different event tuple in another source state, so its base name can clash.
    base = "shooterEvents" + str(kind)
    name = base
    if name in used_names:
        suffix = re.sub(r"[^A-Za-z0-9_]", "_", state) or "state"
        name = base + "_" + suffix
        index = 2
        while name in used_names:
            name = base + "_" + suffix + "_" + str(index)
            index += 1
    return name


def export(source, output, binding_path):
    repository = pathlib.Path(__file__).resolve().parents[2]
    bindings = load_json(binding_path)
    if bindings.get("schema") != 1 or digest(source / "GameAssembly.dll") != bindings["sourceBinarySha256"]:
        raise ValueError("shooter bindings require the audited GameAssembly.dll/schema")
    loader_path = relative(repository, bindings["loaderBindings"]["path"])
    if digest(loader_path) != bindings["loaderBindings"]["sha256"]:
        raise ValueError("loader binding SHA256 mismatch")
    loader = load_json(loader_path)
    if loader["sourceBinarySha256"] != bindings["sourceBinarySha256"]:
        raise ValueError("loader/shooter binary identity differs")
    assets = source / ASSET_ROOT
    pins = {item["path"]: item["sha256"] for item in bindings["sourceFiles"]}
    if len(pins) != len(bindings["sourceFiles"]):
        raise ValueError("duplicate source hash binding")
    used = set()

    def read(path):
        key = path.relative_to(assets).as_posix()
        if key not in pins or digest(path) != pins[key]:
            raise ValueError("source SHA256 mismatch or unbound dependency: " + key)
        used.add(key)
        return path.read_text(encoding="utf-8")

    guids = {}

    def resolve(guid):
        if guid not in guids:
            found = subprocess.run(["rg", "-l", "--glob", "*.meta", "^guid: " + guid + "$", str(assets)],
                                   stdin=subprocess.DEVNULL, capture_output=True, text=True, encoding="utf-8", check=False)
            paths = found.stdout.splitlines()
            if found.returncode != 0 or len(paths) != 1:
                raise ValueError("GUID must resolve to one source asset: " + guid)
            meta = pathlib.Path(paths[0])
            read(meta)
            guids[guid] = meta.with_suffix("")
        return guids[guid]

    csv_path = assets / "Resources/plant_data.txt"
    # This source CSV is GB18030, unlike the Unity YAML and MonoScript files.
    csv_key = csv_path.relative_to(assets).as_posix()
    if csv_key not in pins or digest(csv_path) != pins[csv_key]:
        raise ValueError("source SHA256 mismatch: " + csv_key)
    used.add(csv_key)
    rows = {}
    for line, row in enumerate(csv.reader(csv_path.read_text(encoding="gb18030").splitlines()), 1):
        if line == 1:
            continue
        if len(row) != 8 or int(row[0]) in rows:
            raise ValueError("malformed/duplicate plant CSV row: " + str(line))
        rows[int(row[0])] = (line, row)

    profiles, arrays, generated, seen = [], {}, [], set()
    for binding in bindings["profiles"]:
        kind = binding["kind"]
        state_name = binding.get("state")
        if type(kind) is not int or not 0 <= kind < 65535 or not isinstance(state_name, str) or not state_name:
            raise ValueError("invalid shooter kind/state")
        identity = (kind, state_name)
        if identity in seen:
            raise ValueError("duplicate shooter kind/state: " + str(identity))
        seen.add(identity)
        matches = [entry["runtime"] for entry in loader["entries"]
                   if entry["group"] == "PlantType" and entry["id"] == kind and "runtime" in entry]
        if len(matches) != 1:
            raise ValueError("missing/ambiguous source loader binding: " + str(kind))
        runtime = matches[0]
        prefab_path = relative(assets, runtime["prefab"])
        resource = pathlib.PurePosixPath(runtime["prefab"]).relative_to("Resources").with_suffix("").as_posix()
        if runtime["id"] != kind or runtime["arrayByteOffset"] != 0x20 + kind * 8 or resource.casefold() != runtime["resourcePath"].casefold():
            raise ValueError("inconsistent source loader identity: " + str(kind))
        docs = export_animation.documents(read(prefab_path))
        roots = [export_animation.reference(body, "m_GameObject") for tag, _, body in docs
                 if tag == 4 and export_animation.reference(body, "m_Father") == 0]
        if len(roots) != 1:
            raise ValueError("ambiguous prefab root")
        components = [body for tag, _, body in docs if tag == 114 and
                      export_animation.reference(body, "m_GameObject") == roots[0] and
                      export_animation.field(body, "thePlantType") is not None]
        if len(components) != 1:
            raise ValueError("ambiguous root entity source ID: " + str(kind))
        serialized_kind = int(export_animation.required(components[0], "thePlantType"))
        expected_kind = binding.get("expectedSerializedKind", kind)
        if type(expected_kind) is not int or not -1 <= expected_kind < 65535 or serialized_kind != expected_kind:
            raise ValueError("root entity source ID differs from loader slot: " + str(kind))
        override_evidence = None
        if expected_kind != kind:
            override_evidence = binding.get("runtimeKindOverrideEvidence")
            if not isinstance(override_evidence, dict) or override_evidence.get("method") != "CreatePlant.SetPlant" or \
                str(override_evidence.get("methodVA", "")).lower() != "0x18044c950" or \
                override_evidence.get("sourceBinarySha256") != bindings["sourceBinarySha256"]:
                raise ValueError("serialized kind override requires audited factory/binary evidence: " + str(kind))
        script_guid = re.search(r"guid: ([0-9a-f]{32})", export_animation.required(components[0], "m_Script"))[1]
        script_path = resolve(script_guid)
        script = read(script_path)
        multiple_callbacks = "callbacks" in binding
        callbacks = binding["callbacks"] if multiple_callbacks else [binding["callback"]]
        if not isinstance(callbacks, list) or not callbacks or any(not isinstance(name, str) or not name for name in callbacks) or \
            len(set(callbacks)) != len(callbacks) or multiple_callbacks and "callback" in binding:
            raise ValueError("invalid/ambiguous audited callback names: " + str(kind))
        class_chain, class_name, class_path, class_script = [], binding["scriptClass"], script_path, script
        callback_scripts = [class_script]
        while True:
            parent = re.search(r"\bclass\s+" + re.escape(class_name) + r"\s*:\s*([A-Za-z_]\w*)\b", class_script)
            if not parent or any(entry["class"] == class_name for entry in class_chain):
                raise ValueError("audited Shooter inheritance differs: " + str(kind))
            class_chain.append({"class": class_name, "base": parent[1], "script": class_path.relative_to(assets).as_posix()})
            if parent[1] == "Shooter":
                break
            class_name = parent[1]
            class_path = script_path.parent / (class_name + ".cs")
            read(class_path.with_suffix(".cs.meta"))
            class_script = read(class_path)
            callback_scripts.append(class_script)
        if any(not any(re.search(r"\b" + re.escape(name) + r"\s*\(", body) for body in callback_scripts)
               for name in callbacks):
            raise ValueError("audited Shooter class/callback differs: " + str(kind))
        animators = [body for tag, _, body in docs if tag == 95]
        if len(animators) != 1:
            raise ValueError("one original Animator required: " + str(kind))
        controller_guid = re.search(r"guid: ([0-9a-f]{32})", export_animation.required(animators[0], "m_Controller"))[1]
        controller_path = resolve(controller_guid)
        controller_docs = export_animation.documents(read(controller_path))
        states = [(ident, body) for tag, ident, body in controller_docs
                  if tag == 1102 and export_animation.required(body, "m_Name") == binding["state"]]
        if len(states) != 1:
            raise ValueError("missing/ambiguous audited Animator state: " + str(kind))
        state_id, state = states[0]
        clip_guid = re.search(r"guid: ([0-9a-f]{32})", export_animation.required(state, "m_Motion"))[1]
        clip_path = resolve(clip_guid)
        read(clip_path)
        clip = export_animation.read_clip(clip_path)
        speed = float32(float(export_animation.required(state, "m_Speed")))
        active_field = int(export_animation.required(state, "m_SpeedParameterActive"))
        if active_field not in (0, 1) or active_field and export_animation.field(state, "m_SpeedParameter") != "Speed":
            raise ValueError("unsupported Animator speed parameter: " + str(kind))
        if clip["loop"] or not clip["events"] or any(event["functionName"] not in callbacks or
            event["intParameter"] != 0 or event["floatParameter"] != 0 or event["stringParameter"] for event in clip["events"]):
            raise ValueError("unsupported loop/event callback/parameters: " + str(kind))
        if {event["functionName"] for event in clip["events"]} != set(callbacks):
            raise ValueError("missing audited clip callback: " + str(kind))
        event_policy, duration_policy = binding["precision"]["events"], binding["precision"]["duration"]
        if multiple_callbacks and event_policy != "float32-promoted":
            raise ValueError("multiple callbacks require source float32 timing: " + str(kind))
        source_events = [number(event["time"], event_policy) for event in clip["events"]]
        event_groups = []
        for index, time in enumerate(source_events):
            if multiple_callbacks and event_groups and time == event_groups[-1]["eventClipSeconds"]:
                event_groups[-1]["sourceEventIndices"].append(index)
            else:
                event_groups.append({"eventClipSeconds": time, "sourceEventIndices": [index]})
        if multiple_callbacks:
            for group in event_groups:
                order = [callbacks.index(clip["events"][index]["functionName"]) for index in group["sourceEventIndices"]]
                if order != sorted(set(order)):
                    raise ValueError("ambiguous same-time callback order: " + str(kind))
        events = [group["eventClipSeconds"] for group in event_groups]
        duration = number(clip["duration"], duration_policy)
        line, csv_row = rows[kind]
        interval = float32(float(csv_row[1]))
        if not math.isfinite(interval) or interval <= 0 or not math.isfinite(speed) or speed <= 0 or not math.isfinite(duration) or duration <= 0 or any(t <= 0 or t > duration for t in events) or events != sorted(events):
            raise ValueError("invalid extracted shooter timing: " + str(kind))
        payload = binding["projectileKind"]
        if type(payload) is not int or not 0 <= payload <= 65535:
            raise ValueError("invalid curated projectile kind")
        event_literals = tuple(cpp_number(clip["events"][group["sourceEventIndices"][0]]["time"], event_policy) for group in event_groups)
        if event_literals not in arrays:
            arrays[event_literals] = event_array_name(kind,state_name,set(arrays.values()))
        generated.append("    {" + ", ".join((str(kind), repr(interval) + "f", arrays[event_literals], repr(speed),
            cpp_number(clip["duration"], duration_policy), "true" if active_field else "false", hex(payload) if payload == 65535 else str(payload),
            json.dumps(binding["state"], ensure_ascii=True))) + "},\n")
        profiles.append({"kind": kind, "attackIntervalSeconds": interval, "eventClipSeconds": events,
            "stateSpeed": speed, "clipDurationSeconds": duration, "speedParameterActive": bool(active_field), "projectileKind": payload,
            "visualState": binding["state"],
            "precision": binding["precision"], "source": {"csvLine": line, "runtimeLoader": runtime,
                "serializedKind": serialized_kind, "expectedSerializedKind": expected_kind,
                "runtimeKindOverrideEvidence": override_evidence,
                "script": script_path.relative_to(assets).as_posix(), "controller": controller_path.relative_to(assets).as_posix(),
                "stateFileId": state_id, "state": binding["state"], "callback": binding.get("callback"),
                "callbacks": callbacks, "classChain": class_chain, "originalEvents": clip["events"], "eventGroups": event_groups,
                "clip": clip_path.relative_to(assets).as_posix(), "serializedEventTimes": [event["time"] for event in clip["events"]],
                "serializedClipDuration": clip["duration"]}})

    if used != pins.keys():
        raise ValueError("unused source hash bindings: " + str(sorted(pins.keys() - used)))
    cpp = "// Generated by tools/fusion/export_shooter_profiles.py; timing does not enable native aliases.\n"
    for literals, name in arrays.items():
        cpp += "constexpr double " + name + "[] = {" + ", ".join(literals) + "};\n"
    cpp += "constexpr FusionShooterTimingProfile profiles[] = {\n" + "".join(generated) + "};\n"
    manifest = {"schema": 1, "sourceVersion": bindings["sourceVersion"], "sourceBinarySha256": bindings["sourceBinarySha256"],
        "bindingsSha256": digest(binding_path), "loaderBindings": bindings["loaderBindings"],
        "generatorSha256": digest(pathlib.Path(__file__)), "animationParserSha256": digest(pathlib.Path(export_animation.__file__)),
        "assetRoot": ASSET_ROOT, "sources": [{"path": path, "sha256": pins[path]} for path in sorted(used)],
        "profiles": profiles, "cppSha256": hashlib.sha256(cpp.encode("ascii")).hexdigest(),
        "unsupported": [{"kind": item["kind"], "reason": item["referenceOnlyReason"]} for item in bindings["profiles"] if "referenceOnlyReason" in item],
        "limitations": ["Only curated bindings are exported; remaining Shooter families are not inferred.",
            "Payload kinds are source-audited bindings, not inferred from clip names or GetBulletType stubs.",
            "No native alias, eligibility, targeting, projectile behavior or gameplay controller is enabled by this output.",
            "Legacy decimal-double event policies are deliberate; changing them changes accepted tick quantization."]}
    output.mkdir(parents=True, exist_ok=True)
    (output / "FusionShooterProfiles.inc").write_text(cpp, encoding="ascii", newline="\n")
    (output / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8", newline="\n")
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--bindings", type=pathlib.Path, default=pathlib.Path(__file__).parent / "reference/shooter-profile-bindings-2.3.1.json")
    args = parser.parse_args()
    try:
        manifest = export(args.source.resolve(), args.output.resolve(), args.bindings.resolve())
    except (ValueError, OSError, KeyError, IndexError) as error:
        parser.error(str(error))
    print(json.dumps({"profiles": len(manifest["profiles"]), "referenceOnly": len(manifest["unsupported"]), "cppSha256": manifest["cppSha256"]}, sort_keys=True))
