#include "NetFusionDayLimbs276Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeAdventure273.h"
#include "../Fusion/FusionNativeZombies.h"
#include "../Fusion/FusionNativeZombieVisual.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativeEmbeddedEffects269.h"
#include "../Fusion/FusionNativeWorldParticle269.h"
#include "../Fusion/FusionShooterBatchRules.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/DDInterface.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <string>

namespace
{
using namespace LawnFusion;using LawnNet::NetTrace;
struct Ref {uint16_t id=0;uint32_t token=0,generation=0;};
struct Limb
{
    uint16_t kind,resource;bool head;Ref owner{};TodParticleDefinition* definition=nullptr;
    bool lost=false,observed=false,captured=false,survived=false;uint32_t system=0;int lastAge=-1;
};
struct State
{
    int stage=0,cohort=0,waiting=0,playingFrame=-1;uint32_t started=0,since=0,chat=0,epoch=0,samples=0,diagnostic=0;
    bool prepared=false,born=false,pause=false,resume=false,initialReady=false,lossesReady=false,proofReady=false,restarted=false,completionLogged=false;
    bool lethalCaptured=false,lethalZeroHP=false;Ref lethalPlant{};uint16_t lethalShot=0;uint32_t lethalShotSamples=0,lethalHPSamples=0;
    std::string proofTag;std::set<std::string> frames;std::map<std::string,int> draws;
    // Source zombies270 manifest binds these exact nine source components.
    std::array<Limb,10> limbs={Limb{21,5033,false},Limb{22,5035,false},Limb{23,5037,false},
        Limb{106,5063,false},Limb{107,5064,false},Limb{108,5065,false},
        Limb{100,5059,false},Limb{3,5011,false},Limb{3,5010,true},Limb{100,5059,false}};
}s;
size_t Begin(){return size_t(s.cohort)*3;}
size_t End(){return (std::min)(Begin()+3,s.limbs.size());}
bool Enabled(const char* name){const auto* v=std::getenv(name);return v&&std::strcmp(v,"1")==0;}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool okay,const char* why){if(!okay)a->CoopSmokeFail(why);return okay;}
void Advance(LawnApp* a,int stage)
{s.stage=stage;s.since=GetTickCount();NetTrace("SMOKE_LIMBS276_STAGE role=%s cohort=%d stage=%d epoch=%u",Role(a),s.cohort,stage,s.epoch);}
bool Has(LawnApp* a,int slot,const std::string& text)
{for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==slot&&line.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text)
{
    if(!Require(a,text.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Limbs276 canonical chat proof exceeds limit"))return false;
    if(Has(a,Slot(a),text))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;
    s.chat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);
}
std::string Tag(const std::string& value){return "L276_"+std::to_string(s.epoch)+"_"+value;}
bool Barrier(LawnApp* a,const std::string& value)
{const auto text=Tag(value);Send(a,text);return Has(a,0,text)&&Has(a,1,text);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Limbs276 owned SDL click failed");
}
bool Escape(LawnApp* a)
{
    SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;
    SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;
    return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Limbs276 owned SDL Escape failed");
}
bool Capture(LawnApp* a,const std::string& suffix)
{
    const auto name="fusion_limbs276_case"+std::to_string(s.cohort)+"_"+suffix;if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requested=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requested))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Limbs276 fresh owned BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_LIMBS276_RENDER role=%s frame=%s requestedDraw=%d capturedDraw=%d",Role(a),name.c_str(),requested,a->mCoopSmokeFrameDrawCount);return true;
}
bool World(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);if(!c)return false;
    if(!Require(a,IsNativeSourceAdventure273(b)&&!c->validation&&!a->mFusionRuntime&&c->adventure.run.level==1&&
        c->adventure.run.cycle==1&&c->adventure.sourceDifficulty==2&&!c->adventure.schedulerFailed&&
        !c->adventure.run.over&&c->adventure.run.tutorial.active&&c->adventure.wave==0&&c->money==3000&&
        b->GetColumnCount()==10&&GetNativeFusionRowCount(b)==5&&b->mBackground==BACKGROUND_1_DAY,
        "Limbs276 ordinary source1 tutorial-held world differs"))return false;
    if(!s.epoch)s.epoch=c->epoch;return Require(a,s.epoch==c->epoch,"Limbs276 source epoch changed before proof");
}
bool Choose(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;
    if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return false;
    const auto& bank=c->selection.State();
    if(!Require(a,c->eligible.size()==4&&bank.targetCount==4&&bank.quotas[0]==2&&bank.quotas[1]==2,"Limbs276 native chooser quota differs"))return false;
    int own=0;for(const auto& p:bank.packets)if(p.owner==Slot(a))
    {if(!Require(a,p.kind<=1&&p.isExtra==(Slot(a)==1),"Limbs276 local chooser identity differs"))return false;++own;}
    if(own<s.waiting)return false;
    if(own<2)
    {
        int dense=-1;for(size_t i=0;i<c->eligible.size();++i)if(c->eligible[i].kind==own&&c->eligible[i].isExtra==(Slot(a)==1))dense=int(i);
        if(!Require(a,dense>=0,"Limbs276 actual local card missing"))return false;
        if(std::abs(chooser->mScrollAmount)>.05f)return false;int x=0,y=0;chooser->GetSeedPositionInChooser(dense,x,y);
        if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waiting=own+1;
        return false;
    }
    return bank.packets.size()==4&&Capture(a,"chooser")&&Barrier(a,"CHOOSER");
}
bool Prepare(LawnApp* a)
{
    if(s.prepared)return true;auto* b=a->mBoard;std::string error;
    for(uint16_t kind:{uint16_t(1160),uint16_t(1032),uint16_t(1005)})
    {
        if(!Require(a,EnsureNativePlantVisuals(b,kind,&error),"Limbs276 genuine plant resource preflight failed"))return false;
        ReanimationPreload(GetPlantDefinition(GetNativeSeedForPlant(kind)).mReanimationType);
    }
    for(uint16_t kind:{uint16_t(21),uint16_t(22),uint16_t(23),uint16_t(106),uint16_t(107),uint16_t(108),uint16_t(100),uint16_t(3)})
    {
        if(!Require(a,EnsureNativeSourceZombieVisuals(b,kind,&error),"Limbs276 genuine NPC resource preflight failed"))return false;
        ReanimationPreload(GetZombieDefinition(ZombieType(GetNativeSourceZombieCarrier(kind))).mReanimationType);
    }
    std::wstring executable(32768,L'\0');const DWORD length=GetModuleFileNameW(nullptr,executable.data(),DWORD(executable.size()));
    if(!Require(a,length>0&&length<executable.size(),"Limbs276 cannot locate admitted source resource cache"))return false;
    executable.resize(length);const auto root=std::filesystem::path(executable).parent_path()/L"fusion"/L"effects"/L"zombies270";
    for(size_t i=Begin();i<End();++i)
    {
        auto& limb=s.limbs[i];const auto path=root/std::to_string(limb.resource)/"effect.json";
        auto* definition=EnsureNativeEmbeddedParticle269Resource(b,limb.resource,path.string(),error);
        if(!Require(a,definition&&definition->Data(true),"Limbs276 canonical limb resource cache missing"))return false;
        limb.definition=definition->Data(true); // Read-only identity of the real native definition, never a fixture-created PS.
    }
    s.prepared=true;return true;
}
bool PlantBirth(LawnApp* a,uint16_t kind,int col,int row)
{
    NativePlantData data;data.kind=kind;data.owner=LawnNet::NET_SHARED_OWNER;
    auto* p=a->mBoard->AddPlant(col,row,GetNativeSeedForPlant(kind),SEED_NONE,&data);const auto* actual=GetNativePlantData(p);
    if(!Require(a,p&&actual&&actual->kind==kind&&actual->instanceToken&&p->mNetId&&!p->mIsAsleep&&p->mPlantHealth==p->mPlantMaxHealth,
        "Limbs276 genuine initial source plant factory failed"))return false;
    NetTrace("SMOKE_LIMBS276_PLANT role=%s cohort=%d kind=%u id=%u token=%u col=%d row=%d awake=1 initialFactory=1 acquisition=NotExecuted",
        Role(a),s.cohort,kind,p->mNetId,actual->instanceToken,col,row);
    if(s.cohort==3)s.lethalPlant={p->mNetId,actual->instanceToken,a->mBoard->mPlants.DataArrayGetID(p)};
    return true;
}
bool ZombieBirth(LawnApp* a,uint16_t kind,int row,float x)
{
    NativeSourceZombieSpawnRequest request;request.kind=kind;request.row=row;request.fromWave=0;request.positioned=true;request.shadowX=x;
    auto* z=SpawnNativeSourceZombie(a->mBoard,request);const auto* d=GetNativeSourceZombieData(z);
    if(!Require(a,z&&d&&d->kind==kind&&z->mHasArm&&z->mHasHead&&z->mBodyHealth==z->mBodyMaxHealth&&z->mHelmHealth==z->mHelmMaxHealth,
        "Limbs276 genuine intact typed NPC factory failed"))return false;
    NetTrace("SMOKE_LIMBS276_BIRTH kind=%u id=%u token=%u generation=%u row=%d shadowBirthX=%.3f hp=%d armor=%d postBirthWrites=0",
        kind,z->mNetId,d->instanceToken,a->mBoard->mZombies.DataArrayGetID(z),row,x,z->mBodyHealth,z->mHelmHealth);return true;
}
bool Birth(LawnApp* a)
{
    if(!a->NetIsHost()||s.born)return true;auto* b=a->mBoard;
    if(!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize&&!b->mPaused,"Limbs276 birth requires an empty actual source opening"))return false;
    if(s.cohort==0)
    {
        // HelmetFume's actual Real6 delivery applies80 to body and first armor
        // independently. Doll body2000 crosses1333 while all helmets survive.
        for(int row=0;row<3;++row)if(!PlantBirth(a,1160,0,row)||!ZombieBirth(a,uint16_t(21+row),row,450.f))return false;
    }
    else if(s.cohort==1)
    {
        for(int row=0;row<3;++row)
        {
            for(int col=0;col<2;++col)if(!PlantBirth(a,1032,col,row))return false;
            if(!ZombieBirth(a,uint16_t(106+row),row,580.f))return false;
        }
    }
    else if(s.cohort==2)
    {
        for(int col=0;col<6;++col)if(!PlantBirth(a,1032,col,1))return false;
        if(!PlantBirth(a,1032,0,3)||!ZombieBirth(a,3,1,660.f)||!ZombieBirth(a,100,3,550.f))return false;
    }
    else
    {
        // Source1005 actually emits Bullet3/300. Its SourceExplosion delivers
        // native18U; source HP and the existing native death adapter own death.
        if(!PlantBirth(a,1005,0,1)||!ZombieBirth(a,100,1,550.f))return false;
    }
    s.born=true;s.pause=Escape(a);return s.pause;
}
Zombie* Owner(LawnApp* a,Limb& limb,bool bind)
{
    Zombie* z=nullptr;while(a->mBoard->IterateZombies(z))if(const auto* d=GetNativeSourceZombieData(z);d&&d->kind==limb.kind)
    {
        if(!limb.owner.id&&bind)
        {
            limb.owner={z->mNetId,d->instanceToken,a->mBoard->mZombies.DataArrayGetID(z)};
            NetTrace("SMOKE_LIMBS276_BIND role=%s cohort=%d kind=%u id=%u token=%u localGeneration=%u resource=%u head=%d arm=%d hp=%d armor=%d",
                Role(a),s.cohort,limb.kind,z->mNetId,d->instanceToken,limb.owner.generation,limb.resource,z->mHasHead,z->mHasArm,z->mBodyHealth,z->mHelmHealth);
        }
        if(limb.owner.id==z->mNetId&&limb.owner.token==d->instanceToken)return z;
    }
    return nullptr;
}
bool Initial(LawnApp* a)
{
    for(size_t i=Begin();i<End();++i)
    {
        auto& limb=s.limbs[i];auto* z=Owner(a,limb,true);if(!z)return false;
        const auto* d=GetNativeSourceZombieData(z);const auto* p=FindFusionZombieProfile(limb.kind);float x=0,y=0,rx=0,ry=0;
        if(!Require(a,p&&z->mHasArm&&z->mHasHead&&!z->mMindControlled&&z->mFromWave==0&&z->mBodyHealth==p->maxHealth&&
            z->mHelmHealth==p->firstArmor&&d->instanceToken&&limb.owner.generation,"Limbs276 client did not bind the intact canonical first sample"))return false;
        if(!Require(a,GetNativeSourceZombie269NodePoint(z,"Shadow",x,y)&&GetNativeSourceZombie269NodePoint(z,"",rx,ry),"Limbs276 actual source model nodes unavailable"))return false;
        const float dx=float(a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX),dy=float(a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);
        if(!Require(a,(std::min)(x+dx,rx+dx)>=24&&(std::max)(x+dx,rx+dx)<=a->mWidth-64&&
            (std::min)(y+dy,ry+dy)>=0&&(std::max)(y+dy,ry+dy)<=a->mHeight-8,"Limbs276 source model initial pose is outside the real viewport"))return false;
    }
    if(s.cohort==3)
    {
        Plant* plant=nullptr;Plant* found=nullptr;unsigned count=0;
        while(a->mBoard->IteratePlants(plant))if(const auto* d=GetNativePlantData(plant);d&&d->kind==1005)
        {found=plant;++count;if(!s.lethalPlant.id)s.lethalPlant={plant->mNetId,d->instanceToken,a->mBoard->mPlants.DataArrayGetID(plant)};}
        const auto* d=GetNativePlantData(found);
        if(!Require(a,count==1&&found&&d&&found->mNetId==s.lethalPlant.id&&d->instanceToken==s.lethalPlant.token&&
            found->mRow==1&&found->mPlantCol==0,"Limbs276 lethal source1005 canonical factory baseline differs"))return false;
    }
    return true;
}
bool NoStock(LawnApp* a)
{
    auto* holder=a->mEffectSystem?a->mEffectSystem->mParticleHolder:nullptr;
    if(!Require(a,holder!=nullptr,"Limbs276 native particle holder unavailable"))return false;
    TodParticleSystem* ps=nullptr;while(holder->mParticleSystems.IterateNext(ps))if(!ps->mDead)
    {
        const auto type=ps->mEffectType;
        if(type==PARTICLE_ZOMBIE_HEAD||type==PARTICLE_ZOMBIE_ARM||type==PARTICLE_ZOMBIE_NEWSPAPER_HEAD||type==PARTICLE_ZOMBIE_BALLOON_HEAD||
            type==PARTICLE_ZOMBIE_POGO_HEAD||type==PARTICLE_MOWERED_ZOMBIE_HEAD||type==PARTICLE_MOWERED_ZOMBIE_ARM||type==PARTICLE_ZOMBIE_HEAD_POOL)
        {NetTrace("SMOKE_LIMBS276_STOCK role=%s cohort=%d type=%d system=%u",Role(a),s.cohort,int(type),unsigned(a->ParticleGetID(ps)));return Require(a,false,"Limbs276 source loss leaked a stock head/arm particle system");}
    }
    return true;
}
bool VisibleParticle(LawnApp* a,TodParticleSystem* system,float& x,float& y,int& age)
{
    auto* holder=system->mParticleHolder;
    for(auto* e=system->mEmitterList.mHead;e;e=e->mNext)
    {
        auto* emitter=holder->mEmitters.DataArrayTryToGet(unsigned(e->mValue));
        if(!emitter||emitter->mDead||!emitter->mEmitterDef||!emitter->mEmitterDef->mImage)continue;
        for(auto* p=emitter->mParticleList.mHead;p;p=p->mNext)
        {
            auto* particle=holder->mParticles.DataArrayTryToGet(unsigned(p->mValue));ParticleRenderParams render{};
            if(!particle||!TodParticleEmitter::GetRenderParams(particle,&render)||(render.mAlphaIsSet&&render.mAlpha<=0))continue;
            x=particle->mPosition.x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX;
            y=particle->mPosition.y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY;age=particle->mParticleAge;
            if(std::isfinite(x)&&std::isfinite(y)&&x>=24&&x<=a->mWidth-24&&y>=80&&y<=a->mHeight-16)return true;
        }
    }
    return false;
}
bool Observe(LawnApp* a)
{
    if(!NoStock(a))return false;auto* b=a->mBoard;auto* holder=a->mEffectSystem->mParticleHolder;bool ready=true;
    for(size_t i=Begin();i<End();++i)
    {
        auto& limb=s.limbs[i];auto* owner=Owner(a,limb,false);
        if(owner&&!(limb.head?owner->mHasHead:owner->mHasArm))
        {
            if(!limb.lost)NetTrace("SMOKE_LIMBS276_LOSS role=%s kind=%u id=%u token=%u generation=%u resource=%u head=%d hp=%d armor=%d phase=%d",
                Role(a),limb.kind,limb.owner.id,limb.owner.token,limb.owner.generation,limb.resource,limb.head,owner->mBodyHealth,owner->mHelmHealth,int(owner->mZombiePhase));
            if(s.cohort==0&&!Require(a,owner->mHelmHealth>0,"Limbs276 Doll arm required intact first armor under genuine Real6 damage"))return false;
            limb.lost=true;
        }
        TodParticleSystem* ps=nullptr;TodParticleSystem* found=nullptr;unsigned count=0;
        while(holder->mParticleSystems.IterateNext(ps))if(!ps->mDead&&ps->mParticleDef==limb.definition){found=ps;++count;}
        if(!Require(a,count<=1,"Limbs276 one loss component produced duplicate live native systems"))return false;
        if(found)
        {
            const uint32_t id=unsigned(a->ParticleGetID(found));
            if(!Require(a,found->mEffectType==PARTICLE_FUSION_EMBEDDED269&&!found->mIsAttachment&&id&&(!limb.system||limb.system==id),
                "Limbs276 generated PS lacks actual loss/new canonical system identity"))return false;
            if(!limb.system)limb.system=id;
            // A reliable loss event may arrive before the raw body loss flag.
            // Ordinary cases still require that real flag before evidence.
            if(!limb.lost){ready=false;continue;}
            float x=0,y=0;int age=0;
            if(VisibleParticle(a,found,x,y,age))
            {
                if(!limb.observed)NetTrace("SMOKE_LIMBS276_PS role=%s cohort=%d kind=%u id=%u token=%u ownerGeneration=%u resource=%u localSystem=%u age=%d screen=(%.3f,%.3f) stock=0 actualParticle=1",
                    Role(a),s.cohort,limb.kind,limb.owner.id,limb.owner.token,limb.owner.generation,limb.resource,id,age,x,y);
                limb.observed=true;limb.lastAge=age;
                // Capture while the real native particle is visible, before
                // any peer barrier can consume its short native lifetime.
                if(!limb.captured&&Capture(a,"resource"+std::to_string(limb.resource)))limb.captured=true;
                if(s.cohort==2&&limb.head&&!b->mZombies.DataArrayTryToGet(limb.owner.generation))
                {
                    if(!limb.survived)NetTrace("SMOKE_LIMBS276_SURVIVED role=%s resource=%u localSystem=%u ownerId=%u ownerToken=%u ownerGeneration=%u actualOwnerAllocated=0 particleAge=%d screen=(%.3f,%.3f)",
                        Role(a),limb.resource,id,limb.owner.id,limb.owner.token,limb.owner.generation,age,x,y);
                    limb.survived=true;
                }
            }
        }
        if(limb.system&&!found&&!limb.captured)return Require(a,false,"Limbs276 real particle expired before its loss flag/fresh evidence frame");
        ready&=limb.lost&&limb.observed&&limb.captured;
    }
    if(!s.diagnostic||GetTickCount()-s.diagnostic>=2000)
    {
        s.diagnostic=GetTickCount();for(size_t i=Begin();i<End();++i)
        {auto& c=s.limbs[i];auto* z=Owner(a,c,false);NetTrace("SMOKE_LIMBS276_WAIT role=%s cohort=%d resource=%u id=%u token=%u hp=%d armor=%d arm=%d head=%d lost=%d ps=%u age=%d captured=%d survived=%d",
            Role(a),s.cohort,c.resource,c.owner.id,c.owner.token,z?z->mBodyHealth:-1,z?z->mHelmHealth:-1,z?z->mHasArm:-1,z?z->mHasHead:-1,c.lost,c.system,c.lastAge,c.captured,c.survived);}
    }
    return ready;
}
bool ObserveLethal(LawnApp* a)
{
    if(!NoStock(a))return false;auto* b=a->mBoard;auto& limb=s.limbs[9];auto* owner=Owner(a,limb,false);
    Projectile* shot=nullptr;
    while(b->IterateProjectiles(shot))if(const auto* d=GetNativeProjectileData(shot);d&&!shot->mDead&&shot->mNetId&&
        d->kind==3&&d->damage==300&&!d->isZombieBullet&&shot->mRow==1&&
        FusionShooterBatchProjectileMetadataValid(d->kind,d->damage,d->sunOnHit,d->specialType,shot->mProjectileType,shot->mMotionType))
    {
        if(!s.lethalShot)
        {
            s.lethalShot=shot->mNetId;s.lethalShotSamples=Samples(a);
            NetTrace("SMOKE_LIMBS276_LETHAL_SHOT role=%s id=%u localGeneration=%u kind=3 damage=300 way=0 row=1 age=%d sourcePlantId=%u sourcePlantToken=%u sourceKind1005=exclusive-cohort nativeImpactFlags=18",
                Role(a),shot->mNetId,b->mProjectiles.DataArrayGetID(shot),shot->mProjectileAge,s.lethalPlant.id,s.lethalPlant.token);
        }
        if(!Require(a,shot->mNetId==s.lethalShot,"Limbs276 lethal source100 survived to a second CherryPea300 projectile"))return false;
    }
    const auto* ownerData=GetNativeSourceZombieData(owner);
    if(owner&&ownerData&&ownerData->health>0&&!owner->IsDeadOrDying()&&!Require(a,owner->mBodyHealth==270&&owner->mHelmHealth==0,
        "Limbs276 lethal source100 received unexpected partial damage before CherryPea300"))return false;
    if(owner&&ownerData&&ownerData->health<=0&&owner->mBodyHealth==0&&owner->IsDeadOrDying())
    {
        if(!s.lethalZeroHP)
        {
            s.lethalZeroHP=true;s.lethalHPSamples=Samples(a);
            NetTrace("SMOKE_LIMBS276_LETHAL_TAIL role=%s id=%u token=%u sourceHP=%.3f nativeHP=0 phase=%d arm=%d head=%d samples=%u",
                Role(a),owner->mNetId,ownerData->instanceToken,ownerData->health,int(owner->mZombiePhase),owner->mHasArm,owner->mHasHead,s.lethalHPSamples);
        }
    }
    auto* holder=a->mEffectSystem->mParticleHolder;TodParticleSystem* ps=nullptr;TodParticleSystem* found=nullptr;unsigned count=0;
    while(holder->mParticleSystems.IterateNext(ps))if(!ps->mDead&&ps->mParticleDef==limb.definition){found=ps;++count;}
    if(!Require(a,count<=1,"Limbs276 lethal loss produced duplicate native resource5059 systems"))return false;
    if(found)
    {
        const uint32_t id=unsigned(a->ParticleGetID(found));
        if(!Require(a,id&&found->mEffectType==PARTICLE_FUSION_EMBEDDED269&&!found->mIsAttachment&&(!limb.system||limb.system==id),
            "Limbs276 lethal loss has no canonical detached generated system"))return false;
        limb.system=id;
        const bool allocated=b->mZombies.DataArrayTryToGet(limb.owner.generation)!=nullptr;
        float x=0,y=0;int age=0;
        if(s.lethalShot&&s.lethalZeroHP&&Samples(a)>s.lethalHPSamples&&VisibleParticle(a,found,x,y,age))
        {
            // Source BodyTakeDamage delivers loss synchronously. The existing
            // native death tail need not retire the owner during this PS life.
            // Require the real zero-HP tail plus a later accepted raw snapshot;
            // Root also matches the reliable receiver's canonical event trace.
            limb.lost=limb.observed=true;limb.survived=!allocated;limb.lastAge=age;
            if(!limb.captured)limb.captured=Capture(a,"resource5059");
            if(limb.captured&&Capture(a,"lethal"))
            {
                if(!s.lethalCaptured)NetTrace("SMOKE_LIMBS276_LETHAL role=%s kind=100 id=%u token=%u ownerGeneration=%u actualOwnerAllocated=%d resource=5059 localSystem=%u particleAge=%d screen=(%.3f,%.3f) projectileId=%u projectileKind=3 projectileDamage=300 nativeImpactFlags=18 source1005Id=%u source1005Token=%u samplesAtShot=%u samplesAtZeroHP=%u samplesNow=%u postLethalRawSnapshot=1 reliableReceiverProof=productionTrace ownerDeletionLifetime=NotExecuted",
                    Role(a),limb.owner.id,limb.owner.token,limb.owner.generation,allocated,id,age,x,y,s.lethalShot,s.lethalPlant.id,s.lethalPlant.token,s.lethalShotSamples,s.lethalHPSamples,Samples(a));
                s.lethalCaptured=true;return true;
            }
        }
    }
    if(limb.system&&!found&&!s.lethalCaptured)return Require(a,false,"Limbs276 lethal arm expired before zero-HP reliable evidence frame");
    if(!s.diagnostic||GetTickCount()-s.diagnostic>=2000)
    {
        s.diagnostic=GetTickCount();NetTrace("SMOKE_LIMBS276_LETHAL_WAIT role=%s ownerId=%u token=%u allocated=%d hp=%d sourceHP=%.3f phase=%d shotId=%u resource=5059 localSystem=%u captured=%d samplesAtShot=%u samplesNow=%u",
            Role(a),limb.owner.id,limb.owner.token,b->mZombies.DataArrayTryToGet(limb.owner.generation)!=nullptr,owner?owner->mBodyHealth:-1,ownerData?ownerData->health:0.f,owner?int(owner->mZombiePhase):-1,s.lethalShot,limb.system,s.lethalCaptured,s.lethalShotSamples,Samples(a));
    }
    return false;
}
bool Proof(LawnApp* a,const char* name,bool finished)
{
    LawnNet::NetBuffer bytes;
    for(size_t i=Begin();i<End();++i)
    {const auto& c=s.limbs[i];bytes.WriteU16(c.kind);bytes.WriteU16(c.resource);bytes.WriteU16(c.owner.id);bytes.WriteU32(c.owner.token);}
    if(s.cohort==3){bytes.WriteU16(s.lethalPlant.id);bytes.WriteU32(s.lethalPlant.token);if(std::strcmp(name,"INITIAL")!=0)bytes.WriteU16(s.lethalShot);}
    char value[48];sprintf_s(value,"%s_%08x",name,LawnNet::NetFnv1a32(bytes.Data(),bytes.Size()));
    if(finished){s.proofTag=Tag(value);s.proofReady=true;}
    return Barrier(a,value);
}
void Completion(LawnApp* a)
{
    if(s.completionLogged)return;s.completionLogged=true;
    NetTrace("SMOKE_LIMBS276_COHORT_COMPLETE role=%s cohort=%d resources=%u noStock=1 actorWritesAfterBirth=0",Role(a),s.cohort,unsigned(End()-Begin()));
}
bool Restart(LawnApp* a)
{
    const auto* context=GetNativeContext(a->mBoard);
    if(context&&context->epoch!=s.epoch)
    {
        if(!Require(a,s.proofReady&&!s.proofTag.empty(),"Limbs276 source epoch changed before local loss proof"))return true;
        if(!Has(a,0,s.proofTag)||!Has(a,1,s.proofTag))return true;Completion(a);
        ++s.cohort;s.waiting=0;s.playingFrame=-1;s.epoch=0;s.prepared=s.born=s.pause=s.resume=s.initialReady=s.lossesReady=s.proofReady=s.restarted=s.completionLogged=false;s.proofTag.clear();
        Advance(a,1);return true;
    }
    if(a->NetIsHost()&&!s.restarted)
    {s.restarted=true;Require(a,a->StartFusionSourceAdventure(1,false),"Limbs276 fresh production source1 restart failed");}
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionDayLimbs276(LawnApp* a)
{
    if(!Enabled("PVZ_COOP_SMOKE_FUSION_LIMBS276"))return false;
    if(!a||!a->CoopSmokeActive()||!a->mNetManager||a->mCoopSmokeResultWritten)return true;
    if(!Require(a,Enabled("PVZ_COOP_SMOKE_FUSION_NATIVE")&&a->mCoopSmokeManifest.mScenario==NETSMOKE_SCENARIO_FUSION_VALIDATION,
        "Limbs276 requires owned two-peer native fusion_validation"))return true;
    if(!s.started)s.started=s.since=GetTickCount();
    if(GetTickCount()-s.started>720000||GetTickCount()-s.since>120000)
    {a->CoopSmokeFail("Limbs276 natural loss/lifetime deadline; inspect resource WAIT/PS/SURVIVED and production limb276 traces");return true;}
    if(!s.stage)
    {
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(!Require(a,Slot(a)>=0&&Slot(a)<2&&a->mNetManager->GetDifficulty()==1,"Limbs276 requires two owned peers at coop difficulty1"))return true;
        if(a->NetIsClient()){if(Send(a,"L276_BEGIN"))Advance(a,1);return true;}
        if(!Has(a,1,"L276_BEGIN"))return true;
        if(!Require(a,a->StartFusionSourceAdventure(1,false),"Limbs276 genuine source1 bootstrap rejected"))return true;Advance(a,1);return true;
    }
    const auto* context=GetNativeContext(a->mBoard);
    if(s.stage==20||(s.proofReady&&context&&context->epoch!=s.epoch))return Restart(a);
    if(!a->mBoard||!World(a))return true;auto* b=a->mBoard;const auto& bank=GetNativeContext(b)->selection.State();
    if(s.stage==1){if(Choose(a))Advance(a,2);}
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* chooser=a->mSeedChooserScreen;auto* button=chooser?chooser->mStartButton:nullptr;
        if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver())
        {SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Limbs276 Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase!=FusionBankPhase::Playing||a->mSeedChooserScreen||a->mGameScene!=SCENE_PLAYING)return true;
        if(s.playingFrame<0){s.playingFrame=a->mAppCounter;s.samples=Samples(a);return true;}
        if(a->mAppCounter<=s.playingFrame||Samples(a)-s.samples<2||!Prepare(a)||!Barrier(a,"EMPTY")||!Birth(a))return true;
        if(b->mPaused){s.samples=Samples(a);Advance(a,4);}
    }
    else if(s.stage==4)
    {
        if(!s.initialReady)
        {
            if(!b->mPaused||Samples(a)-s.samples<2||!NoStock(a)||!Initial(a)||!Capture(a,"cohort"))return true;
            s.initialReady=true;
        }
        if(!Proof(a,"INITIAL",false))return true;
        if(a->NetIsHost()&&!s.resume)s.resume=Escape(a);Advance(a,5);
    }
    else if(s.stage==5)
    {
        if(!s.lossesReady){if(b->mPaused||!(s.cohort==3?ObserveLethal(a):Observe(a)))return true;s.lossesReady=true;}
        if(!Proof(a,"LOSSES",false))return true;s.pause=false;Advance(a,6);
    }
    else if(s.stage==6)
    {
        if(!NoStock(a))return true;if(a->NetIsHost()&&!s.pause)s.pause=Escape(a);
        if(b->mPaused){s.samples=Samples(a);Advance(a,7);}
    }
    else if(s.stage==7)
    {
        if(!b->mPaused||Samples(a)-s.samples<2||!NoStock(a)||!Capture(a,"paused")||!Proof(a,"FINISHED",true))return true;
        Completion(a);
        if(s.cohort<3){Advance(a,20);return true;}
        NetTrace("SMOKE_LIMBS276_COMPLETE role=%s sourceLevel=1 wave=0 resources=9 actualLimbEvents=10 actualGeneratedPS=10 stockHeadArmPS=0 lethal100CherryPea300=1 sourceZeroHPNativeDeathTail=1 postLethalRawSnapshot=1 DollIntactArmorReal6=3 actorWritesAfterBirth=0 clockWrites=0 directDamage=0 directFX=0 ownerDeletionLifetime=NotExecuted lateJoin=NotExecuted poolExhaustion=NotExecuted acquisition=NotExecuted fullDayAI=0",Role(a));
        a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
