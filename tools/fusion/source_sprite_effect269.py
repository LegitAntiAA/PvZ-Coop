"""Compile a source one-shot Sprite/Animator effect into one native Tod emitter.

This is an explicit native-compatible projection, not a Unity ParticleSystem.
Every source frame/clip/event and original PNG proof remains in the resource.
"""
import hashlib,json,math,pathlib,re
import export_animation as e
import source_world_particles269 as p
from source_particle_png import crop_png

def export(assets,model_json,output,kind):
 assets=pathlib.Path(assets);model_json=pathlib.Path(model_json);output=pathlib.Path(output);output.mkdir(parents=True,exist_ok=True)
 model=json.loads(model_json.read_text(encoding='utf-8-sig'))
 if not model['applyRootMotion'] or model['ignoredBindings'] or model['omittedInactiveNodes'] or model['deferredComponents'] or len(model['sprites'])!=1 or len(model['nodes'])!=1 or len(model['clips'])!=1:raise ValueError('sprite effect requires complete one-root animated source model')
 clip=next(iter(model['clips'].values()));curves=clip['curves'];sprite=next(x for x in curves if x['kind']=='sprite');position=next(x for x in curves if x['kind']=='position')
 if len(curves)!=2 or clip['loop'] or len(position['keys'])!=2 or any(x['path']!='' for x in curves):raise ValueError('unsupported source sprite-effect tracks')
 death=[x for x in clip['events']if x['functionName']=='Die']
 if len(clip['events'])!=1 or len(death)!=1:raise ValueError('sprite effect needs exact source Die event')
 life=death[0]['time'];keys=position['keys'];a,b=keys[0]['value'],keys[1]['value']
 if not 0<life<=10 or abs(keys[1]['time']-life)>1e-6 or abs(a['x']-b['x'])>1e-6 or abs(a['z']-b['z'])>1e-6:raise ValueError('unsupported source root motion')
 frames=[];sources={x['path']:x['sha256']for x in model['sources']}
 for key in sprite['keys']:
  guid=key['spriteGuid'];v=model['spriteVariants'][guid];source=assets/v['sourceSprite'];raw=source.read_text(encoding='utf-8-sig');sp=p.block(raw,'textureRect');rect={k:float(e.required(sp,k))for k in ['x','y','width','height']}
  match=re.search(r'guid: ([0-9a-f]{32})',e.required(raw,'texture'));texture_guid=match[1]
  # The complete model already carries the exact original texture PNG.
  original=model_json.parent/v['texture'];data=original.read_bytes();import struct
  iw,ih=struct.unpack('>II',data[16:24]);x,y=math.floor(rect['x']+1e-4),math.floor(rect['y']+1e-4);w,h=math.ceil(rect['x']+rect['width']-1e-4)-x,math.ceil(rect['y']+rect['height']-1e-4)-y
  dest=output/'textures'/(guid+'.png');dest.parent.mkdir(exist_ok=True);crop_png(original,dest,x,ih-y-h,w,h)
  logical=p.block(raw,'m_Rect');lw,lh=float(e.required(logical,'width')),float(e.required(logical,'height'));pivot=e.vector(raw,'m_Pivot');offset=e.vector(raw,'textureRectOffset');nw,nh=p.pad_sprite(dest,w,h,lw,lh,pivot,offset)
  frames.append({'sourceGuid':guid,'source':v['sourceSprite'],'sourceTextureGuid':texture_guid,'sourceTextureSha256':hashlib.sha256(data).hexdigest(),'sourceSprite':raw,'sourceTextureRect':rect,'normalizationWidth':lw,'sourcePivot':pivot,'texture':dest.relative_to(output).as_posix(),'textureSha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'width':nw,'height':nh,'pixelsPerUnit':float(e.required(raw,'m_PixelsToUnits'))})
 result=dict(frames[0]);p.pack_frames(output,frames,result);result['frames']=len(frames)
 zero=[[0,0,0,0],[1,0,0,0]];speed=6*(b['y']-a['y'])/life
 vertical=[[0,0,speed,speed],[1,0,-speed,-speed]]
 emitter={'nodeName':model['nodes'][0]['name'],'nodeIndex':-1,'sourcePosition':{'x':0,'y':0,'z':0},'sourceRotation':model['nodes'][0]['rotation'],'sourceScale':model['nodes'][0]['scale'],'sourceActive':True,'rendererEnabled':True,
  'duration':.01,'delay':0,'lifetime':life,'lifetimeRange':[life,life],'speed':0,'speedRange':[0,0],'size':frames[0]['normalizationWidth']/frames[0]['pixelsPerUnit'],'rotation':0,'rotationRange':[0,0],'rate':0,'sourceEmissionRate':0,'burstCount':1,'maxActive':1,'looping':False,'sourceSimulationSpace':1,'sourceScalingMode':1,
  'enabledModules':['InitialModule','EmissionModule','UVModule','VelocityModule'],'shape':{'type':-1,'radius':0,'scale':{'x':1,'y':1,'z':1},'angle':0},'sizeCurve':[[0,1],[1,1]],'color':model['sprites'][0]['color'],'colorCurve':[[0,1,1,1,1],[1,1,1,1,1]],'speedCurve':[[0,1,0,0],[1,1,0,0]],'forceCurves':None,'spinCurves':None,
  'velocityCurves':{'axes':[[zero,zero],[vertical,vertical],[zero,zero]],'orbitalZ':[zero,zero],'world':True},'sprite':result,'spriteFrames':frames,'sourcePrimitive':'SpriteAnimator, not a source ParticleSystem','sourceClip':clip,
  'nativeCompatibility':'original nine Sprite keys and root-motion displacement converted to native lifetime sheet/velocity; variable frame spacing quantized to uniform native frames, max approximately2 ticks; no gameplay effect invented'}
 emitter['sizeRange']=[emitter['size'],emitter['size']]
 out={'schema':2,'sourceKind':kind,'sourcePrefab':model['sourcePrefab'],'sourcePrefabSha256':next(x['sha256']for x in model['sources']if x['path']==model['sourcePrefab']),'bindingScope':'sourceFactoryEffect','sourceComponentIds':[],'nativeAdapter':'source SpriteAnimator -> one existing native TodParticle; actual source frames/root-motion/Die lifetime','sourceModelSha256':hashlib.sha256(model_json.read_bytes()).hexdigest(),'emitters':[emitter],'birthPoints':[],'sources':model['sources'],'ignoredBindings':[],'deferredComponents':[],'limits':['Native-compatible sprite frame spacing, anisotropic pixel projection and10ms lifecycle are not Unity bitwise parity','Actual source model has no Collider, no damage/trigger script; FireMgr declares only Die']}
 (output/'effect.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8');return out
