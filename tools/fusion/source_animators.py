"""Source-scoped controller projection and audited Unity missing-target bindings."""
import hashlib
import json
from pathlib import Path
import re
import zlib


def hierarchy_audit(path,assets,prefab,read):
    """Validate the exact source class chain and startup bodies supplied by audit.

    This proves binding-tree stability, not particle/AI/rendering parity. No
    source component or deferred renderer is removed by this permission.
    """
    path=Path(path);raw=path.read_bytes();audit=json.loads(raw)
    if audit.get('schema')!=1 or audit['sourcePrefab']!=prefab.relative_to(assets).as_posix() or audit['sourcePrefabSha256']!=hashlib.sha256(prefab.read_bytes()).hexdigest():
        raise ValueError('source hierarchy audit prefab differs')
    scripts=audit['scriptChain'];declarations=[]
    binding=audit['runtimeClassBinding'];catalog_path=Path(binding['path']);catalog_raw=catalog_path.read_bytes()
    if hashlib.sha256(catalog_raw).hexdigest()!=binding['sha256']:raise ValueError('source runtime class binding hash differs')
    rows=json.loads(catalog_raw)
    matches=[row for row in rows if row.get('assets',{}).get('className')==binding['className'] and
        row.get('assets',{}).get('loaderBinding',{}).get('runtime',{}).get('prefab')==audit['sourcePrefab']]
    if len(matches)!=1 or not scripts or Path(scripts[0]['path']).stem!=binding['className']:
        raise ValueError('source hierarchy leaf differs from runtime prefab binding')
    for index,row in enumerate(scripts):
        source=assets/row['path'];text=read(source)
        if hashlib.sha256(source.read_bytes()).hexdigest()!=row['sha256']:raise ValueError('source hierarchy script hash differs')
        match=re.search(r'public class (\w+)\s*:\s*(\w+)',text)
        if not match or match[1]!=source.stem:raise ValueError('source hierarchy class declaration differs')
        expected=Path(scripts[index+1]['path']).stem if index+1<len(scripts) else 'MonoBehaviour'
        if match[2]!=expected:raise ValueError('source hierarchy inheritance chain differs')
        for method in ('Awake','Start'):
            if re.search(r'\bvoid\s+'+method+r'\s*\(',text):declarations.append(source.stem+'$$'+method)
    proof=Path(audit['primaryEvidence']);primary_raw=proof.read_bytes()
    if hashlib.sha256(primary_raw).hexdigest()!=audit['primaryEvidenceSha256']:raise ValueError('source startup evidence hash differs')
    primary=json.loads(primary_raw)
    if 'content'in primary:primary=json.loads(next(x['text']for x in primary['content']if x['type']=='text'))
    methods={m['method']:m for m in audit['methods']}
    if not set(declarations).issubset(methods):raise ValueError('source Awake/Start override lacks primary evidence')
    for method,row in methods.items():
        body=primary['functions'].get(row['address'])
        if not body or body['name']!=method or hashlib.sha256(body['decompiled_code'].encode()).hexdigest()!=row['codeSha256']:
            raise ValueError('source startup method body differs')
        code=body['decompiled_code']
        if any(call in code for call in ('Instantiate','AddComponent','SetParent','set_parent','GameObject___ctor','(**(code')):
            raise ValueError('source startup has unproved hierarchy mutation/virtual dispatch')
    return {'manifestSha256':hashlib.sha256(raw).hexdigest(),'audit':audit}


def unbound_bindings(docs,nodes,clips,clip_sources,clip_roots,assets,prefab,resolve_guid,read,
                     field,required,reference,section,audit_path):
    """Absent paths are no-ops only for a complete, audited Animator subtree.

    Class/binding identity comes from Unity's serialized binding table, not an
    exporter guess about similarly named nodes. Original curves/keys survive.
    """
    audit=hierarchy_audit(audit_path,assets,prefab,read)
    prefab_text=read(prefab)
    headers=re.findall(r'^--- !u!(\d+) &(-?\d+)([^\n]*)$',prefab_text,re.M)
    if len(headers)!=len(docs) or any(t.strip()for _,_,t in headers):raise ValueError('incomplete source hierarchy documents')
    by_id={i:(k,t)for k,i,t in docs};objects={i:t for k,i,t in docs if k==1};transforms={i:t for k,i,t in docs if k==4}
    if len(by_id)!=len(docs)or len(transforms)!=len(objects)or any(k==1001 for k,_,_ in docs):raise ValueError('inherited/duplicate source hierarchy')
    for kind,ident,text in docs:
        if any(field(text,n)is not None and reference(text,n)!=0 for n in ('m_CorrespondingSourceObject','m_PrefabInstance','m_PrefabAsset')):
            raise ValueError('inherited source hierarchy is not complete')
        if kind==1:
            components=[int(v)for v in re.findall(r'component: \{fileID: (-?\d+)\}',section(text,'m_Component'))]
            if len(components)!=len(set(components))or any(c not in by_id or reference(by_id[c][1],'m_GameObject')!=ident for c in components):
                raise ValueError('source component ownership differs')
        else:
            owner=reference(text,'m_GameObject')
            if owner not in objects or not re.search(r'component: \{fileID: '+str(ident)+r'\}',section(objects[owner],'m_Component')):
                raise ValueError('source component is absent from its GameObject')
        if kind==4:
            children=[int(v)for v in re.findall(r'fileID: (-?\d+)',section(text,'m_Children'))]
            actual=[i for i,t in transforms.items()if reference(t,'m_Father')==ident]
            if sorted(children)!=sorted(actual):raise ValueError('source Transform child list differs')
    def semantic(value):
        return json.loads(value)if value.startswith('"')else value[1:-1].replace("''", "'")if value.startswith("'")else value
    def source_path(ident,ancestors=()):
        if ident in ancestors:raise ValueError('cyclic source hierarchy')
        text=transforms[ident];parent=reference(text,'m_Father');name=semantic(required(objects[reference(text,'m_GameObject')],'m_Name'))
        if '/'in name:raise ValueError('ambiguous source Transform name')
        if parent and parent not in transforms:raise ValueError('source Transform parent is absent')
        return '/'.join(filter(None,(source_path(parent,ancestors+(ident,)),name)))if parent else ''
    paths=[source_path(i)for i in transforms]
    if sorted(paths)!=sorted(n['path']for n in nodes):raise ValueError('exported source hierarchy path differs')
    if len(paths)!=len(set(paths)):raise ValueError('ambiguous source node path')
    roots={}
    for root in set(clip_roots.values()):
        if root not in paths:raise ValueError('source Animator root is absent')
        relative=[p[len(root)+1:]if root and p!=root else '' if p==root else p for p in paths if not root or p==root or p.startswith(root+'/')]
        hashes=[zlib.crc32(p.encode())for p in relative]
        if len(hashes)!=len(set(hashes)):raise ValueError('ambiguous source path CRC32')
        roots[root]={'paths':relative,'hashes':hashes}
    records=[]
    for name,clip in clips.items():
        bindings=[];source=clip_sources[name];root=clip_roots[name]
        for block in re.split(r'\n    - serializedVersion: 2\n',section(read(source),'m_ClipBindingConstant'))[1:]:
            bindings.append((int(required(block,'path')),int(required(block,'attribute')),int(required(block,'typeID')),
                reference(block,'script'),int(required(block,'isPPtrCurve')),int(required(block,'isIntCurve')),
                int(required(block,'isSerializeReferenceCurve')),int(required(block,'customType'))))
        for index,curve in enumerate(clip['curves']):
            if curve['path']in paths:continue
            raw=curve['sourceBindingPath']
            source_path_value=semantic(raw)
            placeholder=re.fullmatch(r'path_0x([0-9A-Fa-f]{8})_[A-Za-z0-9]+',source_path_value)
            crc=int(placeholder[1],16)if placeholder else zlib.crc32(source_path_value.encode())
            attr={'position':1,'euler':4,'scale':3}.get(curve['kind'])
            expected=(crc,attr,4,0,0,0,0,4 if attr==4 else 0)
            if curve['kind']=='sprite':expected=(crc,0,212,0,1,0,0,23)
            elif curve['kind']=='float'and curve.get('classID')==1 and curve.get('attribute')=='m_IsActive':expected=(crc,zlib.crc32(b'm_IsActive'),1,0,0,0,0,0)
            elif curve['kind']=='float'and curve.get('classID')==212 and curve.get('attribute')=='m_SortingOrder':expected=(crc,zlib.crc32(b'm_SortingOrder'),212,0,0,0,0,26)
            elif attr is None:raise ValueError('unsupported absent source component binding')
            if crc in roots[root]['hashes']or bindings.count(expected)!=1:raise ValueError('absent source binding identity is not proven: '+curve['path'])
            records.append({'path':curve['path'],'sourceBindingPath':raw,'animatorRoot':root,'rawPathHash':f'0x{crc:08X}',
                'clip':name,'curveIndex':index,'curveKind':curve['kind'],'sourceClip':source.relative_to(assets).as_posix(),
                'sourceClipSha256':hashlib.sha256(source.read_bytes()).hexdigest()})
    return records,{'method':'complete audited Animator-relative source hierarchy; absent binding is a Unity no-op',
        'sourcePrefabSha256':hashlib.sha256(prefab.read_bytes()).hexdigest(),'documentCount':len(docs),
        'animatorRoots':{r:{'paths':v['paths'],'pathHashes':[f'0x{x:08X}'for x in v['hashes']]}for r,v in roots.items()},'startupHierarchyAudit':audit}
