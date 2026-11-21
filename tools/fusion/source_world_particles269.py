"""Source-exact extended particle metadata for native Tod: gradients, ranges, burst, speed lifetime."""
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

def range_value(text,key):
 b=block(text,key);mode=int(e.required(b,'minMaxState'))
 if mode not in (0,3):raise ValueError('unsupported initial range '+key)
 hi=float(e.required(b,'scalar'));lo=float(e.required(b,'minScalar')) if mode==3 else hi
 if not all(math.isfinite(v) and v>=0 for v in [lo,hi]):raise ValueError('invalid initial range '+key)
 if lo>hi:lo,hi=hi,lo
 return [lo,hi]

def scalar_curve(text,curve_name='maxCurve'):
 mode=int(e.required(text,'minMaxState'));scale=float(e.required(text,'scalar'))
 if mode==0:return [[0,scale,0,0],[1,scale,0,0]]
 if mode not in (1,2):raise ValueError('unsupported speed curve mode')
 keys=[]
 for b in re.split(r'^        - serializedVersion: 3\n',block(text,curve_name),flags=re.M)[1:]:
  if int(e.required(b,'weightedMode')):raise ValueError('weighted speed curve unsupported')
  keys.append([float(e.required(b,k))*(scale if k!='time' else 1) for k in ['time','value','inSlope','outSlope']])
 if len(keys)<2 or keys[0][0]<0 or keys[-1][0]>1 or any(not math.isfinite(v) for k in keys for v in k):raise ValueError('invalid source scalar curve')
 if keys[0][0]>0:keys.insert(0,[0,keys[0][1],0,0])
 if keys[-1][0]<1:keys.append([1,keys[-1][1],0,0])
 return keys

def velocity_curve(text):
 for name in ['orbitalX','orbitalY','orbitalOffsetX','orbitalOffsetY','orbitalOffsetZ','radial']:
  if constant(text,name)!=0:raise ValueError('nonzero velocity axis/orbit needs native vector adapter '+name)
 return scalar_curve(block(text,'speedModifier'))

def color_curve(text):
 g=block(text,'gradient')
 if int(e.required(g,'minMaxState'))!=1:raise ValueError('unsupported color-over-lifetime mode')
 g=block(g,'maxGradient')
 mode=int(e.required(g,'m_Mode'))
 if mode not in (0,1,2):raise ValueError('unsupported source gradient interpolation')
 c=[];a=[]
 for i in range(int(e.required(g,'m_NumColorKeys'))):
  v=e.vector(g,'key'+str(i));c.append([int(e.required(g,'ctime'+str(i)))/65535,v['r'],v['g'],v['b']])
 for i in range(int(e.required(g,'m_NumAlphaKeys'))):
  v=e.vector(g,'key'+str(i));a.append([int(e.required(g,'atime'+str(i)))/65535,v['a']])
 def sample(keys,t,n):
  if t<=keys[0][0]:return keys[0][n]
  for before,after in zip(keys,keys[1:]):
   if t<after[0] or mode!=1 and t==after[0]:
    if after[0]<=before[0]:raise ValueError('non-increasing source gradient time')
    if mode==1:return before[n]
    f=(t-before[0])/(after[0]-before[0]);return before[n]+(after[n]-before[n])*f
  return keys[-1][n]
 if not c or not a or len(c)>8 or len(a)>8:raise ValueError('invalid source gradient')
 return [[t]+[sample(c,t,i) for i in [1,2,3]]+[sample(a,t,1)] for t in sorted({0.,1.}|{v[0] for v in c+a})]

def initial_color(text):
 mode=int(e.required(text,'minMaxState'))
 if mode not in (0,2):raise ValueError('initial gradient/time color needs birth-time adapter')
 colors=[e.vector(text,'maxColor')] if mode==0 else [e.vector(text,'minColor'),e.vector(text,'maxColor')]
 if any(set(c)!={'r','g','b','a'} or any(not math.isfinite(v) or v<0 or v>64 for v in c.values()) for c in colors):raise ValueError('invalid initial source RGBA color')
 return colors


def pad_sprite(path,w,h,lw,lh,pivot,offset):
 import struct,zlib,binascii
 cx,cy=pivot['x']*lw,pivot['y']*lh
 nw,nh=math.ceil(2*max(cx,lw-cx)),math.ceil(2*max(cy,lh-cy))
 ox=round(nw/2-cx+offset['x']);oy=round(nh/2+cy-offset['y']-h)
 if min(ox,oy)<0 or ox+w>nw or oy+h>nh:raise ValueError('source sprite trim outside restored canvas')
 if nw==w and nh==h and ox==0 and oy==0:return w,h
 raw=path.read_bytes();data=bytearray();pos=8
 while pos<len(raw):
  n=struct.unpack_from('>I',raw,pos)[0]
  if raw[pos+4:pos+8]==b'IDAT':data+=raw[pos+8:pos+8+n]
  pos+=n+12
 d=zlib.decompress(data)
 if len(d)!=(w*4+1)*h or any(d[y*(w*4+1)] for y in range(h)):raise ValueError('invalid source sprite crop for canvas')
 canvas=bytearray((nw*4+1)*nh)
 for y in range(h):canvas[(y+oy)*(nw*4+1)+1+ox*4:(y+oy)*(nw*4+1)+1+(ox+w)*4]=d[y*(w*4+1)+1:(y+1)*(w*4+1)]
 def chunk(k,d):return struct.pack('>I',len(d))+k+d+struct.pack('>I',binascii.crc32(k+d)&0xffffffff)
 path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',nw,nh,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(canvas,9))+chunk(b'IEND',b''))
 return nw,nh


def pack_frames(output,frames,result):
 import struct,zlib,binascii
 w,h=max(f['width'] for f in frames),max(f['height'] for f in frames)
 if any(f['pixelsPerUnit']!=frames[0]['pixelsPerUnit'] for f in frames):raise ValueError('source sprite sequence PPU differs')
 decoded=[]
 for frame in frames:
  raw=(output/frame['texture']).read_bytes();data=bytearray();pos=8
  while pos<len(raw):
   n=struct.unpack_from('>I',raw,pos)[0]
   if raw[pos+4:pos+8]==b'IDAT':data+=raw[pos+8:pos+8+n]
   pos+=n+12
  d=zlib.decompress(data)
  fw,fh=frame['width'],frame['height']
  if len(d)!=(fw*4+1)*fh or any(d[y*(fw*4+1)] for y in range(fh)):raise ValueError('invalid lossless source frame crop')
  canvas=bytearray((w*4+1)*h);ox,oy=(w-fw)//2,(h-fh)//2
  for y in range(fh):canvas[(y+oy)*(w*4+1)+1+ox*4:(y+oy)*(w*4+1)+1+(ox+fw)*4]=d[y*(fw*4+1)+1:(y+1)*(fw*4+1)]
  decoded.append(canvas)
 pixels=b''.join(b'\0'+b''.join(d[y*(w*4+1)+1:(y+1)*(w*4+1)] for d in decoded) for y in range(h))
 def chunk(k,d):return struct.pack('>I',len(d))+k+d+struct.pack('>I',binascii.crc32(k+d)&0xffffffff)
 raw=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w*len(frames),h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(pixels,9))+chunk(b'IEND',b'')
 path=output/'textures'/('sheet-'+hashlib.sha256(raw).hexdigest()[:16]+'.png');path.write_bytes(raw)
 result.update(texture=path.relative_to(output).as_posix(),textureSha256=hashlib.sha256(raw).hexdigest(),width=w*len(frames),height=h,frameWidth=w)


def signed_range(text,key):
 b=block(text,key);mode=int(e.required(b,'minMaxState'))
 if mode not in (0,3):raise ValueError('unsupported source signed range '+key)
 hi=float(e.required(b,'scalar'));lo=float(e.required(b,'minScalar')) if mode==3 else hi
 if not all(math.isfinite(v) for v in [lo,hi]):raise ValueError('nonfinite signed range')
 return sorted([lo,hi])

def range_curve(text):
 mode=int(e.required(text,'minMaxState'))
 if mode in (0,3):
  hi=float(e.required(text,'scalar'));lo=float(e.required(text,'minScalar')) if mode==3 else hi;lo,hi=sorted([lo,hi])
  return [[[0,lo,0,0],[1,lo,0,0]],[[0,hi,0,0],[1,hi,0,0]]]
 if mode==1:
  curve=scalar_curve(text);return [curve,curve]
 if mode==2:return [scalar_curve(text,'minCurve'),scalar_curve(text,'maxCurve')]
 raise ValueError('unsupported force/rotation curve mode')

def collision_profile(text):
 if int(e.required(text,'type'))!=0 or int(e.required(text,'collisionMode'))!=0 or float(e.required(text,'colliderForce'))!=0:
  raise ValueError('only source Plane/3D collision without rigid-body force supported')
 planes=re.findall(r'fileID: (-?\d+)',block(text,'m_Planes'))
 if not planes or any(int(x)!=0 for x in planes):raise ValueError('collision needs actual runtime row-plane binding; prefab external planes unsupported')
 if int(e.required(text,'collisionMessages'))!=1 or int(e.required(text,'interiorCollisions'))!=0:raise ValueError('unsupported source collision callback/interior mode')
 out={'type':0,'mode':0,'planeBinding':'sourceZombieRowPlane','colliderForce':0,'collisionMessages':True,
      'damping':constant(text,'m_Dampen'),'bounce':constant(text,'m_Bounce'),'lifetimeLoss':constant(text,'m_EnergyLossOnCollision'),
      'minimumSpeed':float(e.required(text,'minKillSpeed')),'maximumSpeed':float(e.required(text,'maxKillSpeed')),'radiusScale':float(e.required(text,'radiusScale')),
      'callback':'ZombieHead.OnParticleCollision: stop RotationOverLifetime/emission, bounce=0 after first contact',
      'sourceCallback':'0x1803F58E0','sourcePlaneAssignment':'Zombie.AnimLoseActive 0x18037EF60, Board.plane[row]/planeRoof[row]'}
 if any(not math.isfinite(out[k]) or not 0<=out[k]<=1 for k in ['damping','bounce','lifetimeLoss']):raise ValueError('invalid collision response')
 if not 0<=out['minimumSpeed']<=out['maximumSpeed']<=10000 or not 0<=out['radiusScale']<=10:raise ValueError('invalid collision speed/radius')
 return out

def force_curves(text):
 return {'axes':[range_curve(block(text,k)) for k in ['x','y','z']],'world':bool(int(e.required(text,'inWorldSpace'))),'randomizePerFrame':bool(int(e.required(text,'randomizePerFrame')))}

def rotation_curve(text):
 if e.required(text,'separateAxes')!='0':raise ValueError('source3D rotation over lifetime unsupported')
 return range_curve(block(text,'curve'))


def projected_rectangle(assets,prefab,trans,objects,tid,shape,pose_model):
 if pose_model is None:raise ValueError('rectangle projection needs complete source model pose proof')
 model_path=pathlib.Path(pose_model);model=json.loads(model_path.read_text(encoding='utf-8-sig'))
 source_hash=hashlib.sha256((assets/prefab).read_bytes()).hexdigest()
 if model['sourcePrefab']!=prefab or not any(s['path']==prefab and s['sha256']==source_hash for s in model['sources']):raise ValueError('rectangle model source hash differs')
 def mul(a,b):return [[sum(a[r][k]*b[k][c] for k in range(3)) for c in range(3)] for r in range(3)]
 def rotation(q):
  x,y,z,w=[q[k] for k in ['x','y','z','w']];n=math.sqrt(x*x+y*y+z*z+w*w)
  if not math.isfinite(n) or n<1e-8:raise ValueError('invalid rectangle source quaternion')
  x,y,z,w=[v/n for v in [x,y,z,w]]
  return [[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]]
 chain=[];cursor=tid
 while cursor:
  t=trans[cursor];chain.append((cursor,t));cursor=e.reference(t,'m_Father')
 chain.reverse();paths=[];records=[];basis=[[1.,0.,0.],[0.,1.,0.],[0.,0.,1.]]
 for index,(fileid,t) in enumerate(chain):
  paths.append('' if index==0 else '/'.join([e.required(objects[e.reference(v,'m_GameObject')],'m_Name')for _,v in chain[1:index+1]]))
  q=e.vector(t,'m_LocalRotation');scale=e.vector(t,'m_LocalScale');records.append({'position':e.vector(t,'m_LocalPosition'),'rotation':q,'scale':scale})
  basis=mul(basis,rotation(q))
 for clip in model['clips'].values():
  for curve in clip['curves']:
   if curve['path'] in paths and curve['kind'] in ('euler','rotation','quaternion'):raise ValueError('animated rectangle ancestor rotation requires live basis adapter')
   if curve['path']==paths[-1] and curve['kind']=='scale':raise ValueError('animated rectangle PS local scale requires live basis adapter')
 local=records[-1]['scale'];basis=[[basis[r][c]*local['xyz'[c]] for c in range(3)] for r in range(3)]
 scale=e.vector(shape,'m_Scale');half=[scale['x']*.5,scale['y']*.5]
 if min(half)<=0 or scale['z']!=0:raise ValueError('invalid source rectangle dimensions')
 axes=[[basis[r][i]*half[i] for r in range(2)]for i in range(2)]
 for x,y in axes:
  if abs(x)>1e-6 and abs(y)>1e-6:raise ValueError('skew/rotated rectangle projection unsupported')
 if sum(abs(v[0])>1e-6 for v in axes)>1 or sum(abs(v[1])>1e-6 for v in axes)>1:raise ValueError('overlapping rectangle projection coordinates unsupported')
 launch=[basis[0][2],basis[1][2]]
 if math.hypot(*launch)<1e-6:raise ValueError('rectangle launch normal has zero native projection')
 return {'basis':basis,'halfSize':half,'normal':[0,0,1],'scalingMode':1,'ancestorTrs':records,'staticAncestorRotations':True,'staticParticleScale':True,'offset':[0,0,0],'shapeRotation':[0,0,0],'sourceModelSha256':hashlib.sha256(model_path.read_bytes()).hexdigest(),'sourcePrefabSha256':source_hash,'ancestorPaths':paths,'sourceProjectionPolicy':'Unity Local1: full ancestor rotation, only PS own local scale; actual node point supplies translation','sourceScalingModeReference':'https://docs.unity3d.com/kr/6000.0/ScriptReference/ParticleSystemScalingMode.Local.html'}


def export(assets,prefab,output,source_id,scope_ids=None,pose_model=None):
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
 if scope_ids is None and any(k not in allowed for k,_,_ in docs):raise ValueError('unsupported source effect component')
 objects={i:t for k,i,t in docs if k==1};trans={i:t for k,i,t in docs if k==4}
 root=next(i for i,t in trans.items() if not e.reference(t,'m_Father'))
 children=[int(x) for x in re.findall(r'fileID: (-?\d+)',block(trans[root],'m_Children'))]
 if scope_ids is None and len(trans)!=len(children)+1:raise ValueError('nested particle transform hierarchy needs composed export')
 root_scale=e.vector(trans[root],'m_LocalScale') if scope_ids is None else {'x':1.,'y':1.,'z':1.}
 if max(root_scale.values())-min(root_scale.values())>1e-6 or min(root_scale.values())<=0 or (scope_ids is None and e.vector(trans[root],'m_LocalRotation')!={'x':0.,'y':0.,'z':0.,'w':1.}):raise ValueError('nonuniform/rotated source root requires composed projection')
 ps={e.reference(t,'m_GameObject'):t for k,_,t in docs if k==198};render={e.reference(t,'m_GameObject'):t for k,_,t in docs if k==199}
 emitters=[];sprite_cache={};sources=[]
 selected=[i for i,t in trans.items() if any(k==198 and j in scope_ids and e.reference(v,'m_GameObject')==e.reference(t,'m_GameObject') for k,j,v in docs)] if scope_ids is not None else [root]+children
 for ordinal,tid in enumerate(selected):
  if e.reference(trans[tid],'m_GameObject') not in ps:continue
  t=trans[tid];oid=e.reference(t,'m_GameObject');p=ps[oid];r=render[oid]
  enabled=[]
  for module in re.findall(r'^  ([A-Z]\w+Module):',p,re.M):
   if e.field(block(p,module),'enabled')=='1':enabled.append(module)
  supported={'InitialModule','EmissionModule','ShapeModule','SizeModule','UVModule','ColorModule','VelocityModule','ForceModule','RotationModule','CollisionModule'}
  if set(enabled)-supported:raise ValueError('unsupported enabled modules '+str(set(enabled)-supported))
  if e.required(p,'simulationSpeed')!='1' or e.required(p,'useUnscaledTime')!='0':raise ValueError('source loop/time module needs adapter')
  init=block(p,'InitialModule');emission=block(p,'EmissionModule');shape=block(p,'ShapeModule');uv=block(p,'UVModule');size=block(p,'SizeModule')
  if e.required(init,'size3D')!='0' or e.required(init,'rotation3D')!='0' or constant(init,'gravityModifier')!=0:raise ValueError('unsupported initial3D/gravity')
  if constant(emission,'rateOverDistance')!=0:raise ValueError('distance emission needs adapter')
  bursts=[]
  if int(e.required(emission,'m_BurstCount')):
   b=block(emission,'m_Bursts')
   for burst in re.split(r'^    - serializedVersion: 2\n',b,flags=re.M)[1:]:
    if float(e.required(burst,'time'))!=0 or int(e.required(burst,'cycleCount'))!=1 or float(e.required(burst,'probability'))!=1:raise ValueError('only deterministic birth bursts supported')
    bursts.append(constant(burst,'countCurve'))
   if len(bursts)!=int(e.required(emission,'m_BurstCount')):raise ValueError('burst count differs')
  scale={k:v*root_scale['x'] for k,v in e.vector(t,'m_LocalScale').items()} if tid!=root else root_scale
  if min(scale.values())<=0:raise ValueError('invalid source particle local scale')
  record={'nodeName':e.required(objects[oid],'m_Name'),'nodeIndex':ordinal-1,'sourceTransformFileId':tid,'sourcePosition':{k:v*root_scale['x'] for k,v in e.vector(t,'m_LocalPosition').items()} if scope_ids is None and tid!=root else {'x':0.,'y':0.,'z':0.},'sourceRotation':e.vector(t,'m_LocalRotation'),'sourceScale':scale,'sourceActive':True if scope_ids is not None else bool(int(e.required(objects[oid],'m_IsActive'))),'rendererEnabled':bool(int(e.required(r,'m_Enabled'))),'duration':float(e.required(p,'lengthInSec')),'delay':constant(p,'startDelay'),'lifetime':range_value(init,'startLifetime')[1],'speed':range_value(init,'startSpeed')[1],'size':range_value(init,'startSize')[1],'rotation':signed_range(init,'startRotation')[1],'rate':constant(emission,'rateOverTime'),'maxActive':int(e.required(init,'maxNumParticles')),'sourceStopAction':int(e.required(p,'stopAction')),'sourceSimulationSpace':int(e.required(p,'moveWithTransform')),'sourceScalingMode':int(e.required(p,'scalingMode')),'enabledModules':enabled,'sizeCurve':[[0,1],[1,1]],'shape':{'type':-1,'radius':0,'scale':{'x':1,'y':1,'z':1},'angle':0},'sprite':None,'material':e.required(r,'m_Materials')}
  col=block(init,'startColor')
  colors=initial_color(col);record['color']=colors[-1]
  if len(colors)==2:
   record['initialColorRange']=colors;record['serializedInitialColor']=col
   record['initialColorPolicy']='source two-color endpoints; native birth channel ranges, Unity coupled RGBA random factor not reproduced'
  record['looping']=bool(int(e.required(p,'looping')));record['lifetimeRange']=range_value(init,'startLifetime');record['speedRange']=range_value(init,'startSpeed');record['rotationRange']=signed_range(init,'startRotation');record['emissionEnabled']='EmissionModule' in enabled;record['sourceEmissionRate']=record['rate'];record['rate']=record['rate'] if record['emissionEnabled'] else 0;record['forceCurves']=force_curves(block(p,'ForceModule')) if 'ForceModule' in enabled else None;record['spinCurves']=rotation_curve(block(p,'RotationModule')) if 'RotationModule' in enabled else None
  record['sizeRange']=range_value(init,'startSize');record['burstCount']=sum(bursts);record['colorCurve']=color_curve(block(p,'ColorModule')) if 'ColorModule' in enabled else [[0,1,1,1,1],[1,1,1,1,1]]
  if 'ColorModule' in enabled and int(e.required(block(block(block(p,'ColorModule'),'gradient'),'maxGradient'),'m_Mode'))==1:record['colorInterpolation']='fixed'
  if 'ColorModule' in enabled and int(e.required(block(block(block(p,'ColorModule'),'gradient'),'maxGradient'),'m_Mode'))==2:
   record['nativeGradientCompatibility']='PerceptualBlend source keys retained; native linear RGB/alpha projection, not perceptual color-space parity; constant RGB remains invariant'
  if 'CollisionModule' in enabled:
   record['collision']=collision_profile(block(p,'CollisionModule'));record['serializedCollisionModule']=block(p,'CollisionModule')
  record['speedCurve']=velocity_curve(block(p,'VelocityModule')) if 'VelocityModule' in enabled else [[0,1,0,0],[1,1,0,0]]
  if 'VelocityModule' in enabled:
   velocity=block(p,'VelocityModule');record['velocityCurves']={'axes':[range_curve(block(velocity,k)) for k in ['x','y','z']],'orbitalZ':range_curve(block(velocity,'orbitalZ')),'world':bool(int(e.required(velocity,'inWorldSpace')))}
  record['serializedSourceParticleSystem']=p
  if record['sourceScalingMode']!=1 or e.reference(p,'moveWithCustomTransform')!=0:raise ValueError('source scaling/custom space requires adapter')
  if 'ShapeModule' in enabled:
   ty=int(e.required(shape,'type'))
   random=float(e.required(shape,'randomDirectionAmount'));compat=ty==4 and random==1
   if ty not in (0,4,5,10,18) or not compat and(e.vector(shape,'m_Position')!={'x':0.,'y':0.,'z':0.} or e.vector(shape,'m_Rotation')!={'x':0.,'y':0.,'z':0.}):raise ValueError('unsupported shape type/offset')
   for k in ['randomDirectionAmount','sphericalDirectionAmount','randomPositionAmount']:
    if float(e.required(shape,k))!=0 and not(compat and k=='randomDirectionAmount'):raise ValueError('unsupported shape '+k)
   record['shape']={'type':ty,'radius':float(e.required(block(shape,'radius'),'value')),'scale':e.vector(shape,'m_Scale'),'angle':float(e.required(shape,'angle'))}
   if compat:
    record['shape'].update(position=e.vector(shape,'m_Position'),rotation=e.vector(shape,'m_Rotation'),randomDirectionAmount=1,
     nativeCompatibility='source Cone4 with fully randomized direction -> native2D CIRCLE radial/angular projection; source 3D cone/random coupling and projected ellipse differ, actual offset/rotation/scale retained')
   if ty==18:record['shape']['projectedRectangle']=projected_rectangle(assets,prefab,trans,objects,tid,shape,pose_model)
   if ty==10:
    arc=block(shape,'arc')
    if record['speed']!=0 or e.vector(t,'m_LocalRotation')!={'x':0.,'y':0.,'z':0.,'w':1.} or float(e.required(shape,'radiusThickness'))!=1 or float(e.required(arc,'value'))!=360 or int(e.required(arc,'mode'))!=0 or float(e.required(arc,'spread'))!=0:raise ValueError('circle particle requires full static disk/identity/zero launch speed')
    record['shape'].update(arc=360,radiusThickness=1,nativePolicy='source Circle10 full stationary disk -> native CIRCLE; native radial RNG distribution')
  if 'SizeModule' in enabled:
   if e.required(size,'separateAxes')!='0':raise ValueError('separate size axes unsupported')
   record['sizeHermite']=scalar_curve(block(size,'curve'))
  if record['rendererEnabled']:
   if e.required(r,'m_RenderMode')!='0':raise ValueError('unsupported nonbillboard source particle renderer')
   if 'UVModule' not in enabled:
    mats=re.findall(r'fileID: (\d+), guid: ([0-9a-f]{32})',block(r,'m_Materials'))
    if len(mats)!=1:raise ValueError('ambiguous material texture particle')
    mat=resolve(mats[0][1]);mt=read(mat);texture=block(block(block(mt,'m_SavedProperties'),'m_TexEnvs'),'_MainTex')
    if e.vector(texture,'m_Scale')!={'x':1.,'y':1.} or e.vector(texture,'m_Offset')!={'x':0.,'y':0.}:raise ValueError('source material texture transform needs adapter')
    g=re.search(r'guid: ([0-9a-f]{32})',e.required(texture,'m_Texture'))[1];tex=resolve(g);used.add(tex);raw=tex.read_bytes()
    if raw[:8]!=b'\x89PNG\r\n\x1a\n':raise ValueError('source MainTex is not original PNG')
    import struct
    width,height=struct.unpack('>II',raw[16:24]);dest=output/'textures'/(g+'.png');dest.parent.mkdir(exist_ok=True);shutil.copyfile(tex,dest)
    record['sprite']={'sourceGuid':g,'sourceTexture':tex.relative_to(assets).as_posix(),'sourceTextureSha256':hashlib.sha256(raw).hexdigest(),'texture':dest.relative_to(output).as_posix(),'textureSha256':hashlib.sha256(raw).hexdigest(),'width':width,'height':height,'normalizationWidth':width,'frames':1,'sourcePivot':{'x':.5,'y':.5},'pixelProjection':'actual source MainTex, particle billboard centered normalised width'}
    record['spriteFrames']=[];record['uvPolicy']='source UVModule disabled: original material MainTex one full frame'
   else:
    if e.required(uv,'mode')!='1':raise ValueError('unsupported particle sprite-sheet mode')
    if e.required(uv,'flipU')!='0' or e.required(uv,'flipV')!='0':raise ValueError('particle UV flip requires adapter')
    refs=re.findall(r'sprite: \{fileID: 21300000, guid: ([0-9a-f]{32}),',uv)
    if not refs or len(refs)>16:raise ValueError('invalid source particle sprite sequence')
    record['serializedUVModule']=uv
    for g in refs:
     if g not in sprite_cache:
      source=resolve(g);sp=read(source);texg=re.search('guid: ([0-9a-f]{32})',e.required(sp,'texture'))[1];tex=resolve(texg);used.add(tex);png=tex.read_bytes();import struct;image_width,image_height=struct.unpack('>II',png[16:24])
      rect=block(sp,'textureRect');v={k:float(e.required(rect,k)) for k in ['x','y','width','height']}
      native_rect=dict(v)
      x,y=math.floor(v['x']+1e-4),math.floor(v['y']+1e-4);w,h=math.ceil(v['x']+v['width']-1e-4)-x,math.ceil(v['y']+v['height']-1e-4)-y
      logical=block(sp,'m_Rect')
      logical_width=float(e.required(logical,'width'));logical_height=float(e.required(logical,'height'));pivot=e.vector(sp,'m_Pivot');offset=e.vector(sp,'textureRectOffset')
      if min(logical_width,logical_height)<=0 or not all(0<=v<=1 for v in pivot.values()):raise ValueError('invalid source particle sprite canvas')
      dest=output/'textures'/(g+'.png');dest.parent.mkdir(exist_ok=True);crop_png(tex,dest,x,image_height-y-h,w,h)
      native_width,native_height=pad_sprite(dest,w,h,logical_width,logical_height,pivot,offset)
      sprite_cache[g]={'normalizationWidth':logical_width,'sourcePivot':pivot,'sourceTrimOffset':offset,'sourceLogicalSize':[logical_width,logical_height],'sourceGuid':g,'source':source.relative_to(assets).as_posix(),'sourceTexture':tex.relative_to(assets).as_posix(),'sourceTextureSha256':hashlib.sha256(tex.read_bytes()).hexdigest(),'texture':dest.relative_to(output).as_posix(),'textureSha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'width':native_width,'height':native_height,'pixelsPerUnit':float(e.required(sp,'m_PixelsToUnits')),'sourceSprite':sp,'sourceTextureRect':native_rect,'nativePixelBounds':[x,y,w,h],'pixelQuantizationLimit':'conservative source-texel rectangle; <1px each edge; original source rectangle retained'}
    record['spriteFrames']=[sprite_cache[g] for g in refs]
    record['sprite']=dict(sprite_cache[refs[0]])
    if len(refs)>1:
     if float(e.required(uv,'cycles'))!=1 or range_value(uv,'startFrame') not in ([0.,.9999],[0.,0.],[0.,1.]):raise ValueError('unsupported source sprite sequence cycles/start')
     frame=scalar_curve(block(uv,'frameOverTime'))
     if len(frame)!=2 or abs(frame[0][1])>1e-6 or abs(frame[1][1]-.9999)>1e-6:raise ValueError('unsupported source frame-over-time curve')
     pack_frames(output,record['spriteFrames'],record['sprite'])
    record['sprite']['frames']=len(refs)
    # All source UV frames select the single exact sprite: timing is invariant.
    record['uvPolicy']='source single-sprite invariant' if len(refs)==1 else 'native lifetime animation of every source frame; source random start phase bounded to frame0'
   material_refs=re.findall(r'fileID: (\d+), guid: ([0-9a-f]{32})',block(r,'m_Materials'))
   if len(material_refs)!=1:raise ValueError('ambiguous source material')
   fid,mg=material_refs[0]
   if mg=='0000000000000000f000000000000000':
    if int(fid)==10308:record['materialAdapter']='Unity built-in Default-Particle10308 -> native alpha blend; HDR/soft intersection unavailable'
    elif int(fid)==10754:
     if scope_ids is None or 'UVModule' not in enabled or not record.get('spriteFrames'):raise ValueError('builtin10754 needs original decoded source sprite sequence')
     record['materialAdapter']='Explicit native-alpha compatibility projection of original UV sprites; builtin10754 shader pipeline/uniforms unavailable, not equivalent to10308 and not source blend parity'
     record['sourceBuiltinMaterial']={'fileId':10754,'guid':mg,'pipelineDecoded':False,'imageAuthority':'original enabled UVModule sprite sequence; every original texture/frame and source PS retained','nativeTintPolicy':'identity material projection; source particle initial color/alpha and lifetime gradients retained; original builtin uniforms unavailable'}
    else:raise ValueError('unsupported builtin material')
   else:
    mat=resolve(mg);mt=read(mat)
    shader=re.search(r'guid: ([0-9a-f]{32})',e.required(mt,'m_Shader'))[1]
    if shader=='0000000000000000f000000000000000':
     if int(re.search(r'fileID: (\d+)',e.required(mt,'m_Shader'))[1])!=10720:raise ValueError('unsupported builtin material shader')
     record['shaderSource']='UnityBuiltinParticle10720';record['shaderAdapter']='original material MainTex/color retained; native alpha billboard replaces builtin shader opaque/depth pipeline'
    elif shader=='9b96bf551cb095c48a0cf08916b3d634':
     shaderpath=resolve(shader);read(shaderpath);record['shaderSource']=shaderpath.relative_to(assets).as_posix();record['shaderSha256']=hashlib.sha256(shaderpath.read_bytes()).hexdigest()
    elif shader=='bec799651b2ac204aa54e4b93aaa2d42':
     saved=block(mt,'m_SavedProperties');floats=block(saved,'m_Floats');colors=block(saved,'m_Colors');texture=block(block(saved,'m_TexEnvs'),'_MainTex')
     if float(e.required(floats,'_Brightness'))!=1 or float(e.required(floats,'_Transparent'))!=1 or colors.strip() not in ('','{}'):raise ValueError('Brightness native-alpha policy requires identity brightness/transparency/no material tint')
     if e.vector(texture,'m_Scale')!={'x':1.,'y':1.} or e.vector(texture,'m_Offset')!={'x':0.,'y':0.}:raise ValueError('Brightness material UV transform requires adapter')
     main_guid=re.search(r'guid: ([0-9a-f]{32})',e.required(texture,'m_Texture'))[1];main_texture=resolve(main_guid);main_bytes=main_texture.read_bytes();used.add(main_texture)
     if main_bytes[:8]!=b'\x89PNG\r\n\x1a\n':raise ValueError('Brightness requires real original source PNG')
     shaderpath=resolve(shader);shadertext=read(shaderpath);record['shaderSource']=shaderpath.relative_to(assets).as_posix();record['shaderSha256']=hashlib.sha256(shaderpath.read_bytes()).hexdigest()
     record['shaderAdapter']='Explicit native2D alpha projection: original source PS/UV imagery and identity material uniforms; compiled ShaderGraph blend/vertex pipeline unavailable, no URP shader parity'
     record['nativeMaterialCompatibility']={'brightness':1,'transparency':1,'materialTint':'no source color properties','mainTexture':main_texture.relative_to(assets).as_posix(),'mainTextureSha256':hashlib.sha256(main_bytes).hexdigest(),'sourceShaderTextIsDummy':'DummyShaderTextExporter' in shadertext,'sourceBlendProven':False,'policy':'approved native-alpha material projection; shader/HDR/soft-edge visual debt'}
    else:raise ValueError('unsupported source particle shader '+shader)
    record['materialSource']=mat.relative_to(assets).as_posix();record['materialSha256']=hashlib.sha256(mat.read_bytes()).hexdigest();record['serializedMaterial']=mt;record['materialAdapter']='source particle material -> native alpha blend; original RGB/alpha/sprite preserved; shader/HDR edge bounded'
    # Audited source glow policy, not an inferred Unity shader blend. These
    # source MainTex sprites contain opaque black RGB; alpha-over would cover
    # the lawn. Native additive keeps black at zero energy without editing any
    # image, source RGB/alpha gradient, module, timing or geometry.
    glow_sources={'Resources/plants/umbrellaleaf/laserumbrella/LaserUmbrellaPrefab.prefab':198632746491512912,
                  'Resources/plants/umbrellaleaf/laserumbrella/LightBall.prefab':198884547391421755}
    if scope_ids==[glow_sources.get(prefab)] and record['materialSource']=='Resources/shaders/sun/SunParticle.mat' and record['materialSha256']=='dece942b5c5fbb5a3cd4c309724b302885c25e79c523a7894aed7864ec7c0d87':
     if record.get('shaderSource')!='UnityBuiltinParticle10720' or record['sprite'].get('sourceTextureSha256')!='62fadea9d809b3e3857078c238560833da26bbdde4a0e3c1669734ed78c021ac':raise ValueError('audited native glow source shader/image differs')
     if any(v<0 for v in record['color'].values()) or any(v<0 for key in record['colorCurve'] for v in key[1:]):raise ValueError('audited glow requires nonnegative source color')
     record['nativeBlend']='additive'
     record['nativeBlendPolicy']={'scope':'audited LaserUmbrella/LightBall source glow only','sourceBlendProven':False,'serializedSrcBlend':1,'serializedDstBlend':0,'policy':'explicit nativeTod additive compatibility projection; builtin shader pipeline unavailable; not exact source blend/URP parity','sourceImageAndRGBAlphaTimingUnchanged':True}
  emitters.append(record)
 for p in sorted(used):sources.append({'path':p.relative_to(assets).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
 result={'schema':2,'sourceKind':source_id,'sourcePrefab':prefab,'sourcePrefabSha256':hashlib.sha256((assets/prefab).read_bytes()).hexdigest(),'bindingScope':'embeddedParticleComponents' if scope_ids is not None else 'sourceFactoryEffect','sourceComponentIds':scope_ids,'nativeAdapter':'TodParticleSystem source-compatible gradient/ranged-size/speed-curve subset','emitters':emitters,'birthPoints':[],'sources':sources,'ignoredBindings':[],'deferredComponents':[],'limits':['Native2D sphere/cone projection and random distribution differ from Unity3D','Native10ms quantization and stopAction child-tree shutdown not Unity runtime verified','Shader HDR/soft-particle material edge behavior bounded; native alpha blend','Every source sprite retained; random starting UV phase uses native frame0; native lifetime animation approximates Unity phase','Fractional UV crops conservatively quantized <1texel/edge; sequence canvases center-padded preserving original source pixels/PPU']}
 (output/'effect.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');return result

if __name__=='__main__':
 import argparse
 p=argparse.ArgumentParser();p.add_argument('assets');p.add_argument('prefab');p.add_argument('output');p.add_argument('--source-id',type=int,required=True);a=p.parse_args();q=export(pathlib.Path(a.assets),a.prefab,pathlib.Path(a.output),a.source_id);print('Source effect',q['sourceKind'],'strict native-subset emitters',len(q['emitters']),'birthPoints',len(q['birthPoints']))
