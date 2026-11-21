#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetMystery.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeBasePlants.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../Resources.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using namespace LawnFusion;
constexpr uint16_t kKinds[]={5,6,7};
constexpr int kOwners[]={0,1,0},kRows[]={0,2,4},kColumns[]={2,2,2};
constexpr int kPrices[]={150,25,75},kSunBefore[]={500,350,325},kSunAfter[]={350,325,250};
enum TargetIndex { CHOMP_A,CHOMP_B,PUFF_PREY,FUME_BODY,FUME_SHIELD,FUME_ADJACENT,FUME_OUTSIDE,TARGET_COUNT };
constexpr int kTargetPlants[]={0,0,1,2,2,2,2},kTargetRows[]={0,0,2,4,4,3,4};
constexpr float kTargetDx[]={90,260,135,100,370,100,float(7.0*72.0/1.35+5.0)};
struct BatchState
{
    int stage=0,waitingOwn=0,firstDraw=0,biteTick=0,digestRemaining=0,swallowTick=0;
    int puffPhase=0,puffAge=-1,puffTick=0,puffMaxAge=-1,puffBornTick=0;
    float puffX=0;
    uint32_t since=0,settled=0,lastChat=0,captureSince=0,heldSamples=0,introStart=0,introLast=0;
    int introChecks=0;
    std::array<uint16_t,3> packets{},plants{};
    std::array<int,3> uses{};
    std::array<uint16_t,TARGET_COUNT> targets{};
    uint16_t hitProjectile=0,missProjectile=0;
    bool configured=false,ambientConfigured=false,clicked=false,placed=false,purchaseObserved=false,setup=false,activated=false,chompLocked=false,chompKilled=false;
    bool fumeHit=false,clientFume=false,clientPuff=false,clientDigest=false,clientCombat=false,puffHit=false,puffExpired=false;
    bool movedChompB=false;
    std::string pendingFrame,idMessage;
    std::set<std::string> frames;
} s;
const char* Role(LawnApp* app) { return app->NetIsHost()?"host":"client"; }
uint32_t Samples(LawnApp* app) { return app->NetIsHost()?app->mCoopSmokeSnapshotsSent:app->mCoopSmokeSnapshotsApplied; }
bool Require(LawnApp* app,bool value,const char* error) { if(!value)app->CoopSmokeFail(error);return value; }
void Advance(LawnApp* app,int stage)
{
    s.stage=stage;s.since=GetTickCount();s.settled=0;s.clicked=false;s.placed=false;s.purchaseObserved=false;s.pendingFrame.clear();
    LawnNet::NetTrace("SMOKE_FUSION_BASE_STAGE role=%s stage=%d",Role(app),stage);
}
bool Has(LawnApp* app,int slot,const std::string& message)
{
    for(const auto& line:app->mNetManager->mChatLines)if(line.mSlot==slot&&line.mText==message)return true;
    return false;
}
bool Send(LawnApp* app,const std::string& message)
{
    const int slot=app->mNetManager->GetMySlot();if(Has(app,slot,message))return true;
    if(s.lastChat&&GetTickCount()-s.lastChat<900)return false;
    s.lastChat=GetTickCount();app->mNetManager->SendChat(message);return Has(app,slot,message);
}
bool Barrier(LawnApp* app,const char* message) { Send(app,message);return Has(app,0,message)&&Has(app,1,message); }
bool Settled(uint32_t duration=250) { if(!s.settled)s.settled=GetTickCount();return GetTickCount()-s.settled>=duration; }
bool Capture(LawnApp* app,const char* name)
{
    if(s.frames.contains(name))return true;
    if(s.pendingFrame!=name){s.pendingFrame=name;s.firstDraw=app->mDrawCount;s.captureSince=GetTickCount();return false;}
    if(app->mDrawCount-s.firstDraw<2||GetTickCount()-s.captureSince<250)return false;
    if(!Require(app,app->CoopSmokeCaptureFrame(name),"Base batch actual native frame capture failed"))return false;
    s.frames.insert(name);LawnNet::NetTrace("SMOKE_FUSION_BASE_FRAME role=%s name=%s freshDraws=%d nativeSDL=1",Role(app),name,app->mDrawCount-s.firstDraw);
    return true;
}
bool Click(LawnApp* app,float x,float y)
{
    SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=SDL_GetWindowID(app->mSDLWindow);motion.motion.x=x;motion.motion.y=y;
    SDL_Event down{};down.type=SDL_EVENT_MOUSE_BUTTON_DOWN;down.button.windowID=motion.motion.windowID;down.button.button=SDL_BUTTON_LEFT;
    down.button.down=true;down.button.clicks=1;down.button.x=x;down.button.y=y;
    SDL_Event up=down;up.type=SDL_EVENT_MOUSE_BUTTON_UP;up.button.down=false;
    return Require(app,SDL_PushEvent(&motion)&&SDL_PushEvent(&down)&&SDL_PushEvent(&up),"Base batch owned-window SDL queue failed");
}
bool BoardClick(LawnApp* app,float x,float y)
{
    return Click(app,app->mBoard->mX+app->mDDInterface->mWideScreenOffsetX+x,app->mBoard->mY+app->mDDInterface->mWideScreenOffsetY+y);
}
bool Button(LawnApp* app,GameButton* button)
{
    if(!button||button->mDisabled||button->mBtnNoDraw)return false;
    const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
    if(!button->IsMouseOver())
    {
        SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;e.motion.windowID=SDL_GetWindowID(app->mSDLWindow);e.motion.x=x;e.motion.y=y;
        Require(app,SDL_PushEvent(&e),"Base batch Ready hover failed");return false;
    }
    return Click(app,x,y);
}
Plant* PlantAt(Board* board,int i)
{
    Plant* plant=nullptr;while(board->mPlants.IterateNext(plant))
        if(!plant->mDead&&plant->mIsOnBoard&&plant->mPlantCol==kColumns[i]&&plant->mRow==kRows[i])return plant;
    return nullptr;
}
Zombie* ById(Board* board,uint16_t id)
{
    Zombie* zombie=nullptr;while(board->mZombies.IterateNext(zombie))if(zombie->mNetId==id)return zombie;return nullptr;
}
Projectile* ProjectileById(Board* board,uint16_t id)
{
    Projectile* p=nullptr;while(board->mProjectiles.IterateNext(p))if(p->mNetId==id&&!p->mDead)return p;return nullptr;
}
SeedPacket* Packet(Board* board,int i)
{
    for(int j=0;j<board->mSeedBank->mNumPackets;++j)
    {
        auto* p=&board->mSeedBank->mSeedPackets[j];const auto* d=GetNativePacketData(p);
        if(d&&d->kind==kKinds[i]&&d->owner==kOwners[i]&&d->isExtra==(i==1))return p;
    }
    return nullptr;
}
bool Held(Board* board,int i)
{
    const auto* c=board->mCursorObject;
    return c&&c->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&c->mType==GetNativeSeedForPlant(kKinds[i])&&
        c->mSeedBankIndex>=0&&c->mSeedBankIndex<board->mSeedBank->mNumPackets&&board->mSeedBank->mSeedPackets[c->mSeedBankIndex].mNetId==s.packets[i];
}
bool Pick(LawnApp* app,int i)
{
    auto* p=Packet(app->mBoard,i);auto* bank=app->mBoard->mSeedBank;
    if(!Require(app,p&&p->mActive&&!p->mRefreshing,"Base batch real packet is absent or recharging"))return false;
    return BoardClick(app,float(bank->mX+p->mX+p->mOffsetX+p->mWidth/2),float(bank->mY+p->mY+p->mHeight/2));
}
bool Purchase(LawnApp* app,int i)
{
    auto* plant=PlantAt(app->mBoard,i);auto* p=Packet(app->mBoard,i);
    const auto* d=GetNativePlantData(plant);const auto* card=GetNativePacketData(p);
    if(!plant||!p||!d||!card)return false;
    return Require(app,plant->mSeedType==GetNativeSeedForPlant(kKinds[i])&&plant->mPlantHealth==300&&plant->mPlantMaxHealth==300&&
        !plant->mIsAsleep&&d->kind==kKinds[i]&&d->owner==kOwners[i]&&d->packetId==s.packets[i]&&!d->randomOrigin&&
        d->firstParent==kFusionNoPlant&&d->secondParent==kFusionNoPlant&&card->price==kPrices[i]&&card->rechargeSeconds==7.5f&&
        p->mNetId==s.packets[i]&&p->mRefreshTime==750&&p->mRefreshing&&!p->mActive&&app->mBoard->mSunMoney==kSunAfter[i],
        "Base batch source purchase lost actor/packet/owner/price/awake/HP300/CD750");
}
bool MoveGround(LawnApp* app,Zombie* zombie,Plant* plant,float dx)
{
    float px=0,py=0,zx=0,zy=0;
    if(!Require(app,zombie&&plant&&GetNativeBasePlantGroundAnchor(plant,px,py)&&GetNativeBaseZombieGroundAnchor(zombie,zx,zy),
        "Base batch cannot read actual native ground anchors"))return false;
    zombie->mPosX+=px+dx-zx;zombie->mX=static_cast<int>(zombie->mPosX);
    zombie->mPosY=zombie->GetPosYBasedOnRow(zombie->mRow);zombie->mY=static_cast<int>(zombie->mPosY);
    if(!GetNativeBaseZombieGroundAnchor(zombie,zx,zy))return false;
    return Require(app,std::abs((zx-px)-dx)<1.1f,"Base batch native ground placement exceeds one-pixel draw rounding");
}
Zombie* Spawn(LawnApp* app,Plant* reference,int row,float dx,int shield=0)
{
    auto* z=app->mBoard->AddZombieInRow(ZOMBIE_NORMAL,row,0);
    if(!Require(app,z&&z->mNetId,"Base batch normal0 allocation failed"))return nullptr;
    auto* body=app->ReanimationTryToGet(z->mBodyReanimID);const float rate=body?body->mAnimRate:0;
    if(!Require(app,body&&std::isfinite(rate)&&rate>0,"Base batch lacks real native walking animation"))return nullptr;
    if(shield){z->mShieldType=SHIELDTYPE_DOOR;z->AttachShield();}
    // Initial fixture setup only. All subsequent health/armor changes are native combat.
    z->mBodyHealth=z->mBodyMaxHealth=270;z->mShieldHealth=z->mShieldMaxHealth=shield;
    z->mVelX=0;z->SetAnimRate(rate);z->ApplyAnimRate(0);
    if(!MoveGround(app,z,reference,dx)||!Require(app,!z->FindZombieTarget()&&!z->FindPlantTarget(ATTACKTYPE_CHEW),
        "Base batch setup accidentally enables an unrelated native bite"))return nullptr;
    LawnNet::NetTrace("SMOKE_FUSION_BASE_SETUP id=%u row=%d x=%.3f groundDx=%.3f body=270 shield=%d fixtureControl=initialSpawnPositionArmorAndZeroGroundRate healthWritesAfterSetup=0",z->mNetId,row,z->mPosX,dx,shield);
    return z;
}
bool Setup(LawnApp* app)
{
    // Stage beyond every source range until both real peers have the IDs.
    // This also prevents a throttled chat send from hiding the first natural bite/shot.
    for(size_t i=0;i<s.targets.size();++i)
    {auto* z=Spawn(app,PlantAt(app->mBoard,kTargetPlants[i]),kTargetRows[i],700.0f+float(i)*100.0f,i==FUME_SHIELD?100:0);if(!z)return false;s.targets[i]=z->mNetId;}
    std::ostringstream m;m<<"FBB_IDS";for(auto id:s.targets)m<<' '<<id;s.idMessage=m.str();s.setup=true;return true;
}
bool ReadIds(LawnApp* app)
{
    if(app->NetIsHost())return s.setup;
    for(const auto& line:app->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.rfind("FBB_IDS ",0)==0)
    {
        std::istringstream input(line.mText.substr(8));std::set<unsigned> unique;
        for(auto& id:s.targets){unsigned v=0;if(!Require(app,!!(input>>v)&&v&&v<=65535&&unique.insert(v).second,"Base batch target IDs invalid"))return false;id=uint16_t(v);}
        std::string extra;return Require(app,!(input>>extra),"Base batch target IDs have trailing fields");
    }
    return false;
}
bool TargetHealth(LawnApp* app,int i,int body,int shield)
{
    auto* z=ById(app->mBoard,s.targets[i]);
    return z&&!z->IsDeadOrDying()&&z->mZombieType==ZOMBIE_NORMAL&&!z->mMindControlled&&z->mBodyHealth==body&&z->mShieldHealth==shield;
}
bool ShieldVisible(LawnApp* app,Zombie* z)
{
    auto* body=z?app->ReanimationTryToGet(z->mBodyReanimID):nullptr;
    if(!body||!body->TrackExists("anim_screendoor"))return false;
    auto* track=body->GetTrackInstanceByName("anim_screendoor");
    const auto* image=track->mImageOverride?track->mImageOverride:body->GetCurrentTrackImage("anim_screendoor");
    ReanimatorTransform pose;body->GetCurrentTransform(body->FindTrackIndex("anim_screendoor"),&pose);
    return z->mShieldType==SHIELDTYPE_DOOR&&track->mRenderGroup==RENDER_GROUP_SHIELD&&!track->mForceDontRender&&
        pose.mFrame>=0&&pose.mAlpha>0&&image==IMAGE_REANIM_ZOMBIE_SCREENDOOR1;
}
bool ObserveHost(LawnApp* app)
{
    auto* board=app->mBoard;auto* chomp=PlantAt(board,0);auto* puff=PlantAt(board,1);auto* fume=PlantAt(board,2);
    if(!Require(app,chomp&&puff&&fume&&!puff->mIsAsleep&&!fume->mIsAsleep,"Base batch plant vanished or fell asleep in daytime"))return false;
    auto* a=ById(board,s.targets[CHOMP_A]);auto* b=ById(board,s.targets[CHOMP_B]);
    if(!s.chompLocked&&chomp->mState==STATE_CHOMPER_BITING)
    {
        auto* locked=board->ZombieTryToGet(chomp->mTargetZombieID);
        if(!Require(app,locked&&locked->mNetId==s.targets[CHOMP_A]&&b&&MoveGround(app,b,chomp,60),"Chomper did not lock its first real prey"))return false;
        s.chompLocked=true;
        LawnNet::NetTrace("SMOKE_FUSION_BASE_CHOMP_LOCK role=host plant=%u target=%u challenger=%u challengerMovedCloser=1 nativeTargetGeneration=1 timerWrites=0",chomp->mNetId,locked->mNetId,b->mNetId);
    }
    if(!s.chompKilled&&s.chompLocked&&(!a||a->IsDeadOrDying()))
    {
        if(!Require(app,TargetHealth(app,CHOMP_B,270,0)&&(chomp->mState==STATE_CHOMPER_BITING_GOT_ONE||chomp->mState==STATE_CHOMPER_DIGESTING)&&
            chomp->mLaunchCounter>2990&&chomp->mLaunchCounter<=3000,"Chomper retargeted, lost its natural digest or changed the challenger health"))return false;
        s.chompKilled=true;s.biteTick=board->mMainCounter;s.digestRemaining=chomp->mLaunchCounter;
        // The challenger remains a real normal0 actor, moved out after the lock assertion.
        if(!MoveGround(app,b,chomp,700))return false;s.movedChompB=true;
        LawnNet::NetTrace("SMOKE_FUSION_BASE_CHOMP_BITE role=host plant=%u swallowed=%u challenger=%u challengerHP=270 digestRemaining=%d tick=%d naturalKill=1 healthWrites=0 timerWrites=0",chomp->mNetId,s.targets[CHOMP_A],b->mNetId,s.digestRemaining,s.biteTick);
    }
    if(s.chompKilled&&!s.swallowTick&&chomp->mState==STATE_CHOMPER_SWALLOWING)
    {
        const int elapsed=board->mMainCounter-s.biteTick;
        if(!Require(app,std::abs(elapsed-s.digestRemaining)<=1,"Chomper natural source30s digest was skipped/extended"))return false;
        s.swallowTick=board->mMainCounter;
        LawnNet::NetTrace("SMOKE_FUSION_BASE_CHOMP_DIGEST role=host plant=%u elapsed=%d initialRemaining=%d sourceSeconds=30 naturalClock=1 timerWrites=0",chomp->mNetId,elapsed,s.digestRemaining);
    }
    if(!s.fumeHit)
    {
        auto* first=ById(board,s.targets[FUME_BODY]);auto* shield=ById(board,s.targets[FUME_SHIELD]);
        if(!Require(app,first&&shield,"Fume controls vanished before their real area event"))return false;
        if(first->mBodyHealth!=270||shield->mBodyHealth!=270||shield->mShieldHealth!=100)
        {
            if(!Require(app,TargetHealth(app,FUME_BODY,250,0)&&TargetHealth(app,FUME_SHIELD,250,80)&&
                TargetHealth(app,FUME_ADJACENT,270,0)&&TargetHealth(app,FUME_OUTSIDE,270,0)&&ShieldVisible(app,shield),
                "Fume native single event/NormalAll20/row/source7range differs"))return false;
            s.fumeHit=true;
            LawnNet::NetTrace("SMOKE_FUSION_BASE_FUME role=host body=250 shieldBody=250 shield=80 adjacent=270 outside=270 damage=20 nativeFlags=2 sameEvent=1 nativeShieldVisible=1 healthWrites=0");
            if(!MoveGround(app,first,fume,700)||!MoveGround(app,shield,fume,800))return false;
        }
    }
    auto* prey=ById(board,s.targets[PUFF_PREY]);
    if(!Require(app,prey&&!prey->IsDeadOrDying(),"Puff control vanished"))return false;
    Projectile* p=nullptr;
    while(board->mProjectiles.IterateNext(p))
    {
        if(p->mDead)continue;
        if(!Require(app,p->mRow!=kRows[2],"Fume created a traveling native projectile"))return false;
        if(!IsNativePuffProjectile(p))continue;
        if(!Require(app,p->mProjectileType==PROJECTILE_PUFF&&p->mRow==2&&std::abs(p->mVelX-3.2f)<.0001f,"Puff native projectile identity/speed differs"))return false;
        if(!s.hitProjectile)s.hitProjectile=p->mNetId;
        if(s.puffPhase>=2&&p->mNetId!=s.hitProjectile&&!s.missProjectile)
        {
            s.missProjectile=p->mNetId;s.puffBornTick=board->mMainCounter-p->mProjectileAge;s.puffTick=board->mMainCounter;s.puffAge=p->mProjectileAge;s.puffX=p->mPosX;
            LawnNet::NetTrace("SMOKE_FUSION_BASE_PUFF_MISS role=host id=%u realNaturalShot=1 targetMovedAfterTrigger=1 age=%d velocity=%.6f",p->mNetId,p->mProjectileAge,p->mVelX);
        }
    }
    if(!s.puffHit&&prey->mBodyHealth!=270)
    {
        if(!Require(app,prey->mBodyHealth==250&&s.hitProjectile&&!ProjectileById(board,s.hitProjectile),"Puff did not retire after exactly one normal20 impact"))return false;
        s.puffHit=true;s.puffPhase=1;
        LawnNet::NetTrace("SMOKE_FUSION_BASE_PUFF_HIT role=host id=%u prey=%u before=270 after=250 hits=1 retired=1 damage=20 healthWrites=0",s.hitProjectile,prey->mNetId);
    }
    if(s.puffPhase==1&&puff->mShootingCounter>1)
    {
        if(!MoveGround(app,prey,puff,700))return false;s.puffPhase=2;
        LawnNet::NetTrace("SMOKE_FUSION_BASE_PUFF_TRIGGER role=host plant=%u counter=%d targetMovedAfterTrigger=1 timerWrites=0",puff->mNetId,puff->mShootingCounter);
    }
    if(auto* miss=ProjectileById(board,s.missProjectile))
    {
        const int dt=board->mMainCounter-s.puffTick;
        if(!Require(app,miss->mProjectileAge<=75&&miss->mProjectileAge-s.puffAge==dt&&std::abs((miss->mPosX-s.puffX)-3.2f*dt)<.02f&&
            !NativePuffProjectileExpired(miss),"Real unused Puff speed/strict75 expiry diverged"))return false;
        s.puffMaxAge=(std::max)(s.puffMaxAge,miss->mProjectileAge);s.puffTick=board->mMainCounter;s.puffAge=miss->mProjectileAge;s.puffX=miss->mPosX;
        if(miss->mProjectileAge==75)LawnNet::NetTrace("SMOKE_FUSION_BASE_PUFF_75 role=host id=%u age=75 alive=1 expired=0 actualNativeProjectile=1",miss->mNetId);
    }
    else if(s.missProjectile&&!s.puffExpired)
    {
        if(!Require(app,s.puffMaxAge==75&&board->mMainCounter-s.puffTick==1&&board->mMainCounter-s.puffBornTick==76&&
            TargetHealth(app,PUFF_PREY,250,0),"Unused Puff did not naturally retire exactly after age75"))return false;
        s.puffExpired=true;
        LawnNet::NetTrace("SMOKE_FUSION_BASE_PUFF_EXPIRE role=host id=%u lastAlive=75 removedAge=76 velocity=3.2 health=250 nativeClock=1 timerWrites=0",s.missProjectile);
    }
    return true;
}
bool CombatSnapshot(LawnApp* app)
{
    auto* board=app->mBoard;auto* chomp=PlantAt(board,0);
    Projectile* p=nullptr;while(board->mProjectiles.IterateNext(p))if(!p->mDead&&IsNativePuffProjectile(p))s.clientPuff=true;
    if(chomp&&(chomp->mState==STATE_CHOMPER_BITING_GOT_ONE||chomp->mState==STATE_CHOMPER_DIGESTING))s.clientDigest=true;
    if(TargetHealth(app,FUME_BODY,250,0)&&TargetHealth(app,FUME_SHIELD,250,80)&&ShieldVisible(app,ById(board,s.targets[FUME_SHIELD])))s.clientFume=true;
    auto* a=ById(board,s.targets[CHOMP_A]);
    return chomp&&(!a||a->IsDeadOrDying())&&TargetHealth(app,CHOMP_B,270,0)&&TargetHealth(app,PUFF_PREY,250,0)&&
        TargetHealth(app,FUME_BODY,250,0)&&TargetHealth(app,FUME_SHIELD,250,80)&&TargetHealth(app,FUME_ADJACENT,270,0)&&TargetHealth(app,FUME_OUTSIDE,270,0);
}
bool FinalBusiness(LawnApp* app)
{
    auto* board=app->mBoard;
    if(!Require(app,board->mSunMoney==250&&!board->StageIsNight(),"Base batch currency/daytime changed"))return false;
    for(int i=0;i<3;++i)
    {
        auto* plant=PlantAt(board,i);auto* p=Packet(board,i);const auto* d=GetNativePlantData(plant);
        if(!Require(app,plant&&d&&p&&plant->mNetId==s.plants[i]&&plant->mPlantMaxHealth==300&&!plant->mIsAsleep&&d->kind==kKinds[i]&&
            d->owner==kOwners[i]&&d->packetId==s.packets[i]&&p->mNetId==s.packets[i]&&p->mRefreshTime==750&&!p->mRefreshing&&p->mActive&&
            p->mRefreshCounter==0&&p->mTimesUsed==s.uses[i],"Base batch final actor/packet identity, CD750 or use conservation differs"))return false;
    }
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionBaseBatch(LawnApp* app)
{
    const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE"),*enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_BASE_BATCH");
    if(!app||!native||std::strcmp(native,"1")||!enabled||std::strcmp(enabled,"1")||!app->CoopSmokeActive()||
        app->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(app->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    const uint32_t limit=s.stage<=3?60000U:s.stage==8?45000U:20000U;
    if(GetTickCount()-s.since>limit){char e[100];sprintf_s(e,"Base batch deadline stage=%d role=%s",s.stage,Role(app));app->CoopSmokeFail(e);return true;}
    if(!app->mNetManager||!app->mNetManager->IsActive())return true;
    app->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(app->NetIsHost()&&!s.configured){if(!Require(app,!app->mBoard&&!app->mSeedChooserScreen,"Base batch needs clean lobby"))return true;app->mNetManager->SetDifficulty(1);s.configured=true;}
        if(app->mNetManager->GetPlayerCount()!=2||(app->NetIsClient()&&!app->mNetManager->IsConnectedToHost()))return true;
        if(app->NetIsClient()){Advance(app,1);Send(app,"FBB_START");}
        else if(Has(app,1,"FBB_START")){Advance(app,1);std::string e;if(!app->StartFusionValidation(&e))app->CoopSmokeFail(e.c_str());}
        return true;
    }
    if(!app->mBoard||!IsNativeFusionBoard(app->mBoard)){if(s.stage==1&&app->NetIsClient())Send(app,"FBB_START");return true;}
    auto* board=app->mBoard;const auto* context=GetNativeContext(board);
    if(!Require(app,context&&context->validation&&!app->mFusionRuntime&&board->GetColumnCount()==10&&!board->StageIsNight(),"Base batch lost its daytime native10x5 Board"))return true;
    const auto& state=context->selection.State();const int owner=app->mNetManager->GetMySlot();auto* chooser=app->mSeedChooserScreen;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!board->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;
        const size_t target=(std::min)(size_t(kNativeFusionBankCapacity),context->eligible.size());
        if(!Require(app,state.phase==FusionBankPhase::Choosing&&state.targetCount==target&&target>=3&&state.activeMask==3&&
            state.quotas[0]>=2&&state.quotas[1]>=1&&state.quotas[0]+state.quotas[1]==target&&state.difficulty==1,"Base batch live eligibility/quota differs"))return true;
        if(!Capture(app,"fusion_base_chooser"))return true;
        std::vector<int> choices;for(size_t i=0;i<context->eligible.size();++i)if(context->eligible[i].isExtra==(owner==1))choices.push_back(int(i));
        auto priority=[&](int i){const auto k=context->eligible[i].kind;return owner==1?(k==6?0:10):(k==5?0:k==7?1:10);};
        std::stable_sort(choices.begin(),choices.end(),[&](int a,int b){return priority(a)<priority(b);});
        if(!Require(app,choices.size()>=state.quotas[owner]&&choices.size()>=2&&priority(choices[0])==0&&(owner==1||priority(choices[1])==1),"Base batch required5/6/7 identities absent from real eligible pool"))return true;
        int own=0;for(const auto& p:state.packets)own+=p.owner==owner;if(own<s.waitingOwn)return true;
        if(own<state.quotas[owner])
        {int x=0,y=0;chooser->GetSeedPositionInChooser(choices[own],x,y);if(Click(app,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waitingOwn=own+1;return true;}
        if(state.packets.size()!=target||!Packet(board,0)||!Packet(board,1)||!Packet(board,2))return true;
        int rank=-1;for(int i=0;i<board->mSeedBank->mNumPackets;++i){const int next=NetSeedBankOwnerRank(owner,board->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(app,next>=rank,"Base batch bank lost native own-first ordering"))return true;rank=next;}
        for(int i=0;i<3;++i)s.packets[i]=Packet(board,i)->mNetId;
        if(!Capture(app,"fusion_base_picked"))return true;
        NetTrace("SMOKE_FUSION_BASE_POOL role=%s eligible=%zu target=%u quota0=%u quota1=%u priorities=5normal,6extra,7normal nativeChooser=1 ownFirst=1",Role(app),context->eligible.size(),state.targetCount,state.quotas[0],state.quotas[1]);Advance(app,2);
    }
    else if(s.stage==2)
    {if(state.phase!=FusionBankPhase::Choosing){Advance(app,3);return true;}if(chooser&&!chooser->mSeedsInFlight&&Button(app,chooser->mStartButton))Advance(app,3);}
    else if(s.stage==3)
    {
        if(state.phase==FusionBankPhase::Intro){if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;Capture(app,"fusion_base_intro");return true;}
        if(state.phase!=FusionBankPhase::Playing||app->mGameScene!=SCENE_PLAYING||chooser)return true;
        if(!Require(app,s.introChecks>=10&&s.introLast-s.introStart>=1000&&s.frames.contains("fusion_base_intro")&&board->mSunMoney==500,"Base batch skipped native Ready/full intro or starting500"))return true;
        if(app->NetIsHost()&&!s.ambientConfigured){board->mSunCountDown=100000;board->mZombieCountDown=board->mZombieCountDownStart=100000;s.ambientConfigured=true;NetTrace("SMOKE_FUSION_BASE_AMBIENT wavesAndSkyOnly=deferred nativePlantClocksUnchanged=1 fixtureControl=1");}
        if(!Capture(app,"fusion_base_playing"))return true;
        NetTrace("SMOKE_FUSION_BASE_PLAYING role=%s epoch=%u nativeBoard=1 secondWorld=0 daytime=1 coopDifficulty=1 sourceDifficulty=2 nativeArtAdaptation=1",Role(app),context->epoch);Advance(app,4);
    }
    else if(s.stage>=4&&s.stage<=6)
    {
        const int i=s.stage-4;const std::string gate="FBB_PURCHASE_"+std::to_string(i);
        if(app->NetIsHost())Send(app,gate);if(!Has(app,0,gate))return true;
        if(owner==kOwners[i]&&!s.clicked){s.heldSamples=Samples(app);if(Pick(app,i))s.clicked=true;return true;}
        if(owner==kOwners[i]&&!s.placed)
        {
            if(!Held(board,i)||(app->NetIsClient()&&Samples(app)<=s.heldSamples)||!Settled())return true;
            if(!Require(app,board->mSunMoney==kSunBefore[i],"Base batch pickup charged before native placement"))return true;
            NetTrace("SMOKE_FUSION_BASE_HELD role=%s kind=%u packet=%u price=%d sun=%d snapshotEcho=1 nativeSDL=1",Role(app),kKinds[i],s.packets[i],kPrices[i],board->mSunMoney);
            s.placed=true;BoardClick(app,float(board->GetCellCenterX(kColumns[i],kRows[i])),float(board->GridToPixelY(kColumns[i],kRows[i])+50));return true;
        }
        if(!s.purchaseObserved){if(!Purchase(app,i))return true;s.plants[i]=PlantAt(board,i)->mNetId;s.uses[i]=Packet(board,i)->mTimesUsed;s.purchaseObserved=true;}
        const std::string done="FBB_BOUGHT_"+std::to_string(i);if(!Barrier(app,done.c_str()))return true;
        NetTrace("SMOKE_FUSION_BASE_PURCHASE role=%s kind=%u plant=%u packet=%u owner=%d extra=%d price=%d sun=%d HP=300 sourceCD=750 awake=1 nativeSDL=1",Role(app),kKinds[i],s.plants[i],s.packets[i],kOwners[i],i==1?1:0,kPrices[i],board->mSunMoney);Advance(app,i==2?7:s.stage+1);
    }
    else if(s.stage==7)
    {
        if(!Capture(app,"fusion_base_planted")||!Barrier(app,"FBB_PLANTED"))return true;
        if(app->NetIsHost()&&!s.setup&&!Setup(app))return true;
        if(app->NetIsHost()&&!Send(app,s.idMessage))return true;
        if(!ReadIds(app))return true;
        for(auto id:s.targets){auto* z=ById(board,id);if(!z||z->IsDeadOrDying())return true;}
        if(!Barrier(app,"FBB_TARGETS"))return true;
        if(app->NetIsHost()&&!s.activated)
        {
            for(size_t i=0;i<s.targets.size();++i)
            {
                auto* z=ById(board,s.targets[i]);if(!MoveGround(app,z,PlantAt(board,kTargetPlants[i]),kTargetDx[i]))return true;
                NetTrace("SMOKE_FUSION_BASE_ACTIVATE id=%u row=%d groundDx=%.6f HPnotWritten=1 timerWrites=0 fixtureControl=positionOnly",z->mNetId,z->mRow,kTargetDx[i]);
            }
            s.activated=true;
        }
        Advance(app,8);
    }
    else if(s.stage==8)
    {
        if(app->NetIsHost()){Send(app,s.idMessage);if(!ObserveHost(app))return true;}
        if(CombatSnapshot(app)&&!s.clientCombat)
        {
            s.clientCombat=true;NetTrace("SMOKE_FUSION_BASE_COMBAT role=%s swallowedA=1 challenger=270 puff=250 fume=250 shieldBody=250 shield=80 adjacent=270 outside=270 snapshotAuthority=1",Role(app));
        }
        if(s.clientCombat)Capture(app,"fusion_base_combat");
        auto* chomp=PlantAt(board,0);
        if(app->NetIsHost()&&s.swallowTick&&s.puffExpired&&s.fumeHit&&chomp&&chomp->mState==STATE_READY)Send(app,"FBB_NATURAL_DONE");
        if(!Has(app,0,"FBB_NATURAL_DONE")||!s.clientCombat||!s.frames.contains("fusion_base_combat"))return true;
        if(!Require(app,app->NetIsHost()||(s.clientDigest&&s.clientPuff&&s.clientFume),"Client missed authoritative digest/Puff/Fume presentation"))return true;
        if(!FinalBusiness(app)||!CombatSnapshot(app))return true;
        Advance(app,9);
    }
    else if(s.stage==9)
    {
        if(!FinalBusiness(app)||!CombatSnapshot(app)||Samples(app)<8||!Barrier(app,"FBB_DONE")||!Capture(app,"fusion_base_complete"))return true;
        for(int i=0;i<3;++i){const auto* p=Packet(board,i);NetTrace("SMOKE_FUSION_BASE_CD role=%s kind=%u packet=%u ready=1 refreshTime=%d uses=%d conserved=1 hostCounterAuthority=1",Role(app),kKinds[i],p->mNetId,p->mRefreshTime,p->mTimesUsed);}
        NetTrace("SMOKE_FUSION_BASE_COMPLETE role=%s sun=250 kinds=5,6,7 HPmax=300 CD=750 nativeBoard=1 secondWorld=0 naturalDigest=30 stockArtAdaptation=1 sourceCampaignComplete=0 healthWritesAfterSetup=0 timerWrites=0",Role(app));
        Advance(app,10);app->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
