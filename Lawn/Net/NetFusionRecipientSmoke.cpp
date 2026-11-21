#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/GameButton.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeRecipientShooters.h"
#include "../Fusion/FusionBasePlantRules.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
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
#include <vector>

namespace
{
using namespace LawnFusion;
using LawnNet::NetTrace;
struct Flight { int way=0,index=-1,samples=0,firstTick=0;uint32_t generation=0;float x=0,y=0;bool moved=false,retired=false; };
struct State
{
    int group=0,stage=0,waitingOwn=0,introChecks=0,expectedSun=500,price=0,cooldown=0,observedHits=0,playingAppFrame=-1;
    uint32_t epoch=0,oldEpoch=0,since=0,lastChat=0,lastWheel=0,introFirst=0,introLast=0,pickSamples=0,pauseSamples=0;
    uint16_t plant=0,packet=0;uint32_t token=0;std::vector<uint16_t> targets;std::string ids;
    bool configured=false,picked=false,placed=false,purchased=false,setup=false,pauseRequested=false,resumeRequested=false,pauseSampled=false,before=false,after=false,diagnosed=false,pauseVerified=false,finalLogged=false;
    unsigned paths=0;std::map<uint16_t,Flight> flights;std::set<std::string> frames;std::map<std::string,int> firstDraw;
    std::array<int,5> pauseBody{},pauseShield{};int pauseCounter=0;float pausePhase=-1;uint32_t pausedAt=0;
} s;
int Kind(){return s.group==0?21:23;}
int Owner(){return s.group==0?0:1;}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();s.diagnosed=false;NetTrace("SMOKE_FUSION_RECIPIENT_STAGE role=%s group=%d epoch=%u stage=%d",Role(a),s.group,s.epoch,stage);}
std::string Tag(const char* name){return "FR224_"+std::string(name)+"_"+std::to_string(s.group)+"_"+std::to_string(s.epoch);}
bool Has(LawnApp* a,int slot,const std::string& text){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==slot&&line.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text)
{if(Has(a,Slot(a),text))return true;if(s.lastChat&&GetTickCount()-s.lastChat<800)return false;s.lastChat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);}
bool Barrier(LawnApp* a,const char* name){auto tag=Tag(name);Send(a,tag);return Has(a,0,tag)&&Has(a,1,tag);}
bool Key(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Recipient owned SDL Escape failed");}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Recipient owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y){return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {
        if(s.lastWheel&&GetTickCount()-s.lastWheel<250)return false;s.lastWheel=GetTickCount();
        SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);
        SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;
        Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Recipient chooser owned SDL wheel failed");return false;
    }
    if(std::abs(c->mScrollAmount)>.05f)return false;
    return Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_recipient_"+std::to_string(s.group)+"_"+suffix;if(s.frames.contains(name))return true;
    if(!s.firstDraw.contains(name)){s.firstDraw[name]=a->mDrawCount;return false;}if(a->mDrawCount-s.firstDraw[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Recipient actual renderer capture failed"))return false;s.frames.insert(name);NetTrace("SMOKE_FUSION_RECIPIENT_RENDER role=%s group=%d frame=%s freshDraws=%d",Role(a),s.group,name.c_str(),a->mDrawCount-s.firstDraw[name]);return true;
}
Plant* Actor(Board* b){Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mNetId==s.plant&&d&&d->instanceToken==s.token)return p;}return nullptr;}
Zombie* Target(Board* b,uint16_t id){Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(!z->mDead&&z->mNetId==id)return z;return nullptr;}
SeedPacket* Card(Board* b)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(p);if(d&&d->kind==Kind()&&!d->isExtra&&d->owner==Owner())return p;}return nullptr;}
void ObserveIntroRetirement(LawnApp* a)
{
    auto* b=a->mBoard;int livePlants=0,liveZombies=0,deadCutscene=0;Plant* p=nullptr;Zombie* z=nullptr;
    while(b->mPlants.IterateNext(p)){livePlants+=!p->mDead;NetTrace("SMOKE_FUSION_RECIPIENT_ENTRY_PLANT role=%s group=%d id=%u generation=%u dead=%d onBoard=%d",Role(a),s.group,p->mNetId,b->mPlants.DataArrayGetID(p),p->mDead?1:0,p->mIsOnBoard?1:0);}
    while(b->mZombies.IterateNext(z)){liveZombies+=!z->mDead;deadCutscene+=z->mDead&&z->mFromWave==Zombie::ZOMBIE_WAVE_CUTSCENE;NetTrace("SMOKE_FUSION_RECIPIENT_ENTRY_ZOMBIE role=%s group=%d id=%u generation=%u dead=%d wave=%d",Role(a),s.group,z->mNetId,b->mZombies.DataArrayGetID(z),z->mDead?1:0,z->mFromWave);}
    NetTrace("SMOKE_FUSION_RECIPIENT_ENTRY_WAIT role=%s group=%d epoch=%u appFrame=%d plantAllocated=%u zombieAllocated=%u livePlants=%d liveZombies=%d deadCutscene=%d",Role(a),s.group,s.epoch,a->mAppCounter,b->mPlants.mSize,b->mZombies.mSize,livePlants,liveZombies,deadCutscene);
}
bool Purchase(LawnApp* a)
{
    auto* b=a->mBoard;auto* card=Card(b);const auto* definition=FusionFindPlant(Kind());if(!Require(a,card&&definition&&definition->maxHealth==300,"Recipient source definition/native card missing"))return false;
    const auto* packet=GetNativePacketData(card);s.price=definition->sunCost;s.cooldown=int(std::lround(definition->rechargeSeconds*100));
    if(!Require(a,packet&&packet->price==s.price&&s.price==125&&s.cooldown==750,"Recipient canonical source price/CD differs"))return false;
    if(Slot(a)==Owner()&&!s.picked)
    {if(!card->mActive||card->mRefreshing)return false;auto* bank=b->mSeedBank;s.pickSamples=Samples(a);s.picked=BoardClick(a,float(bank->mX+card->mX+card->mOffsetX+25),float(bank->mY+card->mY+35));return false;}
    if(Slot(a)==Owner()&&!s.placed)
    {
        auto* cursor=b->mCursorObject;const bool held=cursor&&cursor->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&cursor->mSeedBankIndex>=0&&cursor->mSeedBankIndex<b->mSeedBank->mNumPackets&&b->mSeedBank->mSeedPackets[cursor->mSeedBankIndex].mNetId==card->mNetId;
        if(!held||(a->NetIsClient()&&Samples(a)<=s.pickSamples))return false;if(!Require(a,b->mSunMoney==500,"Recipient pickup charged currency"))return false;
        s.placed=BoardClick(a,float(b->GetCellCenterX(s.group?5:2,2)),float(b->GridToPixelY(s.group?5:2,2)+50));return false;
    }
    if(!s.purchased)
    {
        Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&d&&d->kind==Kind()){s.plant=p->mNetId;s.token=d->instanceToken;break;}}
        p=Actor(b);const auto* d=GetNativePlantData(p);if(!d)return false;
        if(!Require(a,d->owner==Owner()&&d->packetId==card->mNetId&&d->firstParent==kFusionNoPlant&&d->secondParent==kFusionNoPlant&&!d->randomOrigin&&s.token&&p->mPlantHealth==300&&p->mPlantMaxHealth==300&&p->mSeedType==GetNativeSeedForPlant(Kind())&&b->mSunMoney==375&&card->mRefreshTime==750&&IsNativeRecipientShooter(p),"Recipient real paid source actor/owner/HP/CD differs"))return false;
        s.packet=card->mNetId;s.expectedSun=375;s.purchased=true;
        NetTrace("SMOKE_FUSION_RECIPIENT_PURCHASE role=%s group=%d epoch=%u kind=%d id=%u token=%u packet=%u owner=%d alias=%d HP=300 cost=125 sun=375 cooldown=750 nativeSDL=1",Role(a),s.group,s.epoch,Kind(),s.plant,s.token,s.packet,Owner(),int(p->mSeedType));
    }
    return Barrier(a,"PAID");
}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;auto* plant=Actor(b);if(!plant)return false;
    if(a->NetIsHost()&&!s.setup)
    {
        float x=0,y=0;if(!Require(a,plant->GetShadowCenter(x,y),"Recipient logical source shadow unavailable"))return false;
        for(int i=0;i<(s.group?5:2);++i)
        {
            FusionRecipientEmissionSpec e;if(!Require(a,TryGetFusionRecipientEmission(Kind(),s.group?i:0,e),"Recipient source emission unavailable"))return false;
            const int row=s.group?(i==0?0:i==1?4:i==2?2:i==3?1:4):2;auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,row,0);
            auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;if(!Require(a,z&&z->mNetId&&body,"Recipient normal native NPC allocation failed"))return false;
            if(s.group&&i==2){z->mShieldType=SHIELDTYPE_DOOR;z->AttachShield();}
            z->mBodyHealth=z->mBodyMaxHealth=5000;z->mHelmHealth=z->mHelmMaxHealth=0;z->mShieldHealth=z->mShieldMaxHealth=s.group&&i==2?3000:0;
            // Native walking always corrects normal actors toward this lane Y.
            // Keep that real anchor and solve X from the actual body rectangle;
            // no artificial Y can drift before a randomized initial deadline.
            z->mPosY=z->GetPosYBasedOnRow(row);z->mY=int(z->mPosY);auto rect=z->GetZombieRect();
            const double rootX=x+e.muzzleX*kFusionBasePixelsX,rootY=y-e.muzzleY*kFusionBasePixelsY;
            const double cy=rect.mY+rect.mHeight*.5,t=s.group?(std::abs(e.velocityY)>.1?(cy-rootY)/e.velocityY:45):((i?735:650)-rootX)/e.velocityX;
            if(!Require(a,std::isfinite(t)&&t>15&&t<180,"Recipient canonical lane cannot intersect source trajectory"))return false;
            const float cx=static_cast<float>(rootX+e.velocityX*t);z->mPosX+=cx-(rect.mX+rect.mWidth*.5f);z->mX=int(z->mPosX);z->mVelX=0;z->ApplyAnimRate(0);rect=z->GetZombieRect();
            const FusionRecipientRect targetRect{double(rect.mX),double(rect.mY),double(rect.mWidth),double(rect.mHeight)};
            if(!Require(a,FusionRecipientProjectileTouchesRect(e.projectileKind,rootX+e.velocityX*t,rootY+e.velocityY*t,e.velocityX,e.velocityY,targetRect),"Recipient actual canonical body misses source collision shape"))return false;
            if(s.group){constexpr size_t acquisitionRay[]={2,3,4,0,1};double distance=0;if(!Require(a,TryGetFusionStarAcquisitionDistance(acquisitionRay[i],row==plant->mRow,x,y-.3*kFusionBasePixelsY,targetRect,distance),"Recipient canonical body misses distinct source acquisition ray"))return false;}
            s.targets.push_back(z->mNetId);NetTrace("SMOKE_FUSION_RECIPIENT_SETUP group=%d index=%d target=%u row=%d body=5000 shield=%d center=%.4f,%.4f oneTimeInitialValues=1 sourceZombieAIUnported=1 canonicalLaneY=1 contactTick=%.4f",s.group,i,z->mNetId,row,z->mShieldHealth,cx,cy,t);
        }
        std::ostringstream ids;ids<<Tag("IDS");for(auto id:s.targets)ids<<' '<<id;s.ids=ids.str();s.setup=true;
    }
    if(a->NetIsHost()){if(!Send(a,s.ids))return false;}
    else if(!s.setup)
    {
        const auto prefix=Tag("IDS")+" ";for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.rfind(prefix,0)==0)
        {std::istringstream in(line.mText.substr(prefix.size()));unsigned n=0;while(in>>n){if(!Require(a,n&&n<=65535,"Recipient malformed NPC identity barrier"))return false;s.targets.push_back(uint16_t(n));}s.setup=true;break;}
    }
    if(!s.setup||s.targets.size()!=size_t(s.group?5:2))return false;
    for(size_t i=0;i<s.targets.size();++i){auto* z=Target(b,s.targets[i]);if(!z||z->mBodyHealth!=5000||z->mShieldHealth!=(s.group&&i==2?3000:0))return false;}
    return Barrier(a,"TARGETS");
}
int Direction(float vx,float vy)
{if(std::abs(vx)<.1f)return vy<0?0:1;if(vx<0)return 2;return vy<0?3:4;}
void Observe(LawnApp* a)
{
    auto* b=a->mBoard;Projectile* p=nullptr;while(b->mProjectiles.IterateNext(p))
    {
        const auto* d=GetNativeProjectileData(p);if(!d||d->kind!=(s.group?39:37)||p->mDead||!IsNativeRecipientProjectile(p))continue;
        float x=0,y=0;if(!GetNativeRecipientProjectileCenter(p,x,y))continue;
        auto [it,fresh]=s.flights.try_emplace(p->mNetId);auto& f=it->second;
        if(fresh){f.x=x;f.y=y;f.way=d->specialType;f.generation=b->mProjectiles.DataArrayGetID(p);f.firstTick=b->mMainCounter;NetTrace("SMOKE_FUSION_RECIPIENT_PROJECTILE role=%s group=%d epoch=%u id=%u payload=%u way=%u type=%d motion=%d damage=%d sun=%d x=%.5f y=%.5f",Role(a),s.group,s.epoch,p->mNetId,d->kind,d->specialType,int(p->mProjectileType),int(p->mMotionType),d->damage,d->sunOnHit,x,y);}
        if(!Require(a,d->damage==20&&!d->sunOnHit&&(s.group?d->specialType==2||d->specialType==9:d->specialType==0),"Recipient native payload/source way changed"))return;
        const float dx=x-f.x,dy=y-f.y;
        if(!f.moved&&std::abs(dx)+std::abs(dy)>3)
        {
            const int index=s.group?Direction(dx,dy):0;f.index=index;f.moved=true;s.paths|=1u<<index;
            if(!Require(a,s.group?((index==2)==(d->specialType==9)):std::abs(dy)<.25f&&dx>0,"Recipient original source way/physical curve differs"))return;
            if(a->NetIsHost())
            {FusionRecipientEmissionSpec e;TryGetFusionRecipientEmission(Kind(),index,e);if(!Require(a,std::abs(p->mVelX-e.velocityX)<1e-4&&std::abs(p->mVelY-e.velocityY)<1e-4,"Recipient native physical velocity differs"))return;}
            NetTrace("SMOKE_FUSION_RECIPIENT_CURVE role=%s group=%d epoch=%u id=%u index=%d way=%u dx=%.6f dy=%.6f vx=%.9g vy=%.9g hostPhysics=%d",Role(a),s.group,s.epoch,p->mNetId,index,d->specialType,dx,dy,p->mVelX,p->mVelY,a->NetIsHost()?1:0);
        }++f.samples;
    }
    for(auto& [id,f]:s.flights)if(!f.retired&&b->mMainCounter>f.firstTick)
    {auto* observed=b->mProjectiles.DataArrayTryToGet(f.generation);if(!observed||observed->mDead){f.retired=true;NetTrace("SMOKE_FUSION_RECIPIENT_RETIRED role=%s group=%d epoch=%u id=%u localGeneration=%u nextFrame=1 deadOrFreed=1",Role(a),s.group,s.epoch,id,f.generation);}}
    if(s.stage==8&&s.paths)Capture(a,"flight");
}
bool Outcome(LawnApp* a,bool final)
{
    auto* b=a->mBoard;bool done=true;for(size_t i=0;i<s.targets.size();++i)
    {
        auto* z=Target(b,s.targets[i]);if(!Require(a,z&&z->mBodyMaxHealth==5000&&z->mHelmHealth==0,"Recipient controlled target disappeared/armour changed"))return false;
        const int damage=5000-z->mBodyHealth,expected=s.group?20:i?0:40;
        if(damage!=expected)done=false;
        if(!Require(a,damage>=0&&damage<=expected&&damage%20==0&&z->mShieldHealth==(s.group&&i==2?3000:0),"Recipient direct damage/shield bypass differs"))return false;
        if(final)NetTrace("SMOKE_FUSION_RECIPIENT_OUTCOME role=%s group=%d epoch=%u index=%zu target=%u row=%d body=%d max=%d shield=%d shieldMax=%d expectedDamage=%d",Role(a),s.group,s.epoch,i,z->mNetId,z->mRow,z->mBodyHealth,z->mBodyMaxHealth,z->mShieldHealth,z->mShieldMaxHealth,expected);
    }
    const auto retired=std::count_if(s.flights.begin(),s.flights.end(),[](const auto& entry){return entry.second.retired;});
    return done&&s.paths==(s.group?31u:1u)&&s.flights.size()>=size_t(s.group?5:2)&&retired>=(s.group?5:2);
}
bool PauseProof(LawnApp* a)
{
    auto* b=a->mBoard;auto* plant=Actor(b);const auto* d=GetNativePlantData(plant);if(!Require(a,plant&&d&&b->mPaused,"Recipient paused actor missing"))return false;
    if(!s.pauseSampled){s.pauseSampled=true;s.pauseSamples=Samples(a);s.pausedAt=GetTickCount();s.pauseCounter=plant->mLaunchCounter;s.pausePhase=d->shootSeconds;for(size_t i=0;i<s.targets.size();++i){auto* z=Target(b,s.targets[i]);s.pauseBody[i]=z->mBodyHealth;s.pauseShield[i]=z->mShieldHealth;}return false;}
    for(size_t i=0;i<s.targets.size();++i){auto* z=Target(b,s.targets[i]);if(!Require(a,z&&z->mBodyHealth==s.pauseBody[i]&&z->mShieldHealth==s.pauseShield[i],"Recipient health changed during actual pause"))return false;}
    if(!Require(a,plant->mLaunchCounter==s.pauseCounter&&d->shootSeconds==s.pausePhase,"Recipient source native counter advanced during pause"))return false;
    if(GetTickCount()-s.pausedAt<400||Samples(a)-s.pauseSamples<2)return false;
    NetTrace("SMOKE_FUSION_RECIPIENT_PAUSE role=%s group=%d epoch=%u stableMs=%u snapshots=%u launch=%d phase=%.6f",Role(a),s.group,s.epoch,GetTickCount()-s.pausedAt,Samples(a)-s.pauseSamples,plant->mLaunchCounter,d->shootSeconds);return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionRecipient(LawnApp* a)
{
    const auto* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_RECIPIENT");const auto* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>45000U){char why[110];sprintf_s(why,"Recipient deadline group=%d stage=%d role=%s paths=%u targets=%zu",s.group,s.stage,Role(a),s.paths,s.targets.size());a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"FR224_BEGIN");}else if(Has(a,1,"FR224_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(s.stage==11)
    {
        if(!Barrier(a,"DONE"))return true;NetTrace("SMOKE_FUSION_RECIPIENT_CASE role=%s group=%d epoch=%u kind=%d paidNative=1 naturalHits=1 pause=1",Role(a),s.group,s.epoch,Kind());
        if(s.group==1){NetTrace("SMOKE_FUSION_RECIPIENT_COMPLETE role=%s kinds=21,23 activities=2 purchases=2 cactusRepeated20=1 cactusOneContact=1 starPaths=5 crossrow=1 leftShieldless=1 sourceZombieAIUnported=1 postSetupActorWrites=0 air10=NotExecuted starStorage=NotExecuted nativeBoard=1 secondWorld=0",Role(a));Advance(a,12);a->CoopSmokeWriteResult(true,nullptr);return true;}
        const auto old=s.epoch;State next;next.group=1;next.oldEpoch=old;next.lastChat=s.lastChat;s=std::move(next);Advance(a,1);if(a->NetIsHost()){std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient()&&!s.oldEpoch)Send(a,"FR224_BEGIN");return true;}
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);if(ctx->epoch<=s.oldEpoch)return true;
    if(!s.epoch){s.epoch=ctx->epoch;NetTrace("SMOKE_FUSION_RECIPIENT_EPOCH role=%s group=%d epoch=%u previous=%u",Role(a),s.group,s.epoch,s.oldEpoch);}
    if(!Require(a,ctx->epoch==s.epoch&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Recipient requires canonical actual native epoch"))return true;
    const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;const int slot=Slot(a);
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;
        if(!Require(a,bank.activeMask==3&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5,"Recipient native shared chooser quota differs"))return true;
        std::vector<int> choices;for(size_t i=0;i<ctx->eligible.size();++i){const auto& e=ctx->eligible[i];if(slot==0?(!e.isExtra&&e.kind!=23):(e.isExtra||e.kind==23))choices.push_back(int(i));}
        auto priority=[&](int i){const auto& e=ctx->eligible[i];return e.kind==(slot==0?21:23)&&!e.isExtra?0:10+int(e.kind);};std::stable_sort(choices.begin(),choices.end(),[&](int x,int y){return priority(x)<priority(y);});
        if(!Require(a,choices.size()>=5&&ctx->eligible[choices[0]].kind==(slot==0?21:23)&&!ctx->eligible[choices[0]].isExtra,"Recipient exact source base card not eligible"))return true;
        int own=0;for(const auto& p:bank.packets)own+=p.owner==slot;if(own<s.waitingOwn)return true;if(own<5){if(Choice(a,chooser,choices[own]))s.waitingOwn=own+1;return true;}
        if(bank.packets.size()!=10)return true;int rank=-1;for(int i=0;i<b->mSeedBank->mNumPackets;++i){int r=NetSeedBankOwnerRank(slot,b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,r>=rank,"Recipient own-first native bank order changed"))return true;rank=r;}
        if(!Capture(a,"chooser"))return true;NetTrace("SMOKE_FUSION_RECIPIENT_POOL role=%s group=%d eligible=%zu target=10 quota0=5 quota1=5 ownFirst=1 nativeChooser=1",Role(a),s.group,ctx->eligible.size());Advance(a,2);
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Recipient actual Ready hover failed");}else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000&&b->mSunMoney==500,"Recipient skipped native full intro"))return true;
        // CutScene marks preview zombies dead in the same Board.Update that
        // starts Playing. Smoke runs before the next ProcessDeleteQueue, so
        // yield one ordinary app frame before checking allocated emptiness.
        if(s.playingAppFrame<0){s.playingAppFrame=a->mAppCounter;ObserveIntroRetirement(a);return true;}if(a->mAppCounter<=s.playingAppFrame)return true;
        if(a->NetIsHost()){if(!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize,"Recipient requires fresh empty activity after natural intro retirement"))return true;NetTrace("SMOKE_FUSION_RECIPIENT_ENTRY_EMPTY role=host group=%d epoch=%u firstAppFrame=%d observedAppFrame=%d plantAllocated=0 zombieAllocated=0 naturalCleanup=1",s.group,s.epoch,s.playingAppFrame,a->mAppCounter);b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;}Advance(a,4);
    }
    else if(s.stage==4){if(Barrier(a,"PLAYING")&&Purchase(a))Advance(a,5);}
    else if(s.stage==5)
    {if(a->NetIsHost()&&!s.pauseRequested)s.pauseRequested=Key(a);if(b->mPaused)Advance(a,6);}
    else if(s.stage==6)
    {if(Setup(a)){s.pauseSampled=false;Advance(a,7);}}
    else if(s.stage==7)
    {
        if(!s.pauseSampled){s.pauseSamples=Samples(a);s.pauseSampled=true;}if(Samples(a)-s.pauseSamples<2)return true;
        if(!s.before){for(size_t i=0;i<s.targets.size();++i){auto* z=Target(b,s.targets[i]);if(!Require(a,z&&z->mBodyHealth==5000&&z->mShieldHealth==(s.group&&i==2?3000:0),"Recipient paused initial NPC state differs"))return true;NetTrace("SMOKE_FUSION_RECIPIENT_BEFORE role=%s group=%d epoch=%u index=%zu target=%u row=%d body=%d max=%d shield=%d shieldMax=%d",Role(a),s.group,s.epoch,i,z->mNetId,z->mRow,z->mBodyHealth,z->mBodyMaxHealth,z->mShieldHealth,z->mShieldMaxHealth);}s.before=true;}
        if(!Capture(a,"before")||!Barrier(a,"BEFORE"))return true;if(a->NetIsHost()&&!s.resumeRequested){s.resumeRequested=Key(a);return true;}if(!b->mPaused&&!a->GetDialogCount())Advance(a,8);
    }
    else if(s.stage==8)
    {
        Observe(a);if(a->mCoopSmokeResultWritten)return true;const bool done=Outcome(a,false);
        if(!Require(a,b->mSunMoney==375&&Actor(b)&&Card(b)&&Card(b)->mNetId==s.packet&&Card(b)->mRefreshTime==750,"Recipient wallet/identity/cooldown changed outside actual purchase"))return true;
        if(done&&a->NetIsHost()&&!s.after){s.after=Key(a);return true;}
        if(done&&s.frames.contains("fusion_recipient_"+std::to_string(s.group)+"_flight")&&Barrier(a,"HITS"))Advance(a,9);
        else if(!s.diagnosed&&GetTickCount()-s.since>5000){s.diagnosed=true;NetTrace("SMOKE_FUSION_RECIPIENT_WAIT role=%s group=%d paths=%u projectiles=%zu",Role(a),s.group,s.paths,s.flights.size());for(auto id:s.targets){auto* z=Target(b,id);NetTrace("SMOKE_FUSION_RECIPIENT_WAIT_TARGET role=%s id=%u body=%d shield=%d",Role(a),id,z?z->mBodyHealth:-1,z?z->mShieldHealth:-1);}}
    }
    else if(s.stage==9){if(b->mPaused){s.pauseSampled=false;Advance(a,10);}}
    else if(s.stage==10){if(!s.pauseVerified){if(!PauseProof(a))return true;s.pauseVerified=true;}if(!s.finalLogged){if(!Outcome(a,true))return true;s.finalLogged=true;}if(Capture(a,"complete"))Advance(a,11);}
    return true;
}
}
