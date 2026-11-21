#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../System/PlayerInfo.h"
#include "../Fusion/FusionNative.h"
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

namespace
{
using namespace LawnFusion;
constexpr int kColumns[] = {7, 2}, kRows[] = {2, 1};
struct WallnutState
{
    int stage = 0, waitingOwn = 0, negativeOwner = 0, moveTick = 0;
    int resumedHealth = 0, normalUses = 0, extraUses = 0;
    int firstDraw = 0, resumeTicks = -1;
    uint32_t since = 0, settled = 0, lastChat = 0, heldSamples = 0;
    uint32_t introStart = 0, introLast = 0, introChecks = 0, negativeSamples = 0, captureSince = 0;
    std::array<uint16_t,2> packets{}, plants{};
    uint16_t eater = 0;
    bool clicked = false, configured = false, purchaseObserved = false;
    std::string eaterMessage, pendingFrame;
    std::set<std::string> frames;
} s;
const char* Role(LawnApp* app) { return app->NetIsHost() ? "host" : "client"; }
uint32_t Samples(LawnApp* app) { return app->NetIsHost() ? app->mCoopSmokeSnapshotsSent : app->mCoopSmokeSnapshotsApplied; }
bool Require(LawnApp* app, bool value, const char* error)
{ if (!value) app->CoopSmokeFail(error); return value; }
void Advance(LawnApp* app, int stage)
{
    s.stage=stage; s.since=GetTickCount(); s.settled=0; s.clicked=false; s.pendingFrame.clear();
    LawnNet::NetTrace("SMOKE_FUSION_WALLNUT_STAGE role=%s stage=%d",Role(app),stage);
}
bool Settled(uint32_t duration=300)
{ if (!s.settled) s.settled=GetTickCount(); return GetTickCount()-s.settled>=duration; }
bool Has(LawnApp* app, int slot, const std::string& message)
{
    for (const auto& line:app->mNetManager->mChatLines) if (line.mSlot==slot && line.mText==message) return true;
    return false;
}
bool Send(LawnApp* app, const std::string& message)
{
    const int owner=app->mNetManager->GetMySlot();
    if (Has(app,owner,message)) return true;
    if (s.lastChat && GetTickCount()-s.lastChat<900) return false;
    s.lastChat=GetTickCount(); app->mNetManager->SendChat(message);
    return Has(app,owner,message);
}
bool Barrier(LawnApp* app, const std::string& message)
{ Send(app,message); return Has(app,0,message) && Has(app,1,message); }
bool Capture(LawnApp* app, const char* name)
{
    if (s.frames.contains(name)) return true;
    if (s.pendingFrame!=name)
    {
        s.pendingFrame=name; s.firstDraw=app->mDrawCount; s.captureSince=GetTickCount(); return false;
    }
    if (app->mDrawCount-s.firstDraw<2 || GetTickCount()-s.captureSince<300) return false;
    if (!Require(app,app->CoopSmokeCaptureFrame(name),"WallNut actual native renderer capture failed")) return false;
    s.frames.insert(name);
    LawnNet::NetTrace("SMOKE_FUSION_WALLNUT_FRAME role=%s name=%s freshDraws=%d nativeSDL=1",Role(app),name,app->mDrawCount-s.firstDraw);
    return true;
}
bool Click(LawnApp* app, float x, float y, uint8_t button=SDL_BUTTON_LEFT)
{
    SDL_Event motion{}; motion.type=SDL_EVENT_MOUSE_MOTION; motion.motion.windowID=SDL_GetWindowID(app->mSDLWindow);
    motion.motion.x=x; motion.motion.y=y;
    SDL_Event down{}; down.type=SDL_EVENT_MOUSE_BUTTON_DOWN; down.button.windowID=motion.motion.windowID;
    down.button.button=button; down.button.clicks=1; down.button.down=true; down.button.x=x; down.button.y=y;
    SDL_Event up=down; up.type=SDL_EVENT_MOUSE_BUTTON_UP; up.button.down=false;
    return Require(app,SDL_PushEvent(&motion) && SDL_PushEvent(&down) && SDL_PushEvent(&up),"WallNut owned-window SDL mouse queue failed");
}
bool BoardClick(LawnApp* app, float x, float y, uint8_t button=SDL_BUTTON_LEFT)
{ return Click(app,x+app->mBoard->mX+app->mDDInterface->mWideScreenOffsetX,y+app->mBoard->mY+app->mDDInterface->mWideScreenOffsetY,button); }
bool Cell(LawnApp* app, int owner, uint8_t button=SDL_BUTTON_LEFT)
{
    auto* board=app->mBoard;
    return BoardClick(app,static_cast<float>(board->GetCellCenterX(kColumns[owner],kRows[owner])),
        static_cast<float>(board->GridToPixelY(kColumns[owner],kRows[owner])+50),button);
}
bool Button(LawnApp* app, GameButton* button)
{
    if (!button || button->mDisabled || button->mBtnNoDraw) return false;
    const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
    if (!button->IsMouseOver())
    {
        SDL_Event event{}; event.type=SDL_EVENT_MOUSE_MOTION; event.motion.windowID=SDL_GetWindowID(app->mSDLWindow);
        event.motion.x=x; event.motion.y=y;
        Require(app,SDL_PushEvent(&event),"WallNut Ready native hover queue failed"); return false;
    }
    return Click(app,x,y);
}
SeedPacket* Packet(Board* board, int owner)
{
    for (int i=0;i<board->mSeedBank->mNumPackets;++i)
    {
        auto* packet=&board->mSeedBank->mSeedPackets[i]; const auto* data=GetNativePacketData(packet);
        if (data && data->kind==3 && data->owner==owner && data->isExtra==(owner==1)) return packet;
    }
    return nullptr;
}
Plant* FindWallnut(Board* board, int owner)
{
    Plant* plant=nullptr;
    while (board->mPlants.IterateNext(plant))
        if (!plant->mDead && plant->mIsOnBoard && plant->mPlantCol==kColumns[owner] && plant->mRow==kRows[owner]) return plant;
    return nullptr;
}
Zombie* Eater(Board* board)
{
    Zombie* zombie=nullptr;
    while (board->mZombies.IterateNext(zombie)) if (!zombie->mDead && zombie->mNetId==s.eater) return zombie;
    return nullptr;
}
bool Held(Board* board, uint16_t packet)
{
    const auto* cursor=board->mCursorObject;
    return cursor && cursor->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK &&
        cursor->mSeedBankIndex>=0 && cursor->mSeedBankIndex<board->mSeedBank->mNumPackets &&
        board->mSeedBank->mSeedPackets[cursor->mSeedBankIndex].mNetId==packet && cursor->mType==SEED_WALLNUT;
}
bool Pick(LawnApp* app, SeedPacket* packet)
{
    if (!Require(app,packet && packet->mActive && !packet->mRefreshing,"WallNut selected real native packet is absent/recharging")) return false;
    const auto* bank=app->mBoard->mSeedBank;
    return BoardClick(app,static_cast<float>(bank->mX+packet->mX+packet->mOffsetX+packet->mWidth/2),
        static_cast<float>(bank->mY+packet->mY+packet->mHeight/2));
}
bool PurchaseMatches(LawnApp* app, int owner, int sun)
{
    auto* plant=FindWallnut(app->mBoard,owner); const auto* data=GetNativePlantData(plant);
    auto* packet=Packet(app->mBoard,owner); const auto* card=GetNativePacketData(packet);
    if (!plant || !data || !packet || !card) return false;
    return Require(app,plant->mSeedType==SEED_WALLNUT && plant->mPlantHealth==4000 && plant->mPlantMaxHealth==4000 &&
        data->kind==3 && data->owner==owner && data->packetId==packet->mNetId && !data->randomOrigin &&
        data->firstParent==kFusionNoPlant && data->secondParent==kFusionNoPlant &&
        card->price==(owner?100:50) && card->rechargeSeconds==30 && packet->mRefreshTime==3000 &&
        packet->mRefreshing && !packet->mActive && app->mBoard->mSunMoney==sun,
        "WallNut purchase lost actual actor/source identity, price, HP4000 or native CD3000");
}
bool StableBusiness(LawnApp* app, bool removed=false)
{
    auto* normal=FindWallnut(app->mBoard,0); auto* extra=FindWallnut(app->mBoard,1);
    auto* a=Packet(app->mBoard,0); auto* b=Packet(app->mBoard,1);
    int count=0; Plant* plant=nullptr;
    while (app->mBoard->mPlants.IterateNext(plant)) if (!plant->mDead && plant->mIsOnBoard) ++count;
    return Require(app,app->mBoard->mSunMoney==350 && a && b && a->mNetId==s.packets[0] && b->mNetId==s.packets[1] &&
        a->mActive && b->mActive && !a->mRefreshing && !b->mRefreshing && a->mRefreshTime==3000 && b->mRefreshTime==3000 &&
        a->mRefreshCounter==0 && b->mRefreshCounter==0 && a->mTimesUsed==s.normalUses && b->mTimesUsed==s.extraUses &&
        extra && extra->mNetId==s.plants[1] && extra->mPlantHealth==4000 && extra->mPlantMaxHealth==4000 &&
        (removed?!normal:(normal && normal->mNetId==s.plants[0] && normal->mPlantHealth==s.resumedHealth &&
            normal->mPlantMaxHealth==4000)) && count==(removed?1:2),
        "WallNut rejected fusion/cancel changed live IDs, HP, currency or actual packet cooldown/use state");
}
bool Crack(LawnApp* app, int stage)
{
    auto* plant=FindWallnut(app->mBoard,0);
    auto* body=plant?app->ReanimationTryToGet(plant->mBodyReanimID):nullptr;
    const auto* expected=stage==1?IMAGE_REANIM_WALLNUT_CRACKED1:IMAGE_REANIM_WALLNUT_CRACKED2;
    return body && body->GetImageOverride("anim_face")==expected;
}
bool SetupEater(LawnApp* app)
{
    auto* board=app->mBoard; auto* plant=FindWallnut(board,0);
    auto* zombie=board->AddZombieInRow(ZOMBIE_NORMAL,kRows[0],0);
    if (!Require(app,plant && zombie && zombie->mNetId,"WallNut real native eater allocation failed")) return false;
    zombie->mPosY=zombie->GetPosYBasedOnRow(kRows[0]); zombie->mY=static_cast<int>(zombie->mPosY);
    bool found=false;
    for (int offset=80;offset>=-80;--offset)
    {
        zombie->mX=plant->mX+offset; zombie->mPosX=static_cast<float>(zombie->mX);
        if (zombie->FindPlantTarget(ATTACKTYPE_CHEW)==plant) { found=true; break; }
    }
    if (!Require(app,found,"WallNut real native chew rectangle could not target its actor")) return false;
    s.eater=zombie->mNetId; s.eaterMessage="FWN_EATER "+std::to_string(s.eater);
    LawnNet::NetTrace("SMOKE_FUSION_WALLNUT_EATER id=%u row=%d x=%d healthNotWritten=1 naturalEatPlant=1 fixtureControl=spawnAndPositionOnly",
        s.eater,zombie->mRow,zombie->mX);
    return true;
}
bool ReadEater(LawnApp* app)
{
    if (app->NetIsHost()) return s.eater!=0;
    for (const auto& line:app->mNetManager->mChatLines)
        if (line.mSlot==0 && line.mText.rfind("FWN_EATER ",0)==0)
        {
            std::istringstream input(line.mText.substr(10)); unsigned id=0; std::string extra;
            if (!Require(app,!!(input>>id) && id && id<=65535 && !(input>>extra),"WallNut native eater ID message invalid")) return false;
            s.eater=static_cast<uint16_t>(id); return true;
        }
    return false;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionWallnut(LawnApp* app)
{
    const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE"),*enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_WALLNUT");
    if (!app || !native || std::strcmp(native,"1") || !enabled || std::strcmp(enabled,"1") || !app->CoopSmokeActive() ||
        app->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION) return false;
    if (app->mCoopSmokeResultWritten) return true;
    if (!s.since) s.since=GetTickCount();
    const uint32_t deadline=s.stage<=3?60000U:((s.stage==8 || s.stage==9 || s.stage==11)?45000U:20000U);
    if (GetTickCount()-s.since>deadline)
    { char error[100]; sprintf_s(error,"WallNut fixture deadline stage=%d role=%s",s.stage,Role(app)); app->CoopSmokeFail(error); return true; }
    if (!app->mNetManager || !app->mNetManager->IsActive()) return true;
    app->mCoopSmokePeerReady=true;
    if (s.stage==0)
    {
        if (app->NetIsHost() && !s.configured)
        {
            if (!Require(app,!app->mBoard && !app->mSeedChooserScreen && app->mPlayerInfo,
                "WallNut isolated fixture requires a clean lobby/profile")) return true;
            app->mNetManager->SetDifficulty(1);
            app->mPlayerInfo->mPurchases[STORE_ITEM_FIRSTAID]=1;
            LawnNet::NetTrace("SMOKE_FUSION_WALLNUT_AID purchased=1 isolatedSmokeProfile=1 fixtureControl=1");
            s.configured=true;
        }
        if (app->mNetManager->GetPlayerCount()!=2 || (app->NetIsClient() && !app->mNetManager->IsConnectedToHost())) return true;
        if (app->NetIsClient()) { Advance(app,1); Send(app,"FWN_START_READY"); }
        else if (Has(app,1,"FWN_START_READY"))
        { Advance(app,1); std::string error; if (!app->StartFusionValidation(&error)) app->CoopSmokeFail(error.c_str()); }
        return true;
    }
    if (!app->mBoard || !IsNativeFusionBoard(app->mBoard))
    { if (s.stage==1 && app->NetIsClient()) Send(app,"FWN_START_READY"); return true; }
    auto* board=app->mBoard; const auto* context=GetNativeContext(board);
    if (!Require(app,context && context->validation && !app->mFusionRuntime && board->GetColumnCount()==10,
        "WallNut fixture lost the same native Board")) return true;
    const auto& state=context->selection.State(); auto* chooser=app->mSeedChooserScreen;
    const int owner=app->mNetManager->GetMySlot();
    if (s.stage==1)
    {
        if (!chooser || !chooser->mNativeFusionChooser || !chooser->mMouseVisible || !board->mCutScene->mSeedChoosing || chooser->mSeedsInFlight) return true;
        const size_t target=(std::min)(kNativeFusionBankCapacity,context->eligible.size());
        if (!Require(app,state.phase==FusionBankPhase::Choosing && state.targetCount==target &&
            state.quotas[0]+state.quotas[1]==target && state.quotas[owner]>0 && state.difficulty==1,
            "WallNut native chooser live-pool quotas are inconsistent")) return true;
        if (!Capture(app,"fusion_wallnut_chooser")) return true;
        int own=0; for (const auto& packet:state.packets) own+=packet.owner==owner;
        if (own<s.waitingOwn) return true;
        if (own<state.quotas[owner])
        {
            int selected=-1;
            for (size_t i=0;i<context->eligible.size();++i)
            {
                const auto& entry=context->eligible[i];
                if (entry.isExtra!=(owner==1)) continue;
                bool chosen=false;
                for (const auto& packet:state.packets) if (packet.kind==entry.kind && packet.isExtra==entry.isExtra) chosen=true;
                if (chosen) continue;
                if (selected<0 || entry.kind==3) selected=static_cast<int>(i);
                if (entry.kind==3) break;
            }
            if (!Require(app,selected>=0,"WallNut fixture has no remaining eligible identity for its native quota")) return true;
            int x=0,y=0; chooser->GetSeedPositionInChooser(selected,x,y);
            if (Click(app,static_cast<float>(chooser->mX+x+25),static_cast<float>(chooser->mY+y+35-static_cast<int>(chooser->mScrollbar->mScrollValue)))) s.waitingOwn=own+1;
            return true;
        }
        if (state.packets.size()!=target || !Packet(board,0) || !Packet(board,1)) return true;
        for (int i=0,rank=-1;i<board->mSeedBank->mNumPackets;++i)
        {
            const int next=NetSeedBankOwnerRank(owner,board->mSeedBank->mSeedPackets[i].mNetOwnerSlot);
            if (!Require(app,next>=rank,"WallNut native bank lost shared own-first ordering")) return true;
            rank=next;
        }
        for (int i=0;i<2;++i) s.packets[i]=Packet(board,i)->mNetId;
        if (!Capture(app,"fusion_wallnut_picked")) return true;
        LawnNet::NetTrace("SMOKE_FUSION_WALLNUT_POOL role=%s eligible=%zu target=%u quota0=%u quota1=%u ownFirst=1 nativeChooser=1",
            Role(app),context->eligible.size(),state.targetCount,state.quotas[0],state.quotas[1]);
        Advance(app,2);
    }
    else if (s.stage==2)
    {
        if (state.phase!=FusionBankPhase::Choosing) { Advance(app,3); return true; }
        if (chooser && !chooser->mSeedsInFlight && Button(app,chooser->mStartButton)) Advance(app,3);
    }
    else if (s.stage==3)
    {
        if (state.phase==FusionBankPhase::Intro)
        {
            if (!s.introStart) s.introStart=GetTickCount(); s.introLast=GetTickCount(); ++s.introChecks;
            Capture(app,"fusion_wallnut_intro"); return true;
        }
        if (state.phase!=FusionBankPhase::Playing || app->mGameScene!=SCENE_PLAYING || chooser) return true;
        if (!Require(app,s.introChecks>=10 && s.introLast-s.introStart>=1000 && s.frames.contains("fusion_wallnut_intro") &&
            board->mSunMoney==500,"WallNut bypassed native Ready/full intro or changed starting sun")) return true;
        if (app->NetIsHost()) { board->mSunCountDown=100000; board->mZombieCountDown=100000; board->mZombieCountDownStart=100000; }
        LawnNet::NetTrace("SMOKE_FUSION_WALLNUT_PLAYING role=%s epoch=%u nativeBoard=1 secondWorld=0 stockAnimation=1 sourceCampaignComplete=0",Role(app),context->epoch);
        Advance(app,4);
    }
    else if (s.stage==4 || s.stage==5)
    {
        const int buyer=s.stage-4;
        // A peer may observe the first chat barrier before its partner. The
        // second buyer waits until the host has left the sun450 assertion.
        if (buyer)
        {
            if (app->NetIsHost()) Send(app,"FWN_EXTRA_PHASE");
            if (!Has(app,0,"FWN_EXTRA_PHASE")) return true;
        }
        if (owner==buyer && !s.clicked)
        { s.heldSamples=Samples(app); if (Pick(app,Packet(board,buyer))) s.clicked=true; return true; }
        if (owner==buyer && !s.purchaseObserved)
        {
            if (!Held(board,s.packets[buyer]) || (app->NetIsClient() && Samples(app)<=s.heldSamples) || !Settled()) return true;
            if (!Require(app,board->mSunMoney==(buyer?450:500),"WallNut pickup charged before a real placement")) return true;
            s.purchaseObserved=true; Cell(app,buyer); return true;
        }
        if (!FindWallnut(board,buyer)) return true;
        if (!PurchaseMatches(app,buyer,buyer?350:450)) return true;
        s.plants[buyer]=FindWallnut(board,buyer)->mNetId;
        const std::string barrier=buyer?"FWN_EXTRA_BOUGHT":"FWN_NORMAL_BOUGHT";
        if (!Barrier(app,barrier)) return true;
        LawnNet::NetTrace("SMOKE_FUSION_WALLNUT_PURCHASE role=%s plant=%u packet=%u kind=3 owner=%d extra=%d price=%d sun=%d HP=4000 sourceCD=3000 nativeSDL=1",
            Role(app),s.plants[buyer],s.packets[buyer],buyer,buyer,buyer?100:50,buyer?350:450);
        s.purchaseObserved=false; Advance(app,buyer?6:5);
    }
    else if (s.stage==6)
    {
        if (!Capture(app,"fusion_wallnut_planted") || !Barrier(app,"FWN_PLANTED")) return true;
        if (app->NetIsHost() && !s.eater && !SetupEater(app)) return true;
        if (app->NetIsHost() && !Send(app,s.eaterMessage)) return true;
        if (!ReadEater(app) || !Eater(board)) return true;
        Advance(app,7);
    }
    else if (s.stage>=7 && s.stage<=9)
    {
        auto* plant=FindWallnut(board,0); auto* body=plant?app->ReanimationTryToGet(plant->mBodyReanimID):nullptr; auto* zombie=Eater(board);
        if (!Require(app,plant && zombie && body && plant->mPlantMaxHealth==4000 && plant->mPlantHealth>0 &&
            FindWallnut(board,1) && FindWallnut(board,1)->mPlantHealth==4000 && board->mSunMoney==350,
            "WallNut natural bite changed another actor/currency or killed the fixture target")) return true;
        const int threshold=s.stage==7?4000:(s.stage==8?4000*2/3:4000/3);
        if (plant->mPlantHealth>=threshold || body->mAnimRate!=0) return true;
        if (app->NetIsHost() && !Require(app,zombie->mIsEating && zombie->FindPlantTarget(ATTACKTYPE_CHEW)==plant,
            "WallNut pause was not driven by actual native eating/targeting")) return true;
        if (s.stage>7 && !Crack(app,s.stage-7)) return true;
        const char* frame=s.stage==7?"fusion_wallnut_eating":(s.stage==8?"fusion_wallnut_crack1":"fusion_wallnut_crack2");
        if (!Capture(app,frame)) return true;
        const std::string barrier="FWN_BITE_"+std::to_string(s.stage);
        if (!Barrier(app,barrier)) return true;
        LawnNet::NetTrace("SMOKE_FUSION_WALLNUT_BITE role=%s phase=%d plant=%u health=%d max=4000 rate=0 actualCrack=%d naturalEatPlant=1",
            Role(app),s.stage-7,s.plants[0],plant->mPlantHealth,s.stage==7?0:s.stage-7);
        if (s.stage==9 && app->NetIsHost())
        {
            zombie->mPosX=900; zombie->mX=900; s.moveTick=board->mMainCounter;
            LawnNet::NetTrace("SMOKE_FUSION_WALLNUT_MOVE id=%u x=900 HPnotWritten=1 fixtureControl=positionOnly",s.eater);
        }
        Advance(app,s.stage+1);
    }
    else if (s.stage==10)
    {
        auto* plant=FindWallnut(board,0); auto* body=plant?app->ReanimationTryToGet(plant->mBodyReanimID):nullptr;
        if (!Require(app,plant && body,"WallNut resume lost its original native actor/reanimation")) return true;
        if (app->NetIsHost() && body->mAnimRate==0 && board->mMainCounter-s.moveTick>50)
        { app->CoopSmokeFail("Native WallNut cosmetic pause exceeded its stock50tick tail after moving the eater"); return true; }
        if (body->mAnimRate<=0) return true;
        if (!Require(app,body->mAnimRate>=10 && body->mAnimRate<=15 && Crack(app,2),"WallNut did not reuse native random resume/crack art")) return true;
        if (app->NetIsHost() && s.resumeTicks<0)
        {
            s.resumeTicks=board->mMainCounter-s.moveTick;
            if (!Require(app,s.resumeTicks<=50,"Native WallNut stock resume took more than50ticks")) return true;
        }
        if (!Capture(app,"fusion_wallnut_resumed") || !Barrier(app,"FWN_RESUMED")) return true;
        s.resumedHealth=plant->mPlantHealth;
        LawnNet::NetTrace("SMOKE_FUSION_WALLNUT_RESUME role=%s plant=%u health=%d rate=%.6f stockAnimation=1 maximumTailTicks=50 observedHostTicks=%d exactSourceCosmetic=0",
            Role(app),s.plants[0],s.resumedHealth,body->mAnimRate,s.resumeTicks);
        Advance(app,11);
    }
    else if (s.stage==11)
    {
        auto* a=Packet(board,0); auto* b=Packet(board,1);
        if (!a || !b || !a->mActive || !b->mActive || a->mRefreshing || b->mRefreshing) return true;
        s.normalUses=a->mTimesUsed; s.extraUses=b->mTimesUsed;
        // The existing native snapshot carries cooldowns, not mTimesUsed.
        // Preserve the local counters; only the host records successful uses.
        const int expectedUses=app->NetIsHost()?1:0;
        if (!Require(app,s.normalUses==expectedUses && s.extraUses==expectedUses,"WallNut real cooldown completion lost native use counters") ||
            !StableBusiness(app) || !Barrier(app,"FWN_CD_READY")) return true;
        LawnNet::NetTrace("SMOKE_FUSION_WALLNUT_COOLDOWN role=%s normal=%u extra=%u ready=1 naturalCD=3000 nativeUses=%d counterAuthority=host",Role(app),s.packets[0],s.packets[1],expectedUses);
        Advance(app,12);
    }
    else if (s.stage==12)
    {
        if (owner!=s.negativeOwner) { s.negativeSamples=Samples(app); Advance(app,13); return true; }
        if (!s.clicked) { s.heldSamples=Samples(app); if (Pick(app,Packet(board,owner))) s.clicked=true; return true; }
        if (!Held(board,s.packets[owner]) || (app->NetIsClient() && Samples(app)<=s.heldSamples) || !Settled()) return true;
        if (!StableBusiness(app)) return true;
        s.negativeSamples=Samples(app); Advance(app,13); Cell(app,0); return true;
    }
    else if (s.stage==13)
    {
        const std::string sent="FWN_NEG_SENT_"+std::to_string(s.negativeOwner);
        if (owner==s.negativeOwner) Send(app,sent);
        if (!Has(app,s.negativeOwner,sent) || Samples(app)-s.negativeSamples<2 || !Settled(900)) return true;
        if (!StableBusiness(app) || !Barrier(app,"FWN_NEG_OK_"+std::to_string(s.negativeOwner))) return true;
        LawnNet::NetTrace("SMOKE_FUSION_WALLNUT_REJECT role=%s buyer=%d target=%u packet=%u sourceRecipe=3+3 result=255 unsupported=1 sun=350 actorHPunchanged=1 cooldownUnchanged=1 nativeSDL=1",
            Role(app),s.negativeOwner,s.plants[0],s.packets[s.negativeOwner]);
        Advance(app,14);
    }
    else if (s.stage==14)
    {
        if (owner==s.negativeOwner)
        {
            if (!s.clicked) { Cell(app,0,SDL_BUTTON_RIGHT); s.clicked=true; return true; }
            if (board->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL) return true;
        }
        if (!StableBusiness(app) || !Barrier(app,"FWN_NEG_CANCEL_"+std::to_string(s.negativeOwner))) return true;
        if (++s.negativeOwner<2) Advance(app,12); else Advance(app,15);
    }
    else if (s.stage==15 || s.stage==17)
    {
        if (app->NetIsClient()) { Advance(app,s.stage+1); return true; }
        if (!s.clicked)
        {
            const auto rect=board->GetShovelButtonRect();
            BoardClick(app,static_cast<float>(rect.mX+rect.mWidth/2),static_cast<float>(rect.mY+rect.mHeight/2)); s.clicked=true; return true;
        }
        if (board->mCursorObject->mCursorType!=CURSOR_TYPE_SHOVEL || !Settled()) return true;
        if (s.stage==15)
        {
            if (!Capture(app,"fusion_wallnut_shovel_held")) return true;
            Advance(app,16); Cell(app,0,SDL_BUTTON_RIGHT);
        }
        else { Advance(app,18); Cell(app,0); }
    }
    else if (s.stage==16)
    {
        if (app->NetIsHost() && board->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL) return true;
        if (!StableBusiness(app) || !Barrier(app,"FWN_SHOVEL_CANCEL")) return true;
        Advance(app,17);
    }
    else if (s.stage==18)
    {
        if (FindWallnut(board,0)) return true;
        if (!StableBusiness(app,true) || !Barrier(app,"FWN_DONE") || Samples(app)<8 || !Capture(app,"fusion_wallnut_complete")) return true;
        LawnNet::NetTrace("SMOKE_FUSION_WALLNUT_COMPLETE role=%s epoch=%u normalPlant=%u extraPlant=%u normalPacket=%u extraPacket=%u sun=350 HPmax=4000 sourceCD=3000 nativeBoard=1 secondWorld=0 stockAnimation=1 sourceSplashImplemented=0 sourceCampaignComplete=0 samples=%u",
            Role(app),context->epoch,s.plants[0],s.plants[1],s.packets[0],s.packets[1],Samples(app));
        Advance(app,19); app->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
