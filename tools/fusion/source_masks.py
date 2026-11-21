"""Source SpriteMask metadata and portable pixel reference; no scene simulation."""
import math
import struct
import zlib

def mask_record(text, node, sprite_guid, field, reference):
    def integer(name):
        value=int(field(text,name))
        if not -2147483648<=value<=2147483647:raise ValueError("invalid mask integer "+name)
        return value
    cutoff=float(field(text,"m_MaskAlphaCutoff"))
    enabled=integer("m_Enabled");custom=integer("m_IsCustomRangeActive")
    if enabled not in (0,1) or custom not in (0,1) or not math.isfinite(cutoff) or not 0<=cutoff<=1:
        raise ValueError("invalid source SpriteMask state")
    return {"node":node,"spriteGuid":sprite_guid,"enabled":bool(enabled),"alphaCutoff":cutoff,
            "customRange":bool(custom),"frontSortingLayerId":integer("m_FrontSortingLayerID"),
            "backSortingLayerId":integer("m_BackSortingLayerID"),"frontSortingOrder":integer("m_FrontSortingOrder"),
            "backSortingOrder":integer("m_BackSortingOrder"),"sortPoint":integer("m_SpriteSortPoint")}

def uv_at_point(vertices, indices, x, y):
    """Original source triangles, preserving their exact UVs."""
    for offset in range(0,len(indices),3):
        a,b,c=(vertices[indices[offset+i]] for i in range(3))
        determinant=(b["y"]-c["y"])*(a["x"]-c["x"])+(c["x"]-b["x"])*(a["y"]-c["y"])
        if abs(determinant)<1e-15:continue
        u=((b["y"]-c["y"])*(x-c["x"])+(c["x"]-b["x"])*(y-c["y"]))/determinant
        v=((c["y"]-a["y"])*(x-c["x"])+(a["x"]-c["x"])*(y-c["y"]))/determinant
        w=1-u-v
        if min(u,v,w)>=-1e-12:return tuple(u*a[k]+v*b[k]+w*c[k] for k in ("u","v"))
    return None

def bilinear_alpha(alpha, width, height, u, v):
    """Top-down normalized UVs; clamp to source texel centers."""
    x=max(0.,min(width-1.,u*width-.5));y=max(0.,min(height-1.,v*height-.5))
    ix=int(math.floor(x));iy=int(math.floor(y));fx=x-ix;fy=y-iy
    def at(a,b):return alpha[min(height-1,b)*width+min(width-1,a)]/255.
    return (at(ix,iy)*(1-fx)+at(ix+1,iy)*fx)*(1-fy)+(at(ix,iy+1)*(1-fx)+at(ix+1,iy+1)*fx)*fy

def visible(interaction, covered):
    if interaction not in (0,1,2):raise ValueError("unknown SpriteMaskInteraction")
    return interaction==0 or (covered if interaction==1 else not covered)

def deform_vertex(position, indices, weights, palettes):
    """Reference LBS after palette=rootInv*boneWorld*originalBindPose."""
    if len(indices)!=4 or len(weights)!=4 or any(not math.isfinite(w) or w<0 for w in weights) or abs(sum(weights)-1)>1e-4:
        raise ValueError("invalid source four-influence weights")
    out=[0.,0.,0.]
    for index,weight in zip(indices,weights):
        if not weight:continue
        if index<0 or index>=len(palettes):raise ValueError("bone index outside palette")
        matrix=palettes[index]
        for row in range(3):out[row]+=weight*(sum(matrix[row][i]*position[i] for i in range(3))+matrix[row][3])
    return out


def read_png_alpha(path):
    """Small source-reference decoder for noninterlaced8-bit RGB/RGBA PNGs."""
    raw=path.read_bytes()
    if raw[:8]!=b"\x89PNG\r\n\x1a\n":raise ValueError("not a source PNG")
    width,height,bits,color,compression,filtering,interlace=struct.unpack(">IIBBBBB",raw[16:29])
    if bits!=8 or color not in (2,6) or compression or filtering or interlace:raise ValueError("unsupported reference PNG layout")
    channels=4 if color==6 else 3;stride=width*channels;data=bytearray();offset=8
    while offset<len(raw):
        size=struct.unpack_from(">I",raw,offset)[0];kind=raw[offset+4:offset+8]
        if kind==b"IDAT":data.extend(raw[offset+8:offset+8+size])
        if kind==b"tRNS":raise ValueError("RGB color-key needs an explicit reference decoder")
        offset+=size+12
    pixels=zlib.decompress(data)
    if len(pixels)!=(stride+1)*height:raise ValueError("source PNG scanline size differs")
    previous=bytearray(stride);alpha=[]
    def paeth(a,b,c):
        p=a+b-c;da=abs(p-a);db=abs(p-b);dc=abs(p-c)
        return a if da<=db and da<=dc else b if db<=dc else c
    for y in range(height):
        kind=pixels[y*(stride+1)];row=bytearray(pixels[y*(stride+1)+1:(y+1)*(stride+1)])
        if kind>4:raise ValueError("invalid PNG row filter")
        for x in range(stride):
            a=row[x-channels] if x>=channels else 0;b=previous[x];c=previous[x-channels] if x>=channels else 0
            predictor=(0,a,b,(a+b)//2,paeth(a,b,c))[kind]
            row[x]=(row[x]+predictor)&255
        alpha.extend(row[3::4] if channels==4 else [255]*width);previous=row
    return width,height,alpha
