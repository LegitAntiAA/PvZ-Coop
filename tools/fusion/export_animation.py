"""Export an inspectable Unity SpriteRenderer/AnimationClip pilot, without rasterizing motion.

Only the source constructs explicitly checked here are accepted. Sprite textures
remain unchanged; coordinates, pivots, original key times and slopes are retained.
"""

import argparse
import hashlib
import json
import math
import pathlib
import re
import shutil
import struct
import subprocess
import zlib


def scalar(text):
    text = text.strip()
    if text.startswith("{"):
        return {key: scalar(value) for key, value in re.findall(r"(\w+): ([^,}]+)", text)}
    if text in ("Infinity", "-Infinity"):
        return text
    return float(text)


def field(text, name, default=None):
    match = re.search(r"^ +" + re.escape(name) + r":([^\n]*)$", text, re.M)
    return match.group(1).strip() if match else default


def required(text, name):
    value = field(text, name)
    if value is None:
        raise ValueError("missing Unity field " + name)
    return value


def reference(text, name):
    # Unity local identifiers routinely exceed the 53-bit exact float range.
    return int(re.search(r"fileID: (-?\d+)", required(text, name))[1])


def vector(text, name):
    return scalar(required(text, name))


def documents(text):
    result = []
    for match in re.finditer(r"^--- !u!(\d+) &(-?\d+)\n(.*?)(?=^--- !u!|\Z)", text, re.M | re.S):
        result.append((int(match[1]), int(match[2]), match[3]))
    return result


def section(text, name):
    match = re.search(r"^  " + re.escape(name) + r":([^\n]*)(.*?)(?=^  m_|\Z)", text, re.M | re.S)
    if not match:
        return ""
    return "" if match[1].strip() == "[]" else match[2]


def read_clip(path, source_skin=False):
    text = path.read_text(encoding="utf-8")
    if section(text, "m_RotationCurves") or section(text, "m_CompressedRotationCurves"):
        raise ValueError(str(path) + ": quaternion/compressed curves are not supported by this pilot")
    curves = []
    for field_name, kind in (("m_EulerCurves", "euler"), ("m_PositionCurves", "position"), ("m_ScaleCurves", "scale"), ("m_FloatCurves", "float")):
        curve_section = section(text, field_name)
        for block in re.split(r"\n  - (?:serializedVersion: 2\n    )?curve:\n", curve_section)[1:]:
            curve_match = re.search(r"m_Curve:(.*?)(?=\n +m_PreInfinity:)", block, re.S)
            if not curve_match:
                raise ValueError(str(path) + ": cannot read curve keys")
            keys = []
            for key_block in re.split(r"\n\s+- serializedVersion: 3\n", curve_match[1])[1:]:
                key = {name: scalar(required(key_block, name)) for name in ("time", "value", "inSlope", "outSlope", "weightedMode", "inWeight", "outWeight")}
                if key["weightedMode"] != 0:
                    raise ValueError(str(path) + ": weighted curves require an additional evaluator")
                keys.append(key)
            if not keys or any(a["time"] >= b["time"] for a, b in zip(keys, keys[1:])):
                raise ValueError(str(path) + ": empty or unordered curve")
            curve = {"kind": kind, "path": required(block, "path"), "keys": keys,
                     "preInfinity": int(required(block, "m_PreInfinity")), "postInfinity": int(required(block, "m_PostInfinity"))}
            if kind == "float":
                curve["attribute"] = required(block, "attribute")
                curve["classID"] = int(required(block, "classID"))
                supported = {(1, "m_IsActive"), (212, "m_Color.r"), (212, "m_Color.g"), (212, "m_Color.b"),
                             (212, "m_Color.a"), (212, "m_Enabled"), (212, "m_SortingOrder")}
                if source_skin and curve["classID"]==114 and curve["attribute"]=="m_AutoRebind":
                    script=required(block,"script")
                    if reference(block,"script")!=-972417856 or "f68e4d9210748575b7313c5ba1460876" not in script or any(key["value"]!=0 for key in keys):
                        raise ValueError("source SpriteSkin auto-rebind requires implementation")
                    curve["kind"]="skinAutoRebind";curve["scriptFileId"]=-972417856;curve["scriptGuid"]="f68e4d9210748575b7313c5ba1460876";supported.add((114,"m_AutoRebind"))
                if (curve["classID"], curve["attribute"]) not in supported:
                    raise ValueError(str(path) + ": unsupported float binding " + str(curve["classID"]) + "/" + curve["attribute"])
                if curve["classID"] == 212:
                    curve["scriptFileId"] = reference(block, "script")
                    if curve["scriptFileId"] != 0 or any(not math.isfinite(key["value"]) for key in keys):
                        raise ValueError(str(path) + ": invalid SpriteRenderer float curve")
                    if curve["attribute"] == "m_SortingOrder" and any(
                            key["value"] != int(key["value"]) or not -32768 <= key["value"] <= 32767 for key in keys):
                        raise ValueError(str(path) + ": invalid SpriteRenderer sorting order")
                    if curve["attribute"] == "m_Enabled":
                        if any(key["value"] not in (0, 1) for key in keys):
                            raise ValueError(str(path) + ": nonbinary SpriteRenderer enabled curve")
                        for a, b in zip(keys, keys[1:]):
                            if a["value"] != b["value"] and math.isfinite(float(a["outSlope"])) and math.isfinite(float(b["inSlope"])):
                                raise ValueError(str(path) + ": interpolated SpriteRenderer enabled curve is unsupported")
            curves.append(curve)
    for block in re.split(r"\n  - serializedVersion: 2\n", section(text, "m_PPtrCurves"))[1:]:
        if required(block, "attribute") != "m_Sprite" or int(required(block, "classID")) != 212:
            raise ValueError(str(path) + ": unsupported object binding")
        keys = []
        for time, value in re.findall(r"- time: ([^\n]+)\n +value: ([^\n]+)", block):
            guid = re.search(r"guid: ([0-9a-f]{32})", value)
            keys.append({"time": float(time), "spriteGuid": guid[1] if guid else None})
        if not keys:
            raise ValueError(str(path) + ": empty object curve")
        curves.append({"kind": "sprite", "path": required(block, "path"), "keys": keys})
    events = []
    for event in re.split(r"\n  - time:", section(text, "m_Events"))[1:]:
        time, body = event.split("\n", 1)
        events.append({"time": float(time), "functionName": required(body, "functionName"),
                       "intParameter": int(required(body, "intParameter")), "floatParameter": float(required(body, "floatParameter")),
                       "stringParameter": field(body, "data", "")})
    if float(required(text, "m_StartTime")) != 0:
        raise ValueError(str(path) + ": nonzero clip start requires a separate evaluator")
    return {"name": required(text, "m_Name"), "duration": float(required(text, "m_StopTime")),
            "additiveReferencePoseTime": float(required(text, "m_AdditiveReferencePoseTime")),
            "hasAdditiveReferencePose": bool(int(required(text, "m_HasAdditiveReferencePose"))),
            "loop": bool(int(required(text, "m_LoopTime"))), "sourceSampleRate": float(required(text, "m_SampleRate")),
            "curves": curves, "events": events}


def source_unbound_bindings(assets, prefab, docs, clips, clip_sources, node_paths,
                            sprite_variants, resolve_guid, read, sprite_masks_supported=False, particle_binding_proof=None):
    """Prove absent Transform targets without resolving guesses or losing keys."""
    prefab_text = read(prefab)
    headers = re.findall(r"^--- !u!(\d+) &(-?\d+)([^\n]*)$", prefab_text, re.M)
    if len(headers) != len(docs) or any(tail.strip() for _, _, tail in headers):
        return [], None  # Stripped/inherited documents are not a complete hierarchy.
    by_id = {ident: (kind, text) for kind, ident, text in docs}
    particle_proof = None
    particles = {(kind, ident): text for kind, ident, text in docs if kind in (198, 199)}
    if particles and particle_binding_proof is not None:
        manifest_path = pathlib.Path(particle_binding_proof['manifest'])
        manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
        zombie_proof = 'sourceZombieKind' in particle_binding_proof
        key_name = 'sourceZombieKind' if zombie_proof else 'modelKey'
        key = particle_binding_proof[key_name]
        family = [row for row in manifest['zombies' if zombie_proof else 'projectiles'] if row['kind' if zombie_proof else 'modelKey'] == key]
        if len(family) != 1 or family[0]['status'] != 'Passed':
            raise ValueError('particle hierarchy proof family is not ready')
        covered, resources = set(), []
        for row in manifest['bindings']:
            if row[key_name] != key:
                continue
            if row['status'] != 'Passed' or row['sourcePrefab'] != prefab.relative_to(assets).as_posix():
                raise ValueError('particle hierarchy proof prefab differs')
            for kind, name, hash_name in [(198, 'sourceParticleFileId', 'serializedParticleSha256'), (199, 'sourceRendererFileId', 'serializedRendererSha256')]:
                identity = (kind, row[name])
                if identity not in particles or identity in covered or hashlib.sha256(particles[identity].encode()).hexdigest() != row[hash_name]:
                    raise ValueError('particle hierarchy proof component differs')
                covered.add(identity)
            records = [r for r in manifest['resources'] if r['resourceKind'] == row['resourceKind']]
            if len(records) != 1 or records[0]['status'] != 'Passed':
                raise ValueError('missing decoded native particle resource')
            resource = records[0];relative = pathlib.Path(resource['package'])
            if relative.is_absolute() or '..' in relative.parts:
                raise ValueError('unsafe native particle resource proof path')
            path = manifest_path.parent / relative
            raw = path.read_bytes();used_resource = json.loads(raw)
            if hashlib.sha256(raw).hexdigest() != resource['effectSha256'] or used_resource['schema'] != 2 or used_resource['sourcePrefab'] != row['sourcePrefab'] or used_resource['sourcePrefabSha256'] != hashlib.sha256(prefab.read_bytes()).hexdigest() or used_resource['ignoredBindings'] or used_resource['deferredComponents']:
                raise ValueError('decoded native particle resource proof differs')
            resources.append({'resourceKind':row['resourceKind'], 'sha256':resource['effectSha256']})
        if covered != set(particles) or len(resources) != family[0]['count']:
            raise ValueError('particle hierarchy proof must cover every source component')
        audit_path = pathlib.Path(particle_binding_proof['hierarchyAudit'])
        audit = json.loads(audit_path.read_text(encoding='utf-8'))
        evidence_path = pathlib.Path(audit['baseAwakeEvidence'])
        if audit['sourcePrefabSha256'] != hashlib.sha256(prefab.read_bytes()).hexdigest() or (not audit['leafDoesNotOverrideAwake']and 'leafAwakeEvidence'not in audit)or audit['createsAnimatorRootChildren'] or hashlib.sha256(evidence_path.read_bytes()).hexdigest() != audit['baseAwakeEvidenceSha256']:
            raise ValueError('source Awake hierarchy audit differs')
        leaf_script = assets / audit['leafScript']
        leaf_text = read(leaf_script)
        if hashlib.sha256(leaf_script.read_bytes()).hexdigest() != audit['leafScriptSha256']:
            raise ValueError('particle leaf overrides its audited Awake')
        source_scripts=[{'path':audit['leafScript'],'sha256':audit['leafScriptSha256'],**({'awakeEvidence':audit['leafAwakeEvidence']}if 'leafAwakeEvidence'in audit else{})}]+audit.get('inheritedScripts', [])
        for inherited in source_scripts:
            script = assets / inherited['path']; body = read(script)
            if hashlib.sha256(script.read_bytes()).hexdigest() != inherited['sha256']:
                raise ValueError('inherited source script overrides its audited Awake')
            if re.search(r'\bAwake\s*\(', body):
                proof = inherited.get('awakeEvidence')
                if proof is None:raise ValueError('inherited source Awake needs exact code proof')
                proof_path=pathlib.Path(proof['path']); proof_raw=proof_path.read_bytes()
                if hashlib.sha256(proof_raw).hexdigest()!=proof['sha256']:raise ValueError('inherited Awake evidence hash differs')
                proof_json=json.loads(proof_raw)
                if 'content'in proof_json:proof_json=json.loads(next(x['text']for x in proof_json['content']if x['type']=='text'))
                matches=[x for x in proof_json['functions'].values()if x['name']==proof['method']]
                if len(matches)!=1 or hashlib.sha256(matches[0]['decompiled_code'].encode()).hexdigest()!=proof['codeSha256'] or any(call in matches[0]['decompiled_code']for call in ['Instantiate','AddComponent','SetParent','set_parent','GameObject___ctor']):
                    raise ValueError('inherited Awake can change source hierarchy')
                code=matches[0]['decompiled_code']; virtuals=proof.get('emptyVirtualOverrides',[])
                if code.count('(**(code')!=len(virtuals):raise ValueError('inherited Awake has unproved virtual dispatch')
                for override in virtuals:
                    owner=assets/override['script']; owner_text=read(owner)
                    if hashlib.sha256(owner.read_bytes()).hexdigest()!=override['scriptSha256'] or not re.search(r'\boverride\s+void\s+'+re.escape(override['method'])+r'\s*\(\s*\)',owner_text):
                        raise ValueError('empty virtual override source binding differs')
                    if override['qualifiedMethod']!=owner.stem+'$$'+override['method']:raise ValueError('override metadata owner differs from source class')
                    if not any(s['path']==override['script']for s in source_scripts):raise ValueError('empty override is outside audited inheritance chain')
                    metadata_path=pathlib.Path(override['metadata']); metadata_raw=metadata_path.read_bytes()
                    if hashlib.sha256(metadata_raw).hexdigest()!=override['metadataSha256']:raise ValueError('override metadata proof differs')
                    methods=json.loads(metadata_raw)['ScriptMethod']; target=[m for m in methods if m['Name']==override['qualifiedMethod']]
                    if len(target)!=1 or target[0]['Address']+0x180000000!=int(override['address'],16):raise ValueError('override address binding differs')
                    empty_path=pathlib.Path(override['evidence']); empty_raw=empty_path.read_bytes()
                    if hashlib.sha256(empty_raw).hexdigest()!=override['evidenceSha256']:raise ValueError('empty override primary hash differs')
                    empty=json.loads(empty_raw)
                    if 'content'in empty:empty=json.loads(next(x['text']for x in empty['content']if x['type']=='text'))
                    actual=empty['functions'].get(override['address']); body_code=actual['decompiled_code']if actual else''
                    if hashlib.sha256(body_code.encode()).hexdigest()!=override['codeSha256']or not re.fullmatch(r'[^{}]*\{\s*return;\s*\}\s*',body_code):raise ValueError('override is not a proven empty body')
                    offset=override['vtableOffset'];slot=override['slot'];dump_path=pathlib.Path(override['dump']);dump_raw=dump_path.read_bytes()
                    if offset!=0x138+slot*16 or code.count('+ '+hex(offset)+')')!=1 or hashlib.sha256(dump_raw).hexdigest()!=override['dumpSha256']:raise ValueError('virtual slot proof differs')
                    base=dump_raw.decode('utf-8-sig');start=base.index('public class '+script.stem+' :');end=base.find('// Namespace:',start+20);base=base[start:end if end>=0 else None]
                    if not re.search(r'Slot: '+str(slot)+r'\s*\n\s*protected virtual void '+re.escape(override['method'])+r'\(\)',base):raise ValueError('source virtual slot declaration differs')
        evidence = json.loads(evidence_path.read_text(encoding='utf-8'))
        if 'content' in evidence:
            evidence = json.loads(next(item['text'] for item in evidence['content'] if item['type'] == 'text'))
        methods = [value for value in evidence['functions'].values() if value['name'] == audit['baseAwakeName']]
        if len(methods) != 1 or hashlib.sha256(methods[0]['decompiled_code'].encode()).hexdigest() != audit['baseAwakeCodeSha256'] or any(call in methods[0]['decompiled_code'] for call in ['Instantiate', 'AddComponent', 'SetParent', 'set_parent', 'GameObject___ctor']):
            raise ValueError('base Awake can change the Animator-root hierarchy')
        particle_proof = {'manifestSha256':hashlib.sha256(manifest_path.read_bytes()).hexdigest(), 'resources':resources, 'hierarchyAudit':audit}
    blocked = {1001} | (set() if sprite_masks_supported else {331}) | (set() if particle_proof else {198, 199})
    if len(by_id) != len(docs) or any(kind in blocked for kind, _, _ in docs):
        return [], None
    if any(sprite["hasBoneMetadata"] for sprite in sprite_variants.values()):
        return [], None
    if re.search(r"^ +(?:m_RootBone|m_BoneTransforms|m_Bones):", prefab_text, re.M):
        return [], None

    def yaml_string(value):
        if value.startswith("'"):
            if not value.endswith("'"):
                raise ValueError("unterminated Unity YAML string")
            return value[1:-1].replace("''", "'")
        return json.loads(value) if value.startswith('"') else value

    objects = {ident: text for kind, ident, text in docs if kind == 1}
    transforms = {ident: text for kind, ident, text in docs if kind == 4}
    if len(transforms) != len(objects):
        return [], None
    roots = [ident for ident, text in transforms.items() if reference(text, "m_Father") == 0]
    animators = [text for kind, _, text in docs if kind == 95]
    if len(roots) != 1 or len(animators) != 1:
        return [], None
    root = roots[0]
    if reference(animators[0], "m_GameObject") != reference(transforms[root], "m_GameObject"):
        return [], None
    scripts = []
    for kind, ident, text in docs:
        for name in ("m_CorrespondingSourceObject", "m_PrefabInstance", "m_PrefabAsset"):
            if field(text, name) is not None and reference(text, name) != 0:
                return [], None
        if kind == 1:
            components = [int(value) for value in re.findall(r"component: \{fileID: (-?\d+)\}", section(text, "m_Component"))]
            if not components or len(components) != len(set(components)):
                return [], None
            if any(component not in by_id or reference(by_id[component][1], "m_GameObject") != ident for component in components):
                return [], None
        else:
            owner = reference(text, "m_GameObject")
            if owner not in objects or not re.search(r"component: \{fileID: " + str(ident) + r"\}", section(objects[owner], "m_Component")):
                return [], None
        if kind == 4:
            parent = reference(text, "m_Father")
            children = [int(value) for value in re.findall(r"fileID: (-?\d+)", section(text, "m_Children"))]
            actual = [child for child, body in transforms.items() if reference(body, "m_Father") == ident]
            if (parent and parent not in transforms) or sorted(children) != sorted(actual):
                return [], None
        if kind == 114:
            script = resolve_guid(re.search(r"guid: ([0-9a-f]{32})", required(text, "m_Script"))[1])
            if "SpriteSkin" in script.stem:
                return [], None
            read(script)
            scripts.append({"class": script.stem, "source": script.relative_to(assets).as_posix(),
                            "sha256": hashlib.sha256(script.read_bytes()).hexdigest()})

    def hierarchy_path(ident, ancestors=()):
        if ident in ancestors:
            raise ValueError("cyclic Transform hierarchy")
        body = transforms[ident]
        parent = reference(body, "m_Father")
        name = yaml_string(required(objects[reference(body, "m_GameObject")], "m_Name"))
        if "/" in name:
            raise ValueError("ambiguous hierarchy path")
        return "" if not parent else "/".join(filter(None, (hierarchy_path(parent, ancestors + (ident,)), name)))

    paths = [hierarchy_path(ident) for ident in transforms]
    hashes = [zlib.crc32(path.encode("utf-8")) for path in paths]
    if len(set(paths)) != len(paths) or len(set(hashes)) != len(hashes):
        return [], None
    hierarchy = sorted(zip(paths, hashes))
    hierarchy_sha = hashlib.sha256(json.dumps(hierarchy, ensure_ascii=False, separators=(",", ":")).encode("utf-8")).hexdigest()
    prefab_sha = hashlib.sha256(prefab.read_bytes()).hexdigest()
    candidates, rejected, meanings = {}, set(), {}
    for clip_name, clip in clips.items():
        source = clip_sources[clip_name]
        clip_text = read(source)
        bindings = []
        for block in re.split(r"\n    - serializedVersion: 2\n", section(clip_text, "m_ClipBindingConstant"))[1:]:
            bindings.append((int(required(block, "path")), int(required(block, "attribute")),
                             int(required(block, "typeID")), reference(block, "script"),
                             int(required(block, "isPPtrCurve")), int(required(block, "isIntCurve")),
                             int(required(block, "isSerializeReferenceCurve")), int(required(block, "customType"))))
        for index, curve in enumerate(clip["curves"]):
            if curve["path"] in node_paths:
                continue
            path = curve["path"]
            raw = curve["sourceBindingPath"]
            semantic = yaml_string(raw)
            placeholder = re.fullmatch(r"path_0x([0-9A-Fa-f]{8})_[A-Za-z0-9]+", semantic)
            path_hash = int(placeholder[1], 16) if placeholder else zlib.crc32(semantic.encode("utf-8"))
            attribute = {"position": 1, "euler": 4, "scale": 3}.get(curve["kind"])
            expected = (path_hash, attribute, 4, 0, 0, 0, 0, 4 if attribute == 4 else 0)
            if curve["kind"] == "sprite":
                attribute = 0
                expected = (path_hash, 0, 212, 0, 1, 0, 0, 23)
            elif curve["kind"] == "float" and curve.get("classID") == 1 and curve.get("attribute") == "m_IsActive":
                attribute = zlib.crc32(b"m_IsActive")
                expected = (path_hash, attribute, 1, 0, 0, 0, 0, 0)
            elif curve['kind']=='float'and curve.get('classID')==212 and curve.get('attribute')=='m_SortingOrder'and curve.get('scriptFileId')==0:
                attribute=zlib.crc32(b'm_SortingOrder')
                expected=(path_hash,attribute,212,0,0,0,0,26)
            meanings.setdefault(path_hash, set()).add(raw)
            if attribute is None or path_hash in hashes or bindings.count(expected) != 1:
                rejected.add(path)
                continue
            candidates.setdefault(path, []).append({"path": path, "sourceBindingPath": raw,
                "rawPathHash": f"0x{path_hash:08X}", "clip": clip_name,
                "curveIndex": index, "curveKind": curve["kind"],
                "sourceClip": source.relative_to(assets).as_posix(), "sourceClipSha256": hashlib.sha256(source.read_bytes()).hexdigest()})
    records = [record for path, items in sorted(candidates.items()) if path not in rejected
               for record in items if len(meanings[int(record["rawPathHash"], 16)]) == 1]
    proof = {"method": "complete Animator-root hierarchy; no relative-path CRC32 target",
             "pathHash": "CRC32/UTF-8 of semantic Unity YAML path", "sourcePrefabSha256": prefab_sha,
             "documentCount": len(docs), "transformCount": len(transforms), "animatorRootFileId": root,
             "hierarchy": [{"path": path, "hash": f"0x{value:08X}"} for path, value in hierarchy],
             "hierarchySha256": hierarchy_sha, "scripts": scripts}
    if particle_proof is not None:
        proof['decodedNativeParticleProof'] = particle_proof
    return records, proof if records else None


def export(assets, prefab_path, output, active_only=False, sprite_prefix=None, allow_unused_sprite_bones=False, extra_sprites=(),
           guid_lookup=None, sprite_renderer_file_id=None, source_features=(), particle_binding_proof=None,
           portrait_game_object_file_id=None, source_hierarchy_proof=None):
    features=set(source_features)
    if features-{"spriteMaskV1","spriteSkinV1"}:raise ValueError("unsupported source renderer feature")
    masks_enabled="spriteMaskV1" in features
    skins_enabled="spriteSkinV1" in features
    prefab = assets / prefab_path
    folder = prefab.parent
    used = set()

    def read(path, binary=False):
        used.add(path)
        return path.read_bytes() if binary else path.read_text(encoding="utf-8")

    guids = dict(guid_lookup) if guid_lookup is not None else {}
    if guid_lookup is None:
        for meta in sorted(folder.rglob("*.meta")):
            match = re.search(r"^guid: ([0-9a-f]{32})$", meta.read_text(encoding="utf-8"), re.M)
            if match:
                guids[match[1]] = meta.with_suffix("")

    def resolve_guid(guid):
        if guid not in guids:
            if shutil.which("rg"):
                found = subprocess.run(["rg", "-l", "--glob", "*.meta", "^guid: " + guid + "$", str(assets)], stdin=subprocess.DEVNULL, capture_output=True, text=True, encoding="utf-8", check=False)
                matches = [pathlib.Path(path).with_suffix("") for path in found.stdout.splitlines()]
            else:
                matches = [meta.with_suffix("") for meta in assets.rglob("*.meta") if re.search(r"^guid: " + guid + "$", meta.read_text(encoding="utf-8"), re.M)]
            if len(matches) != 1:
                raise ValueError("GUID must resolve to one source asset: " + guid)
            guids[guid] = matches[0]
        used.add(pathlib.Path(str(guids[guid]) + ".meta"))
        return guids[guid]
    docs = documents(read(prefab))
    portrait_extraction = None
    if portrait_game_object_file_id is not None:
        if sprite_renderer_file_id is not None:raise ValueError('portrait source selections are mutually exclusive')
        original_objects={ident:text for kind,ident,text in docs if kind==1}
        original_transforms={ident:text for kind,ident,text in docs if kind in (4,224)}
        object_transforms={reference(text,'m_GameObject'):ident for ident,text in original_transforms.items()}
        if portrait_game_object_file_id not in object_transforms:raise ValueError('source card GameObject is absent')
        card=object_transforms[portrait_game_object_file_id]
        direct=[ident for ident,text in original_transforms.items()if reference(text,'m_Father')==card]
        images=[ident for ident in direct if required(original_objects[reference(original_transforms[ident],'m_GameObject')],'m_Name')=='Image']
        masks=[ident for ident in direct if required(original_objects[reference(original_transforms[ident],'m_GameObject')],'m_Name')=='Mask']
        if len(images)!=1 or len(masks)!=1:raise ValueError('composite card requires exact Image/Mask children')
        content=set(images+masks)
        while True:
            expanded=content|{ident for ident,text in original_transforms.items()if reference(text,'m_Father')in content}
            if expanded==content:break
            content=expanded
        kept=set(content);cursor=card
        while cursor:
            if cursor in kept:raise ValueError('cyclic source card ancestry')
            kept.add(cursor);cursor=reference(original_transforms[cursor],'m_Father')
        kept_objects={reference(original_transforms[i],'m_GameObject')for i in kept}
        content_objects={reference(original_transforms[i],'m_GameObject')for i in content}
        source_components=[];extracted=[];activity=[]
        for kind,ident,text in docs:
            if kind==1 and ident in kept_objects:
                activity.append({'gameObjectFileId':ident,'originalActive':bool(int(required(text,'m_IsActive')))})
                if object_transforms[ident]not in content:text=re.sub(r'(^ +m_IsActive:) [01]$',r'\g<1> 1',text,flags=re.M)
                extracted.append((kind,ident,text))
            elif kind in (4,224)and ident in kept:
                if kind!=4:raise ValueError('composite card needs actual RectTransform layout conversion')
                extracted.append((kind,ident,text))
            elif kind==212 and reference(text,'m_GameObject')in content_objects:extracted.append((kind,ident,text))
            elif kind==331 and reference(text,'m_GameObject')in content_objects:extracted.append((kind,ident,text))
            elif kind not in (1,4,224)and reference(text,'m_GameObject')in kept_objects:
                source_components.append({'typeId':kind,'fileId':ident,'serializedComponent':text})
                if kind==95:
                    controller=re.search(r'guid: ([0-9a-f]{32})',required(text,'m_Controller'))
                    if controller:read(resolve_guid(controller[1]))
        if not any(kind==331 for kind,_,_ in extracted):raise ValueError('composite portrait source mask is absent')
        portrait_extraction={'cardGameObjectFileId':portrait_game_object_file_id,'sourceCardTransformFileId':card,
            'sourceImageTransformFileId':images[0],'sourceMaskTransformFileId':masks[0],
            'uiVisiblePose':'actual Almanac Image subtree serialized visible pose with its source SpriteMask; root card/window UI decoration excluded',
            'sourceActivity':activity,'sourceComponents':source_components,
            'limits':['Static source UI composition; nested Animator is retained as proof, animated card playback is not implied']}
        docs=extracted
    if sprite_renderer_file_id is not None:
        selected = [(ident, text) for kind, ident, text in docs if kind == 212 and ident == sprite_renderer_file_id]
        if len(selected) != 1:
            raise ValueError("portrait SpriteRenderer fileID must identify one original component")
        selected_id, selected_text = selected[0]
        selected_object = reference(selected_text, "m_GameObject")
        original_transforms = {ident: text for kind, ident, text in docs if kind in (4, 224)}
        object_transforms = {reference(text, "m_GameObject"): ident for ident, text in original_transforms.items()}
        kept_transforms, kept_objects = set(), set()
        current = object_transforms[selected_object]
        while current:
            if current in kept_transforms:
                raise ValueError("cyclic portrait Transform ancestry")
            text = original_transforms[current]
            kept_transforms.add(current)
            kept_objects.add(reference(text, "m_GameObject"))
            current = reference(text, "m_Father")
        activity = []
        extracted = []
        for kind, ident, text in docs:
            if kind == 114 and reference(text, "m_GameObject") in kept_objects:
                script = re.search(r"guid: ([0-9a-f]{32})", required(text, "m_Script"))
                if script and "SpriteSkin" in resolve_guid(script[1]).stem:
                    raise ValueError("portrait selection contains SpriteSkin; skinning is unsupported")
            if kind == 1 and ident in kept_objects:
                activity.append({"gameObjectFileId": ident, "originalActive": bool(int(required(text, "m_IsActive")))})
                # Source UI pages become active when opened. Activate only the
                # exact selected portrait's ancestry, retaining original state.
                text = re.sub(r"(^ +m_IsActive:) [01]$", r"\g<1> 1", text, flags=re.M)
                extracted.append((kind, ident, text))
            elif kind in (4, 224) and ident in kept_transforms:
                if kind != 4:
                    raise ValueError("portrait RectTransform ancestry needs an explicit layout conversion")
                extracted.append((kind, ident, text))
            elif kind == 212 and ident == selected_id:
                extracted.append((kind, ident, re.sub(r"(^ +m_Enabled:) [01]$", r"\g<1> 1", text, flags=re.M)))
        portrait_extraction = {"spriteRendererFileId": selected_id, "gameObjectFileId": selected_object,
            "uiVisiblePose": "selected portrait renderer and its retained ancestors active",
            "originalRendererEnabled": bool(int(required(selected_text, "m_Enabled"))),
            "sourceActivity": activity, "sourceSpriteRenderer": selected_text}
        docs = extracted
    objects = {}
    transforms = {}
    sprite_docs = []
    animator = None
    animator_docs = []
    nonrendering = []
    source_rect_transforms=[]
    for kind, ident, text in docs:
        if kind == 1:
            objects[ident] = {"name": required(text, "m_Name"), "active": bool(int(required(text, "m_IsActive"))), "tag": required(text, "m_TagString")}
        elif kind == 4 or kind ==224:
            if kind==224:
                # Outside a Canvas/RectTransform parent, the original local
                # TRS is an ordinary world Transform; no UI layout is sampled.
                parent=reference(text,'m_Father')
                if any(t in(222,223)for t,_,_ in docs)or any(t==224 and i==parent for t,i,_ in docs):
                    raise ValueError('RectTransform requires actual Canvas/layout conversion')
                source_rect_transforms.append({'fileId':ident,'serializedComponent':text,'nativeRepresentation':'original world local TRS; non-Canvas node, non-RectTransform parent'})
            transforms[ident] = {"object": reference(text, "m_GameObject"), "parentId": reference(text, "m_Father"),
                                 "position": vector(text, "m_LocalPosition"), "rotation": vector(text, "m_LocalRotation"), "scale": vector(text, "m_LocalScale")}
        elif kind == 212:
            sprite_docs.append((ident, text))
        elif kind == 95:
            animator_docs.append((ident,text))
        elif kind in (50, 58, 60, 61, 82, 114, 198, 199, 210, 331):
            nonrendering.append({"typeId": kind, "fileId": ident, "serializedComponent": text})
        else:
            raise ValueError("unsupported prefab component " + str(kind))
    transform_ids = list(transforms)
    object_to_node = {value["object"]: index for index, value in enumerate(transforms.values())}
    roots = [ident for ident, node in transforms.items() if node["parentId"] == 0]
    if len(roots) != 1:
        raise ValueError("expected one Transform root")

    def node_path(ident):
        node = transforms[ident]
        return "" if node["parentId"] == 0 else "/".join(filter(None, (node_path(node["parentId"]), objects[node["object"]]["name"])))

    nodes = []
    for ident, node in transforms.items():
        parent = transform_ids.index(node["parentId"]) if node["parentId"] else -1
        nodes.append(dict(objects[node["object"]], parent=parent, path=node_path(ident),
                          position=node["position"], rotation=node["rotation"], scale=node["scale"]))
    # Independent Unity Animators resolve paths relative to their own GameObject.
    # Preserve every controller; flatten only its namespace into existing native
    # layers, never merge two controllers' clip keys or dispatch their events.
    animator_docs.sort(key=lambda row:(nodes[object_to_node[reference(row[1],"m_GameObject")]]["path"].count('/'),
        len(nodes[object_to_node[reference(row[1],"m_GameObject")]]["path"]),row[0]))
    if len({reference(text,"m_GameObject")for _,text in animator_docs})!=len(animator_docs):
        raise ValueError("multiple Animators on one source GameObject are ambiguous")
    animator = animator_docs[0][1] if animator_docs else None
    animator_path = nodes[object_to_node[reference(animator, "m_GameObject")]]["path"] if animator is not None else ""
    textures = {}
    sprite_variants = {}
    empty_sprite_variants = {}

    def sprite_definition(guid):
        if guid in empty_sprite_variants:return {}
        if guid in sprite_variants:
            return sprite_variants[guid]
        sprite_path = resolve_guid(guid)
        sprite_text = read(sprite_path)
        bone_metadata = False
        for empty_field in ("m_Bones", "m_Bindpose"):
            if field(sprite_text, empty_field) != "[]":
                bone_metadata = True
                if not allow_unused_sprite_bones and not skins_enabled:
                    raise ValueError(str(sprite_path) + ": skinned sprite metadata requires a separate converter")
        texture_guid = re.search(r"guid: ([0-9a-f]{32})", required(sprite_text, "texture"))[1]
        texture_path = resolve_guid(texture_guid)
        used.add(texture_path)
        png = texture_path.read_bytes()
        if png[:8] != b"\x89PNG\r\n\x1a\n":
            raise ValueError("pilot textures must be PNG")
        width, height = struct.unpack(">II", png[16:24])
        rect_section = re.search(r"^    textureRect:\n(.*?)(?=^    \w)", sprite_text, re.M | re.S)[1]
        rect = {name: float(required(rect_section, name)) for name in ("x", "y", "width", "height")}
        if rect['width']==0 and rect['height']==0:
            logical=re.search(r'^  m_Rect:\n(.*?)(?=^  \w)',sprite_text,re.M|re.S)[1]
            count=int(required(sprite_text,'m_VertexCount'))
            channels=re.search(r'm_Channels:\n(.*?)(?=\n +m_DataSize:)',sprite_text,re.S)[1]
            first=re.search(r'- stream: (\d+)\n +offset: (\d+)\n +format: (\d+)\n +dimension: (\d+)',channels)
            raw=re.search(r'_typelessdata: ([0-9a-fA-F]+)',sprite_text)
            data=bytes.fromhex(raw[1])if raw else b''
            if float(required(logical,'width'))!=0 or float(required(logical,'height'))!=0 or not first or tuple(map(int,first.groups()))!=(0,0,0,3)or not 0<count<=65535 or len(data)<count*12 or any(v!=0 for n in range(count)for v in struct.unpack_from('<3f',data,n*12)):
                raise ValueError(str(sprite_path)+': zero-size Sprite has unproved nonempty geometry')
            empty_sprite_variants[guid]={'sourceSprite':sprite_path.relative_to(assets).as_posix(),'sourceSpriteSha256':hashlib.sha256(sprite_path.read_bytes()).hexdigest(),'serializedSprite':sprite_text,
                'vertexCount':count,'allSourcePositionsZero':True,'sourceTextureRect':rect,'nativeRepresentation':'null Sprite; exact zero-area visual, no replacement pixels or fabricated geometry'}
            return {}
        if rect["width"] <= 0 or rect["height"] <= 0:
            raise ValueError(str(sprite_path)+": invalid sprite rectangle "+str(rect))
        # Original float32 rects can sum 0.00002 pixels beyond an integer edge.
        if rect["x"] < -0.001 or rect["y"] < -0.001 or rect["x"] + rect["width"] > width+0.001 or rect["y"] + rect["height"] > height+0.001:
            raise ValueError(str(sprite_path) + ": sprite rectangle " + str(rect) + " outside texture " + str((width, height)))
        logical_rect = re.search(r"^  m_Rect:\n(.*?)(?=^  \w)", sprite_text, re.M | re.S)[1]
        vertex_count = int(required(sprite_text, "m_VertexCount"))
        channel_text = re.search(r"m_Channels:\n(.*?)(?=\n +m_DataSize:)", sprite_text, re.S)[1]
        channels = [tuple(map(int, values)) for values in re.findall(r"- stream: (\d+)\n +offset: (\d+)\n +format: (\d+)\n +dimension: (\d+)", channel_text)]
        sizes = {0: 4, 1: 2, 2: 1, 3: 1, 4: 2, 5: 2, 6: 1, 7: 1, 8: 2, 9: 2, 10: 4, 11: 4}
        strides = {}
        for stream, offset, fmt, dimension in channels:
            if dimension:
                strides[stream] = max(strides.get(stream, 0), offset + sizes[fmt]*dimension)
        if not channels or channels[0][2:] != (0, 3) or vertex_count <= 0:
            raise ValueError("unsupported sprite position format")
        stream_offsets = {}
        total = 0
        for stream, stride in sorted(strides.items()):
            total = (total + 15) // 16 * 16
            stream_offsets[stream] = total
            total += stride*vertex_count
        payload = bytes.fromhex(required(sprite_text, "_typelessdata"))
        pos_stream, pos_offset, _, _ = channels[0]
        uv_transform = vector(sprite_text, "uvTransform")
        raw_uvs = []
        if len(channels) > 4 and channels[4][2:] == (0, 2):
            stream, offset, _, _ = channels[4]
            raw_uvs = [struct.unpack_from("<ff", payload, stream_offsets[stream]+strides[stream]*index+offset) for index in range(vertex_count)]
        use_raw_uvs = bool(raw_uvs and any(u != 0 or v != 0 for u, v in raw_uvs))
        mesh_vertices = []
        for index in range(vertex_count):
            vx, vy, vz = struct.unpack_from("<fff", payload, stream_offsets[pos_stream]+strides[pos_stream]*index+pos_offset)
            u, v = raw_uvs[index] if use_raw_uvs else ((vx*uv_transform["x"]+uv_transform["y"])/width, (vy*uv_transform["z"]+uv_transform["w"])/height)
            mesh_vertices.append({"x": vx, "y": vy, "z": vz, "u": u, "v": 1-v})
        index_bytes = bytes.fromhex(required(sprite_text, "m_IndexBuffer"))
        indices = list(struct.unpack("<"+"H"*(len(index_bytes)//2), index_bytes))
        if not indices or len(indices)%3 or max(indices) >= vertex_count:
            raise ValueError("invalid source sprite triangles")
        mesh = {"vertices": mesh_vertices, "indices": indices, "uvSource": "vertexData" if use_raw_uvs else "serializedUvTransform"}
        texture_name = hashlib.sha256(png).hexdigest()[:12] + ".png"
        textures[texture_name] = texture_path
        sprite = {"texture": "textures/" + texture_name, "serializedSprite": sprite_text, "hasBoneMetadata": bone_metadata, "mesh": mesh,
                  "sourceSprite": sprite_path.relative_to(assets).as_posix(),
                  "textureRect": dict(rect, yTop=height - rect["y"] - rect["height"]),
                  "logicalSize": {key: float(required(logical_rect, key)) for key in ("width", "height")},
                  "pivot": vector(sprite_text, "m_Pivot"), "textureRectOffset": vector(sprite_text, "textureRectOffset"),
                  "pixelsPerUnit": float(required(sprite_text, "m_PixelsToUnits"))}
        if bone_metadata and skins_enabled:
            from source_skin import streams
            sprite["skin"]=streams(sprite_text,field,required)
        if sprite["pixelsPerUnit"] <= 0:
            raise ValueError("invalid pixels-per-unit")
        sprite_variants[guid] = sprite
        return sprite

    sprites = []
    for ident, text in sprite_docs:
        sprite_node = object_to_node[reference(text, "m_GameObject")]
        if sprite_prefix is not None and nodes[sprite_node]["path"] != "" and not (nodes[sprite_node]["path"] == sprite_prefix or nodes[sprite_node]["path"].startswith(sprite_prefix + "/")):
            continue
        sprite_guid = re.search(r"guid: ([0-9a-f]{32})", required(text, "m_Sprite"))
        if not sprite_guid and reference(text, "m_Sprite") != 0:
            raise ValueError("unresolved external sprite GUID: " + required(text, "m_Sprite"))
        # A Unity renderer may start empty and receive its sprite from an
        # AnimationClip object curve (for example blink overlays).
        definition = sprite_definition(sprite_guid[1]) if sprite_guid else {}
        sprite = {**definition, "guid": sprite_guid[1] if sprite_guid and sprite_guid[1]not in empty_sprite_variants else None,
                  "node": sprite_node,
                  "sortingOrder": int(required(text, "m_SortingOrder")), "sortingLayerId": int(required(text, "m_SortingLayerID")),
                  "enabled": bool(int(required(text, "m_Enabled"))), "color": vector(text, "m_Color"),
                  "flipX": bool(int(required(text, "m_FlipX"))), "flipY": bool(int(required(text, "m_FlipY")))}
        if sprite_guid and sprite_guid[1]in empty_sprite_variants:sprite['sourceEmptySpriteGuid']=sprite_guid[1]
        if masks_enabled:
            sprite["maskInteraction"]=int(required(text,"m_MaskInteraction"))
            if sprite["maskInteraction"] not in (0,1,2):raise ValueError("unsupported source mask interaction")
        sprites.append(sprite)
    source_masks=[]
    if masks_enabled:
        from source_masks import mask_record
        for kind,ident,text in docs:
            if kind!=331:continue
            match=re.search(r"guid: ([0-9a-f]{32})",required(text,"m_Sprite"))
            if not match:raise ValueError("SpriteMask needs an original source sprite")
            definition=sprite_definition(match[1])
            mask_sprite=read(resolve_guid(match[1]))
            texture_guid=re.search(r"guid: ([0-9a-f]{32})",required(mask_sprite,"texture"))[1]
            sampler_text=read(pathlib.Path(str(resolve_guid(texture_guid))+".meta"))
            sampler={key:int(required(sampler_text,name)) for key,name in (("filter","filterMode"),("wrap","wrapU"))}
            if sampler["filter"] not in (0,1) or sampler["wrap"] not in (0,1) or int(required(sampler_text,"wrapV"))!=sampler["wrap"]:
                raise ValueError("unsupported source mask sampler")
            record=mask_record(text,object_to_node[reference(text,"m_GameObject")],match[1],field,reference)
            record.update(fileId=ident,serializedComponent=text,sampler=sampler)
            source_masks.append(record)
    controller_guid = re.search(r"guid: ([0-9a-f]{32})", required(animator, "m_Controller"))[1] if animator is not None else None
    controller_path = resolve_guid(controller_guid) if controller_guid else None
    controller_text = read(controller_path) if controller_path else ""
    layers = []
    states = []
    clips = {}
    clip_sources = {}
    clip_roots = {}
    source_animators = []
    used_state_ids = set()
    for ordinal,(animator_id,animator_text) in enumerate(animator_docs):
        root_path=nodes[object_to_node[reference(animator_text,"m_GameObject")]]["path"]
        scope=root_path+"::" if ordinal else ""
        guid=re.search(r"guid: ([0-9a-f]{32})",required(animator_text,"m_Controller"))[1]
        source_controller=read(resolve_guid(guid));source_docs=documents(source_controller)
        state_machines={ident:text for kind,ident,text in source_docs if kind==1107}
        remap={};next_id=-1
        for kind,ident,text in source_docs:
            if kind!=1102:continue
            if not ordinal:remap[ident]=ident
            else:
                while next_id in used_state_ids:next_id-=1
                remap[ident]=next_id;next_id-=1
            used_state_ids.add(remap[ident])
        first_layer=len(layers)
        main=next((text for kind,ident,text in source_docs if kind==91),"")
        for layer in re.split(r"\n  - serializedVersion: 5\n",section(main,"m_AnimatorLayers"))[1:]:
            machine=state_machines[reference(layer,"m_StateMachine")]
            original_ids=[int(value)for value in re.findall(r"m_State: \{fileID: (-?\d+)\}",machine)]
            default=reference(machine,"m_DefaultState")
            layers.append({"name":scope+required(layer,"m_Name"),"blendingMode":int(required(layer,"m_BlendingMode")),
                "defaultState":remap[default]if default else 0,"states":[remap[x]for x in original_ids],
                "defaultWeight":float(required(layer,"m_DefaultWeight"))})
        for kind,ident,text in source_docs:
            if kind!=1102:continue
            motion_guid=re.search(r"guid: ([0-9a-f]{32})",required(text,"m_Motion"));clip_name=None
            if motion_guid:
                clip_path=resolve_guid(motion_guid[1]);used.add(clip_path);clip=read_clip(clip_path,skins_enabled)
                for curve in clip["curves"]:
                    curve["sourceBindingPath"]=curve["path"]
                    curve["path"]="/".join(filter(None,(root_path,curve["path"])))
                clip_name=scope+clip["name"];clip["name"]=clip_name
                if clip_name in clips and clip_sources[clip_name]!=clip_path:raise ValueError("ambiguous source clip name")
                clips[clip_name]=clip;clip_sources[clip_name]=clip_path;clip_roots[clip_name]=root_path
            state={"id":remap[ident],"name":scope+required(text,"m_Name"),"clip":clip_name,
                "speed":float(required(text,"m_Speed")),"speedParameterActive":bool(int(required(text,"m_SpeedParameterActive"))),
                "speedParameter":field(text,"m_SpeedParameter","")}
            if ordinal:state.update(sourceStateId=ident,sourceStateName=required(text,"m_Name"),sourceAnimatorFileId=animator_id)
            states.append(state)
        source_animators.append({"fileId":animator_id,"path":root_path,"node":object_to_node[reference(animator_text,"m_GameObject")],
            "enabled":bool(int(required(animator_text,"m_Enabled"))),"applyRootMotion":bool(int(required(animator_text,"m_ApplyRootMotion"))),
            "controllerGuid":guid,"serializedAnimator":animator_text,"serializedController":source_controller,
            "layers":list(range(first_layer,len(layers))),"primary":ordinal==0})
    node_paths = {node["path"] for node in nodes}
    colliders = []
    for component in nonrendering:
        if component["typeId"] == 61:
            text = component["serializedComponent"]
            colliders.append({"type": "box", "node": object_to_node[reference(text, "m_GameObject")],
                              "fileId": component["fileId"], "enabled": bool(int(required(text, "m_Enabled"))),
                              "trigger": bool(int(required(text, "m_IsTrigger"))), "offset": vector(text, "m_Offset"), "size": vector(text, "m_Size")})
    omitted_nodes = []
    if active_only:
        source_nodes = nodes
        def active(index):
            return nodes[index]["active"] and (nodes[index]["parent"] < 0 or active(nodes[index]["parent"]))
        kept = [index for index in range(len(nodes)) if active(index) and
                (sprite_prefix is None or nodes[index]["path"] == "" or nodes[index]["path"] == sprite_prefix or nodes[index]["path"].startswith(sprite_prefix + "/"))]
        omitted_nodes = [node["path"] for index, node in enumerate(nodes) if index not in kept]
        remap = {old: new for new, old in enumerate(kept)}
        sprites = [dict(sprite, node=remap[sprite["node"]]) for sprite in sprites if sprite["node"] in remap and sprite["enabled"]]
        nodes = [dict(nodes[old], parent=remap.get(nodes[old]["parent"], -1)) for old in kept]
        node_paths = {node["path"] for node in nodes}
    else:
        source_nodes = nodes
    for clip in clips.values():
        for curve in clip["curves"]:
            if curve["kind"] == "float" and curve["classID"] == 212 and curve["path"] in node_paths:
                matches = [sprite for sprite in sprites if nodes[sprite["node"]]["path"] == curve["path"]]
                if len(matches) != 1:
                    raise ValueError("SpriteRenderer float binding requires one exact renderer: " + curve["path"])
    ignored_bindings = sorted({curve["path"] for clip in clips.values() for curve in clip["curves"] if curve["path"] not in node_paths})
    for clip in clips.values():
        for curve in clip["curves"]:
            if curve["kind"] == "sprite":
                for key in curve["keys"]:
                    if key["spriteGuid"] is not None:
                        sprite_definition(key["spriteGuid"])
                        if key['spriteGuid']in empty_sprite_variants:
                            key['sourceEmptySpriteGuid']=key['spriteGuid'];key['spriteGuid']=None
    extra_sprite_guids = {}
    for relative in extra_sprites:
        path = assets / relative
        meta = pathlib.Path(str(path) + ".meta")
        guid = re.search(r"^guid: ([0-9a-f]{32})$", read(meta), re.M)[1]
        guids[guid] = path
        sprite_definition(guid)
        extra_sprite_guids[relative] = guid
    source_skins=[]
    if skins_enabled:
        from source_skin import rigs
        source_skins=rigs(docs,object_to_node,{ident:index for index,ident in enumerate(transform_ids)},nodes,sprites,sprite_variants,clips,resolve_guid,read,field,required,reference,section)
    unbound_bindings, unbound_proof = [], None
    if ignored_bindings and not (active_only or sprite_prefix is not None or allow_unused_sprite_bones or portrait_extraction):
        unbound_bindings, unbound_proof = source_unbound_bindings(
            assets, prefab, docs, clips, clip_sources, node_paths, sprite_variants, resolve_guid, read, masks_enabled, particle_binding_proof)
        proven_paths = {record["path"] for record in unbound_bindings}
        ignored_bindings = [path for path in ignored_bindings if path not in proven_paths]
        if ignored_bindings and source_hierarchy_proof is not None:
            from source_animators import unbound_bindings as prove_unbound
            unbound_bindings,unbound_proof=prove_unbound(docs,nodes,clips,clip_sources,clip_roots,assets,prefab,
                resolve_guid,read,field,required,reference,section,source_hierarchy_proof)
            proven_paths={record["path"]for record in unbound_bindings}
            ignored_bindings=[path for path in ignored_bindings if path not in proven_paths]
    anchors = {}
    world_transforms = {}
    shadow_index = next((index for index, node in enumerate(nodes) if node["path"] == "Shadow"), None)
    def world_transform(index):
        if index in world_transforms:
            return world_transforms[index]
        node = nodes[index]
        p, q, s = node["position"], node["rotation"], node["scale"]
        x, y, z, w = (q[axis] for axis in ("x", "y", "z", "w"))
        matrix = [[(1-2*(y*y+z*z))*s["x"], 2*(x*y-z*w)*s["y"], 2*(x*z+y*w)*s["z"]],
                  [2*(x*y+z*w)*s["x"], (1-2*(x*x+z*z))*s["y"], 2*(y*z-x*w)*s["z"]],
                  [2*(x*z-y*w)*s["x"], 2*(y*z+x*w)*s["y"], (1-2*(x*x+y*y))*s["z"]]]
        position = [p[axis] if node["parent"] >= 0 else 0 for axis in ("x", "y", "z")]
        if node["parent"] >= 0:
            parent_matrix, parent_position = world_transform(node["parent"])
            position = [parent_position[row]+sum(parent_matrix[row][axis]*position[axis] for axis in range(3)) for row in range(3)]
            matrix = [[sum(parent_matrix[row][axis]*matrix[axis][col] for axis in range(3)) for col in range(3)] for row in range(3)]
        world_transforms[index] = matrix, position
        return matrix, position
    def world_matrix(index):
        matrix, position = world_transform(index)
        return matrix[0][0], matrix[1][0], matrix[0][1], matrix[1][1], position[0], position[1]
    if shadow_index is not None:
        transform = world_matrix(shadow_index)
        anchors["shadow"] = {"path": "Shadow", "x": transform[4], "y": transform[5], "relativeToRuntimeRoot": True}
    bounds_points = []
    def active_node(index):
        return nodes[index]["active"] and (nodes[index]["parent"] < 0 or active_node(nodes[index]["parent"]))
    for sprite in sprites:
        if not sprite["enabled"] or sprite["guid"] is None or not active_node(sprite["node"]):
            continue
        a, b, c, d, x, y = world_matrix(sprite["node"])
        for vertex in sprite["mesh"]["vertices"]:
            vx, vy = vertex["x"]*(-1 if sprite["flipX"] else 1), vertex["y"]*(-1 if sprite["flipY"] else 1)
            bounds_points.append((a*vx+c*vy+x, b*vx+d*vy+y))
    if not bounds_points:
        raise ValueError("no visible source mesh for package bounds")
    visual_bounds = {"minX": min(x for x,y in bounds_points), "maxX": max(x for x,y in bounds_points),
                     "minY": min(y for x,y in bounds_points), "maxY": max(y for x,y in bounds_points)}
    output.mkdir(parents=True, exist_ok=True)
    (output / "textures").mkdir(exist_ok=True)
    for name, path in sorted(textures.items()):
        shutil.copyfile(path, output / "textures" / name)
    package = {"schema": 1, "sourcePrefab": pathlib.PurePosixPath(prefab_path).as_posix(), "unitsPerNativePixel": 0.01,
               "applyRootMotion": bool(int(required(animator, "m_ApplyRootMotion"))) if animator is not None else False,
               "static": animator is None,
               "root": next(index for index, node in enumerate(nodes) if node["parent"] < 0), "nodes": nodes,
               "sourceHierarchy": source_nodes if omitted_nodes else None,
               "sprites": sorted(sprites, key=lambda sprite: (sprite["sortingLayerId"], sprite["sortingOrder"])),
               "spriteVariants": sprite_variants,
               "extraSpriteGuids": extra_sprite_guids, "anchors": anchors, "visualBounds": visual_bounds,
               "clips": clips, "controller": {"states": states, "layers": layers, "serializedController": controller_text},
               "nonRenderingComponents": nonrendering,
               "colliders": colliders,
               "ignoredBindings": ignored_bindings, "omittedInactiveNodes": omitted_nodes,
               "spritePrefix": sprite_prefix, "unusedSpriteBonesExplicitlyAllowed": allow_unused_sprite_bones,
               "sources": [{"path": path.relative_to(assets).as_posix(), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()} for path in sorted(used)],
               "deferredComponents": sorted({kind for kind, _, _ in docs if kind in ((198,199) if masks_enabled else (198,199,331))}),
               "validation": "Exported source data. Native rendering and source-runtime comparison are separate verification steps."}
    if masks_enabled:
        package["rendererFeatures"]=["spriteMaskV1"]
        package["sourceMasks"]=source_masks
    if skins_enabled:
        package["rendererFeatures"]=sorted(features)
        package["sourceSkins"]=source_skins
    if unbound_bindings:
        package["sourceUnboundBindings"] = unbound_bindings
        package["sourceUnboundHierarchyProof"] = unbound_proof
    if empty_sprite_variants:package['sourceEmptySprites']=empty_sprite_variants
    if source_rect_transforms:package['sourceWorldRectTransforms']=source_rect_transforms
    if len(source_animators)>1:
        package['sourceAnimators']=source_animators
        package['independentAnimatorDispatch']='Source Animator roots retain independent controller state; caller must supply extraStates with exact exported layer IDs. No shared-clock or event broadcast is implied.'
    if portrait_extraction is not None:
        package["uiPortraitExtraction"] = portrait_extraction
    (output / "animation.json").write_text(json.dumps(package, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8", newline="\n")
    return {"nodes": len(nodes), "sprites": len(sprites), "textures": len(textures), "clips": list(clips),
            "events": {name: clip["events"] for name, clip in clips.items()}, "ignoredBindings": ignored_bindings}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", required=True, type=pathlib.Path)
    parser.add_argument("--prefab", default="Resources/plants/peashooter/PeashooterPrefab.prefab")
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--active-only", action="store_true", help="omit initially inactive hierarchies; intended for a separately labelled initial background preview")
    parser.add_argument("--sprite-prefix", help="export only this visual subtree and the root sprite, retaining the transform hierarchy")
    parser.add_argument("--allow-unused-sprite-bones", action="store_true", help="only after confirming the selected prefab has no SpriteSkin; original bone/mesh metadata is preserved verbatim")
    parser.add_argument("--extra-sprite", action="append", default=[], help="source-relative Sprite asset used by a verified runtime appearance override")
    parser.add_argument("--source-feature",action="append",default=[],choices=["spriteMaskV1","spriteSkinV1"],help="staging-only source renderer metadata; requires matching native renderer implementation")
    parser.add_argument("--source-hierarchy-proof",type=pathlib.Path,help="exact startup class-chain evidence for absent Unity path bindings; does not admit deferred renderer components")
    args = parser.parse_args()
    print(json.dumps(export(args.assets.resolve(), args.prefab, args.output.resolve(), args.active_only, args.sprite_prefix, args.allow_unused_sprite_bones, args.extra_sprite, source_features=args.source_feature,source_hierarchy_proof=args.source_hierarchy_proof), ensure_ascii=True))
