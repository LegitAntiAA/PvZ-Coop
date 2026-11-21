"""Source Unity2D skin streams. IDs are original prefab/assembly identities."""
import math
import re
import struct

def streams(text, field, required):
    bind=re.search(r"^    m_Bindpose:\n(.*?)(?=^    \w)",text,re.M|re.S)
    if not bind:raise ValueError("skin has no original bind-pose array")
    matrices=[]
    for block in re.split(r"^    - e00:",bind[1],flags=re.M)[1:]:
        values={"e00":float(block.splitlines()[0].strip())}
        values.update({k:float(v) for k,v in re.findall(r"^ +([e][0-3][0-3]): ([^\n]+)",block,re.M)})
        if len(values)!=16 or any(not math.isfinite(v) for v in values.values()):raise ValueError("invalid source skin bind pose")
        matrix=[[values[f"e{r}{c}"] for c in range(4)] for r in range(4)]
        if matrix[3]!=[0.,0.,0.,1.]:raise ValueError("non-affine source skin bind pose")
        matrices.append(matrix)
    channels=[tuple(map(int,m)) for m in re.findall(r"- stream: (\d+)\n +offset: (\d+)\n +format: (\d+)\n +dimension: (\d+)",re.search(r"m_Channels:\n(.*?)(?=\n +m_DataSize:)",text,re.S)[1])]
    if len(channels)<14 or not matrices:raise ValueError("source skin lacks four-influence streams")
    sizes={0:4,1:2,2:1,3:1,4:2,5:2,6:1,7:1,8:2,9:2,10:4,11:4};strides={}
    for stream,offset,fmt,dimension in channels:
        if dimension:strides[stream]=max(strides.get(stream,0),offset+sizes[fmt]*dimension)
    offsets={};total=0;count=int(required(text,"m_VertexCount"))
    for stream,stride in sorted(strides.items()):total=(total+15)//16*16;offsets[stream]=total;total+=stride*count
    payload=bytes.fromhex(required(text,"_typelessdata"));ws,wo,wf,wd=channels[12];bs,bo,bf,bd=channels[13]
    if (wf,wd)!=(0,4) or bf not in (10,11) or bd!=4:raise ValueError("unsupported source skin weight/index layout")
    weights=[];indices=[]
    for i in range(count):
        w=struct.unpack_from("<4f",payload,offsets[ws]+strides[ws]*i+wo);b=struct.unpack_from("<4I",payload,offsets[bs]+strides[bs]*i+bo)
        if any(not math.isfinite(x) or x<0 for x in w):raise ValueError("invalid source skin weight")
        weights.append(w);indices.append(b)
    return {"bindPoses":matrices,"weights":weights,"boneIndices":indices,
            "weightFormat":"float32x4","indexFormat":"uint32x4"}

def rigs(docs, object_to_node, transform_to_node, nodes, sprites, variants, clips, resolve_guid, read, field, required, reference, section):
    output=[]
    for kind,ident,text in docs:
        if kind!=114 or "m_BoneTransforms:" not in text:continue
        script=required(text,"m_Script");match=re.search(r"guid: ([0-9a-f]{32})",script)
        assembly=resolve_guid(match[1]) if match else None
        if not assembly or assembly.name!="Unity.2D.Animation.Runtime.dll" or reference(text,"m_Script")!=-972417856:
            raise ValueError("unproved SpriteSkin assembly/class identity")
        # resolve_guid records the assembly meta; retain exact binary bytes too.
        read(assembly, binary=True)
        owner=reference(text,"m_GameObject");node=object_to_node[owner]
        root=reference(text,"m_RootBone");bones=[int(x) for x in re.findall(r"fileID: (-?\d+)",section(text,"m_BoneTransforms"))]
        if root and root not in transform_to_node or any(x and x not in transform_to_node for x in bones):raise ValueError("external/unresolved skin bone Transform")
        if any(re.search(r"fileID: "+str(x)+r", guid:",section(text,"m_BoneTransforms")) for x in bones):raise ValueError("external source skin palette")
        auto=int(required(text,"m_AutoRebind"));enabled=int(required(text,"m_Enabled"))
        if auto!=0 or enabled not in (0,1):raise ValueError("dynamic source auto-rebind is not implemented")
        renderer=[s for s in sprites if s["node"]==node]
        if len(renderer)>1:raise ValueError("ambiguous skin SpriteRenderer")
        guid=renderer[0]["guid"] if renderer else None
        # Unity SpriteSkinUtility.Validate0xF33FE0 supplies the exact nullable fallback.
        data=variants.get(guid,{}).get("skin") if guid else None
        validation=0 if not guid else 1 if not data else 3 if not root else 5 if len(bones)!=len(data["bindPoses"]) else 6 if any(not x for x in bones) else 7 if any(i>=len(bones) for row in data["boneIndices"] for i in row) else 8
        reachable={guid} if guid else set()
        for clip in clips.values():
            for curve in clip["curves"]:
                if curve["kind"]=="sprite" and curve["path"]==nodes[node]["path"]:
                    reachable.update(key["spriteGuid"] for key in curve["keys"] if key["spriteGuid"])
        by_sprite={}
        for candidate in sorted(reachable):
            candidate_data=variants[candidate].get("skin")
            by_sprite[candidate]=1 if not candidate_data else 3 if not root else 5 if len(bones)!=len(candidate_data["bindPoses"]) else 6 if any(not x for x in bones) else 7 if any(i>=len(bones) for row in candidate_data["boneIndices"] for i in row) else 8
        output.append({"fileId":ident,"node":node,"enabled":bool(enabled),"autoRebind":False,"rootBoneNode":transform_to_node.get(root,-1),
            "boneNodes":[transform_to_node.get(x,-1) for x in bones],"sourceValidationResult":validation,"sourceValidationBySprite":by_sprite,"sourceSpriteGuid":guid,
            "scriptGuid":match[1],"scriptFileId":-972417856,"serializedComponent":text})
    return output
