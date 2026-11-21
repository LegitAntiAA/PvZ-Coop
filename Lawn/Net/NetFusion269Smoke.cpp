#include "NetFusion269Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Coin.h"
#include "../GridItem.h"
#include "../Fusion/FusionNativeRollingActors269.h"
#include "../Fusion/FusionNativeDroppedCards269.h"
#include "../Fusion/FusionNativeCompanions269.h"
#include "../Fusion/FusionNativeRemainingLeaves269.h"
#include "../Fusion/FusionNativeWater268.h"
#include "../Fusion/FusionNativeCold.h"
#include "../Fusion/FusionMorePlants269BackendRules.h"
#include "../Fusion/FusionBasePlantRules.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativeStar269.h"
#include "../Fusion/FusionNativeHypno269.h"
#include "../Fusion/FusionNativeDoom269.h"
#include "../Fusion/FusionNativeSolar269.h"
#include "../Fusion/FusionNativeZombies.h"
#include "../Fusion/FusionNativeZombieVisual.h"
#include "../Fusion/FusionNativeWorldActors269.h"
#include "../Fusion/FusionNativeLaserConnections269.h"
#include "../Fusion/FusionNativeWorldRemainder269.h"
#include "../Fusion/FusionNativeLastPlants269.h"
#include "../Fusion/FusionNativeEquipment.h"
#include "../Fusion/FusionNativeEffects269.h"
#include "../Fusion/FusionNativeUmbrellaBatch.h"
#include "../Fusion/FusionRecipe.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
namespace
{
using namespace LawnFusion;using LawnNet::NetTrace;
struct Spec {uint16_t kind;int col,row;};
const std::vector<Spec> groups[]={
    {{1082,2,0},{1093,2,1},{1094,2,2},{1095,2,3},{1104,2,4}},
    {{1084,3,0},{920,3,2},{1089,2,4}},
    {{1026,2,1},{924,3,3}},
    {{910,2,0},{930,2,2},{934,2,4}},
    {{916,3,0},{923,3,2},{1148,3,4}},
    {{251,2,0},{255,2,4},{928,8,2},{1150,2,2}},
    {{244,5,1},{245,1,3},{241,2,4}},
    {{905,1,0},{911,3,0},{912,1,4},{922,3,2},{1110,3,3},{1169,3,4},{1178,5,1}},
    {{917,2,0},{1041,3,2},{1147,3,3},{1157,6,3},{1164,5,4},{1166,7,0}},
    {{242,2,0},{243,2,1},{249,2,2},{254,4,3},{256,2,4}},
    {{248,3,4},{250,5,0},{1151,2,1},{1174,3,5},{1175,2,3}},
    {{34,2,0},{909,2,1},{926,2,4},{918,2,5},{1056,5,2}},
    {{253,3,2}},
    {{937,3,0}}};
struct Ref {uint16_t id=0;uint32_t token=0;};
struct CherryWitness {uint32_t token=0;uint16_t target=0;uint32_t targetToken=0;std::array<int,2> hp{};};
struct State
{
    int group=0,stage=0,waiting=0,entry=-1,start=-1,introChecks=0,mask=0,selectedGroup=-1,completedGroups=0,completedPlants=0;
    int ui=0,uiPhase=0,moneyBefore=0,moneyAfter=0,moneyTick=0,invulnerableSamples=0,firstEatingTick=-1,hotkeys=0;
    uint32_t since=0,chat=0,wheel=0,epoch=0,previousEpoch=0,introFirst=0,introLast=0,samples=0,diagnostic=0;
    bool configured=false,setup=false,pauseRequested=false,resumeRequested=false,baseline=false,idsSent=false,idsParsed=false,idsReady=false,selectorRead=false;
    bool coverDamaged=false,chargeFired=false,fullChargeFired=false,cloud=false,adaptive=false;
    std::array<Ref,8> actors{};std::array<Ref,2> items{};
    std::array<int,8> initialPlantHP{};std::array<unsigned,8> phases{};
    std::array<uint32_t,8> lastShots{};
    std::array<bool,8> shooting{},shot{},payloadSeen{};
    std::array<bool,2> captured{},released{},controlled{};
    std::array<bool,3> paid{};std::array<std::array<bool,2>,3> converted{};
    std::array<std::array<Ref,2>,3> moneyChildren{};
    std::array<std::array<bool,2>,3> moneyDecayed{},moneyRetired{};
    std::set<uint16_t> rewardCoins;std::map<uint16_t,CherryWitness> cherryWitness;
    std::array<bool,4> gone{},moving{},edgeSeen{};
    std::array<float,2> rollingX{};
    std::map<uint16_t,std::pair<float,float>> goldPaths;
    bool goldMotion=false,pit=false,putDownSeen=false,beanPayload=false;
    bool wheatGone=false,wheatAdvanced=false;Ref wheatChild{},wheatCard{};uint16_t wheatKind=0xffff;float wheatFirst=-1,wheatLast=0;int wheatLastProduction=-1;
    std::array<Ref,2> laserHelpers{};bool laserInitialPair=false,laserStandalone=false,laserHeld=false,laserHeldPairBroken=false,laserMoved=false,laserRepaired=false,laserBall=false;
    std::array<int,3> laserEnergyMin{{20000,20000,20000}},laserEnergyMax{};int laserUI=0;
    bool worldBeam=false,worldRay=false,worldHoming=false,worldFly=false,worldVolley=false,worldMeteorite=false,worldFX51=false,worldGrab=false,world30=false;
    unsigned worldRingPeak=0;std::set<uint16_t> worldRingIds;uint16_t chompVictim=0;
    std::array<Ref,2> cards{};std::array<Ref,4> planted{};
        int cardStep=0,cardSun=0;
    bool cardPutDown=false;
    std::array<bool,8> restEvent{},restAux{},restShot{};std::array<int,8> restMinHP{};
    bool fertilizeSeen=false,fertilizeUsed=false;Ref fertilize{},fertilizeTarget{},iceChild{};
    std::map<uint16_t,uint32_t> restChildren;
    std::array<std::map<uint16_t,int>,2> held{};
    std::vector<uint16_t> zombies;std::vector<int> initialHP,initialAir;std::vector<bool> hit;
    std::set<uint16_t> payloads;std::set<std::string> frames;std::map<std::string,int> draws;
}s;
const auto& Specs(){return groups[s.group];}
int Expected(){return (1<<int(Specs().size()))-1;}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool okay,const char* why){if(!okay)a->CoopSmokeFail(why);return okay;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();s.diagnostic=0;NetTrace("SMOKE_BATCH269_STAGE role=%s group=%d epoch=%u stage=%d",Role(a),s.group,s.epoch,stage);}
std::string Tag(const char* name){return "B269_"+std::to_string(s.group)+"_"+std::to_string(s.epoch)+"_"+name;}
bool Has(LawnApp* a,int who,const std::string& value){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==who&&line.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value)
{if(!Require(a,value.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Batch269 chat proof exceeded native limit"))return false;if(Has(a,Slot(a),value))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(value);return Has(a,Slot(a),value);}
bool Barrier(LawnApp* a,const char* name){const auto value=Tag(name);Send(a,value);return Has(a,0,value)&&Has(a,1,value);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Batch269 owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Escape(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Batch269 owned SDL Escape failed");}
bool OwnedKey(LawnApp* a,SDL_Keycode key)
{
    SDL_Event down{};down.type=SDL_EVENT_KEY_DOWN;down.key.windowID=SDL_GetWindowID(a->mSDLWindow);down.key.key=key;down.key.down=true;
    SDL_Event up=down;up.type=SDL_EVENT_KEY_UP;up.key.down=false;
    return Require(a,SDL_PushEvent(&down)&&SDL_PushEvent(&up),"Batch269 owned SDL hotkey failed");
}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Batch269 chooser wheel failed");return false;}
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_batch269_g"+std::to_string(s.group)+"_"+suffix;if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requested=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requested))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Batch269 fresh owned BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_BATCH269_RENDER role=%s group=%d frame=%s requestedDraw=%d capturedDraw=%d",Role(a),s.group,name.c_str(),requested,a->mCoopSmokeFrameDrawCount);return true;
}
Plant* At(Board* b,int col,int row,int kind)
{Plant* p=nullptr;while(b->IteratePlants(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&d&&d->kind==kind&&p->mPlantCol==col&&p->mRow==row)return p;}return nullptr;}
Plant* Actor(Board* b,int i)
{Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mNetId==s.actors[i].id)
 {const auto* d=GetNativePlantData(p);return d&&d->instanceToken==s.actors[i].token?p:nullptr;}return nullptr;}
Zombie* ZombieById(Board* b,uint16_t id)
{Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->mDead&&z->mNetId==id)return z;return nullptr;}
Plant* MakePlant(LawnApp* a,uint16_t kind,int col,int row,int owner=-1)
{
    NativePlantData data;data.kind=kind;data.owner=uint8_t(owner<0?col%2:owner);
    for(std::size_t i=0;!IsNativeBasePlantKind(kind)&&i<GetFusionRecipeCount();++i)if(GetFusionRecipes()[i].result==kind){data.firstParent=GetFusionRecipes()[i].existing;data.secondParent=GetFusionRecipes()[i].used;break;}
    if(!IsNativeBasePlantKind(kind)&&data.firstParent==kFusionNoPlant&&data.secondParent==kFusionNoPlant)data.owner=LawnNet::NET_SHARED_OWNER;
    std::string error;if(!Require(a,EnsureNativePlantVisuals(a->mBoard,kind,&error),error.empty()?"Batch269 actual source model missing":error.c_str()))return nullptr;
    auto* p=a->mBoard->AddPlant(col,row,GetNativeSeedForPlant(kind),SEED_NONE,&data);const auto* bound=GetNativePlantData(p);
    if(!Require(a,p&&bound&&bound->kind==kind&&bound->instanceToken&&p->mNetId&&!p->mDead,"Batch269 initial real typed factory failed"))return nullptr;return p;
}
Zombie* MakeZombie(LawnApp* a,int row,float center,int health=20000000,bool walking=false,ZombieType kind=ZOMBIE_NORMAL,int maximum=0)
{
    auto* b=a->mBoard;auto* z=b->AddZombieInRow(kind,row,0);
    if(!Require(a,z&&z->mNetId&&a->ReanimationTryToGet(z->mBodyReanimID),"Batch269 native control factory failed"))return nullptr;
    z->mBodyHealth=health;z->mBodyMaxHealth=maximum?maximum:health;
    if(kind==ZOMBIE_BALLOON)z->mFlyingHealth=z->mFlyingMaxHealth=20000000;
    const auto rect=z->GetZombieRect();z->mPosX+=center-float(rect.mX+rect.mWidth*.5);z->mX=int(z->mPosX);
    z->mPosY=z->GetPosYBasedOnRow(row);z->mY=int(z->mPosY);z->CheckForPool();z->CompletePoolEntry();
    if(!walking){z->mVelX=0;z->ApplyAnimRate(0);}
    s.zombies.push_back(z->mNetId);
    NetTrace("SMOKE_BATCH269_INITIAL_NPC group=%d index=%zu id=%u type=%d row=%d x=%.9g body=%d maximum=%d walking=%d initialOnly=1 sourceAI=NotClaimed",s.group,s.zombies.size()-1,z->mNetId,int(kind),row,z->mPosX,health,z->mBodyMaxHealth,walking?1:0);return z;
}
float X(Board* b,int col,int row){return float(b->GetCellCenterX(col,row));}
bool AlignInitialChewContact(LawnApp* a,int plantIndex,int zombieIndex)
{
    auto* b=a->mBoard;const auto& spec=Specs()[plantIndex];
    auto* target=At(b,spec.col,spec.row,spec.kind);auto* eater=ZombieById(b,s.zombies[zombieIndex]);
    if(!Require(a,target&&eater,"Batch269 initial contact actors missing"))return false;
    const auto plantRect=target->GetPlantRect(),attack=eater->GetZombieAttackRect();
    eater->mPosX+=plantRect.mX+plantRect.mWidth*.5f-attack.mX-attack.mWidth*.5f;eater->mX=int(eater->mPosX);
    const auto aligned=eater->GetZombieAttackRect();
    if(!Require(a,GetRectOverlap(aligned,plantRect)>=20&&eater->FindPlantTarget(ATTACKTYPE_CHEW)==target,"Batch269 native initial attack rect did not target source"))return false;
    NetTrace("SMOKE_BATCH269_INITIAL_CONTACT kind=%u source=%u token=%u eater=%u overlap=%d actualFindTarget=1 initialOnly=1",spec.kind,target->mNetId,GetNativePlantData(target)->instanceToken,eater->mNetId,GetRectOverlap(aligned,plantRect));
    return true;
}
bool PreflightBatch269Resources(LawnApp* a)
{
    auto* b=a->mBoard;std::set<uint16_t> plants;for(const auto& group:groups)for(const auto& spec:group)plants.insert(spec.kind);
    unsigned errors=0,checked=0,skipped=0;
    for(uint16_t kind:plants)
    {
        if(GetNativeSeedForPlant(kind)==SEED_NONE)
        {++skipped;NetTrace("SMOKE_BATCH269_PREFLIGHT resource=plant kind=%u result=Skipped reason=SeedNONE actors=0",kind);continue;}
        std::string modelError,portraitError;const bool model=EnsureNativePlantVisuals(b,kind,&modelError);
        const bool portrait=EnsureNativePortraitVisuals(b,kind,&portraitError);++checked;
        if(!model){++errors;NetTrace("SMOKE_BATCH269_PREFLIGHT resource=model kind=%u result=Failed error=%s",kind,modelError.c_str());}
        if(!portrait){++errors;NetTrace("SMOKE_BATCH269_PREFLIGHT resource=portrait kind=%u result=Failed error=%s",kind,portraitError.c_str());}
        if(model&&portrait)NetTrace("SMOKE_BATCH269_PREFLIGHT resource=plant kind=%u result=Passed actors=0",kind);
        ReleaseUnusedNativeVisuals(b); // No actors yet: release each bundle before the next native GPU load.
    }
    constexpr uint16_t shots[]={0,8,9,13,14,15,20,21,24,28,29,30,37,39,41,42,43,44,45,46,47,48,49,51,55,59,60,86,91,97};
    for(uint16_t kind:shots)
    {
        std::string error;const bool okay=EnsureNativeProjectileVisuals(b,kind,&error);++checked;
        NetTrace("SMOKE_BATCH269_PREFLIGHT resource=projectile kind=%u result=%s error=%s actors=0",kind,okay?"Passed":"Failed",error.c_str());
        if(!okay)++errors;ReleaseUnusedNativeVisuals(b);
    }
    constexpr uint16_t npcs[]={0,2,4,8,11,12,13,41,42,111,109};
    for(uint16_t kind:npcs)
    {
        std::string error;const bool okay=EnsureNativeSourceZombieVisuals(b,kind,&error);++checked;
        NetTrace("SMOKE_BATCH269_PREFLIGHT resource=sourceNPC kind=%u result=%s error=%s actors=0",kind,okay?"Passed":"Failed",error.c_str());
        if(!okay)++errors;ReleaseUnusedNativeSourceZombieVisuals(b);
    }
    NetTrace("SMOKE_BATCH269_PREFLIGHT_COMPLETE uniqueMain=%zu checked=%u skipped=%u errors=%u actors=0 runtimeAcceptance=0",plants.size(),checked,skipped,errors);
    return Require(a,errors==0,"Batch269 native resource batch preflight failed; see all per-kind errors");
}
bool PreflightWorld269Resources(LawnApp* a)
{
    auto* b=a->mBoard;unsigned errors=0;
    for(const auto& spec:Specs())
    {std::string model,portrait;const bool okay=EnsureNativePlantVisuals(b,spec.kind,&model),preview=EnsureNativePortraitVisuals(b,spec.kind,&portrait);
     NetTrace("SMOKE_BATCH269_WORLD_PREFLIGHT kind=%u model=%d portrait=%d modelError=%s portraitError=%s",spec.kind,okay,preview,model.c_str(),portrait.c_str());errors+=!okay;errors+=!preview;ReleaseUnusedNativeVisuals(b);}
    for(uint16_t kind:{29,30,49,91})
    {std::string error;const bool okay=EnsureNativeProjectileVisuals(b,kind,&error);errors+=!okay;NetTrace("SMOKE_BATCH269_WORLD_PREFLIGHT projectile=%u okay=%d error=%s",kind,okay,error.c_str());ReleaseUnusedNativeVisuals(b);}
    for(uint16_t kind:{49,51,99})
    {std::string error;const bool okay=EnsureNativeSourceEffect269(b,kind,&error);errors+=!okay;NetTrace("SMOKE_BATCH269_WORLD_PREFLIGHT effect=%u okay=%d error=%s",kind,okay,error.c_str());}
    const bool fly=EnsureNativeWorldRemainder269Assets(b,NativeWorldRemainder269Family::LourFly,0);
    const bool meteorite=EnsureNativeWorldRemainder269Assets(b,NativeWorldRemainder269Family::Meteorite,2);
    const bool torch=EnsureNativeLastPlants269TorchConversions(b);errors+=!fly;errors+=!meteorite;errors+=!torch;
    NetTrace("SMOKE_BATCH269_WORLD_PREFLIGHT_COMPLETE mains=5 Lour=%d Meteorite2=%d Torch=%d errors=%u actors=0 acceptance=0",fly,meteorite,torch,errors);
    return Require(a,!errors,"Batch269 world5 resource preflight failed; see complete errors");
}
bool MakeWorld269Feed(LawnApp* a)
{
    auto* b=a->mBoard;auto* torch=At(b,5,2,1056);float x=0,y=0;
    if(!torch||!GetNativeSourcePlantNodePoint(torch,"",x,y))return Require(a,false,"Batch269 initial1056 source anchor missing");
    x=X(b,3,2);y-=float(.6*GetNativeFusionPixelsY(b));
    auto* q=b->AddProjectile(int(x-20),int(y-20),torch->mRenderOrder-1,2,PROJECTILE_PEA);
    if(!q)return Require(a,false,"Batch269 initial canonical29 allocation failed");
    q->mProjectileType=PROJECTILE_PEA;q->mMotionType=MOTION_STRAIGHT;q->mPosX=x-20;q->mPosY=y-20;q->mPosZ=q->mVelZ=q->mAccZ=0;
    q->mShadowY=y;q->mVelX=float(.06*kFusionBasePixelsX);q->mVelY=q->mRotation=q->mRotationSpeed=0;q->mX=int(q->mPosX);q->mY=int(q->mPosY);
    NativeProjectileData data;data.kind=29;data.damage=20;
    const bool okay=AttachNativeProjectile(b,q,data);if(!okay)q->Die();
    NetTrace("SMOKE_BATCH269_WORLD_INITIAL_FEED kind=29 way=0 damage=20 id=%u initialOnly=1 canonicalMetadata=%d",q->mNetId,okay);
    return Require(a,okay,"Batch269 canonical29 initial metadata refused");
}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;
    if(!Require(a,a->NetIsHost()&&!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize,"Batch269 setup requires initial unpaused empty world")||
        !Require(a,ConfigureNativeFusionBackground(b,(s.group==8||s.group==10||s.group==11)?BACKGROUND_3_POOL:BACKGROUND_1_DAY),"Batch269 Day terrain refused"))return false;
    if(s.group==7&&!PreflightBatch269Resources(a))return false;
    if(s.group==11&&!PreflightWorld269Resources(a))return false;
    if(s.group==13)
    {
        std::string error;unsigned errors=0;
        const bool ball=EnsureNativeLaser269Visuals(b,&error);
        NetTrace("SMOKE_BATCH269_LASER_PREFLIGHT ball4937=%d error=%s actors=0",ball,error.c_str());errors+=!ball;
        for(uint16_t kind:{uint16_t(99),uint16_t(100)})
        {error.clear();const bool ready=EnsureNativeSourceEffect269(b,kind,&error);errors+=!ready;
         NetTrace("SMOKE_BATCH269_LASER_PREFLIGHT effect=%u ready=%d error=%s actors=0",kind,ready,error.c_str());}
        if(!Require(a,!errors,"Batch269937 native resource preflight failed"))return false;
    }
    if(s.group==3&&!Require(a,EnsureNativeSourceEffect269(b,52),"Batch269 source52 resource preflight failed"))return false;
    if(s.group==6)
    {if(!MakePlant(a,14,5,1)||!MakePlant(a,1030,4,1)||!MakePlant(a,1030,6,1)||
        !MakePlant(a,6,1,3)||!MakePlant(a,1165,2,4))return false;}
    if(s.group==7)
    {for(int i:{1,3,4,5})if(!MakePlant(a,0,Specs()[i].col,Specs()[i].row))return false;
     if(!MakePlant(a,0,0,1))return false;}
    if(s.group==8)
    {for(int i:{1,2,3})if(!MakePlant(a,12,Specs()[i].col,Specs()[i].row))return false;
     if(!MakePlant(a,0,5,4))return false;}
    if(s.group==9)
    {if(!MakePlant(a,0,0,3))return false;auto* target=MakePlant(a,3,8,3,1);if(!target)return false;
     s.fertilizeTarget={target->mNetId,GetNativePlantData(target)->instanceToken};}
    for(const auto& spec:Specs())
    {auto* p=s.group==5&&spec.kind<900?SpawnNativeRollingActor269(b,spec.kind,spec.col,spec.row):MakePlant(a,spec.kind,spec.col,spec.row,s.group==8&&spec.kind==1157?1:-1);
     if(!Require(a,p!=nullptr,"Batch269 initial source factory failed"))return false;}
    if(s.group==0)
    {
        if(!MakeZombie(a,0,X(b,8,0),20000000,false,ZOMBIE_BALLOON))return false;
        for(int row=1;row<5;++row)if(!MakeZombie(a,row,X(b,0,row)))return false;
        if(!MakeZombie(a,3,X(b,2,3)+65,20000000,true))return false;
    }
    else if(s.group==1)
    {
        for(int row:{0,2})if(!MakePlant(a,1081,1,row)||!MakeZombie(a,row,X(b,0,row)))return false;
        if(!MakeZombie(a,4,X(b,2,4)+65,20000000,true))return false;
    }
    else if(s.group==2)
    {
        for(int row:{1,3})
            if(!MakeZombie(a,row,X(b,7,row),5000,false,ZOMBIE_NORMAL,20000)||!MakeZombie(a,row,X(b,8,row)))return false;
        for(int row:{0,4})if(!Require(a,CreateNativeEquipment(b,FusionEquipmentType::Bucket,X(b,0,row),float(b->GridToPixelY(0,row)+30),false)!=nullptr,"Batch269 initial real Bucket factory failed"))return false;
    }
    else if(s.group==3)
    {
        if(!MakeZombie(a,0,X(b,4,0))||!MakeZombie(a,1,X(b,4,1))||!MakeZombie(a,2,X(b,8,2))||!MakeZombie(a,4,X(b,8,4)))return false;
    }
    else if(s.group==4)
    {
        GetNativeContext(b)->money=6000; // Initial isolated budget for source1000/1000/3000 UI skills.
        for(int i=0;i<3;++i)
        {const int row=Specs()[i].row;if(!MakeZombie(a,row,X(b,4,row),500,false,ZOMBIE_NORMAL,10000)||
            !MakeZombie(a,row,X(b,5,row),i==2?2000:3500,false,ZOMBIE_NORMAL,10000))return false;}
    }
    else if(s.group==5)
    {
        // Real planar helper shots cross the GoldMagnet's adjacent source rows.
        for(int row:{1,3})if(!MakePlant(a,0,0,row))return false;
        if(!MakeZombie(a,0,X(b,5,0))||!MakeZombie(a,4,X(b,5,4))||!MakeZombie(a,0,X(b,0,0))||
           !MakeZombie(a,2,X(b,8,2))||!MakeZombie(a,1,X(b,9,1))||!MakeZombie(a,3,X(b,9,3)))return false;
    }
    else if(s.group==6)
    {
        // Actual same-cell source setup; no callback/timer edits after factory.
        if(!MakeZombie(a,4,X(b,9,4)))return false;
    }
    else if(s.group==7)
    {
        if(!MakeZombie(a,0,X(b,1,0)+140,20000000,true)||!MakeZombie(a,0,X(b,8,0))||!MakeZombie(a,2,X(b,8,2))||
           !MakeZombie(a,3,X(b,8,3),20000000,false,ZOMBIE_BALLOON)||!MakeZombie(a,4,X(b,3,4)+140,20000000,true,ZOMBIE_PAIL)||
           !MakeZombie(a,4,X(b,9,4),20000000,false,ZOMBIE_BALLOON)||!MakeZombie(a,1,X(b,9,1)))return false;
        if(!AlignInitialChewContact(a,0,0)||!AlignInitialChewContact(a,5,4))return false;
        if(!CreateNativeEquipment(b,FusionEquipmentType::Bucket,X(b,0,4),float(b->GridToPixelY(0,4)+30),false))return false;
    }
    else if(s.group==8)
    {
        if(!MakeZombie(a,0,X(b,2,0)+140,20000000,true)||!MakeZombie(a,2,X(b,3,2)+140,20000000,true)||
           !MakeZombie(a,3,X(b,9,3))||!MakeZombie(a,4,X(b,5,4)+140,20000000,true)||
           !MakeZombie(a,0,X(b,9,0))||!MakeZombie(a,1,X(b,9,1)))return false;
        for(int row:{0,4})if(!CreateNativeEquipment(b,FusionEquipmentType::Bucket,X(b,0,row),float(b->GridToPixelY(0,row)+30),false))return false;
    }
    else if(s.group==9)
    {for(int row:{0,1,2,4})if(!MakeZombie(a,row,X(b,row==2?4:8,row)))return false;}
    else if(s.group==10)
    {if(!MakeZombie(a,4,X(b,4,4))||!MakeZombie(a,5,X(b,4,5))||!MakeZombie(a,3,X(b,3,3))||!MakeZombie(a,1,X(b,2,1)+140,20000000,true))return false;}
    else if(s.group==11)
    {
        if(!MakeZombie(a,0,X(b,4,0))||!MakeZombie(a,4,X(b,4,4),80)||!MakeZombie(a,1,X(b,0,1))||
           !MakeZombie(a,2,X(b,5,2)+150,20000000,true)||!MakeWorld269Feed(a))return false;
    }
    else if(s.group==12){if(!MakeZombie(a,0,X(b,9,0)))return false;}
    else if(s.group==13)
    {if(!MakePlant(a,937,3,1,1)||!MakePlant(a,937,3,2,1))return false;
     for(int row=0;row<3;++row)if(!MakeZombie(a,row,X(b,8,row)))return false;}
    s.setup=true;s.pauseRequested=Escape(a);
    NetTrace("SMOKE_BATCH269_SETUP group=%d mainKinds=%zu targets=%zu initialFactory=1 sourceAI=NotClaimed postSetupActorWrites=0",s.group,Specs().size(),s.zombies.size());return true;
}
int TargetCount(){constexpr int counts[]={6,3,4,4,6,6,1,7,6,4,4,4,1,3};return counts[s.group];}
bool TargetManifest(LawnApp* a)
{
    if(s.idsReady)return true;const auto prefix=Tag("IDS")+" ";
    if(a->NetIsHost())
    {
        std::string text=prefix;for(auto id:s.zombies)text+=std::to_string(id)+" ";text.pop_back();
        if(!s.idsSent){if(!Send(a,text))return false;s.idsSent=true;NetTrace("SMOKE_BATCH269_TARGETS role=host group=%d manifest=%s",s.group,text.c_str());}
        if(!Has(a,1,Tag("IDS_ACK")))return false;
    }
    else
    {
        if(!s.idsParsed)for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(prefix))
        {std::istringstream in(line.mText.substr(prefix.size()));unsigned id=0;while(in>>id){if(!Require(a,id>0&&id<=65535,"Batch269 invalid target UID"))return false;s.zombies.push_back(uint16_t(id));}s.idsParsed=true;break;}
        if(!s.idsParsed||!Send(a,Tag("IDS_ACK")))return false;
    }
    if(!Require(a,s.zombies.size()==std::size_t(TargetCount()),"Batch269 target manifest count differs"))return false;
    s.initialHP.resize(s.zombies.size());s.initialAir.resize(s.zombies.size());s.hit.resize(s.zombies.size());
    for(size_t i=0;i<s.zombies.size();++i){auto* z=ZombieById(a->mBoard,s.zombies[i]);if(!z)return false;s.initialHP[i]=z->mBodyHealth;s.initialAir[i]=z->mFlyingHealth;}
    s.idsReady=true;return true;
}
bool Baseline(LawnApp* a)
{
    if(!TargetManifest(a)||GetNativeFusionRowCount(a->mBoard)!=((s.group==8||s.group==10||s.group==11)?6:5))return false;
    if(s.group==3&&!Require(a,EnsureNativeSourceEffect269(a->mBoard,52),"Batch269 peer source52 resource preflight failed"))return false;
    for(int i=0;i<int(Specs().size());++i)
    {
        auto* p=At(a->mBoard,Specs()[i].col,Specs()[i].row,Specs()[i].kind);
        if(s.group==5&&i<2)
        {p=nullptr;while(a->mBoard->IteratePlants(p))if(!p->mDead&&p->mRow==Specs()[i].row)
          if(const auto* r=GetNativeRollingActor269State(p))if(r->factoryKind==Specs()[i].kind)break;}
        const auto* d=GetNativePlantData(p);if(!p||!d)return false;
        if(!s.actors[i].id){s.actors[i]={p->mNetId,d->instanceToken};s.initialPlantHP[i]=p->mPlantHealth;s.restMinHP[i]=p->mPlantHealth;if(s.group==5&&i<2)s.rollingX[i]=float(p->mX);
            NetTrace("SMOKE_BATCH269_BIRTH role=%s group=%d index=%d kind=%u id=%u token=%u health=%d",Role(a),s.group,i,d->kind,p->mNetId,d->instanceToken,p->mPlantHealth);}
    }
    if(s.group==13)
        for(int i=0;i<2;++i)if(!s.laserHelpers[i].id)
        {auto* helper=At(a->mBoard,3,i+1,937);const auto* d=GetNativePlantData(helper);if(!helper||!d)return false;s.laserHelpers[i]={helper->mNetId,d->instanceToken};
         NetTrace("SMOKE_BATCH269_LASER_HELPER role=%s index=%d id=%u token=%u owner=%u countedMAIN=0",Role(a),i,helper->mNetId,d->instanceToken,d->owner);}
    if(s.group==9&&!s.fertilizeTarget.id)
    {auto* p=At(a->mBoard,8,3,3);const auto* d=GetNativePlantData(p);if(!p||!d)return false;s.fertilizeTarget={p->mNetId,d->instanceToken};}
    if((s.group==2||s.group==7||s.group==8)&&!s.items[0].id)
    {
        std::vector<Ref> refs;Coin* coin=nullptr;while(a->mBoard->IterateCoins(coin))if(const auto* d=GetNativeEquipmentData(coin))
            if(!coin->mDead&&d->type==FusionEquipmentType::Bucket)refs.push_back({coin->mNetId,d->instanceToken});
        if(refs.size()!=std::size_t(s.group==7?1:2))return false;std::sort(refs.begin(),refs.end(),[](const Ref& x,const Ref& y){return x.id<y.id;});s.items[0]=refs[0];if(refs.size()>1)s.items[1]=refs[1];
    }
    return true;
}
Coin* Card(Board* b,int index)
{Coin* c=nullptr;while(b->IterateCoins(c))if(!c->mDead&&c->mNetId==s.cards[index].id)
 if(const auto* d=GetNativeDroppedCard269Data(c))if(d->instanceToken==s.cards[index].token)return c;return nullptr;}
bool RightClick(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=SDL_GetWindowID(a->mSDLWindow);d.button.button=SDL_BUTTON_RIGHT;d.button.clicks=1;d.button.down=true;
 SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Batch269 owned SDL PutDown failed");}
void CardUI(LawnApp* a)
{
    if(s.group!=6||!a->NetIsClient()||a->mBoard->mPaused||s.cardStep>=4)return;
    auto* b=a->mBoard;const int index=s.cardStep==0?0:1;auto* c=Card(b,index);const auto* d=GetNativeDroppedCard269Data(c);
    const int uses=index==0?0:s.cardStep-1;
    if(s.uiPhase==0&&c&&d&&d->landed&&d->heldSlot==255&&c->mTimesDropped==uses)
    {s.cardSun=b->mSunMoney;if(BoardClick(a,c->mPosX+25,c->mPosY+35))s.uiPhase=1;return;}
    if(s.uiPhase==1&&c&&d&&d->heldSlot==1)
    {
        if(!s.cardPutDown){if(RightClick(a))s.uiPhase=2;return;}
        const int col=s.cardStep==0?8:4+s.cardStep;const int row=s.cardStep==0?0:3;
        if(BoardClick(a,X(b,col,row),float(b->GridToPixelY(col,row)+50)))s.uiPhase=3;return;
    }
    if(s.uiPhase==2&&c&&d&&d->heldSlot==255&&b->mCursorObject->mCursorType==CURSOR_TYPE_NORMAL)
    {s.cardPutDown=true;s.putDownSeen=true;s.uiPhase=0;NetTrace("SMOKE_BATCH269_CARD_PUTDOWN role=client id=%u token=%u uses=%d actualSDL=1",c->mNetId,d->instanceToken,c->mTimesDropped);return;}
    if(s.uiPhase==3)
    {
        const int col=s.cardStep==0?8:4+s.cardStep;const int row=s.cardStep==0?0:3;
        auto* p=At(b,col,row,index==0?1161:6);const auto* pd=GetNativePlantData(p);if(!p||!pd)return;
        if(b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL||(c&&(d->heldSlot!=255||c->mTimesDropped!=uses+1)))return;
        if(!Require(a,b->mSunMoney==s.cardSun&&pd->owner==1&&!pd->packetId&&!pd->randomOrigin,"Batch269 zero-cost source card placement/provenance differs"))return;
        s.planted[s.cardStep]={p->mNetId,pd->instanceToken};
        NetTrace("SMOKE_BATCH269_CARD_PLANTED role=client step=%d card=%u cardToken=%u kind=%u plant=%u token=%u uses=%d cost=0 sun=%d cursorPutDown=1 actualSDL=1",s.cardStep,s.cards[index].id,s.cards[index].token,pd->kind,p->mNetId,pd->instanceToken,uses+1,b->mSunMoney);
        ++s.cardStep;s.uiPhase=0;
    }
}
bool ObserveNew(LawnApp* a)
{
    auto* b=a->mBoard;
    if(s.group==5)
    {
        if(!Require(a,!s.hit[2],"Batch269 rolling behind-origin control unexpectedly hit"))return false;
        for(int i=0;i<2;++i)
        {auto* p=Actor(b,i);if(p){s.moving[i]|=p->mX>s.rollingX[i]+80;s.edgeSeen[i]|=p->mX>X(b,9,p->mRow);
          if(const auto* r=GetNativeRollingActor269State(p))if(!Require(a,r->factoryKind==Specs()[i].kind&&r->kind==255,"Batch269 rolling source factory provenance changed"))return false;}
         else if(s.baseline){if(!Require(a,s.moving[i]&&s.edgeSeen[i]&&s.hit[i],"Batch269 rolling retired without real travel/edge/contact proof"))return false;s.gone[i]=true;}}
        s.pit|=b->GetGridItemAt(GRIDITEM_CRATER,8,2)!=nullptr;
        if(s.baseline&&!Actor(b,2))s.gone[2]=true;
        Projectile* q=nullptr;while(b->IterateProjectiles(q))if(!q->mDead)if(const auto* d=GetNativeProjectileData(q))if(d->goldAttracted)
        {if(!Require(a,(d->specialType==4||d->specialType==5)&&q->mMotionType==MOTION_THREEPEATER,"Batch269 Gold capture metadata differs"))return false;
         auto [it,inserted]=s.goldPaths.try_emplace(q->mNetId,float(q->mPosX),float(q->mPosY));
         if(!inserted&&std::abs(q->mPosX-it->second.first)>1&&std::abs(q->mPosY-it->second.second)>1)s.goldMotion=true;}
        Coin* c=nullptr;while(b->IterateCoins(c))if(!c->mDead&&c->mType==COIN_SILVER&&IsNativeMoneyCoin(c)&&GetNativeCoinValue(c,0)==25)s.rewardCoins.insert(c->mNetId);
    }
    if(s.group==6)
    {
        Coin* c=nullptr;while(b->IterateCoins(c))if(!c->mDead)if(const auto* d=GetNativeDroppedCard269Data(c))
        {const int i=d->kind==1161?0:d->kind==6?1:-1;if(i<0)continue;
         if(!Require(a,d->cost==0&&c->mNetId&&d->instanceToken,"Batch269 actual source dropped card metadata invalid"))return false;
         if(!s.cards[i].id){s.cards[i]={c->mNetId,d->instanceToken};NetTrace("SMOKE_BATCH269_CARD_BIRTH role=%s kind=%u id=%u token=%u cost=0",Role(a),d->kind,c->mNetId,d->instanceToken);}
        }
        for(int step=0;step<4;++step){const int col=step==0?8:4+step,row=step==0?0:3;
         if(auto* p=At(b,col,row,step==0?1161:6))if(const auto* d=GetNativePlantData(p))
         {if(!Require(a,d->owner==1&&!d->packetId&&!d->randomOrigin,"Batch269 planted source card ownership differs"))return false;s.planted[step]={p->mNetId,d->instanceToken};}}
        for(int i=0;i<2;++i)if(!Actor(b,i)&&s.cards[i].id)s.gone[i]=true;
        Projectile* q=nullptr;while(b->IterateProjectiles(q))if(!q->mDead)if(const auto* d=GetNativeProjectileData(q))if(d->kind==97&&q->mRow==4)s.beanPayload=true;
    }
    return true;
}
void RestUI(LawnApp* a)
{
    if(!a->NetIsClient()||a->mBoard->mPaused)return;auto* b=a->mBoard;
    if((s.group==7&&s.ui<1)||(s.group==8&&s.ui<2))
    {
        auto* p=Actor(b,s.group==7?5:s.ui==0?3:5);if(!p)return;if(s.group==7&&s.uiPhase==0&&p->mPlantHealth>=s.initialPlantHP[5])return;auto* c=ResolveNativeEquipment(b,s.items[s.ui].id,s.items[s.ui].token);const auto* d=GetNativeEquipmentData(c);
        if(s.uiPhase==0&&d&&d->phase==NativeEquipmentPhase::World){if(BoardClick(a,c->mPosX+20,c->mPosY+20))s.uiPhase=1;return;}
        if(s.uiPhase==1&&d&&d->phase==NativeEquipmentPhase::Cursor&&d->claimSlot==1)
        {if(BoardClick(a,X(b,p->mPlantCol,p->mRow),float(b->GridToPixelY(p->mPlantCol,p->mRow)+50)))s.uiPhase=2;return;}
        if(s.uiPhase==2&&((s.ui==0&&!c)||(s.ui==1&&d&&d->weapon&&d->sourcePlantId==p->mNetId)))
        {NetTrace("SMOKE_BATCH269_SUMMON_ITEM role=client index=%d source=%u token=%u actualSDL=1",s.ui,p->mNetId,GetNativePlantData(p)->instanceToken);++s.ui;s.uiPhase=0;}
    }
    if(s.group==9&&s.fertilize.id&&!s.fertilizeUsed)
    {
        Coin* c=nullptr;Coin* candidate=nullptr;
        while(b->IterateCoins(candidate))if(!candidate->mDead&&candidate->mNetId==s.fertilize.id){c=candidate;break;}
        const auto* d=GetNativeFertilize269Data(c);
        if(d&&!Require(a,d->instanceToken==s.fertilize.token,"Batch269 Fertilize full generation changed"))return;
        if(s.uiPhase==0&&d&&d->phase==NativeFertilize269Phase::World){if(BoardClick(a,c->mPosX+20,c->mPosY+20))s.uiPhase=1;}
        else if(s.uiPhase==1&&d&&d->phase==NativeFertilize269Phase::Cursor&&d->heldSlot==1)
        {if(BoardClick(a,X(b,8,3),float(b->GridToPixelY(8,3)+50)))s.uiPhase=2;}
    }
}
bool RestGoneAllowed(int i)
{
    // Natural retirement after every required outcome for this source was
    // observed does not invalidate another plant's slower grouped scenario.
    if(s.group>=7&&(s.mask&(1<<i)))return true;
    return (s.group==8&&i==1)||(s.group==9&&(i==3||i==4))||(s.group==10&&(i==0||i==1))||(s.group==11&&i==4);
}
bool ObserveRest(LawnApp* a)
{
    if(s.group<7||s.group>=12)return true;auto* b=a->mBoard;bool anyHit=false;for(bool hit:s.hit)anyHit|=hit;
    Projectile* q=nullptr;while(b->IterateProjectiles(q))if(!q->mDead)
    {
        if(const auto* child=GetNativeCompanion269State(q))for(int i=0;i<int(Specs().size());++i)
            if(child->parentId==s.actors[i].id&&child->parentToken==s.actors[i].token)
            {s.restAux[i]=true;if(child->onTarget&&child->shotEmitted)s.restShot[i]=true;
             if(s.group==7&&i==3&&child->family==FusionCompanion269Family::CherryLittleFly&&child->instanceToken&&child->targetId&&child->targetToken)
             {auto* z2=ZombieById(b,s.zombies[2]);auto* z3=ZombieById(b,s.zombies[3]);
              if(z2&&z3)s.cherryWitness[q->mNetId]={child->instanceToken,child->targetId,child->targetToken,{z2->mBodyHealth,z3->mBodyHealth}};}}
        if(const auto* d=GetNativeProjectileData(q))
        {
            if(s.group==7&&d->kind==0&&d->fireState&&d->lastTorchId==s.actors[6].id&&d->lastTorchToken==s.actors[6].token)s.restEvent[6]=true;
            if(s.group==9&&d->kind==8&&d->specialType==16&&d->effectVariant==3)s.restEvent[0]=true;
            if(s.group==9&&q->mRow==1&&FusionMore269Payload(d->kind))s.restEvent[1]=true;
            if(s.group==9&&d->kind==51)s.restEvent[2]=true;
            if(s.group==10&&d->kind==9&&q->mRow==3)s.restShot[4]=true;
        }
    }
    Coin* coin=nullptr;while(b->IterateCoins(coin))if(!coin->mDead)
    {
        if(s.group==7&&!IsNativeMoneyCoin(coin)&&GetNativeCoinValue(coin,-1)==15)s.restEvent[0]=true;
        if(const auto* d=GetNativeEquipmentData(coin))
        {if(s.group==7&&d->sourcePlantId==s.actors[5].id&&d->sourcePlantToken==s.actors[5].token)s.restEvent[5]=true;
         if(s.group==8&&d->weapon&&d->sourcePlantId==s.actors[5].id&&d->sourcePlantToken==s.actors[5].token)s.restEvent[5]=true;}
        if(s.group==9)if(const auto* d=GetNativeFertilize269Data(coin))
        {if(!s.fertilize.id){s.fertilize={coin->mNetId,d->instanceToken};NetTrace("SMOKE_BATCH269_FERT_BIRTH role=%s id=%u token=%u",Role(a),coin->mNetId,d->instanceToken);}s.fertilizeSeen=true;
         if(d->phase==NativeFertilize269Phase::Using&&d->targetId==s.fertilizeTarget.id&&d->targetToken==s.fertilizeTarget.token)s.restAux[3]=true;}
        if(const auto* d=GetNativeDroppedCard269Data(coin))
        {if(s.group==9&&d->kind==256)s.restEvent[4]=true;
         if(s.group==10&&d->kind==13)s.restAux[0]=true;}
    }
    if(s.group==8||s.group==10)
    {Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->mDead)if(const auto* d=GetNativeSourceZombieData(z))
     {if(s.group==8)
      {for(int i:{2,3})if(d->sourceParentId==s.actors[i].id&&d->sourceParentToken==s.actors[i].token)
       {s.restAux[i]=true;s.restChildren[z->mNetId]=d->instanceToken;}
       //1041 source dies BEFORE spawning; commonfactory deliberately detaches
       //its parent pair. Unique actual13/row2/friendly is this source's output.
       if(d->kind==13&&z->mRow==2&&z->mMindControlled&&!d->sourceParentId&&!d->sourceParentToken&&z->mNetId&&d->instanceToken)
       {s.restAux[1]=true;s.iceChild={z->mNetId,d->instanceToken};s.restChildren[z->mNetId]=d->instanceToken;}}

      if(s.group==10&&z->mRow==0&&(d->kind==0||d->kind==2||d->kind==4)&&!z->mMindControlled){s.restEvent[1]=true;s.restChildren[z->mNetId]=d->instanceToken;}}}
    if(s.group==7)
    {if(auto* p=Actor(b,0))if(p->mPlantHealth<s.initialPlantHP[0]&&s.restEvent[0])s.mask|=1;
     for(int i:{1,4})if(s.restAux[i]&&s.restShot[i]&&anyHit)s.mask|=1<<i;
     if(!s.restEvent[3])for(const auto& [id,witness]:s.cherryWitness)
     {
         bool present=false;Projectile* child=nullptr;while(b->IterateProjectiles(child))if(!child->mDead&&child->mNetId==id)
          if(const auto* state=GetNativeCompanion269State(child))if(state->instanceToken==witness.token)present=true;
         auto* z2=ZombieById(b,s.zombies[2]);auto* z3=ZombieById(b,s.zombies[3]);
         if(!present&&z2&&z3&&witness.hp[0]-z2->mBodyHealth>=1800)
         {s.restEvent[3]=true;NetTrace("SMOKE_BATCH269_CHERRY_RETIRE role=%s source=%u token=%u AUX=%u AUXToken=%u target=%u targetToken=%u cohort=%u/%u damage=%d/%d actualSourceCherry1800=1 airExplosionParity=NotClaimed",Role(a),s.actors[3].id,s.actors[3].token,id,witness.token,witness.target,witness.targetToken,s.zombies[2],s.zombies[3],witness.hp[0]-z2->mBodyHealth,witness.hp[1]-z3->mBodyHealth);break;}
     }
     if(s.restAux[3]&&s.restEvent[3])s.mask|=8;
     if(s.shot[2]&&s.hit[5])s.mask|=4;
     if(auto* p=Actor(b,5)){s.restMinHP[5]=(std::min)(s.restMinHP[5],p->mPlantHealth);
       if(p->mPlantHealth<s.initialPlantHP[5])s.restEvent[5]=true;
       if(s.restEvent[5]&&p->mPlantHealth>s.restMinHP[5])s.restAux[5]=true;}
     if(s.restAux[5])s.mask|=32;if(s.restEvent[6])s.mask|=64;}
    if(s.group==8)
    {if(auto* z=ZombieById(b,s.zombies[0]))if(z->mMindControlled){s.restEvent[0]=true;s.hit[0]=true;}
     if(auto* p=Actor(b,0))if(GetNativePlantData(p)->batchBagCount<5&&s.restEvent[0])s.mask|=1;
     if(auto* z=ZombieById(b,s.zombies[5]))if(z->mIceTrapCounter>0)s.restEvent[1]=true;
     if(s.restAux[1]&&s.iceChild.id&&s.iceChild.token&&!Actor(b,1)&&s.restEvent[1]&&!ZombieById(b,s.zombies[1]))
     {if(!s.hit[1])NetTrace("SMOKE_BATCH269_EXPECTED_RETIRE role=%s group=8 target=%u source=%u token=%u child=%u childToken=%u freeze=1 actualSourceDieWithLoot=1",Role(a),s.zombies[1],s.actors[1].id,s.actors[1].token,s.iceChild.id,s.iceChild.token);
      s.mask|=2;s.hit[1]=true;}
     if(s.restAux[2])s.mask|=4;if(s.restAux[3])s.mask|=8;
     if(auto* p=Actor(b,4))if(p->mPlantHealth<s.initialPlantHP[4])s.mask|=16;
     if(s.restEvent[5]&&s.hit[4])s.mask|=32;}
    if(s.group==9)
    {if(s.restEvent[0]&&anyHit)s.mask|=1;if(s.restEvent[1]&&s.shot[1]&&s.hit[1])s.mask|=2;
     if(s.restEvent[2]&&s.hit[2])s.mask|=4;
     if(auto* fresh=At(b,8,3,1027))if(fresh->mNetId!=s.fertilizeTarget.id&&s.restAux[3]&&s.fertilizeSeen){s.fertilizeUsed=true;s.mask|=8;}
     Plant* fresh=nullptr;while(b->IteratePlants(fresh))if(!fresh->mDead&&fresh->mPlantCol==2&&fresh->mRow==4&&fresh->mNetId!=s.actors[4].id&&GetNativePlantData(fresh))s.restEvent[4]=true;
     if(!Actor(b,4)&&s.restEvent[4])s.mask|=16;}
    if(s.group==10)
    {if(auto* p=Actor(b,0))if(GetNativePlantData(p)->batchMode&&GetNativePlantData(p)->batchShotCount)s.restEvent[0]=true;
     if(!Actor(b,0)&&s.restAux[0]&&s.hit[0])s.mask|=1;
     if(s.restEvent[1]&&!Actor(b,1))s.mask|=2;
     if(auto* p=Actor(b,2))if(p->mPlantHealth<s.initialPlantHP[2])s.mask|=4;
     if(auto* p=Actor(b,3))
     {
      if(p->mState==STATE_CHOMPER_BITING)
      {auto* victim=ZombieById(b,s.zombies[1]);unsigned sameRow=0;Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->mDead&&!z->mMindControlled&&z->mRow==p->mRow)++sameRow;
       if(victim&&victim->mRow==p->mRow&&sameRow==1)s.chompVictim=victim->mNetId;}
      if(p->mState==STATE_CHOMPER_BITING_GOT_ONE||p->mState==STATE_CHOMPER_DIGESTING)
      {
       const auto* d=GetNativePlantData(p);const int total=p->mLaunchCounter+int(std::lround(d->shootSeconds*100));
       // Raw/FNS carries phase and elapsed source digest, not Chomper's host-only
       // LaunchCounter. The client binds the unique actual victim while BITING.
       const bool digest=d->shootSeconds>=0&&d->shootSeconds<=11.f&&(!a->NetIsHost()||(total>=800&&total<=1100));
       if(digest&&s.chompVictim==s.zombies[1]&&!ZombieById(b,s.chompVictim))
       {if(!s.hit[1])NetTrace("SMOKE_BATCH269_EXPECTED_RETIRE role=%s group=10 target=%u source=%u token=%u phase=%d digestTotalHost=%d canonicalElapsed=%.9g priorUniqueBiteTarget=1 actualSourceChomp=1",Role(a),s.chompVictim,s.actors[3].id,s.actors[3].token,int(p->mState),a->NetIsHost()?total:-1,d->shootSeconds);
        s.hit[1]=true;if(p->mState==STATE_CHOMPER_DIGESTING)s.restEvent[3]=true;}
      }
     }
     if(s.restEvent[3]&&!ZombieById(b,s.zombies[1]))s.mask|=8;
     if(auto* z=ZombieById(b,s.zombies[2]))if(const auto* d=GetNativeZombieActionData(z))if(d->waterGrapTicks>0)s.restEvent[4]=true;
     if(s.restEvent[4]&&s.restShot[4]&&s.hit[2])s.mask|=16;}
    return true;
}
bool ObserveWorld269(LawnApp* a)
{
    if(s.group!=11)return true;auto* b=a->mBoard;unsigned ringCount=0;
    Projectile* q=nullptr;while(b->IterateProjectiles(q))if(!q->mDead)
    {
        if(const auto* beam=GetNativeWorld269BeamState(q))if(beam->ownerId==s.actors[0].id&&beam->ownerToken==s.actors[0].token)s.worldBeam=true;
        if(const auto* aux=GetNativeWorldRemainder269AuxState(q))
        {if(aux->family==NativeWorldRemainder269Family::Meteorite&&aux->type==2)s.worldMeteorite=true;
         if(aux->family==NativeWorldRemainder269Family::LourFly&&aux->parent.id==s.actors[2].id&&aux->parent.token==s.actors[2].token)
         {s.worldFly=true;if(aux->shots>=3)s.worldVolley=true;}}
        if(const auto* d=GetNativeProjectileData(q))
        {if(d->kind==49&&d->specialType==9&&q->mRow==1)s.worldRay=true;
         if(s.worldMeteorite&&d->kind==49&&d->specialType==2&&d->damage==100){++ringCount;s.worldRingIds.insert(q->mNetId);}
         if(d->kind==91&&d->specialType==6)if(const auto* shot=GetNativeWorldRemainder269ShotState(q))if(shot->parent.id==s.actors[2].id&&shot->parent.token==s.actors[2].token)s.worldHoming=true;
         if(d->kind==30&&d->lastTorchId==s.actors[4].id&&d->lastTorchToken==s.actors[4].token&&IsNativeLastPlants269KelpProjectile(q))s.world30=true;}
    }
    s.worldRingPeak=(std::max)(s.worldRingPeak,ringCount);
    if(a->mEffectSystem&&a->mEffectSystem->mParticleHolder)
    {auto* holder=a->mEffectSystem->mParticleHolder;TodParticleSystem* system=nullptr;while(holder->mParticleSystems.IterateNext(system))
      if(!system->mDead&&system->mEffectType==PARTICLE_FUSION_SOURCE51&&system->mParticleHolder==holder&&system->mEmitterList.mSize>0)s.worldFX51=true;}
    if(auto* kelp=Actor(b,4)){const auto* d=GetNativePlantData(kelp);if((d->batchMode==1||d->batchMode==2)&&d->waterTargetId==s.zombies[3]&&d->waterTargetToken)s.worldGrab=true;}
    else if(!Require(a,s.worldGrab&&s.world30,"Batch2691056 retired without actual grab and stamped30"))return false;
    if(s.worldHoming&&s.worldFly&&!ZombieById(b,s.zombies[1]))
    {if(!s.hit[1])NetTrace("SMOKE_BATCH269_WORLD_EXPECTED_RETIRE role=%s target=%u source=%u token=%u homing91=1 actualLourBirth=1",Role(a),s.zombies[1],s.actors[2].id,s.actors[2].token);s.hit[1]=true;}
    if(s.worldGrab&&!Actor(b,4)&&!ZombieById(b,s.zombies[3]))
    {if(!s.hit[3])NetTrace("SMOKE_BATCH269_WORLD_EXPECTED_RETIRE role=%s target=%u source=%u token=%u actualGrabSink=1",Role(a),s.zombies[3],s.actors[4].id,s.actors[4].token);s.hit[3]=true;}
    if(s.worldBeam&&s.hit[0])s.mask|=1;
    if(s.worldRay&&s.worldMeteorite&&s.worldFX51&&!s.worldRingIds.empty()&&s.hit[2])s.mask|=2;
    if(s.worldHoming&&s.worldFly&&s.worldVolley&&s.hit[1])s.mask|=4;
    if(auto* passive=Actor(b,3))
    {const auto* d=GetNativePlantData(passive);const auto* stats=FusionFindPlant(918);
     // Raw launch/shoot counters are host-only; ghosts adopt the canonical
     // source clock/pose DTO rather than ticking the native Pea carrier AI.
     const bool idle=stats&&passive->mPlantHealth==stats->maxHealth&&passive->mPlantMaxHealth==stats->maxHealth&&d->shootSeconds== -1&&!d->batchShotCount&&!d->productionTicks&&!d->pendingProductionTicks&&
         (a->NetIsClient()||(!passive->mShootingCounter&&!passive->mLaunchCounter));
     if(!idle)NetTrace("SMOKE_BATCH269_PASSIVE_ERROR role=%s health=%d maximum=%d sourceMaximum=%d shoot=%.9g shots=%u production=%d pending=%d rawShoot=%d rawLaunch=%d",Role(a),passive->mPlantHealth,passive->mPlantMaxHealth,stats?stats->maxHealth:-1,d->shootSeconds,d->batchShotCount,d->productionTicks,d->pendingProductionTicks,passive->mShootingCounter,passive->mLaunchCounter);
     if(!Require(a,idle,"Batch269918 passive source attributes/idle changed"))return false;
     if(d->idleSeconds>=16)s.mask|=8;}
    if(s.world30&&s.worldGrab&&!Actor(b,4)&&s.hit[3])s.mask|=16;
    return true;
}
void LaserUI269(LawnApp* a);
void UseUI(LawnApp* a)
{
    if(!a->NetIsClient()||a->mBoard->mPaused||s.start<0)return;LaserUI269(a);CardUI(a);RestUI(a);auto* b=a->mBoard;
    if(s.group==2&&s.ui<2)
    {
        auto* p=Actor(b,1);const auto* d=GetNativePlantData(p);if(!p||!d||(s.ui==1&&!s.chargeFired))return;
        auto* coin=ResolveNativeEquipment(b,s.items[s.ui].id,s.items[s.ui].token);const auto* item=GetNativeEquipmentData(coin);
        if(s.uiPhase==0&&item&&item->phase==NativeEquipmentPhase::World)
        {if(BoardClick(a,coin->mPosX+20,coin->mPosY+20))s.uiPhase=1;return;}
        if(s.uiPhase==1&&item&&item->phase==NativeEquipmentPhase::Cursor&&item->claimSlot==1)
        {if(BoardClick(a,X(b,p->mPlantCol,p->mRow),float(b->GridToPixelY(p->mPlantCol,p->mRow)+50)))s.uiPhase=2;return;}
        if(s.uiPhase==2&&!coin&&d->batchMode==(s.ui==0?4:5))
        {NetTrace("SMOKE_BATCH269_ITEM role=client index=%d source=%u token=%u item=%u itemToken=%u status=%u ticks=%d actualSDL=1",s.ui,p->mNetId,d->instanceToken,s.items[s.ui].id,s.items[s.ui].token,d->batchMode,d->productionTicks);++s.ui;s.uiPhase=0;}
    }
    if(s.group!=4||s.ui>=3)return;
    if(s.hotkeys<4)
    {
        const auto cursor=b->mCursorObject->mCursorType;
        if(s.hotkeys==0&&!Require(a,cursor==CURSOR_TYPE_NORMAL,"Batch269 hotkey probe requires initial NORMAL cursor"))return;
        if(s.hotkeys==0&&OwnedKey(a,SDLK_V))s.hotkeys=1;
        else if(s.hotkeys==1&&cursor==CURSOR_TYPE_GLOVE&&OwnedKey(a,SDLK_V))s.hotkeys=2;
        else if(s.hotkeys==2&&cursor==CURSOR_TYPE_NORMAL&&OwnedKey(a,SDLK_Z))s.hotkeys=3;
        else if(s.hotkeys==3&&cursor==CURSOR_TYPE_MONEY_SIGN)
        {s.hotkeys=4;NetTrace("SMOKE_BATCH269_HOTKEYS role=client Vglove=1 Vcancel=1 Zmoney=1 actualOwnedSDL=1 beforeFirstPayment=1");}
        return;
    }
    auto* p=Actor(b,s.ui);const auto* d=GetNativePlantData(p);if(!p||!d)return;
    if(s.uiPhase==0)
    {
        if(!CanActivateNativeUmbrellaBatchSkill(p))return;
        if(b->mCursorObject->mCursorType!=CURSOR_TYPE_MONEY_SIGN)
        {auto rect=b->GetShovelButtonRect();b->GetZenButtonRect(OBJECT_TYPE_MONEY_SIGN,rect);BoardClick(a,float(rect.mX+rect.mWidth/2),float(rect.mY+rect.mHeight/2));return;}
        s.moneyBefore=GetNativeContext(b)->money;float x=0,y=0;if(p->GetShadowCenter(x,y)&&BoardClick(a,x,y-20))s.uiPhase=1;return;
    }
    if(s.uiPhase==1&&(d->batchMode&4))
    {
        const int fee=s.ui==2?3000:1000;
        if(!Require(a,GetNativeContext(b)->money==s.moneyBefore-fee,"Batch269 Money debit differs from source fee"))return;
        s.paid[s.ui]=true;s.moneyAfter=GetNativeContext(b)->money;s.moneyTick=b->mMainCounter;
        float x=0,y=0;if(p->GetShadowCenter(x,y)&&BoardClick(a,x,y-20))s.uiPhase=2;return;
    }
    if(s.uiPhase==2&&b->mMainCounter-s.moneyTick>=30)
    {
        if(!Require(a,GetNativeContext(b)->money==s.moneyAfter,"Batch269 cooldown repeat spent Money twice"))return;
        NetTrace("SMOKE_BATCH269_PAID role=client index=%d source=%u token=%u fee=%d before=%d after=%d repeatRejected=1",s.ui,p->mNetId,d->instanceToken,s.ui==2?3000:1000,s.moneyBefore,s.moneyAfter);
        ++s.ui;s.uiPhase=0;
    }
}
Plant* LaserHelper(Board* b,int i)
{Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mNetId==s.laserHelpers[i].id){const auto* d=GetNativePlantData(p);if(d&&d->instanceToken==s.laserHelpers[i].token)return p;}return nullptr;}
bool LaserPair(Plant* upper,Plant* lower)
{
    const auto* a=GetNativeWorld269PlantState(upper);const auto* b=GetNativeWorld269PlantState(lower);const auto* ad=GetNativePlantData(upper);const auto* bd=GetNativePlantData(lower);
    return upper&&lower&&a&&b&&ad&&bd&&a->connected&&b->connected&&a->main&&!b->main&&
        a->connectedPlant.id==lower->mNetId&&a->connectedPlant.token==bd->instanceToken&&b->connectedPlant.id==upper->mNetId&&b->connectedPlant.token==ad->instanceToken;
}
bool ObserveLaser269(LawnApp* a,int elapsed)
{
    if(s.group!=13)return true;auto* b=a->mBoard;auto* main=Actor(b,0);auto* first=LaserHelper(b,0);auto* second=LaserHelper(b,1);
    if(!Require(a,main&&first&&second,"Laser937 source/helper full generation disappeared"))return false;
    const auto* standalone=GetNativeWorld269PlantState(second);const auto& hold=GetNativeContext(b)->gloveHolds[1];
    const bool held=hold.plantId==s.actors[0].id&&hold.instanceToken==s.actors[0].token;
    if(LaserPair(main,first)&&standalone&&!standalone->connected)
    {s.laserInitialPair=true;if(std::any_of(standalone->zombies.begin(),standalone->zombies.end(),[&](const auto& ref){return ref.id==s.zombies[2]&&ref.token;}))
       if(auto* z=ZombieById(b,s.zombies[2]);z&&z->mBodyHealth<s.initialHP[2])s.laserStandalone=true;}
    if(held)
    {s.laserHeld=true;const auto* m=GetNativeWorld269PlantState(main);if(m&&!m->connected&&LaserPair(first,second))s.laserHeldPairBroken=true;}
    if(s.laserHeld&&!held&&main->mPlantCol==5&&main->mRow==0)
    {const auto* m=GetNativeWorld269PlantState(main);if(m&&!m->connected){s.laserMoved=true;if(LaserPair(first,second))s.laserRepaired=true;}}
    for(int i=0;i<3;++i)
    {auto* p=i==0?main:i==1?first:second;if(const auto* state=GetNativeWorld269PlantState(p))
      {s.laserEnergyMin[i]=(std::min)(s.laserEnergyMin[i],state->energy);s.laserEnergyMax[i]=(std::max)(s.laserEnergyMax[i],state->energy);}}
    Plant* owner=s.laserRepaired?first:main;float x=0,y=0;int row=0;
    if(GetNativeLaserConnections269BallPoint(owner,x,y,row))
    {auto* peer=owner==main?first:second;float ax=0,ay=0,bx=0,by=0,nx=0,ny=0;
     if(!Require(a,owner->GetShadowCenter(ax,ay)&&peer->GetShadowCenter(bx,by)&&std::abs(x-((ax+bx)*.5f-float(.4*kFusionBasePixelsX)))<.01f&&
        std::abs(y-((ay+by)*.5f-float(GetNativeFusionPixelsY(b))))<.01f&&row==(std::max)(owner->mRow,peer->mRow),"Laser937 canonical ball geometry differs from source"))return false;
     if(!Require(a,GetNativeLaser269NodePoint(owner,"fightball_1",nx,ny)&&std::isfinite(nx)&&std::isfinite(ny),"Laser937 actual ball node sampler unavailable"))return false;
     s.laserBall=true;}
    bool damage=false;for(std::size_t i=0;i<s.zombies.size();++i)if(auto* z=ZombieById(b,s.zombies[i]);z&&z->mBodyHealth<s.initialHP[i])damage=true;
    if(s.laserInitialPair&&s.laserStandalone&&s.laserHeld&&s.laserHeldPairBroken&&s.laserMoved&&s.laserRepaired&&s.laserBall&&damage)s.mask|=1;
    if(!s.diagnostic||GetTickCount()-s.diagnostic>5000)NetTrace("SMOKE_BATCH269_LASER_PROOF role=%s elapsed=%d initialPair=%d standaloneTargetAndDamage=%d held=%d heldOldColumnRepaired=%d movedSameIdentity=%d repaired=%d ballNode=%d clientUI=%d energy=%d:%d/%d:%d/%d:%d exclusiveStandaloneHitAttribution=NotClaimed",Role(a),elapsed,s.laserInitialPair,s.laserStandalone,s.laserHeld,s.laserHeldPairBroken,s.laserMoved,s.laserRepaired,s.laserBall,s.laserUI,s.laserEnergyMin[0],s.laserEnergyMax[0],s.laserEnergyMin[1],s.laserEnergyMax[1],s.laserEnergyMin[2],s.laserEnergyMax[2]);
    return true;
}
void LaserUI269(LawnApp* a)
{
    if(s.group!=13||!a->NetIsClient()||a->mBoard->mPaused||s.start<0||a->mBoard->mMainCounter-s.start<600||!s.laserStandalone||s.laserUI>=4)return;
    auto* b=a->mBoard;auto* p=Actor(b,0);if(!p)return;const auto cursor=b->mCursorObject?b->mCursorObject->mCursorType:CURSOR_TYPE_NORMAL;const auto& hold=GetNativeContext(b)->gloveHolds[1];
    if(s.laserUI==0&&cursor==CURSOR_TYPE_NORMAL&&OwnedKey(a,SDLK_V)){s.laserUI=1;return;}
    if(s.laserUI==1&&cursor==CURSOR_TYPE_GLOVE){float x=0,y=0;if(p->GetShadowCenter(x,y)&&BoardClick(a,x,y-20))s.laserUI=2;return;}
    if(s.laserUI==2&&hold.plantId==s.actors[0].id&&hold.instanceToken==s.actors[0].token&&s.laserHeldPairBroken)
    {if(BoardClick(a,X(b,5,0),float(b->GridToPixelY(5,0)+50)))s.laserUI=3;return;}
    if(s.laserUI==3&&!hold.plantId&&p->mPlantCol==5&&p->mRow==0&&s.laserRepaired)
    {s.laserUI=4;NetTrace("SMOKE_BATCH269_LASER_SDL role=client source=%u token=%u pickup=1 canonicalHold=1 col=3->5 row=0 oldColumnRepaired=1 identityKept=1 actualV=1",p->mNetId,s.actors[0].token);}
}
bool ObserveWheat269(LawnApp* a,int elapsed)
{
    if(s.group!=12)return true;auto* b=a->mBoard;
    // Exact complete source pool. This observer neither filters nor draws RNG.
    constexpr uint16_t pool[]={1000,1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1011,1012,1013,1014,1015,1016,1017,1020,1023,1024,1025,1026,1027,1028,1029,1030,1032,1033,1034,1037,1038,1039,1040,1041,1042,1043,1045,1046,1047,1048,1052,1053,1054,1055,1057,1058,1059,1060,1061,1062,1063,1064,1070,1071,1072,1073,1074,1075,1079,1080,1081,1084,1085,1090,1093,1094,1095,1096,1100,1102,1103,1104,1105,1106,1107,1108,1111,1113,1115,1116,1117,1118,1119,1120,1121,1122,1123,1124,1126,1127,1128,1129,1131,1132,1134,1135,1138,1139,1140,1141,1142,1143,1144,1145,1146,1147,1148,1149,1150,1154,1155,1157,1158,1159,1160,1161,1162,1163,1166,1171,1172,1173,1174,1176,1177,1178,1179,1180,1181,1182,1185,1186,1187,1188,1189,1191,1192};
    const auto allowed=[&](uint16_t kind){return std::find(std::begin(pool),std::end(pool),kind)!=std::end(pool);};
    if(auto* original=Actor(b,0))
    {const auto* d=GetNativePlantData(original);if(d)s.wheatLastProduction=d->productionTicks;
     return Require(a,!s.wheatGone,"Wheat original generation returned after retirement");}
    if(!s.wheatGone)
    {if(!Require(a,s.start>=0&&elapsed>=3099&&s.wheatLastProduction>=1&&s.wheatLastProduction<=10,"Wheat original lost before observed natural31s deadline"))return false;
     s.wheatGone=true;NetTrace("SMOKE_BATCH269_WHEAT_RETIRE role=%s source=%u token=%u elapsed=%d lastRemaining=%d natural31s=1",Role(a),s.actors[0].id,s.actors[0].token,elapsed,s.wheatLastProduction);}
    Plant* child=nullptr;while(b->IteratePlants(child))
    {const auto* d=GetNativePlantData(child);const auto* w=GetNativeWheat269State(child);
     if(child->mDead||!d||child->mPlantCol!=3||child->mRow!=2||!w||!w->isFromWheat||!allowed(d->kind)||
        (child->mNetId==s.actors[0].id&&d->instanceToken==s.actors[0].token))continue;
     if(!s.wheatChild.id){s.wheatChild={child->mNetId,d->instanceToken};s.wheatKind=d->kind;s.wheatFirst=w->wheatTime;
       NetTrace("SMOKE_BATCH269_WHEAT_CHILD role=%s id=%u token=%u kind=%u col=3 row=2 isFromWheat=1 time=%.9g fullPool=138",Role(a),child->mNetId,d->instanceToken,d->kind,w->wheatTime);}
     if(child->mNetId==s.wheatChild.id&&d->instanceToken==s.wheatChild.token)
     {s.wheatLast=w->wheatTime;if(w->wheatTime>s.wheatFirst+.01f)s.wheatAdvanced=true;}}
    Coin* card=nullptr;while(b->IterateCoins(card))
    {const auto* d=GetNativeDroppedCard269Data(card);if(card->mDead||!d||d->cost||!allowed(d->kind))continue;
     const float x=float(card->mPosX+card->mWidth*.5f),y=float(card->mPosY+card->mHeight*.5f);
     if(std::abs(x-X(b,3,2))>36||std::abs(y-float(b->GridToPixelY(3,2)+50))>100)continue;
     if(!s.wheatCard.id){s.wheatCard={card->mNetId,d->instanceToken};s.wheatKind=d->kind;
       NetTrace("SMOKE_BATCH269_WHEAT_CARD role=%s id=%u token=%u kind=%u cost=0 fullPool=138 actualNativeCoin=1",Role(a),card->mNetId,d->instanceToken,d->kind);}}
    if(s.wheatGone&&((s.wheatChild.id&&s.wheatAdvanced)||s.wheatCard.id))s.mask|=1;
    if(s.wheatGone&&s.start>=0&&(!s.diagnostic||GetTickCount()-s.diagnostic>5000))NetTrace("SMOKE_BATCH269_WHEAT_WAIT role=%s elapsed=%d child=%u token=%u first=%.9g current=%.9g advanced=%d card=%u fullPool138Distribution=NotExecuted",Role(a),elapsed,s.wheatChild.id,s.wheatChild.token,s.wheatFirst,s.wheatLast,s.wheatAdvanced,s.wheatCard.id);
    return true;
}
bool Observe(LawnApp* a)
{
    auto* b=a->mBoard;const int elapsed=s.start<0?0:b->mMainCounter-s.start;
    if(!ObserveWheat269(a,elapsed)||!ObserveLaser269(a,elapsed))return false;
    if(!Require(a,a->mGameScene!=SCENE_ZOMBIES_WON,"Batch269 native control reached terminal scene"))return false;
    for(int i=0;i<int(Specs().size());++i)
    {
        auto* p=Actor(b,i);const auto* d=GetNativePlantData(p);
        if(!p||!d)
        {if((s.group==12&&s.wheatGone)||(s.group==5&&i<3)||(s.group==6&&i<2)||RestGoneAllowed(i))continue;
         if(s.group==5&&i==3&&!s.goldPaths.empty()&&s.goldMotion&&!s.rewardCoins.empty())
         {if(!s.gone[3])NetTrace("SMOKE_BATCH269_GOLD_RETIRED role=%s id=%u token=%u priorCapture=%zu priorMotion=1 priorPayout38=%zu naturalOnly=1",Role(a),s.actors[i].id,s.actors[i].token,s.goldPaths.size(),s.rewardCoins.size());s.gone[3]=true;continue;}
         char why[192];sprintf_s(why,"Batch269 source disappeared group=%d index=%d kind=%u id=%u token=%u priorMask=%02x lastHP=%d",s.group,i,Specs()[i].kind,s.actors[i].id,s.actors[i].token,s.mask,s.restMinHP[i]);
         NetTrace("SMOKE_BATCH269_SOURCE_MISSING role=%s group=%d index=%d kind=%u id=%u token=%u lastHP=%d priorMask=%02x",Role(a),s.group,i,Specs()[i].kind,s.actors[i].id,s.actors[i].token,s.restMinHP[i],s.mask);
         if(!Require(a,false,why))return false;}
        s.restMinHP[i]=(std::min)(s.restMinHP[i],p->mPlantHealth);
        s.phases[i]|=1U<<d->batchMode;
        if(d->shootSeconds>=0)s.shooting[i]=true;const bool emitted=d->batchShotCount>s.lastShots[i];if(emitted)s.shot[i]=true;s.lastShots[i]=d->batchShotCount;
        if(s.group==0&&i==3)
        {auto* eater=ZombieById(b,s.zombies.back());if(eater&&eater->mIsEating&&std::abs(eater->mPosX-p->mX)<160)
            {if(!Require(a,p->mPlantHealth==s.initialPlantHP[i],"Batch269 source1095 took damage during real eating"))return false;if(s.firstEatingTick<0)s.firstEatingTick=b->mMainCounter;s.invulnerableSamples=b->mMainCounter-s.firstEatingTick;}}
        if(s.group==1&&i==2&&p->mPlantHealth<s.initialPlantHP[i])s.coverDamaged=true;
        if(s.group==2&&i==1)
        {if(emitted&&d->batchMode==4&&d->batchBagCount==4)s.chargeFired=true;if(emitted&&d->batchMode==5&&d->batchBagCount==5)s.fullChargeFired=true;}
        if(s.group==3&&i==0)
        {
            if(a->NetIsHost())
            {const auto found=GetNativeContext(b)->doom269Clouds.find(d->instanceToken);if(found!=GetNativeContext(b)->doom269Clouds.end()&&!found->second.empty())s.cloud=true;}
            else if(!s.cloud&&a->mEffectSystem&&a->mEffectSystem->mParticleHolder)
            {
                auto* holder=a->mEffectSystem->mParticleHolder;TodParticleSystem* system=nullptr;
                while(holder->mParticleSystems.IterateNext(system))
                    if(!system->mDead&&system->mEffectType==PARTICLE_FUSION_SOURCE52&&system->mParticleHolder==holder&&
                        system->mParticleDef&&system->mParticleDef->mEmitterDefCount==17&&system->mEmitterList.mSize==17)
                    {s.cloud=true;NetTrace("SMOKE_BATCH269_CLOUD role=client source=%u token=%u system=%u emitters=17 actualNativeHolder=1",p->mNetId,d->instanceToken,unsigned(a->ParticleGetID(system)));break;}
            }
        }
        if(s.group==3&&i==1)if(const auto* state=GetNativeDoom269PlantState(p))
            if(state->ordinal>=2&&state->interval<1.5f)s.adaptive=true;
    }
    Projectile* q=nullptr;while(b->IterateProjectiles(q))
    {
        const auto* data=GetNativeProjectileData(q);if(q->mDead||!data)continue;s.payloads.insert(data->kind);
        if(s.group==0)
        {constexpr uint16_t payload[]={37,45,43,44,39};for(int i=0;i<5;++i)if(data->kind==payload[i]&&q->mRow==Specs()[i].row)s.payloadSeen[i]=true;}
        if(s.group==2&&data->kind==14)for(int i=0;i<2;++i)if(q->mRow==Specs()[i].row)s.payloadSeen[i]=true;
        if(s.group==3&&data->kind==75)s.payloadSeen[2]=true;
        if(s.group==1)if(const auto* state=GetNativeStar269ProjectileState(q))
        {
            for(int i=0;i<2;++i)
            {
                if(state->ownerId==s.actors[i].id&&state->ownerToken==s.actors[i].token&&data->specialType==10&&!state->colliderEnabled)
                {s.held[i][q->mNetId]=data->damage;s.captured[i]=true;}
                auto found=s.held[i].find(q->mNetId);
                if(found!=s.held[i].end()&&!state->ownerId&&data->specialType==6&&state->colliderEnabled&&data->damage==found->second*(i==0?2:3))s.released[i]=true;
            }
        }
    }
    if(s.group==4)
    {
        Zombie* z=nullptr;while(b->IterateZombies(z))if(const auto* d=GetNativeSourceZombieData(z))
        {
            if(z->mDead||(d->kind!=41&&d->kind!=42))continue;
            for(int i=0;i<3;++i)if(d->sourceParentId==s.actors[i].id&&d->sourceParentToken==s.actors[i].token)
            {s.moneyChildren[i][d->kind-41]={z->mNetId,d->instanceToken};s.converted[i][d->kind-41]=true;if(d->health<float(z->mBodyMaxHealth))s.moneyDecayed[i][d->kind-41]=true;}
        }
        for(int i=0;i<3;++i)for(int k=0;k<2;++k)if(s.moneyChildren[i][k].id&&!ZombieById(b,s.moneyChildren[i][k].id))
            {if(!Require(a,s.moneyDecayed[i][k],"Batch269 Money retired without observed source decay"))return false;s.moneyRetired[i][k]=true;}
        Coin* coin=nullptr;while(b->IterateCoins(coin))if(!coin->mDead&&coin->mType==COIN_DIAMOND&&IsNativeMoneyCoin(coin)&&GetNativeCoinValue(coin,0)==1000)s.rewardCoins.insert(coin->mNetId);
    }
    if(s.group>=7&&!ObserveRest(a))return false;
    if(!ObserveWorld269(a))return false;
    for(size_t i=0;i<s.zombies.size();++i)
    {
        auto* z=ZombieById(b,s.zombies[i]);
        if(z)
        {if(z->mBodyHealth<s.initialHP[i]||z->mFlyingHealth<s.initialAir[i])s.hit[i]=true;if(s.group==2&&i%2==0&&z->mMindControlled)s.controlled[i/2]=true;}
        else if(s.baseline)
        {
            const bool converted=s.group==4&&s.converted[i/2][i%2==0?1:0];
            const bool completedEater=(s.group==7&&((i==0&&(s.mask&1))||(i==4&&(s.mask&32))))||
                (s.group==8&&((i==0&&(s.mask&1))||(i==3&&(s.mask&16))))||(s.group==10&&i==3&&(s.mask&4));
            if(!s.hit[i]&&!converted&&!completedEater){char why[128];sprintf_s(why,"Batch269 missing target group=%d index=%zu UID=%u without observed damage/conversion",s.group,i,s.zombies[i]);a->CoopSmokeFail(why);return false;}
        }
    }
    if(!ObserveNew(a))return false;
    if(s.start>=0&&!b->mPaused)
    {if(elapsed>=200&&!Capture(a,"action"))return false;if(elapsed>=(s.group==13?1000:s.group==12?3100:s.group==11?1600:500)&&!Capture(a,"late"))return false;}
    if(s.group<7)s.mask=0;
    if(s.group==0)for(int i=0;i<5;++i){if(s.shooting[i]&&s.shot[i]&&s.payloadSeen[i]&&s.hit[i]&&(i!=3||s.invulnerableSamples>=300))s.mask|=1<<i;}
    if(s.group==1){for(int i=0;i<2;++i)if(s.captured[i]&&s.released[i])s.mask|=1<<i;if(s.coverDamaged)s.mask|=4;}
    if(s.group==2)for(int i=0;i<2;++i){if(s.shot[i]&&s.payloadSeen[i]&&s.controlled[i]&&s.hit[i*2+1]&&(i==0||(s.chargeFired&&s.fullChargeFired)))s.mask|=1<<i;}
    if(s.group==3){if(s.cloud&&s.shot[0]&&s.hit[0]&&s.hit[1])s.mask|=1;if(s.adaptive&&s.shot[1]&&s.hit[2])s.mask|=2;if(s.shooting[2]&&s.payloadSeen[2]&&s.hit[3])s.mask|=4;}
    if(s.group==4)for(int i=0;i<3;++i)if(s.converted[i][0]&&s.converted[i][1]&&s.moneyRetired[i][0]&&s.moneyRetired[i][1]&&s.rewardCoins.size()>=12&&(a->NetIsHost()||s.paid[i]))s.mask|=1<<i;
    if(s.group==5)
    {for(int i=0;i<2;++i)if(s.moving[i]&&s.gone[i]&&s.hit[i]&&!s.hit[2])s.mask|=1<<i;
     if(s.gone[2]&&s.pit&&s.hit[3])s.mask|=4;
     if(!s.goldPaths.empty()&&s.goldMotion&&!s.rewardCoins.empty())s.mask|=8;}
    if(s.group==6)
    {if(s.gone[0]&&s.cards[0].id&&s.planted[0].id&&!Card(b,0))s.mask|=1;
     if(s.gone[1]&&s.cards[1].id&&s.planted[1].id&&s.planted[2].id&&s.planted[3].id&&!Card(b,1))s.mask|=2;
     if(s.beanPayload&&s.hit[0])s.mask|=4;}
    if(!s.diagnostic||GetTickCount()-s.diagnostic>5000)
    {s.diagnostic=GetTickCount();NetTrace("SMOKE_BATCH269_PROOF role=%s group=%d elapsed=%d mask=%02x expected=%02x invulnTicks=%d captured=%d/%d released=%d/%d charge=%d/%d mc=%d/%d cloud=%d adaptive=%d ui=%d uiPhase=%d keys=%d MoneyRewards=%zu",Role(a),s.group,elapsed,s.mask,Expected(),s.invulnerableSamples,s.captured[0],s.captured[1],s.released[0],s.released[1],s.chargeFired,s.fullChargeFired,s.controlled[0],s.controlled[1],s.cloud,s.adaptive,s.ui,s.uiPhase,s.hotkeys,s.rewardCoins.size());
     if(s.group>=5)NetTrace("SMOKE_BATCH269_NEW_PROOF role=%s group=%d rollMove=%d/%d edge=%d/%d retired=%d/%d missHit=%d DoomGone=%d pit=%d goldUIDs=%zu goldMotion=%d payout38=%zu cards=%u:%u/%u:%u planted=%u/%u/%u/%u clientStep=%d PutDown=%d bean97=%d",Role(a),s.group,s.moving[0],s.moving[1],s.edgeSeen[0],s.edgeSeen[1],s.gone[0],s.gone[1],s.group==5?s.hit[2]:0,s.gone[2],s.pit,s.goldPaths.size(),s.goldMotion,s.rewardCoins.size(),s.cards[0].id,s.cards[0].token,s.cards[1].id,s.cards[1].token,s.planted[0].id,s.planted[1].id,s.planted[2].id,s.planted[3].id,s.cardStep,s.putDownSeen,s.beanPayload);
     if(s.group>=7)NetTrace("SMOKE_BATCH269_REST_PROOF role=%s group=%d mask=%02x expected=%02x aux=%d%d%d%d%d%d%d shot=%d%d%d%d%d%d%d event=%d%d%d%d%d%d%d childNPC=%zu Fert=%u:%u using=%d upgraded=%d ui=%d sourceNPCparity=NotClaimed periodic917=NotExecuted stage243=0 Travel=NotExecuted",Role(a),s.group,s.mask,Expected(),s.restAux[0],s.restAux[1],s.restAux[2],s.restAux[3],s.restAux[4],s.restAux[5],s.restAux[6],s.restShot[0],s.restShot[1],s.restShot[2],s.restShot[3],s.restShot[4],s.restShot[5],s.restShot[6],s.restEvent[0],s.restEvent[1],s.restEvent[2],s.restEvent[3],s.restEvent[4],s.restEvent[5],s.restEvent[6],s.restChildren.size(),s.fertilize.id,s.fertilize.token,s.restAux[3],s.fertilizeUsed,s.ui);
     if(s.group==11)NetTrace("SMOKE_BATCH269_WORLD_PROOF role=%s mask=%02x beam=%d ray49=%d homing91=%d Lour=%d volley=%d Meteorite2=%d FX51=%d ringUIDs=%zu ringPeak=%u exact180BirthCount=NotExecuted grab=%d stamped30=%d",Role(a),s.mask,s.worldBeam,s.worldRay,s.worldHoming,s.worldFly,s.worldVolley,s.worldMeteorite,s.worldFX51,s.worldRingIds.size(),s.worldRingPeak,s.worldGrab,s.world30); }
    return true;
}
bool Complete(LawnApp* a)
{return (a->NetIsHost()||(s.group!=4||s.ui==3)&&(s.group!=2||s.ui==2)&&(s.group!=6||(s.cardStep==4&&s.putDownSeen))&&(s.group!=7||s.ui==1)&&(s.group!=8||s.ui==2)&&(s.group!=9||s.fertilizeUsed)&&(s.group!=13||s.laserUI==4))&&s.mask==Expected()&&s.frames.contains("fusion_batch269_g"+std::to_string(s.group)+"_action")&&s.frames.contains("fusion_batch269_g"+std::to_string(s.group)+"_late");}
bool FinalDigest(LawnApp* a)
{
    LawnNet::NetBuffer digest;digest.WriteU16(uint16_t(Specs().size()));digest.WriteI32(GetNativeContext(a->mBoard)->money);
    for(int i=0;i<int(Specs().size());++i)
    {auto* p=Actor(a->mBoard,i);const auto* d=GetNativePlantData(p);digest.WriteU16(s.actors[i].id);digest.WriteU32(s.actors[i].token);digest.WriteI32(p?p->mPlantHealth:-1);digest.WriteI32(d?d->batchMode:-1);digest.WriteI32(d?d->batchBagCount:-1);digest.WriteI32(d?d->batchShotCount:-1);digest.WriteI32(d?d->productionTicks:-1);}
    for(auto id:s.zombies)
    {auto* z=ZombieById(a->mBoard,id);digest.WriteU16(id);digest.WriteU8(z?1:0);digest.WriteI32(z?z->mBodyHealth:-1);digest.WriteI32(z?z->mHelmHealth:-1);digest.WriteI32(z?z->mShieldHealth:-1);digest.WriteI32(z?z->mFlyingHealth:-1);digest.WriteU8(z&&z->mMindControlled?1:0);NetTrace("SMOKE_BATCH269_FINAL_NPC role=%s group=%d id=%u present=%d body=%d helm=%d shield=%d",Role(a),s.group,id,z?1:0,z?z->mBodyHealth:-1,z?z->mHelmHealth:-1,z?z->mShieldHealth:-1);}
    if(s.group==4)for(const auto& row:s.moneyChildren)for(const auto& child:row){digest.WriteU16(child.id);digest.WriteU32(child.token);}
    if(s.group>=5)
    {for(const auto& card:s.cards){digest.WriteU16(card.id);digest.WriteU32(card.token);}
     for(const auto& p:s.planted){digest.WriteU16(p.id);digest.WriteU32(p.token);}
     for(int i=0;i<2;++i){auto* c=Card(a->mBoard,i);digest.WriteU8(c?1:0);digest.WriteI32(c?c->mTimesDropped:-1);}
     digest.WriteU8(s.pit?1:0);}
    if(s.group>=7)
    {
        digest.WriteU16(uint16_t(s.restChildren.size()));for(const auto& [id,token]:s.restChildren)
        {digest.WriteU16(id);digest.WriteU32(token);auto* z=ZombieById(a->mBoard,id);digest.WriteU8(z?1:0);digest.WriteI32(z?z->mBodyHealth:-1);}
        digest.WriteU16(s.fertilize.id);digest.WriteU32(s.fertilize.token);
        digest.WriteU8(s.fertilizeSeen);digest.WriteU8(s.fertilizeUsed);
        if(s.group==9){auto* p=At(a->mBoard,8,3,1027);const auto* d=GetNativePlantData(p);digest.WriteU16(p?p->mNetId:0);digest.WriteU32(d?d->instanceToken:0);}
    }
    if(s.group==12)
    {digest.WriteU16(s.wheatChild.id);digest.WriteU32(s.wheatChild.token);digest.WriteU16(s.wheatCard.id);digest.WriteU32(s.wheatCard.token);digest.WriteU16(s.wheatKind);
     Plant* child=nullptr;bool present=false;while(a->mBoard->IteratePlants(child))if(!child->mDead&&child->mNetId==s.wheatChild.id){const auto* d=GetNativePlantData(child);if(d&&d->instanceToken==s.wheatChild.token){const auto* w=GetNativeWheat269State(child);present=true;digest.WriteI32(w?int(w->wheatTime*100):-1);break;}}
     digest.WriteU8(present?1:0);NetTrace("SMOKE_BATCH269_WHEAT_PROOF role=%s retired=%d child=%u token=%u kind=%u markClockAdvanced=%d present=%d card=%u cardToken=%u fullPoolDistribution=NotExecuted",Role(a),s.wheatGone,s.wheatChild.id,s.wheatChild.token,s.wheatKind,s.wheatAdvanced,present,s.wheatCard.id,s.wheatCard.token);}
    if(s.group==13)
    {for(int i=0;i<3;++i){auto* p=i==0?Actor(a->mBoard,0):LaserHelper(a->mBoard,i-1);const auto* d=GetNativePlantData(p);const auto* state=GetNativeWorld269PlantState(p);
       digest.WriteU16(p?p->mNetId:0);digest.WriteU32(d?d->instanceToken:0);digest.WriteI32(p?p->mPlantCol:-1);digest.WriteI32(p?p->mRow:-1);
       digest.WriteU8(state&&state->connected?1:0);digest.WriteU8(state&&state->main?1:0);digest.WriteU16(state?state->connectedPlant.id:0);digest.WriteU32(state?state->connectedPlant.token:0);digest.WriteI32(state?state->energy:-1);}}
    char hash[32];sprintf_s(hash,"HP_%08x",LawnNet::NetFnv1a32(digest.Data(),digest.Size()));return Barrier(a,hash);
}

}
namespace LawnNet
{
bool NetSmokeUpdateFusion269(LawnApp* a)
{
    const char* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_BATCH269"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>200000){char why[160];sprintf_s(why,"269 bounded deadline group=%d stage=%d mask=%02x expected=%02x ui=%d",s.group,s.stage,s.mask,Expected(),0);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(!s.selectorRead)
    {
        s.selectorRead=true;const char* selected=std::getenv("PVZ_COOP_SMOKE_FUSION_BATCH269_GROUP");
        if(selected&&*selected)
        {
            if(!Require(a,((selected[0]>='0'&&selected[0]<='9'&&!selected[1])||(selected[0]=='1'&&(selected[1]=='0'||selected[1]=='1'||selected[1]=='2'||selected[1]=='3')&&!selected[2])),"269 group selector must be 0..13"))return true;
            s.selectedGroup=s.group=std::atoi(selected);
        }
        NetTrace("SMOKE_BATCH269_SCOPE role=%s selectedGroup=%d groupsExpected=%d sourcePlantsExpected=%zu",Role(a),s.selectedGroup,s.selectedGroup<0?14:1,s.selectedGroup<0?std::size_t(53):Specs().size());
    }
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"B269_BEGIN");}
        else if(Has(a,1,"B269_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(s.stage==10)
    {
        if(!Barrier(a,"NEXT"))return true;
        ++s.completedGroups;s.completedPlants+=int(Specs().size());
        if(s.selectedGroup>=0||s.group==13){NetTrace("SMOKE_BATCH269_COMPLETE role=%s groups=%d sourcePlants=%d freshFrames=%d selectedGroup=%d initialFactory=1 acquisition=NotExecuted sourceAI=NotClaimed meteorite=%s Solar69=NotExecuted Travel=NotExecuted acceptedAdded=0 fullParity=NotClaimed postSetupActorWrites=0",Role(a),s.completedGroups,s.completedPlants,s.completedGroups*4,s.selectedGroup,(s.selectedGroup<0||s.selectedGroup==11)?"Type2SubsetObserved":"NotExecuted");a->CoopSmokeWriteResult(true,nullptr);return true;}
        State next;next.group=s.group+1;next.previousEpoch=s.epoch;next.chat=s.chat;next.selectorRead=true;next.selectedGroup=s.selectedGroup;next.completedGroups=s.completedGroups;next.completedPlants=s.completedPlants;s=std::move(next);Advance(a,1);
        if(a->NetIsHost()){std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard))return true;auto* b=a->mBoard;const auto* context=GetNativeContext(b);
    if(context->epoch<=s.previousEpoch)return true;if(!s.epoch)s.epoch=context->epoch;
    if(!Require(a,context->epoch==s.epoch&&context->validation&&context->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10,"269 requires real matching native validation world"))return true;
    const auto& bank=context->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
        int own=0;for(const auto& packet:bank.packets)if(packet.owner==Slot(a))++own;if(own<s.waiting)return true;
        if(own<5){const int wanted=5*Slot(a)+own;int dense=-1;for(std::size_t i=0;i<context->eligible.size();++i)if(context->eligible[i].kind==wanted&&!context->eligible[i].isExtra)dense=int(i);if(!Require(a,dense>=0,"269 native chooser card missing"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;}
        if(bank.packets.size()==10)Advance(a,2);
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;
        if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"269 Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000,"269 full native intro skipped"))return true;
        if(s.entry<0){s.entry=a->mAppCounter;return true;}if(a->mAppCounter<=s.entry)return true;
        if(a->NetIsHost()&&!s.setup&&!Setup(a))return true;if(b->mPaused)Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(!b->mPaused||!Baseline(a))
        {
            if(!s.diagnostic||GetTickCount()-s.diagnostic>5000){s.diagnostic=GetTickCount();NetTrace("SMOKE_BATCH269_BASELINE_WAIT role=%s group=%d ids=%zu ready=%d paused=%d",Role(a),s.group,s.zombies.size(),s.idsReady?1:0,b->mPaused?1:0);}
            return true;
        }
        if(!Observe(a))return true;if(!s.samples){s.samples=Samples(a);return true;}
        if(Samples(a)-s.samples<2||!Capture(a,"before")||!Barrier(a,"BASELINE"))return true;
        if(s.start<0)
        {s.start=b->mMainCounter;NetTrace("SMOKE_BATCH269_BASELINE_CLOCK role=%s group=%d localStart=%d paused=1 snapshots=%u",Role(a),s.group,s.start,Samples(a));}
        s.baseline=true;
        if(a->NetIsHost()&&!s.resumeRequested)s.resumeRequested=Escape(a);
        if(s.resumeRequested||a->NetIsClient())Advance(a,5);
    }
    else if(s.stage==5)
    {
        if(b->mPaused)return true;UseUI(a);if(!Observe(a))return true;if(a->mCoopSmokeResultWritten)return true;
        const int minimum=s.group==0?500:s.group==1?900:s.group==2?1000:s.group==3?1200:s.group==4?3500:s.group==5?800:s.group==8?4800:s.group==12?3300:s.group==11?1800:1200;
        if(b->mMainCounter-s.start<minimum||!Complete(a)||!Barrier(a,"EFFECTS"))return true;
        s.pauseRequested=false;Advance(a,6);
    }
    else if(s.stage==6)
    {
        if(a->NetIsHost()&&!s.pauseRequested)s.pauseRequested=Escape(a);
        if(b->mPaused){s.samples=Samples(a);Advance(a,7);}
    }
    else if(s.stage==7)
    {
        if(!b->mPaused||Samples(a)-s.samples<2||!Complete(a))return true;
        if(!Observe(a)||!FinalDigest(a)||!Capture(a,"complete")||!Barrier(a,"FINAL"))return true;
        NetTrace("SMOKE_BATCH269_CASE role=%s group=%d epoch=%u kinds=%zu mask=%02x finalSameHP=1 snapshots=%u acquisition=NotExecuted sourceAI=NotClaimed meteorite=%s Solar69=NotExecuted Travel=NotExecuted acceptedAdded=0 postSetupActorWrites=0",Role(a),s.group,s.epoch,Specs().size(),s.mask,Samples(a),s.group==11?"Type2SubsetObserved":"NotExecuted");Advance(a,10);
    }
    return true;
}
}
