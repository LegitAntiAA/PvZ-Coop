"""Strict source TrailModule subset, rendered by native Trail20 adaptation."""
import hashlib,json,math,re
def trail_profile(p,r,block,required,constant,scalar_curve,initial_color):
 t=block(p,'TrailModule')
 expected={'enabled':'1','mode':'0','ratio':'1','textureMode':'0','ribbonCount':'1','worldSpace':'1','dieWithParticles':'1','sizeAffectsWidth':'1','sizeAffectsLifetime':'0','inheritParticleColor':'1','generateLightingData':'0','splitSubEmitterRibbons':'0','attachRibbonsToTransform':'0'}
 if any(required(t,k)!=v for k,v in expected.items())or constant(t,'lifetime')!=1:raise ValueError('unsupported TrailModule lifetime/mode/owner subset')
 if required(r,'m_RenderMode')!='5':raise ValueError('Trail360 requires genuine trails-only None renderer')
 mats=block(r,'m_Materials')
 if not re.fullmatch(r'\s*- \{fileID: 0\}\s*- \{fileID: 10308, guid: 0000000000000000f000000000000000, type: 0\}\s*',mats):raise ValueError('unsupported Trail360 materials')
 distance=float(required(t,'minVertexDistance'))
 if not math.isfinite(distance)or distance<=0 or distance>1:raise ValueError('invalid TrailModule vertex distance')
 if not re.fullmatch(r'\{x: 1, y: 1\}',required(t,'textureScale')):raise ValueError('unsupported trail texture scale')
 for n in ['colorOverLifetime','colorOverTrail']:
  if initial_color(block(t,n))!=[{'r':1.,'g':1.,'b':1.,'a':1.}]:raise ValueError('Trail360 requires constant identity trail color, particle RGBA retained')
 width=scalar_curve(block(t,'widthOverTrail'))
 return {'schema':1,'policy':'nativeTrail20FlatAlpha360','mode':0,'ratio':1,'lifetime':1,'worldSpace':True,'dieWithParticles':True,'sizeAffectsWidth':True,'sizeAffectsLifetime':False,'inheritParticleColor':True,'textureMode':0,'textureScale':[1,1],'minVertexDistance':distance,'widthCurve':width,'nativeMaximumPoints':20,'sourceRendererMode':5,'sourceBuiltinMaterial':10308,'serializedTrailModule':t,'serializedParticleRenderer':r,'limits':['Native Trail retains at most20 points, earlier geometry drops when full; native10ms sampling differs fromUnity vertex subdivision','Built-in10308 uses explicit native flat alpha material, no original texture is invented; HDR/softintersection absent','Particle lifetime1 multiplier and dieWithParticles1 means no trail vertex can outlive the native parent particle; native particle deletion owns cleanup']}

def projected_edge(assets,prefab,trans,objects,tid,shape,pose_model,reference,required,vector):
 if pose_model is None:raise ValueError('edge requires complete source model pose proof')
 model=json.loads(pose_model.read_text(encoding='utf-8-sig'));sha=hashlib.sha256((assets/prefab).read_bytes()).hexdigest()
 if model['sourcePrefab']!=prefab or not any(s['path']==prefab and s['sha256']==sha for s in model['sources']):raise ValueError('edge model source hash differs')
 chain=[];cursor=tid
 while cursor:chain.append((cursor,trans[cursor]));cursor=reference(trans[cursor],'m_Father')
 chain.reverse();records=[];paths=[];basis=[[1.,0.,0.],[0.,1.,0.],[0.,0.,1.]]
 for idx,(fid,t)in enumerate(chain):
  paths.append(''if idx==0 else '/'.join(required(objects[reference(v,'m_GameObject')],'m_Name')for _,v in chain[1:idx+1]))
  q=vector(t,'m_LocalRotation');sc=vector(t,'m_LocalScale');records.append({'position':vector(t,'m_LocalPosition'),'rotation':q,'scale':sc});x,y,z,w=[q[k]for k in 'xyzw'];length=math.sqrt(x*x+y*y+z*z+w*w)
  if not math.isfinite(length)or length<1e-8:raise ValueError('invalid source edge quaternion')
  x,y,z,w=[v/length for v in[x,y,z,w]];rot=[[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]];basis=[[sum(basis[r][k]*rot[k][c]for k in range(3))for c in range(3)]for r in range(3)]
 invariant=[]
 for name,clip in model['clips'].items():
  for c in clip['curves']:
   if c['path']in paths and c['kind']in('euler','rotation','quaternion')or c['path']==paths[-1]and c['kind']=='scale':
    # Genuine207 armraise writes identity Euler keys on an identity-rest root.
    # Prove ALL values and tangents; a nonconstant/other-property binding stays
    # unsupported. Original curve is retained in both model and proof record.
    q=records[paths.index(c['path'])]['rotation']
    identity=c['kind']=='euler'and all(abs(q[k])<1e-7 for k in 'xyz')and abs(q['w']-1)<1e-7 and c['keys']and all(all(abs(float(key[field][axis]))<1e-7 for axis in 'xyz'for field in ['value','inSlope','outSlope'])for key in c['keys'])
    if not identity:raise ValueError('animated edge frame unsupported')
    invariant.append({'clip':name,'curve':c,'proof':'all Euler keys/tangents exactly zero; rest quaternion identity'})
 local=records[-1]['scale'];basis=[[basis[r][c]*local['xyz'[c]]for c in range(3)]for r in range(3)]
 axis=[basis[0][0],basis[1][0]];normal=[basis[0][1],basis[1][1]]
 if abs(axis[0])>1e-5 and abs(axis[1])>1e-5 or math.hypot(*normal)<1e-6:raise ValueError('edge cannot project into native axis-aligned box/direction')
 if required(block_radius:=shape[shape.index('    radius:'):],'mode')!='0':raise ValueError('nonrandom source edge radius mode unsupported')
 return {'basis':basis,'ancestorTrs':records,'ancestorPaths':paths,'invariantSourceCurves':invariant,'edgeAxis':[1,0,0],'launchAxis':[0,1,0],'sourceModelSha256':hashlib.sha256(pose_model.read_bytes()).hexdigest(),'sourcePrefabSha256':sha,'sourceProjectionPolicy':'native axis-aligned projection of Unity SingleSidedEdge; source planar edgeX and launchY retained, native10ms RNG geometry adaptation','normalDirectionReference':'Unity ParticleSystemForceField SingleSidedEdge example emitted frombottom toward positiveY'}
