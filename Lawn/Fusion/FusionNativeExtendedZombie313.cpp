#include "FusionNativeExtendedZombie313.h"
#include "FusionNativeGroundShooter343.h"
#include "FusionNativeSnowZombie363.h"
#include "FusionNativeExtendedZombie347.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionUltimateGargantuar270Rules.h"
#include "FusionBasePlantRules.h"
#include "FusionNativePlacement.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace LawnFusion
{
namespace
{
bool Host(const Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);return z&&d&&FindFusionExtendedZombie313Profile(d->kind)&&z->mApp&&z->mBoard&&z->mApp->mBoard==z->mBoard&&!z->mNetGhost&&!z->mApp->NetIsClient();}
bool Ready(const NativeExtendedZombie313Hooks& h)
{return h.stateFor&&h.graph.stateFor&&h.input&&h.randomInt&&h.randomFloat&&h.preflight&&h.contacts&&h.bodyContacts&&h.eatContact&&h.baseContact&&h.baseBite&&h.plantTraits&&h.setVelocity&&h.translateRoot&&h.sourceNode&&h.spawn&&h.placeRoot&&h.transferThrowStatus&&h.alignShadow&&h.adjustImpFlyNode&&h.changeRow&&h.crush&&h.plantDamage&&h.zombieDamage&&h.projectile&&h.kirovBomb&&h.kirovCrash&&h.machineDeath&&h.ironMindControl&&h.ironHead&&h.armorFall&&h.sourceBoard212&&h.effect&&h.sound&&h.flash&&h.shake&&h.retire&&h.failed;}
struct Run {LawnApp* app;Board* board;uint32_t uid;uint16_t kind;Zombie* z;NativeExtendedZombie313State* s;const NativeExtendedZombie313Hooks& h;};
Run Capture(Zombie* z,NativeExtendedZombie313State& s,const NativeExtendedZombie313Hooks& h)
{return{z->mApp,z->mBoard,z->mBoard->mZombies.DataArrayGetID(z),GetNativeSourceZombieData(z)->kind,z,&s,h};}
bool Refresh(Run& r)
{if(r.app->mBoard!=r.board)return false;r.z=r.board->mZombies.DataArrayTryToGet(r.uid);const auto* d=GetNativeSourceZombieData(r.z);if(!r.z||r.z->mDead||!d||d->kind!=r.kind||d->instanceToken!=r.uid)return false;r.s=r.h.stateFor(r.z);return r.s&&r.s->initialized&&!r.s->failed&&r.h.graph.stateFor(r.z)&&r.h.graph.stateFor(r.z)->localUID==r.uid;}
bool Fail(Run& r,const char* why)
{if(Refresh(r))r.s->failed=true;if(r.app->mBoard==r.board)r.h.failed(r.board,why);return false;}
bool Input(Run& r,NativeExtendedZombie313Input& i)
{return r.h.input(r.z,i)&&std::isfinite(i.sourceSpeed)&&i.sourceSpeed>=0&&std::isfinite(i.sourceRootX)&&std::isfinite(i.sourceRootY)&&std::isfinite(i.sourceAxisX)&&std::isfinite(i.sourceAxisY)&&std::isfinite(i.sourceShadowX)&&std::isfinite(i.sourceShadowY)&&std::isfinite(i.landY);}
bool Preflight(Run& r,NativeExtendedZombie313Operation op)
{std::string error;return r.h.preflight(r.z,op,&error);}
bool PlantLive(Plant* p){return p&&!p->mDead&&!p->mSquished&&p->mIsOnBoard&&p->mPlantHealth>0&&GetNativePlantData(p);}
bool Opposite(Zombie* self,Zombie* target)
{return target&&target!=self&&!target->mDead&&target->mRow==self->mRow&&target->mMindControlled!=self->mMindControlled;}
bool SoundRange(Run& r,int first,int end)
{const int sound=r.h.randomInt(r.board,first,end);if(sound<first||sound>=end)return Fail(r,"Source313 sound RNG outside its prepared source range.");r.h.sound(r.z,uint16_t(sound),.5f);return true;}
bool SetImps(Run& r)
{
    if(r.z->mMindControlled||r.s->sourceStatus==1)return true;NativeExtendedZombie313Input input;
    if(!Input(r,input)||input.beforeDying)return true;
    if(!Preflight(r,NativeExtendedZombie313Operation::Hammer))return false;
    if(!r.h.effect(r.z,11,input.sourceAxisX-3,input.sourceAxisY+1))return false;if(!Refresh(r))return true;
    const int count=r.h.randomInt(r.board,1,4);if(count<1||count>3)return false;
    const uint16_t childKind=r.kind==212?213:37;
    for(int n=0;n<count;++n)
    {
        const float jitter=r.h.randomFloat(r.board,-1,1);if(!std::isfinite(jitter)||jitter< -1||jitter>1)return false;
        if(!r.h.spawn(r.z,childKind,input.sourceAxisX-3+jitter,false))return false;
        if(!Refresh(r))return true;
    }
    return true;
}
bool Throw(Run& r)
{
    if(r.z->mMindControlled||r.s->sourceStatus==1)return true;NativeExtendedZombie313Input i;
    if(!Input(r,i))return false;if(!Preflight(r,NativeExtendedZombie313Operation::Throw))return false;
    if(r.kind==43||r.kind==212)
    {if(!r.h.projectile(r.z,r.kind))return false;if(!Refresh(r))return true;r.s->ballRemoved=true;return true;}
    if(r.kind!=35&&r.kind!=36)return false;
    float x=0,y=0;if(!r.h.sourceNode(r.z,"Shoot",x,y))return false;
    Zombie* child=r.h.spawn(r.z,37,9.9f,false);const uint32_t uid=child?r.board->mZombies.DataArrayGetID(child):0;
    if(!uid||!Refresh(r))return uid!=0;
    NativeExtendedZombie313Input childInput;auto* cs=r.h.stateFor(child);
    if(!cs||!r.h.input(child,childInput)||!r.h.placeRoot(child,x,childInput.sourceRootY+(i.slope?.3f:0)))return false;
    child=r.board->mZombies.DataArrayTryToGet(uid);cs=child?r.h.stateFor(child):nullptr;
    if(!cs||!ThrowNativeExtendedImp313(child,*cs,r.h))return false;
    if(!Refresh(r))return true;child=r.board->mZombies.DataArrayTryToGet(uid);
    if(child&&!r.h.transferThrowStatus(r.z,child))return false;if(!Refresh(r))return true;
    if(!SoundRange(r,110,112))return false;if(!Refresh(r))return true;r.s->carriedImpRemoved=true;return true;
}
bool Hammer(Run& r)
{
    NativeExtendedZombie313Input i;if(!Input(r,i))return false;if(i.beforeDying||r.s->sourceStatus==1)return true;
    if(!Preflight(r,NativeExtendedZombie313Operation::Hammer))return false;
    r.h.sound(r.z,74,.5f);r.h.shake(r.board,.05f);if(!Refresh(r))return true;
    NativeExtendedZombie313Box box{i.sourceAxisX+(r.z->mMindControlled?1.f:-1.f),i.sourceAxisY+(r.z->mMindControlled?-1.f:1.f),1.3f,2};
    std::vector<NativeExtendedZombie313Ref> hits;if(!r.h.contacts(r.z,box,hits))return false;std::set<int> columns;
    for(const auto& hit:hits)
    {
        if(hit.zombie)
        {auto* q=r.board->mZombies.DataArrayTryToGet(hit.localUID);if(Opposite(r.z,q)&&FusionGarg313Target313(GetNativeFusionZombieStatus(q),true,true))
            {if(!r.h.zombieDamage(q,4,1800))return false;if(!Refresh(r))return true;}}
        else if(!r.z->mMindControlled)
        {auto* p=r.board->mPlants.DataArrayTryToGet(hit.localUID);if(!PlantLive(p)||p->mRow!=r.z->mRow)continue;
         bool nut=false,big=false,cal=false,umbrella=false,canBite=false;int level=0;
         if(!r.h.plantTraits(p,nut,big,level,cal,umbrella,canBite))return false;
         if(GetNativePlantData(p)->kind!=1095&&!big)columns.insert(p->mPlantCol);}
    }
    // Source collects collided columns, then copies EVERY source plant in each
    // cell before applying callbacks (including non-overlapping layers).
    std::vector<uint32_t> plants;Plant* p=nullptr;
    while(r.board->IteratePlants(p))if(PlantLive(p)&&p->mRow==r.z->mRow&&columns.contains(p->mPlantCol))plants.push_back(r.board->mPlants.DataArrayGetID(p));
    for(uint32_t uid:plants)
    {
        p=r.board->mPlants.DataArrayTryToGet(uid);if(!PlantLive(p))continue;bool nut=false,big=false,cal=false,umbrella=false,canBite=false;int level=0;
        if(!r.h.plantTraits(p,nut,big,level,cal,umbrella,canBite))return false;
        const auto plan=PlanFusionUltimateGarg270PlantHit(GetNativePlantData(p)->kind,big,nut,level);
        if(plan==FusionUltimateGarg270PlantHit::Damage500||plan==FusionUltimateGarg270PlantHit::Damage1000)
        {r.h.flash(p);if(!r.h.plantDamage(p,plan==FusionUltimateGarg270PlantHit::Damage500?500:1000,0))return false;}
        else if(plan==FusionUltimateGarg270PlantHit::Crush)
        {const int sound=r.h.randomInt(r.board,8,10);if(sound<8||sound>=10||!r.h.crush(r.z,p,true,sound))return false;}
        if(!Refresh(r))return true;
    }
    return(r.kind==43||r.kind==212)?SetImps(r):true;
}
bool ProbeGarg(Run& r,const NativeExtendedZombie313Input& i)
{
    r.z->mIsEating=false;
    if(!r.z->mMindControlled&&i.sourceAxisX>=i.sourceRightBoundary)return true;
    // Source checks actual Axis.x against board rightboundary before overlap.
    // The shared input/preflight world gate owns special GridItem/IZE branches.
    NativeExtendedZombie313Box box{i.sourceShadowX+(r.z->mMindControlled?1.f:-1.f),i.sourceShadowY+1,1.3f,2};
    std::vector<NativeExtendedZombie313Ref> hits;if(!r.h.contacts(r.z,box,hits))return false;
    for(const auto& hit:hits)
    {
        if(hit.zombie)
        {auto* q=r.board->mZombies.DataArrayTryToGet(hit.localUID);if(Opposite(r.z,q)&&FusionGarg313Target313(GetNativeFusionZombieStatus(q),true,true)){r.z->mIsEating=true;break;}}
        else if(!r.z->mMindControlled)
        {auto* p=r.board->mPlants.DataArrayTryToGet(hit.localUID);if(PlantLive(p)&&p->mRow==r.z->mRow&&GetNativePlantData(p)->kind!=1095&&!IsFusionSourceFlyingKind(GetNativePlantData(p)->kind)){r.z->mIsEating=true;break;}}
    }
    return true;
}
bool BodyContacts(Run& r,const NativeExtendedZombie313Input& input)
{
    if(!FusionBalloon343Kind(r.kind)&&r.kind!=38&&r.kind!=39&&r.kind!=40&&r.kind!=210&&r.kind!=219)return true;
    std::vector<NativeExtendedZombie313Ref> hits;if(!r.h.bodyContacts(r.z,hits))return false;
    if(FusionBalloon343Kind(r.kind))
    {
        if(r.s->sourceStatus!=10)return true;
        for(const auto& ref:hits)if(!ref.zombie){auto* p=r.board->mPlants.DataArrayTryToGet(ref.localUID);if(PlantLive(p)&&p->mRow==r.z->mRow&&GetNativePlantData(p)->kind==913)return FallNativeExtendedZombie313(r.z,*r.s,r.h);}
        return true;
    }
    if(r.kind==38||r.kind==39)
    {
        std::vector<uint32_t> contacts;
        for(const auto& ref:hits)if(!ref.zombie)
        {
            auto* p=r.board->mPlants.DataArrayTryToGet(ref.localUID);if(!PlantLive(p)||p->mRow!=r.z->mRow)continue;contacts.push_back(ref.localUID);
            if(std::find(r.s->ironEnterContacts.begin(),r.s->ironEnterContacts.end(),ref.localUID)!=r.s->ironEnterContacts.end())continue;
            bool nut=false,big=false,cal=false,umbrella=false,canBite=false;int level=0;if(!r.h.plantTraits(p,nut,big,level,cal,umbrella,canBite))return false;
            // Source Iron Enter only caltrop plants; canBeBitten decides150 vscrush.
            if(!cal)continue;if(!Preflight(r,NativeExtendedZombie313Operation::IronContact))return false;
            if(canBite){if(!r.h.plantDamage(p,150,0))return false;r.h.flash(p);}
            else{const int sound=r.h.randomInt(r.board,8,10);if(sound<8||sound>=10||!r.h.crush(r.z,p,false,sound))return false;}
            if(!Refresh(r))return true;
        }
        r.s->ironEnterContacts=std::move(contacts);return true;
    }
    if((r.kind!=40&&r.kind!=210&&r.kind!=219)||input.changingRow)return true;
    for(const auto& ref:hits)if(!ref.zombie&&!r.z->mMindControlled)
    {
        auto* p=r.board->mPlants.DataArrayTryToGet(ref.localUID);if(!PlantLive(p)||p->mRow!=r.z->mRow)continue;
        bool nut=false,big=false,cal=false,umbrella=false,canBite=false;int level=0;if(!r.h.plantTraits(p,nut,big,level,cal,umbrella,canBite))return false;
        if(umbrella){GetNativeSourceZombieData(r.z)->originSpeed=0;return true;}if(cal)continue;
        if(!Preflight(r,NativeExtendedZombie313Operation::MachineContact))return false;
        bool bounce=true;if(p->mPlantHealth>999)
        {if(!r.h.plantDamage(p,1000,0))return false;r.h.flash(p);if(!Refresh(r))return true;auto* d=GetNativeSourceZombieData(r.z);d->originSpeed=(std::max)(.2f,d->originSpeed*.6f);}
        else if(!r.h.eatContact(r.z,ref,bounce))return false;
        if(!Refresh(r))return true;if(bounce){if(!SoundRange(r,54,56))return false;if(!Refresh(r))return true;return r.h.changeRow(r.z,2.f/r.s->uniqueSpeed);}
    }
    for(const auto& ref:hits)if(ref.zombie)
    {
        bool reacted=false;if(!Preflight(r,NativeExtendedZombie313Operation::MachineContact)||!r.h.eatContact(r.z,ref,reacted))return false;
        if(!Refresh(r))return true;if(reacted){if(!SoundRange(r,54,56))return false;if(!Refresh(r))return true;return r.h.changeRow(r.z,2.f/r.s->uniqueSpeed);}
    }
    return true;
}
}
bool NativeExtendedZombie313StateValid(uint16_t kind,const NativeExtendedZombie313State& s)
{
    const auto* g=FindFusionSourceGraph313(kind);if(!g||!FusionExtendedZombie313ViewValid(kind,s))return false;
    return true;
}
bool InitializeNativeExtendedZombie313(Zombie* z,NativeExtendedZombie313State& s,const NativeExtendedZombie313Hooks& h)
{
    if(!Host(z)||!Ready(h))return false;auto r=Capture(z,s,h);if(s.initialized)return !s.failed&&h.graph.stateFor(z)&&h.graph.stateFor(z)->localUID==r.uid;
    NativeExtendedZombie313Input input;if(!Input(r,input)||!Preflight(r,NativeExtendedZombie313Operation::Initialize)||!NativeSourceGraph313ClockTracksReady(z,h.graph))return false;
    if(!h.graph.stateFor(z))return false;
    s={};s.initialized=true;s.bornFrame=input.frame;
    s.sourceStatus=(r.kind==35||r.kind==36||r.kind==43||r.kind==212)?15:r.kind==213?16:(FusionBalloon343Kind(r.kind)||r.kind==26)?10:0;
    s.uniqueType=r.kind==38?4:r.kind==39?5:0;s.visible=r.kind!=37;s.rootFlip=z->mMindControlled;s.bombPresent=r.kind==26;
    if(FusionExtendedZombie347Kind(r.kind)&&!InitializeNativeExtended347Fields(z,s,h))return Fail(r,"Source347 genuine initialization failed after preflight.");
    if(r.kind==37)s.impMaximumVx=input.factoryAxisColumn<6?-2.f:-3.f;
    if(r.kind==35||r.kind==36){const int weapon=h.randomInt(r.board,0,3);if(weapon<0||weapon>2)return Fail(r,"Source313 weapon RNG invalid after preflight.");s.weapon=uint8_t(weapon);}
    if(r.kind==40){const int first=h.randomInt(r.board,0,3);if(first<0||first>2)return Fail(r,"Source313 Machine RNG invalid.");int type=first;if(input.eveWorld)type=h.randomInt(r.board,1,3);if(type<0||type>2)return Fail(r,"Source313 Machine alternate RNG invalid.");s.machineType=uint8_t(type);h.sound(z,53,.5f);}
    if(r.kind==26&&input.playMachineSound)h.sound(z,input.alternateMachineSound?92:76,1.f);
    if(FusionBalloon343Kind(r.kind)&&input.playMachineSound)h.sound(z,86,.5f);
    if(r.kind==212&&!h.sourceBoard212(z,true))return Fail(r,"Source212 Board lifetime initialization failed after preflight.");
    if(!InitializeNativeSourceGraph313(z,*h.graph.stateFor(z),h.graph))return Fail(r,"Source313 native graph initialization failed.");if(!Refresh(r))return true;
    return PlayNativeSourceGraph313(r.z,*h.graph.stateFor(r.z),0,"walk",0,h.graph);
}
bool ThrowNativeExtendedImp313(Zombie* z,NativeExtendedZombie313State& s,const NativeExtendedZombie313Hooks& h)
{
    if(!Host(z)||GetNativeSourceZombieData(z)->kind!=37||!Ready(h)||!s.initialized||s.failed)return false;
    auto r=Capture(z,s,h);if(!h.translateRoot(z,0,1.5f)||!Refresh(r))return false;s.sourceStatus=16;
    return PlayNativeSourceGraph313(r.z,*h.graph.stateFor(r.z),0,"thrown",0,h.graph);
}
bool ApplyNativeExtendedZombie313BodyDamage(Zombie* z,NativeExtendedZombie313State& s,int damage,const NativeExtendedZombie313Hooks& h)
{
    if(!Host(z)||!Ready(h)||!FusionExtendedZombie313OwnBodyDamage(GetNativeSourceZombieData(z)->kind))return false;
    auto r=Capture(z,s,h);NativeExtendedZombie313Input input;if(!Input(r,input)||s.failed||!s.initialized)return true;auto* data=GetNativeSourceZombieData(z);FusionExtendedBody313Plan plan;
    if(!TryPlanFusionExtendedBody313(data->kind,data->health,z->mBodyMaxHealth,damage,s.bodyStage,s.uniqueType,s.sourceStatus,z->mMindControlled,input.sourceShadowX,plan))return Fail(r,"Invalid source313 body subtraction plan.");
    if((plan.triggerThrow||plan.removeIronHead)&&!Preflight(r,NativeExtendedZombie313Operation::BodyChange))return Fail(r,"Source313 body callback unavailable before subtraction.");
    data->health=plan.health;z->mBodyHealth=NativeSourceZombieLivenessHealth(plan.health);s.bodyStage=plan.bodyStage;s.sourceStatus=int8_t(plan.sourceStatus);s.uniqueType=plan.uniqueType;
    if(plan.triggerThrow)h.graph.stateFor(z)->parameters.triggers|=FusionSourceGraph313Trigger(data->kind==221?FusionSourceGraph313Param::Fly:FusionSourceGraph313Param::Throw);
    if(plan.removeIronHead&&!h.ironHead(z))return Fail(r,"Source313 prepared LoseIronHead failed after source subtraction.");return true;
}
bool FallNativeExtendedZombie313(Zombie* z,NativeExtendedZombie313State& s,const NativeExtendedZombie313Hooks& h)
{
    if(!Host(z)||!Ready(h))return false;auto r=Capture(z,s,h);NativeExtendedZombie313Input input;FusionFlightArmor313Plan plan;
    if(!Input(r,input)||!TryPlanFusionFlightFall313(r.kind,s.fallen,input.inWater,z->mHelmMaxHealth,plan))return false;if(!plan.fallTriggered)return true;
    if(!Preflight(r,NativeExtendedZombie313Operation::Fall))return false;
    s.fallen=true;s.sourceStatus=int8_t(plan.sourceStatus);h.sound(z,77,.5f);if(plan.die2){h.retire(z,2);return true;}
    h.graph.stateFor(z)->parameters.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Fall);z->mHelmHealth=plan.health;z->mHelmMaxHealth=plan.maximum;if(!plan.health)z->mHelmType=HELMTYPE_NONE;
    return true; // Source Fall clears/reassigns armor reference, no invented detached armor FX.
}
bool ApplyNativeExtendedZombie313ArmorDamage(Zombie* z,NativeExtendedZombie313State& s,int damage,int& remaining,const NativeExtendedZombie313Hooks& h)
{
    if(ApplyNativeSnow363FirstArmor(z,damage,remaining))return true;
    if(FusionGroundShooter343Kind(GetNativeSourceZombieData(z)?GetNativeSourceZombieData(z)->kind:0xffff)&&ApplyNativeGround343FirstArmor(z,damage,remaining))return true;
    if(!Host(z)||!Ready(h))return false;auto r=Capture(z,s,h);NativeExtendedZombie313Input input;FusionFlightArmor313Plan plan;
    if(!Input(r,input)||!TryPlanFusionFlightArmor313(r.kind,s.sourceStatus,s.fallen,input.inWater,z->mHelmHealth,z->mHelmMaxHealth,damage,plan))return false;
    if(plan.fallTriggered){if(!FallNativeExtendedZombie313(z,s,h))return false;remaining=0;return true;}
    z->mHelmHealth=plan.health;if(!plan.health)z->mHelmType=HELMTYPE_NONE;remaining=plan.overflow;
    if(r.kind==29&&s.sourceStatus!=10&&!plan.clearArmor)
    {if(float(plan.health)<float(plan.maximum)*2/3&&s.firstArmorStage<1)s.firstArmorStage=1;if(float(plan.health)<float(plan.maximum)/3&&s.firstArmorStage<2)s.firstArmorStage=2;}
    return true; // Source method clears first-armor reference; direct DropArmor is a separate callback.
}
bool UpdateNativeExtendedZombie313(Zombie* z,NativeExtendedZombie313State& s,const NativeExtendedZombie313Hooks& h)
{
    if(!Host(z)||!Ready(h))return false;if(z->mDead||z->mBoard->mPaused||z->mApp->mGameScene!=SCENE_PLAYING)return true;
    if(!s.initialized||s.failed)return true;if(s.logicTick==z->mBoard->mMainCounter)return true;s.logicTick=z->mBoard->mMainCounter;
    auto r=Capture(z,s,h);NativeExtendedZombie313Input i;if(!Input(r,i))return Fail(r,"Source313 actual world input unavailable.");
    if(r.kind==37&&!s.visible&&i.frame>s.bornFrame)s.visible=true;
    if(FusionExtendedZombie313BaseLimbCallbacks(r.kind)&&s.sourceStatus==0&&!i.beforeDying&&((r.kind!=202&&r.kind!=218)||s.isThrow))
    {bool attacking=false;if(!h.baseContact(z,attacking))return Fail(r,"Source313 shared base contact unavailable.");if(!Refresh(r))return true;r.z->mIsEating=attacking;}
    if(r.kind==35||r.kind==36||r.kind==43||r.kind==212){if(!ProbeGarg(r,i))return Fail(r,"Source313 Garg contact failed.");}
    if(FusionExtendedZombie347Kind(r.kind))
    {if(!UpdateNativeExtended347Fields(z,s,h))return Fail(r,"Source347 prepared update failed.");if(!Refresh(r))return true;}
    if((r.kind==26||r.kind==205||r.kind==206)&&!i.beforeDying)
    {
        float vx=0,vy=0;if(r.kind!=206&&(!TryGetFusionKirov313Velocity(i.sourceSpeed,s.rootFlip,i.travelDebuff6,i.slope,i.sourceRootX,vx,vy)||!h.setVelocity(z,vx,vy)))return Fail(r,"Source313 Kirov native rigid velocity unavailable.");
        if(s.bombPresent)
        {
            std::vector<NativeExtendedZombie313Ref> hits;if(!h.contacts(z,{i.sourceShadowX,i.sourceShadowY,1,1},hits))return Fail(r,"Source313 Kirov contact query failed.");
            for(auto ref:hits)
            {
                bool target=false;if(ref.zombie){auto* q=r.board->mZombies.DataArrayTryToGet(ref.localUID);target=z->mMindControlled&&Opposite(z,q)&&FusionKirov313Target313(GetNativeFusionZombieStatus(q),true,true);}
                else{auto* p=r.board->mPlants.DataArrayTryToGet(ref.localUID);target=!z->mMindControlled&&PlantLive(p)&&p->mRow==z->mRow&&GetNativePlantData(p)->kind!=1095;}
                if(target){if(!Preflight(r,NativeExtendedZombie313Operation::KirovBomb))return Fail(r,"Kirov bomb closure unavailable before trigger/RNG.");h.graph.stateFor(z)->parameters.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Throw);s.bombPresent=false;if(!SoundRange(r,3,5))return false;if(!Refresh(r))return true;}
            }
        }
    }
    if(r.kind==37)
    {
        if(s.sourceStatus==16&&!i.beforeDying)
        {FusionImp313FlightStep flight;if(!TryStepFusionImp313Flight(0,s.impVerticalSpeed,s.impMaximumVx,i.sourceShadowY,i.landY,false,.01f,flight))return false;
         if(flight.triggerLand)h.graph.stateFor(z)->parameters.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Landing);
         else{s.impVerticalSpeed=flight.vy;if(!h.setVelocity(z,flight.vx,flight.vy))return Fail(r,"Source313 Imp native flight velocity failed.");}}
        if(s.adjustingImp)
        {if(i.sourceShadowY>i.landY){if(!h.translateRoot(z,0,-.03f))return false;}else{s.adjustingImp=false;if(!h.alignShadow(z,i.sourceShadowX,i.landY))return false;}}
        if(!i.changingRow&&!h.adjustImpFlyNode(z,i.sourceShadowX,i.landY))return Fail(r,"Source313 Imp genuine Fly subtree anchor unavailable.");
    }
    if((r.kind==40||r.kind==210||r.kind==219)&&i.slope)
    {if(i.sourceShadowX>=1.5f)s.uniqueSpeed=1;else if(z->mMindControlled)s.uniqueSpeed=.5f;else s.uniqueSpeed+=.01f;}
    if(!BodyContacts(r,i))return Fail(r,"Source313 actual body contact callback failed.");if(!Refresh(r))return true;
    return UpdateNativeSourceGraph313(r.z,*h.graph.stateFor(r.z),h.graph);
}
bool DispatchNativeExtendedZombie313Event(Zombie* z,FusionSourceGraph313Event event,int sourceInt,const NativeExtendedZombie313Hooks& h)
{
    if(FusionExtendedZombie347Kind(GetNativeSourceZombieData(z)?GetNativeSourceZombieData(z)->kind:0xffff))return DispatchNativeExtended347Event(z,event,sourceInt,h);
    if(!Host(z)||!Ready(h))return false;auto* s=h.stateFor(z);if(!s||!s->initialized||s->failed)return false;auto r=Capture(z,*s,h);NativeExtendedZombie313Input i;
    switch(event)
    {
    case FusionSourceGraph313Event::Throw:return Throw(r);
    case FusionSourceGraph313Event::Crash:
        if(r.kind!=26)return Hammer(r);
        if(!Input(r,i)||!Preflight(r,NativeExtendedZombie313Operation::KirovCrash))return false;
        if(!h.kirovCrash(z,i.sourceShadowX,i.sourceShadowY,2,1000))return false;if(Refresh(r))h.retire(r.z,2);return true;
    case FusionSourceGraph313Event::DieSound:h.sound(z,112,.5f);return true;
    case FusionSourceGraph313Event::FallSound:h.sound(z,74,.5f);h.shake(r.board,.05f);return true;
    case FusionSourceGraph313Event::Destroy:
        if(r.kind==38||r.kind==39){if(!Input(r,i)||!h.effect(z,54,i.sourceShadowX,i.sourceShadowY+.6f))return false;h.sound(z,43,.5f);}
        h.retire(z,2);return true;
    case FusionSourceGraph313Event::StartLand:s->sourceStatus=17;s->adjustingImp=true;return h.setVelocity(z,-1.5f,0);
    case FusionSourceGraph313Event::Land:s->sourceStatus=0;return h.setVelocity(z,0,0);
    case FusionSourceGraph313Event::FallComplete:s->sourceStatus=0;return true;
    case FusionSourceGraph313Event::Particle:
        if(!Input(r,i)||!h.effect(z,11,i.sourceAxisX,i.sourceAxisY))return false;if(Refresh(r))r.s->sourceStatus=0;return true;
    case FusionSourceGraph313Event::Bomb:return h.kirovBomb(z);
    case FusionSourceGraph313Event::ReplaceBomb:s->bombPresent=true;return true;
    case FusionSourceGraph313Event::Bite:return h.baseBite(z);
    // Existing common typed bite/EatEffect is Root's graph event wrapper; it
    // dispatches only genuine base-limb families37/213 and grounded25/29.
    default:return false;
    }
}
bool MindControlNativeExtendedZombie313(Zombie* z,int level,const NativeExtendedZombie313Hooks& h)
{
    if(!Host(z)||!Ready(h))return false;const auto kind=GetNativeSourceZombieData(z)->kind;
    if(kind!=38&&kind!=39)return false;
    // Actual override367C70 returns immediately for normal controlLevel0.
    if(level<1||z->mMindControlled)return true;
    std::string error;if(!h.preflight(z,NativeExtendedZombie313Operation::IronContact,&error))return false;
    return h.ironMindControl(z);
}
bool SetNativeExtendedZombie313BeforeDying(Zombie* z,NativeExtendedZombie313State& s)
{
    if(!Host(z)||!FusionExtendedZombie313BaseLimbCallbacks(GetNativeSourceZombieData(z)->kind))return false;
    s.beforeDying=true;return true;
}
bool GetNativeExtendedZombie313Semantics(Zombie* z,const NativeExtendedZombie313State& s,const NativeExtendedZombie313Hooks& h,NativeExtendedZombie313Semantics& output)
{
    const auto* data=GetNativeSourceZombieData(z);if(!data||!h.input||!FusionExtendedZombie313ViewValid(data->kind,s))return false;
    NativeExtendedZombie313Input input;if(!h.input(z,input))return false;
    NativeExtendedZombie313Semantics result;result.kind=data->kind;result.status=s.sourceStatus;result.beforeDying=s.beforeDying;result.inWater=input.inWater;result.rootFlip=s.rootFlip;
    result.ownBodyDamage=FusionExtendedZombie313OwnBodyDamage(data->kind);result.baseLimbCallbacks=FusionExtendedZombie313BaseLimbCallbacks(data->kind);
    output=result;return true;
}
NativeExtendedZombie313Death BeginNativeExtendedZombie313Death(Zombie* z,NativeExtendedZombie313State& s,int& reason,const NativeExtendedZombie313Hooks& h)
{
    if(!Host(z)||!Ready(h))return NativeExtendedZombie313Death::NotHandled;auto r=Capture(z,s,h);if(!s.initialized||s.failed)return NativeExtendedZombie313Death::Refused;
    if(s.deathConsumed)return NativeExtendedZombie313Death::Proceed;
    int normalized;if(!TryNormalizeFusionExtendedDeath313(r.kind,reason,normalized))return NativeExtendedZombie313Death::Refused;
    NativeExtendedZombie313Input input;if(!Input(r,input))return NativeExtendedZombie313Death::Refused;
    const auto operation=r.kind==40?NativeExtendedZombie313Operation::MachineDeath:r.kind==26?NativeExtendedZombie313Operation::KirovDeath:NativeExtendedZombie313Operation::IronDeath;
    if(!Preflight(r,operation))return NativeExtendedZombie313Death::Refused;
    s.deathConsumed=true;reason=normalized;s.sourceStatus=1;
    if(FusionExtendedZombie347Kind(r.kind))return BeforeRetireNativeExtended347(z,reason,h)?(Refresh(r)?NativeExtendedZombie313Death::Proceed:NativeExtendedZombie313Death::OwnerRetired):NativeExtendedZombie313Death::Refused;
    if(r.kind==212&&!h.sourceBoard212(z,false))return NativeExtendedZombie313Death::Refused;
    if(!Refresh(r))return NativeExtendedZombie313Death::OwnerRetired;
    if(r.kind==40)
    {if(!h.machineDeath(z,s.machineType)){Fail(r,"Source40 prepared death failed after event consumption; no retry.");return NativeExtendedZombie313Death::Refused;}return Refresh(r)?NativeExtendedZombie313Death::Proceed:NativeExtendedZombie313Death::OwnerRetired;}
    if(r.kind==26)
    {h.sound(z,43,.5f);if(!h.effect(z,34,input.sourceShadowX+(input.sourceStatusLevel<10?0:-1.f),input.sourceShadowY+(input.sourceStatusLevel<10?1.5f:.3f)))return NativeExtendedZombie313Death::Refused;return NativeExtendedZombie313Death::Proceed;}
    if((r.kind==38||r.kind==39)&&reason==2)
    {h.sound(z,43,.5f);if(!h.effect(z,54,input.sourceShadowX,input.sourceShadowY+.6f))return NativeExtendedZombie313Death::Refused;}
    if(reason==0)h.graph.stateFor(z)->parameters.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Die);
    return NativeExtendedZombie313Death::Proceed;
}
}
