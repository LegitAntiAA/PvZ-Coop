#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionWire.h"
#include "../Fusion/FusionNativeCherryShooter.h"
#include "../Fusion/FusionCherryShooterRules.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/DDInterface.h"
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
constexpr int kColumn=3,kRow=2;
struct State
{
    int stage=0,waitingOwn=0,parentTick=0,parentFuse=0,fusedTick=0,previousCounter=0,previousTick=0,emptyCycles=0;
    int expectedFireTick=0,triggers=0,hits=0,lastTracedHits=0;
    uint32_t since=0,lastChat=0,introFirst=0,introLast=0,introChecks=0,pickSamples=0,purchaseSamples=0,staleSamples=0,staleAt=0;
    uint16_t incoming=0,parent=0,fused=0;
    bool reverse=false,configured=false,clicked=false,fusionClicked=false,suppressed=false,spawned=false,replayed=false;
    float groundX=0,groundY=0;
    std::array<uint16_t,3> targets{};
    std::map<uint16_t,std::pair<int,float>> projectiles;
    std::map<std::string,int> firstDraw;
    std::set<std::string> frames;
} s;
const char* Role(LawnApp* app) { return app->NetIsHost()?"host":"client"; }
const char* Direction() { return s.reverse?"CherryPea":"PeaCherry"; }
int ParentKind() { return s.reverse?2:0; }
int IncomingKind() { return s.reverse?0:2; }
int FinalSun() { return s.reverse?150:100; }
int IncomingCD() { return s.reverse?750:5000; }
uint32_t Samples(LawnApp* app) { return app->NetIsHost()?app->mCoopSmokeSnapshotsSent:app->mCoopSmokeSnapshotsApplied; }
bool Require(LawnApp* app,bool value,const char* error) { if(!value)app->CoopSmokeFail(error);return value; }
void Advance(LawnApp* app,int stage)
{
    s.stage=stage;s.since=GetTickCount();s.clicked=false;
    LawnNet::NetTrace("SMOKE_FUSION_CS_STAGE role=%s direction=%s stage=%d",Role(app),Direction(),stage);
}
bool Has(LawnApp* app,int owner,const std::string& text)
{ for(const auto& line:app->mNetManager->mChatLines)if(line.mSlot==owner && line.mText==text)return true;return false; }
bool Send(LawnApp* app,const std::string& text)
{
    const int owner=app->mNetManager->GetMySlot();
    if(Has(app,owner,text))return true;
    if(s.lastChat && GetTickCount()-s.lastChat<800)return false;
    s.lastChat=GetTickCount();app->mNetManager->SendChat(text);
    return Has(app,owner,text);
}
bool Barrier(LawnApp* app,const char* text) { Send(app,text);return Has(app,0,text)&&Has(app,1,text); }
bool Capture(LawnApp* app,const char* name)
{
    if(s.frames.contains(name))return true;
    if(!s.firstDraw.contains(name)){s.firstDraw[name]=app->mDrawCount;return false;}
    if(app->mDrawCount-s.firstDraw[name]<2)return false;
    if(!Require(app,app->CoopSmokeCaptureFrame(name),"CherryShooter actual renderer capture failed"))return false;
    s.frames.insert(name);
    LawnNet::NetTrace("SMOKE_FUSION_CS_RENDER role=%s frame=%s firstDraw=%d currentDraw=%d freshDraws=%d",Role(app),name,
        s.firstDraw[name],app->mDrawCount,app->mDrawCount-s.firstDraw[name]);
    return true;
}
bool Click(LawnApp* app,float x,float y)
{
    SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=SDL_GetWindowID(app->mSDLWindow);motion.motion.x=x;motion.motion.y=y;
    SDL_Event down{};down.type=SDL_EVENT_MOUSE_BUTTON_DOWN;down.button.windowID=motion.motion.windowID;down.button.button=SDL_BUTTON_LEFT;
    down.button.clicks=1;down.button.down=true;down.button.x=x;down.button.y=y;
    SDL_Event up=down;up.type=SDL_EVENT_MOUSE_BUTTON_UP;up.button.down=false;
    return Require(app,SDL_PushEvent(&motion)&&SDL_PushEvent(&down)&&SDL_PushEvent(&up),"CherryShooter scoped SDL queue failed");
}
bool BoardClick(LawnApp* app,float x,float y)
{ return Click(app,x+app->mBoard->mX+app->mDDInterface->mWideScreenOffsetX,y+app->mBoard->mY+app->mDDInterface->mWideScreenOffsetY); }
bool Cell(LawnApp* app)
{ return BoardClick(app,static_cast<float>(app->mBoard->GetCellCenterX(kColumn,kRow)),static_cast<float>(app->mBoard->GridToPixelY(kColumn,kRow)+50)); }
bool Button(LawnApp* app,GameButton* button)
{
    if(!button || button->mDisabled || button->mBtnNoDraw)return false;
    const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
    if(!button->IsMouseOver())
    {
        SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=SDL_GetWindowID(app->mSDLWindow);motion.motion.x=x;motion.motion.y=y;
        Require(app,SDL_PushEvent(&motion),"CherryShooter Ready hover failed");return false;
    }
    LawnNet::NetTrace("SMOKE_FUSION_CS_READY role=%s hoverObserved=1 nativeSDL=1",Role(app));return Click(app,x,y);
}
SeedPacket* Packet(Board* board,int owner,int kind,bool extra)
{
    for(int i=0;i<board->mSeedBank->mNumPackets;++i)
    {
        auto* packet=&board->mSeedBank->mSeedPackets[i];const auto* data=GetNativePacketData(packet);
        if(data && data->owner==owner && data->kind==kind && data->isExtra==extra)return packet;
    }
    return nullptr;
}
bool Pick(LawnApp* app,SeedPacket* packet)
{
    if(!Require(app,packet && packet->mActive && !packet->mRefreshing,"CherryShooter card not ready"))return false;
    const auto* bank=app->mBoard->mSeedBank;
    return BoardClick(app,static_cast<float>(bank->mX+packet->mX+packet->mOffsetX+packet->mWidth/2),
        static_cast<float>(bank->mY+packet->mY+packet->mHeight/2));
}
bool Held(Board* board,uint16_t id)
{
    const auto* cursor=board->mCursorObject;
    if(!cursor || cursor->mCursorType!=CURSOR_TYPE_PLANT_FROM_BANK || cursor->mSeedBankIndex<0 || cursor->mSeedBankIndex>=board->mSeedBank->mNumPackets)return false;
    const auto& packet=board->mSeedBank->mSeedPackets[cursor->mSeedBankIndex];return packet.mNetId==id && cursor->mType==packet.mPacketType;
}
Plant* At(Board* board)
{ Plant* plant=nullptr;while(board->mPlants.IterateNext(plant))if(!plant->mDead && plant->mIsOnBoard && plant->mPlantCol==kColumn && plant->mRow==kRow)return plant;return nullptr; }
Zombie* Target(Board* board,uint16_t id)
{ Zombie* zombie=nullptr;while(board->mZombies.IterateNext(zombie))if(zombie->mNetId==id)return zombie;return nullptr; }
bool Pool(LawnApp* app,const NativeContext& context)
{
    const auto& state=context.selection.State();const size_t count=(std::min)(context.eligible.size(),kNativeFusionBankCapacity);
    if(!Require(app,state.activeMask==3 && state.targetCount==count && state.quotas[0]+state.quotas[1]==count &&
        state.quotas[0]>=4 && state.quotas[1]>=4,"CherryShooter live pool/quota mismatch"))return false;
    std::set<std::pair<FusionPlantId,bool>> ids;
    for(const auto& card:context.eligible)
    {
        const auto* definition=FusionFindPlant(card.kind);
        FusionBankEligibleCard sourceCard;
        if(!Require(app,definition && definition->hasStats && GetNativeSeedForPlant(card.kind)!=SEED_NONE &&
            ids.emplace(card.kind,card.isExtra).second && TryMakeFusionEligibleCard(card.kind,card.isExtra,sourceCard) && card==sourceCard &&
            card.rechargeSeconds==definition->rechargeSeconds,"CherryShooter pool lost original identity/cost/CD"))return false;
    }
    for(FusionPlantId kind:{FusionPlantId(0),FusionPlantId(1),FusionPlantId(2),FusionPlantId(4)})
        if(!Require(app,ids.contains({kind,false}) && ids.contains({kind,true}),"CherryShooter pool filtered mandatory normal/extra identity"))return false;
    return true;
}
bool SetupTargets(LawnApp* app)
{
    Board* board=app->mBoard;const int x[]={560,610,560},rows[]={kRow,kRow,kRow+1};
    for(size_t i=0;i<s.targets.size();++i)
    {
        auto* zombie=board->AddZombieInRow(ZOMBIE_NORMAL,rows[i],0);
        if(!Require(app,zombie && zombie->mNetId,"CherryShooter target allocation failed"))return false;
        auto* body=app->ReanimationTryToGet(zombie->mBodyReanimID);const float walking=body?body->mAnimRate:0;
        if(!Require(app,body && std::isfinite(walking) && walking>0,"CherryShooter original native walking rate missing"))return false;
        zombie->mPosX=static_cast<float>(x[i]);zombie->mX=x[i];zombie->mPosY=zombie->GetPosYBasedOnRow(rows[i]);zombie->mY=static_cast<int>(zombie->mPosY);
        zombie->mVelX=0;zombie->mMindControlled=false;
        if(i==0){zombie->mHelmType=HELMTYPE_BUCKET;zombie->AttachHelmet();zombie->mShieldType=SHIELDTYPE_DOOR;zombie->AttachShield();}
        zombie->mBodyHealth=zombie->mBodyMaxHealth=5000;
        zombie->mHelmHealth=zombie->mHelmMaxHealth=i==0?120:0;zombie->mShieldHealth=zombie->mShieldMaxHealth=i==0?120:0;
        zombie->SetAnimRate(walking);zombie->ApplyAnimRate(0.0f);s.targets[i]=zombie->mNetId;
        if(!Require(app,body->mAnimRate==0 && zombie->mOriginalAnimRate==walking,"CherryShooter native walking freeze invalid"))return false;
        LawnNet::NetTrace("SMOKE_FUSION_CS_SETUP index=%zu id=%u row=%d x=%d y=%.3f body=5000 first=%d second=%d originalRate=%.6f liveRate=0 nativeY=1 fixtureControl=1",
            i,zombie->mNetId,rows[i],x[i],zombie->mPosY,zombie->mHelmHealth,zombie->mShieldHealth,walking);
    }
    s.spawned=true;return true;
}
std::string Ids()
{ std::ostringstream out;out<<"CS46_IDS";for(auto id:s.targets)out<<' '<<id;return out.str(); }
bool ReadIds(LawnApp* app)
{
    if(app->NetIsHost())return s.spawned;
    for(const auto& line:app->mNetManager->mChatLines)if(line.mSlot==0 && line.mText.rfind("CS46_IDS ",0)==0)
    {
        std::istringstream input(line.mText.substr(9));std::set<unsigned> unique;
        for(auto& id:s.targets){unsigned value=0;if(!Require(app,!!(input>>value)&&value&&value<=65535&&unique.insert(value).second,"CherryShooter target IDs invalid"))return false;id=static_cast<uint16_t>(value);}
        std::string extra;return Require(app,!(input>>extra),"CherryShooter target IDs trailing data");
    }
    return false;
}
bool Stable(LawnApp* app)
{
    auto* board=app->mBoard;auto* plant=At(board);const auto* data=GetNativePlantData(plant);auto* packet=Packet(board,1,IncomingKind(),true);
    if(!Require(app,plant && data && plant->mNetId==s.fused && data->kind==1001 && data->firstParent==ParentKind() &&
        data->secondParent==IncomingKind() && data->owner==1 && data->packetId==s.incoming && data->randomOrigin==0 &&
        plant->mSeedType==SEED_PEASHOOTER && plant->mPlantHealth==300 && plant->mPlantMaxHealth==300 && plant->mDoSpecialCountdown==0 &&
        FusionNativeAnimationSpeedValid(1001,data->animationSpeed) && board->mSunMoney==FinalSun() && packet && packet->mNetId==s.incoming &&
        packet->mRefreshTime==IncomingCD() && (!app->NetIsHost() || packet->mTimesUsed==1),"CherryShooter live identity/HP/balance/recharge drift"))return false;
    Coin* coin=nullptr;while(board->mCoins.IterateNext(coin))if(!Require(app,coin->mDead,"CherryShooter generated an unexpected Coin/sun"))return false;
    return true;
}
bool Monitor(LawnApp* app)
{
    if(!Stable(app))return false;
    auto* board=app->mBoard;auto* plant=At(board);const auto* data=GetNativePlantData(plant);
    if(data->shootSeconds>=0)Capture(app,"fusion_cs_shoot");
    if(app->NetIsHost() && plant->mShootingCounter>s.previousCounter)
    {
        int initial=0;TryGetFusionCherryShotCounter(data->animationSpeed,initial);
        const int tickGap=board->mMainCounter-s.previousTick;
        if(!Require(app,plant->mShootingCounter<=initial && initial-plant->mShootingCounter<=(std::max)(1,tickGap),"CherryShooter native shot counter did not start from source deadline"))return false;
        s.expectedFireTick=board->mMainCounter+plant->mShootingCounter-1;++s.triggers;
        LawnNet::NetTrace("SMOKE_FUSION_CS_TRIGGER number=%d tick=%d counter=%d expectedFire=%d speed=%.9g nativeClock=1",s.triggers,board->mMainCounter,plant->mShootingCounter,s.expectedFireTick,data->animationSpeed);
    }
    s.previousCounter=plant->mShootingCounter;s.previousTick=board->mMainCounter;
    Projectile* projectile=nullptr;
    while(board->mProjectiles.IterateNext(projectile))
    {
        if(projectile->mDead)continue;const auto* metadata=GetNativeProjectileData(projectile);
        if(!Require(app,metadata && metadata->kind==1 && metadata->damage==60 && metadata->sunOnHit==0 &&
            projectile->mProjectileType==PROJECTILE_PEA,"CherryShooter emitted an incorrect projectile"))return false;
        float cx=0,cy=0;if(!Require(app,GetNativeCherryProjectileCenter(projectile,cx,cy),"CherryShooter projectile centre invalid"))return false;
        if(app->NetIsHost())
        {
            if(!Require(app,std::abs(projectile->mVelX-3.2f)<.0001f && std::abs(cy-(s.groundY-.6003f*60))<.05f &&
                std::abs(cx-(s.groundX+.514f*60+projectile->mProjectileAge*3.2f))<.15f,"CherryShooter muzzle/velocity does not match native coordinate contract"))return false;
            const auto found=s.projectiles.find(projectile->mNetId);
            if(found==s.projectiles.end())
            {
                const int fireTick=board->mMainCounter-projectile->mProjectileAge+1;
                if(!Require(app,s.triggers>0 && std::abs(fireTick-s.expectedFireTick)<=1 && s.projectiles.size()<static_cast<size_t>(s.triggers),"CherryShooter duplicate/early/late projectile"))return false;
                s.projectiles[projectile->mNetId]={board->mMainCounter,cx};
                LawnNet::NetTrace("SMOKE_FUSION_CS_PROJECTILE id=%u number=%zu fireTick=%d expectedFire=%d centerX=%.3f centerY=%.3f age=%d vx=3.2 damage=60 sun=0 nativeActor=1",
                    projectile->mNetId,s.projectiles.size(),fireTick,s.expectedFireTick,cx,cy,projectile->mProjectileAge);
            }
            else
            {
                const int dt=board->mMainCounter-found->second.first;
                if(!Require(app,std::abs(cx-found->second.second-3.2f*dt)<.1f,"CherryShooter real native projectile displacement changed"))return false;
                found->second={board->mMainCounter,cx};
            }
        }
        Capture(app,"fusion_cs_projectile");
    }
    auto* front=Target(board,s.targets[0]);auto* rear=Target(board,s.targets[1]);auto* adjacent=Target(board,s.targets[2]);
    if(!front || !rear || !adjacent)return true;
    const int loss=5240-front->mBodyHealth-front->mHelmHealth-front->mShieldHealth;
    const int hits=loss/60;
    const int shield=(std::max)(0,120-60*hits),helmet=(std::max)(0,120-60*(std::max)(0,hits-2));
    const int body=5000-60*(std::max)(0,hits-4);
    if(!Require(app,!front->mDead && !rear->mDead && !adjacent->mDead && loss>=0 && loss%60==0 && hits<=12 &&
        front->mShieldHealth==shield && front->mHelmHealth==helmet && front->mBodyHealth==body &&
        rear->mBodyHealth==5000 && adjacent->mBodyHealth==5000 &&
        rear->mHelmHealth==0 && rear->mShieldHealth==0 && adjacent->mHelmHealth==0 && adjacent->mShieldHealth==0,
        "CherryShooter damage is not single60/shield-helmet-body or damaged a control"))return false;
    for(size_t i=0;i<s.targets.size();++i)
    {
        auto* target=Target(board,s.targets[i]);const int expectedX=i==1?610:560,expectedRow=i==2?kRow+1:kRow;
        if(!Require(app,target && target->mZombieType==ZOMBIE_NORMAL && target->mRow==expectedRow && std::abs(target->mPosX-expectedX)<.01f &&
            std::abs(target->mPosY-target->GetPosYBasedOnRow(expectedRow))<.01f && target->mOriginalAnimRate>0,
            "CherryShooter frozen native target moved or changed family"))return false;
    }
    s.hits=hits;
    if(hits>s.lastTracedHits)
    {
        if(app->NetIsHost() && !Require(app,hits==s.lastTracedHits+1,"CherryShooter host skipped a60-damage transition"))return false;
        LawnNet::NetTrace("SMOKE_FUSION_CS_DAMAGE role=%s hits=%d body=%d first=%d second=%d rear=5000 adjacent=5000 noAoE=1 noPierce=1 sun=%d",Role(app),hits,body,helmet,shield,FinalSun());
        s.lastTracedHits=hits;
    }
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionCherryShooter(LawnApp* app)
{
    const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE"),*enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER");
    if(!app || !native || std::strcmp(native,"1") || !enabled || std::strcmp(enabled,"1") || !app->CoopSmokeActive() ||
        app->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(app->mCoopSmokeResultWritten)return true;
    if(!s.since)
    {
        s.since=GetTickCount();const char* direction=std::getenv("PVZ_COOP_SMOKE_FUSION_CHERRYSHOOTER_DIRECTION");
        if(!Require(app,direction && (!std::strcmp(direction,"PeaCherry") || !std::strcmp(direction,"CherryPea")),"CherryShooter strict direction flag missing"))return true;
        s.reverse=!std::strcmp(direction,"CherryPea");
    }
    if(GetTickCount()-s.since>(s.stage<=3?60000U:30000U)){app->CoopSmokeFail("CherryShooter stage deadline expired");return true;}
    if(!app->mNetManager || !app->mNetManager->IsActive())return true;app->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(app->NetIsHost() && !s.configured)
        {if(!Require(app,!app->mBoard && !app->mSeedChooserScreen,"CherryShooter lobby was not clean"))return true;app->mNetManager->SetDifficulty(1);s.configured=true;}
        if(app->mNetManager->GetPlayerCount()!=2 || (app->NetIsClient() && !app->mNetManager->IsConnectedToHost()))return true;
        if(app->NetIsClient()){Send(app,"CS46_START");Advance(app,1);}
        else if(Has(app,1,"CS46_START")){Advance(app,1);std::string error;if(!app->StartFusionValidation(&error))app->CoopSmokeFail(error.c_str());}
        return true;
    }
    if(!app->mBoard || !IsNativeFusionBoard(app->mBoard)){if(s.stage==1 && app->NetIsClient())Send(app,"CS46_START");return true;}
    Board* board=app->mBoard;const auto* context=GetNativeContext(board);auto* chooser=app->mSeedChooserScreen;
    if(!Require(app,context && !app->mFusionRuntime && context->validation && board->GetColumnCount()==10 && board->GetCellPitchX()==72,
        "CherryShooter is not the actual native10x5 shared Board"))return true;
    const auto& state=context->selection.State();
    if(s.stage>=8 && !Monitor(app))return true;
    if(s.stage==1)
    {
        if(!chooser || !chooser->mMouseVisible || !board->mCutScene->mSeedChoosing || chooser->mSeedsInFlight)return true;
        if(!Pool(app,*context) || !Capture(app,"fusion_cs_chooser"))return true;
        const int owner=app->mNetManager->GetMySlot();int own=0;for(const auto& p:state.packets)own+=p.owner==owner;
        if(own<s.waitingOwn)return true;
        if(own<state.quotas[owner])
        {
            std::vector<int> choices;
            for(size_t i=0;i<context->eligible.size();++i)
            {
                const auto& card=context->eligible[i];
                if(card.isExtra!=(owner==1))continue;
                const bool selected=std::any_of(state.packets.begin(),state.packets.end(),[&](const auto& packet){return !packet.mysteryPending && packet.kind==card.kind && packet.isExtra==card.isExtra;});
                if(!selected)choices.push_back(static_cast<int>(i));
            }
            if(!Require(app,choices.size()>=static_cast<size_t>(state.quotas[owner]-own),"CherryShooter cannot fill actual native quota"))return true;
            int x=0,y=0;chooser->GetSeedPositionInChooser(choices.front(),x,y);
            if(Click(app,static_cast<float>(chooser->mX+x+25),static_cast<float>(chooser->mY+y+35-static_cast<int>(chooser->mScrollbar->mScrollValue))))s.waitingOwn=own+1;
            return true;
        }
        if(state.packets.size()!=state.targetCount || !Capture(app,"fusion_cs_picked"))return true;
        auto* incoming=Packet(board,1,IncomingKind(),true);if(!Require(app,incoming,"CherryShooter incoming real packet missing"))return true;s.incoming=incoming->mNetId;
        NetTrace("SMOKE_FUSION_CS_POOL role=%s eligible=%zu target=%u quota0=%u quota1=%u unfiltered=1",Role(app),context->eligible.size(),state.targetCount,state.quotas[0],state.quotas[1]);Advance(app,2);
    }
    else if(s.stage==2)
    {if(state.phase!=FusionBankPhase::Choosing)Advance(app,3);else if(chooser && !chooser->mSeedsInFlight && Button(app,chooser->mStartButton))Advance(app,3);}
    else if(s.stage==3)
    {
        if(state.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;if(s.introLast-s.introFirst>=300)Capture(app,"fusion_cs_intro");return true;}
        if(state.phase!=FusionBankPhase::Playing || app->mGameScene!=SCENE_PLAYING || chooser)return true;
        if(!Require(app,s.introChecks>=10 && s.introLast-s.introFirst>=1000 && s.frames.contains("fusion_cs_intro") && board->mSunMoney==500,
            "CherryShooter bypassed real Ready/full intro/fresh500sun"))return true;
        NetTrace("SMOKE_FUSION_CS_PLAYING role=%s direction=%s epoch=%u nativeBoard=1 secondWorld=0 sun=500",Role(app),Direction(),context->epoch);Advance(app,4);
    }
    else if(s.stage==4)
    {
        if(!Barrier(app,"CS46_PLAY"))return true;
        if(app->NetIsHost() && !s.suppressed){board->mSunCountDown=100000;board->mZombieCountDown=board->mZombieCountDownStart=100000;s.suppressed=true;}
        if(app->NetIsHost()){if(Has(app,1,"CS46_HELD"))Advance(app,5);return true;}
        auto* incoming=Packet(board,1,IncomingKind(),true);
        if(!s.clicked){s.pickSamples=Samples(app);if(Pick(app,incoming))s.clicked=true;return true;}
        if(!Held(board,s.incoming) || Samples(app)<=s.pickSamples || !Capture(app,"fusion_cs_held"))return true;
        if(!Require(app,board->mSunMoney==500 && !At(board) && incoming && incoming->mTimesUsed==0 && !incoming->mRefreshing,"CherryShooter pickup echo mutated board"))return true;
        NetTrace("SMOKE_FUSION_CS_HELD role=client packet=%u incoming=%d extra=1 snapshotEcho=1 parentNotPlaced=1 nativeSDL=1",s.incoming,IncomingKind());Send(app,"CS46_HELD");
        if(Has(app,1,"CS46_HELD"))Advance(app,5);
    }
    else if(s.stage==5)
    {
        if(app->NetIsClient()){Advance(app,6);return true;}
        auto* parentPacket=Packet(board,0,ParentKind(),false);
        if(!s.clicked){if(Pick(app,parentPacket))s.clicked=true;return true;}
        if(!Held(board,parentPacket?parentPacket->mNetId:0))return true;
        if(Cell(app))Advance(app,6);
    }
    else if(s.stage==6)
    {
        auto* plant=At(board);const auto* data=GetNativePlantData(plant);
        if(data && data->kind==ParentKind())
        {
            if(!s.parent){s.parent=plant->mNetId;s.parentTick=board->mMainCounter;s.parentFuse=plant->mDoSpecialCountdown;NetTrace("SMOKE_FUSION_CS_PARENT role=%s id=%u kind=%d fuse=%d tick=%d",Role(app),s.parent,ParentKind(),s.parentFuse,s.parentTick);}
            if(app->NetIsClient() && !s.fusionClicked && Held(board,s.incoming)){if(Cell(app))s.fusionClicked=true;}return true;
        }
        if(!data || data->kind!=1001)return true;
        if(!s.fused){s.fused=plant->mNetId;s.fusedTick=board->mMainCounter;s.purchaseSamples=Samples(app);if(!Require(app,s.parent && s.parent!=s.fused,"CherryShooter lost observed parent identity"))return true;}
        if(!Stable(app))return true;
        if(app->NetIsHost() && s.reverse && !Require(app,s.parentFuse>0 && s.fusedTick-s.parentTick<s.parentFuse,"Cherry parent exploded before actual SDL fusion"))return true;
        if(!Capture(app,"fusion_cs_fused"))return true;
        if(app->NetIsHost() && !GetNativeCherryShooterGroundAnchor(plant,s.groundX,s.groundY)){app->CoopSmokeFail("CherryShooter ground anchor missing");return true;}
        NetTrace("SMOKE_FUSION_CS_PURCHASE role=%s direction=%s old=%u fused=%u packet=%u parents=%d,%d owner=1 extra=1 hp=300 sun=%d sourceCD=%d parentElapsed=%d parentFuse=%d nativeSDL=1",
            Role(app),Direction(),s.parent,s.fused,s.incoming,ParentKind(),IncomingKind(),FinalSun(),IncomingCD(),s.fusedTick-s.parentTick,s.parentFuse);
        s.previousCounter=plant->mLaunchCounter;s.previousTick=board->mMainCounter;Advance(app,7);
    }
    else if(s.stage==7)
    {
        if(!Stable(app))return true;auto* plant=At(board);const auto* data=GetNativePlantData(plant);
        if(!Require(app,board->mProjectiles.mSize==0 && board->mZombies.mSize==0 && data->shootSeconds==-1.0f,"CherryShooter fired without a target"))return true;
        if(app->NetIsHost())
        {
            const int dt=board->mMainCounter-s.previousTick;
            if(plant->mLaunchCounter>s.previousCounter)
            {
                if(!Require(app,plant->mLaunchCounter>=143-dt && plant->mLaunchCounter<=158 && s.previousCounter<=dt+1,"CherryShooter empty counter did not reset naturally"))return true;
                ++s.emptyCycles;NetTrace("SMOKE_FUSION_CS_EMPTY role=host cycle=%d counter=%d deltaTicks=%d shots=0 forcedClock=0",s.emptyCycles,plant->mLaunchCounter,dt);
            }
            s.previousCounter=plant->mLaunchCounter;s.previousTick=board->mMainCounter;
            if(s.emptyCycles>=2)Send(app,"CS46_EMPTY");
            if(Has(app,1,"CS46_EMPTY_ACK")){if(!SetupTargets(app))return true;s.previousCounter=0;s.previousTick=board->mMainCounter;Advance(app,8);}
        }
        else if(Has(app,0,"CS46_EMPTY") && Capture(app,"fusion_cs_idle")){Send(app,"CS46_EMPTY_ACK");if(Has(app,1,"CS46_EMPTY_ACK"))Advance(app,8);}
        if(app->NetIsHost() && s.emptyCycles>=2)Capture(app,"fusion_cs_idle");
    }
    else if(s.stage==8)
    {
        if(app->NetIsHost() && !Send(app,Ids()))return true;
        if(!ReadIds(app))return true;
        if(!Target(board,s.targets[0]) || !Target(board,s.targets[1]) || !Target(board,s.targets[2]))return true;
        if(Capture(app,"fusion_cs_targets"))Advance(app,9);
    }
    else if(s.stage==9)
    {
        if(s.hits<5 || !s.frames.contains("fusion_cs_shoot") || !s.frames.contains("fusion_cs_projectile") || !Capture(app,"fusion_cs_after"))return true;
        if(!Barrier(app,"CS46_COMBAT"))return true;Advance(app,10);
        if(app->NetIsClient())
        {
            auto* unused=Packet(board,1,1,true);const auto* meta=GetNativePacketData(unused);
            if(!Require(app,unused && meta && unused->mActive && !unused->mRefreshing && unused->mTimesUsed==0 && meta->price==100 && board->CanTakeSunMoney(100),"CherryShooter stale test lacks affordable unused real packet"))return true;
            FusionCommand command;command.action=FusionCommandKind::Plant;command.packetId=unused->mNetId;command.column=kColumn;command.row=kRow;command.targetId=s.parent;
            if(!Require(app,app->SubmitNativeFusionCommand(command),"CherryShooter stale intent transport failed"))return true;s.replayed=true;s.staleSamples=Samples(app);s.staleAt=GetTickCount();
            NetTrace("SMOKE_FUSION_CS_STALE_SENT role=client old=%u current=%u packet=%u ready=1 affordable=1 nativeAuthenticatedStream=1",s.parent,s.fused,unused->mNetId);
        }
    }
    else if(s.stage==10)
    {
        if(app->NetIsClient())
        {
            if(!s.replayed || GetTickCount()-s.staleAt<900 || Samples(app)-s.staleSamples<2)return true;
            auto* unused=Packet(board,1,1,true);if(!Require(app,unused && unused->mActive && !unused->mRefreshing && unused->mTimesUsed==0,"CherryShooter stale identity consumed a packet"))return true;
            Send(app,"CS46_STALE_OK");
        }
        if(!Has(app,1,"CS46_STALE_OK"))return true;
        auto* unused=Packet(board,1,1,true);
        if(!Require(app,unused && unused->mActive && !unused->mRefreshing && unused->mTimesUsed==0,"CherryShooter stale identity changed host/client unused packet"))return true;
        if(!Barrier(app,"CS46_DONE") || Samples(app)<8 || Samples(app)-s.purchaseSamples<2 || !Capture(app,"fusion_cs_complete"))return true;
        auto* front=Target(board,s.targets[0]);
        NetTrace("SMOKE_FUSION_CS_COMPLETE role=%s direction=%s epoch=%u fused=%u packet=%u front=%u rear=%u adjacent=%u hits=%d body=%d first=%d second=%d sun=%d sourceCD=%d staleNoCharge=1 nativeBoard=1 secondWorld=0 samples=%u",
            Role(app),Direction(),context->epoch,s.fused,s.incoming,s.targets[0],s.targets[1],s.targets[2],s.hits,front->mBodyHealth,front->mHelmHealth,front->mShieldHealth,FinalSun(),IncomingCD(),Samples(app));
        Advance(app,11);app->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
