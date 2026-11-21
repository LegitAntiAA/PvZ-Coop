"""Strict source ParticleSystem -> native Tod definition-data subset. No Unity simulation."""
import hashlib,json,math,pathlib,re,shutil,subprocess,sys
from source_particle_png import crop_png
sys.dont_write_bytecode=True
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
import export_animation as e

def block(text,key):
 m=re.search(r'^( +)'+re.escape(key)+r':[^\n]*\n',text,re.M)
 if not m:return ''
 end=len(text);indent=len(m[1])
 for z in re.finditer(r'^( +)\S',text[m.end():],re.M):
  if len(z[1])<=indent and not text[m.end()+z.start():].lstrip().startswith('-'):end=m.end()+z.start();break
 return text[m.end():end]
def constant(text,key):
 b=block(text,key)
 if not b or int(e.required(b,'minMaxState'))!=0:raise ValueError('unsupported nonconstant '+key)
 x=float(e.required(b,'scalar'))
 if not math.isfinite(x):raise ValueError('nonfinite '+key)
 return x

def export(assets,prefab,output,source_id):
 assets=pathlib.Path(assets);output=pathlib.Path(output);output.mkdir(parents=True,exist_ok=True)
 used=set();guids={}
 def read(p):used.add(p);return p.read_text(encoding='utf-8')
 def resolve(g):
  if g not in guids:
   r=subprocess.run(['rg','-l','--glob','*.meta','^guid: '+g+'$',str(assets)],capture_output=True,text=True,encoding='utf-8',check=False)
   found=[pathlib.Path(x) for x in r.stdout.splitlines()]
   if len(found)!=1:raise ValueError('ambiguous source GUID '+g)
   used.add(found[0]);guids[g]=found[0].with_suffix('')
  return guids[g]
 docs=e.documents(read(assets/prefab));allowed={1,4,198,199}
 if any(k not in allowed for k,_,_ in docs):raise ValueError('unsupported source effect component')
 objects={i:t for k,i,t in docs if k==1};trans={i:t for k,i,t in docs if k==4}
 root=next(i for i,t in trans.items() if not e.reference(t,'m_Father'))
 children=[int(x) for x in re.findall(r'fileID: (-?\d+)',block(trans[root],'m_Children'))]
 if len(trans)!=len(children)+1:raise ValueError('nested particle transform hierarchy needs composed export')
 if e.vector(trans[root],'m_LocalScale')!={'x':1.,'y':1.,'z':1.} or e.vector(trans[root],'m_LocalRotation')!={'x':0.,'y':0.,'z':0.,'w':1.}:raise ValueError('source effect root projection requires full hierarchy bake')
 ps={e.reference(t,'m_GameObject'):t for k,_,t in docs if k==198};render={e.reference(t,'m_GameObject'):t for k,_,t in docs if k==199}
 emitters=[];sprite_cache={};sources=[]
 for ordinal,tid in enumerate([root]+children):
  t=trans[tid];oid=e.reference(t,'m_GameObject');p=ps[oid];r=render[oid]
  enabled=[]
  for module in re.findall(r'^  ([A-Z]\w+Module):',p,re.M):
   if e.field(block(p,module),'enabled')=='1':enabled.append(module)
  supported={'InitialModule','EmissionModule','ShapeModule','SizeModule','UVModule'}
  if set(enabled)-supported:raise ValueError('unsupported enabled modules '+str(set(enabled)-supported))
  if e.required(p,'looping')!='0' or e.required(p,'simulationSpeed')!='1' or e.required(p,'useUnscaledTime')!='0':raise ValueError('source loop/time module needs adapter')
  init=block(p,'InitialModule');emission=block(p,'EmissionModule');shape=block(p,'ShapeModule');uv=block(p,'UVModule');size=block(p,'SizeModule')
  if e.required(init,'size3D')!='0' or e.required(init,'rotation3D')!='0' or constant(init,'gravityModifier')!=0:raise ValueError('unsupported initial3D/gravity')
  if int(e.required(emission,'m_BurstCount')) or constant(emission,'rateOverDistance')!=0:raise ValueError('burst/distance emission needs adapter')
  scale=e.vector(t,'m_LocalScale')
  if max(scale.values())-min(scale.values())>1e-5:raise ValueError('anisotropic source child scale needs adapter')
  record={'nodeName':e.required(objects[oid],'m_Name'),'nodeIndex':ordinal-1,'sourceTransformFileId':tid,'sourcePosition':e.vector(t,'m_LocalPosition') if tid!=root else {'x':0.,'y':0.,'z':0.},'sourceRotation':e.vector(t,'m_LocalRotation'),'sourceScale':scale,'sourceActive':bool(int(e.required(objects[oid],'m_IsActive'))),'rendererEnabled':bool(int(e.required(r,'m_Enabled'))),'duration':float(e.required(p,'lengthInSec')),'delay':constant(p,'startDelay'),'lifetime':constant(init,'startLifetime'),'speed':constant(init,'startSpeed'),'size':constant(init,'startSize'),'rotation':constant(init,'startRotation'),'rate':constant(emission,'rateOverTime'),'maxActive':int(e.required(init,'maxNumParticles')),'sourceStopAction':int(e.required(p,'stopAction')),'sourceSimulationSpace':int(e.required(p,'moveWithTransform')),'sourceScalingMode':int(e.required(p,'scalingMode')),'enabledModules':enabled,'sizeCurve':[[0,1],[1,1]],'shape':{'type':-1,'radius':0,'scale':{'x':1,'y':1,'z':1},'angle':0},'sprite':None,'material':e.required(r,'m_Materials')}
  col=block(init,'startColor')
  if int(e.required(col,'minMaxState'))!=0:raise ValueError('random/gradient initial color needs adapter')
  record['color']=e.vector(col,'maxColor')
  if record['sourceScalingMode']!=1 or e.reference(p,'moveWithCustomTransform')!=0:raise ValueError('source scaling/custom space requires adapter')
  if 'ShapeModule' in enabled:
   ty=int(e.required(shape,'type'))
   if ty not in (0,4) or e.vector(shape,'m_Position')!={'x':0.,'y':0.,'z':0.} or e.vector(shape,'m_Rotation')!={'x':0.,'y':0.,'z':0.}:raise ValueError('unsupported shape type/offset')
   for k in ['randomDirectionAmount','sphericalDirectionAmount','randomPositionAmount']:
    if float(e.required(shape,k))!=0:raise ValueError('unsupported shape '+k)
   record['shape']={'type':ty,'radius':float(e.required(block(shape,'radius'),'value')),'scale':e.vector(shape,'m_Scale'),'angle':float(e.required(shape,'angle'))}
  if 'SizeModule' in enabled:
   curve=block(size,'curve')
   if e.required(size,'separateAxes')!='0' or int(e.required(curve,'minMaxState'))!=1:raise ValueError('unsupported size mode')
   keys=[]
   for b in re.split(r'^        - serializedVersion: 3\n',block(curve,'maxCurve'),flags=re.M)[1:]:
    v={k:float(e.required(b,k)) for k in ['time','value','inSlope','outSlope','weightedMode']};keys.append(v)
   if len(keys)!=2 or keys[0]['time']!=0 or keys[1]['time']!=1 or any(v['weightedMode'] for v in keys):raise ValueError('unsupported size curve')
   slope=keys[1]['value']-keys[0]['value']
   if keys[0]['outSlope']!=slope or keys[1]['inSlope']!=slope:raise ValueError('nonlinear source size curve needs sampling adapter')
   factor=float(e.required(curve,'scalar'));record['sizeCurve']=[[v['time'],v['value']*factor] for v in keys]
  if record['rendererEnabled']:
   if e.required(r,'m_RenderMode')!='0' or 'UVModule' not in enabled or e.required(uv,'mode')!='1':raise ValueError('only original sprite billboards supported')
   if e.required(uv,'flipU')!='0' or e.required(uv,'flipV')!='0':raise ValueError('particle UV flip requires adapter')
   refs=re.findall(r'sprite: \{fileID: 21300000, guid: ([0-9a-f]{32}),',uv)
   if len(refs)!=1:raise ValueError('multi-sprite particle sheet needs native atlas adapter')
   g=refs[0]
   if g not in sprite_cache:
    source=resolve(g);sp=read(source);texg=re.search('guid: ([0-9a-f]{32})',e.required(sp,'texture'))[1];tex=resolve(texg);used.add(tex);png=tex.read_bytes();import struct;image_width,image_height=struct.unpack('>II',png[16:24])
    rect=block(sp,'textureRect');v={k:float(e.required(rect,k)) for k in ['x','y','width','height']}
    if any(abs(x-round(x))>1e-3 for x in v.values()):raise ValueError('fractional particle sprite crop unsupported')
    x,y,w,h=(round(v[k]) for k in ['x','y','width','height'])
    logical=block(sp,'m_Rect')
    if float(e.required(logical,'width'))!=w or float(e.required(logical,'height'))!=h or e.vector(sp,'m_Pivot')!={'x':.5,'y':.5} or e.vector(sp,'textureRectOffset')!={'x':0.,'y':0.}:raise ValueError('particle sprite pivot/trim requires canvas adapter')
    dest=output/'textures'/(g+'.png');dest.parent.mkdir(exist_ok=True);crop_png(tex,dest,x,image_height-y-h,w,h)
    sprite_cache[g]={'sourceGuid':g,'source':source.relative_to(assets).as_posix(),'sourceTexture':tex.relative_to(assets).as_posix(),'sourceTextureSha256':hashlib.sha256(tex.read_bytes()).hexdigest(),'texture':dest.relative_to(output).as_posix(),'textureSha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'width':w,'height':h,'pixelsPerUnit':float(e.required(sp,'m_PixelsToUnits')),'sourceSprite':sp}
   record['sprite']=sprite_cache[g]
   # All source UV frames select the single exact sprite: timing is invariant.
   record['uvPolicy']='source single-sprite sequence invariant'
   material_refs=re.findall(r'fileID: (\d+), guid: ([0-9a-f]{32})',block(r,'m_Materials'))
   if len(material_refs)!=1:raise ValueError('ambiguous source material')
   fid,mg=material_refs[0]
   if mg=='0000000000000000f000000000000000':
    if int(fid)!=10308:raise ValueError('unsupported builtin material')
    record['materialAdapter']='Unity built-in Default-Particle10308 -> native alpha blend; HDR/soft intersection unavailable'
   else:
    mat=resolve(mg);mt=read(mat)
    shader=re.search(r'guid: ([0-9a-f]{32})',e.required(mt,'m_Shader'))[1]
    if shader!='9b96bf551cb095c48a0cf08916b3d634':raise ValueError('unsupported source particle shader '+shader)
    shaderpath=resolve(shader);read(shaderpath);record['shaderSource']=shaderpath.relative_to(assets).as_posix();record['shaderSha256']=hashlib.sha256(shaderpath.read_bytes()).hexdigest()
    record['materialSource']=mat.relative_to(assets).as_posix();record['materialSha256']=hashlib.sha256(mat.read_bytes()).hexdigest();record['serializedMaterial']=mt;record['materialAdapter']='source particle material -> native alpha blend; original RGB/alpha/sprite preserved; shader/HDR edge bounded'
  emitters.append(record)
 for p in sorted(used):sources.append({'path':p.relative_to(assets).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
 result={'schema':1,'sourceKind':source_id,'sourcePrefab':prefab,'sourcePrefabSha256':hashlib.sha256((assets/prefab).read_bytes()).hexdigest(),'nativeAdapter':'TodParticleSystem17-emitter source-compatible subset','emitters':emitters,'birthPoints':[r['sourcePosition'] for r in emitters[1:9]],'sources':sources,'ignoredBindings':[],'deferredComponents':[],'limits':['Native2D sphere/cone projection and random distribution differ from Unity3D','Native10ms quantization and stopAction child-tree shutdown not Unity runtime verified','Shader HDR/soft-particle material edge behavior bounded; native alpha blend','Source single-sprite sheet timing invariant; nonlinear curves rejected']}
 (output/'effect.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');return result

if __name__=='__main__':
 import argparse
 p=argparse.ArgumentParser();p.add_argument('assets');p.add_argument('prefab');p.add_argument('output');p.add_argument('--source-id',type=int,required=True);a=p.parse_args();q=export(pathlib.Path(a.assets),a.prefab,pathlib.Path(a.output),a.source_id);print('Source effect',q['sourceKind'],'strict native-subset emitters',len(q['emitters']),'birthPoints',len(q['birthPoints']))
