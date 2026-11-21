#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetSmokeCoinClick.h"
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
#include "../Fusion/FusionNativeLobber.h"
#include "../Fusion/FusionShooterTiming.h"
#include "../Fusion/FusionNativeButter.h"
#include "../Fusion/FusionNativeWire.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../Sexy.TodLib/Reanimator.h"
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
struct Ref { uint16_t id = 0; uint32_t token = 0; };
struct Group
{
    int kind, row, normal, alternate;
    Ref plant;
    std::array<uint16_t,3> targets{};
    std::array<int,3> health{};
    bool baseline = false, normalDamage = false, alternateDamage = false, pierceObserved = false;
    bool phaseOpen = false, phaseAlternate = false, normalPhase = false, alternatePhase = false;
    int fireTick = 0;
    float phaseDuration = 0, phaseLast = 0;
    std::set<int> payloads;
};
struct Flight { int payload = 0, limit = 0; uint32_t generation = 0; float vx = 0; };
struct Buy { int who, card, row, result, parent; };
constexpr Buy buys[] = {
    {1,28,2,28,-1}, {0,26,2,26,-1}, {1,1,2,1111,26},
    {0,26,2,26,-1}, {1,28,2,1113,26},
    {0,32,2,32,-1}, {1,28,2,1121,32},
    {0,26,2,26,-1}, {0,32,2,1124,26}};
constexpr int buyEnds[] = {1,3,5,7,9};
struct State
{
    int stage = 0, group = 0, buy = 0, waitingOwn = 0, money = 500;
    int introChecks = 0, pauseState = 0, pauseTick = 0, pauseButter = 0, pauseChecks = 0;
    uint32_t since = 0, lastChat = 0, introFirst = 0, introLast = 0, pauseSince = 0, sunCreditAt = 0;
    uint32_t pickSamples = 0;
    bool configured = false, setup = false, clicked = false, placed = false, observed = false;
    bool shovelPicked = false, shovelSent = false, butter400 = false, hidden50 = false, negative = false;
    bool sunClicked = false, sunCollected = false;
    uint16_t collectedSun = 0, clickedSun = 0;
    uint16_t pauseTarget = 0;
    std::string setupMessage;
    Ref parent;
    std::array<Group,5> groups{{{28,2,54,55},{1111,2,53,53},{1113,2,56,57},{1121,2,66,67},{1124,2,70,70}}};
    std::array<uint16_t,9> targets{};
    std::map<uint16_t,int> uses;
    std::map<uint16_t,Flight> flights;
    std::set<uint16_t> sunCoins;
    std::set<std::string> frames;
    std::map<std::string,int> firstDraw;
} s;
const char* Role(LawnApp* a) { return a->NetIsHost() ? "host" : "client"; }
int Slot(LawnApp* a) { return a->mNetManager->GetMySlot(); }
uint32_t Samples(LawnApp* a) { return a->NetIsHost() ? a->mCoopSmokeSnapshotsSent : a->mCoopSmokeSnapshotsApplied; }
bool Require(LawnApp* a, bool ok, const char* why) { if (!ok) a->CoopSmokeFail(why); return ok; }
void Advance(LawnApp* a, int stage)
{
    s.stage = stage; s.since = GetTickCount(); s.clicked = s.placed = s.observed = false;
    LawnNet::NetTrace("SMOKE_FUSION_CORN_STAGE role=%s stage=%d group=%d buy=%d",Role(a),stage,s.group,s.buy);
}
bool Has(LawnApp* a, int slot, const std::string& text)
{ for (const auto& line : a->mNetManager->mChatLines) if (line.mSlot == slot && line.mText == text) return true; return false; }
bool Send(LawnApp* a, const std::string& text)
{
    if (Has(a,Slot(a),text)) return true;
    if (s.lastChat && GetTickCount()-s.lastChat < 800) return false;
    s.lastChat = GetTickCount(); a->mNetManager->SendChat(text); return Has(a,Slot(a),text);
}
bool Barrier(LawnApp* a, const std::string& text)
{ Send(a,text); return Has(a,0,text) && Has(a,1,text); }
bool Capture(LawnApp* a, const std::string& name)
{
    if (s.frames.contains(name)) return true;
    if (!s.firstDraw.contains(name)) { s.firstDraw[name] = a->mDrawCount; return false; }
    if (a->mDrawCount-s.firstDraw[name] < 2) return false;
    if (!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Corn renderer capture failed")) return false;
    s.frames.insert(name);
    LawnNet::NetTrace("SMOKE_FUSION_CORN_RENDER role=%s frame=%s freshDraws=%d",Role(a),name.c_str(),a->mDrawCount-s.firstDraw[name]);
    return true;
}
bool Click(LawnApp* a, float x, float y)
{
    SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION; motion.motion.windowID = SDL_GetWindowID(a->mSDLWindow); motion.motion.x = x; motion.motion.y = y;
    SDL_Event down{}; down.type = SDL_EVENT_MOUSE_BUTTON_DOWN; down.button.windowID = motion.motion.windowID; down.button.button = SDL_BUTTON_LEFT; down.button.clicks = 1; down.button.down = true; down.button.x = x; down.button.y = y;
    SDL_Event up = down; up.type = SDL_EVENT_MOUSE_BUTTON_UP; up.button.down = false;
    return Require(a,SDL_PushEvent(&motion) && SDL_PushEvent(&down) && SDL_PushEvent(&up),"Corn owned SDL click failed");
}
bool Key(LawnApp* a, SDL_Keycode key)
{
    SDL_Event down{}; down.type = SDL_EVENT_KEY_DOWN; down.key.windowID = SDL_GetWindowID(a->mSDLWindow); down.key.key = key; down.key.down = true;
    SDL_Event up = down; up.type = SDL_EVENT_KEY_UP; up.key.down = false;
    return Require(a,SDL_PushEvent(&down) && SDL_PushEvent(&up),"Corn owned SDL key failed");
}
bool BoardClick(LawnApp* a, float x, float y)
{ return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY); }
bool Cell(LawnApp* a, int row)
{ return BoardClick(a,float(a->mBoard->GetCellCenterX(2,row)),float(a->mBoard->GridToPixelY(2,row)+50)); }
bool Ready(LawnApp* a, GameButton* button)
{
    if (!button || button->mDisabled || button->mBtnNoDraw) return false;
    const float x = button->mX+button->mWidth*.5f, y = button->mY+button->mHeight*.5f;
    if (!button->IsMouseOver())
    {
        SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION; motion.motion.windowID = SDL_GetWindowID(a->mSDLWindow); motion.motion.x = x; motion.motion.y = y;
        Require(a,SDL_PushEvent(&motion),"Corn native Ready hover failed"); return false;
    }
    return Click(a,x,y);
}
Plant* At(Board* b, int row)
{ Plant* p = nullptr; while (b->mPlants.IterateNext(p)) if (!p->mDead && p->mIsOnBoard && p->mPlantCol == 2 && p->mRow == row) return p; return nullptr; }
Zombie* Target(Board* b, uint16_t id)
{ Zombie* z = nullptr; while (b->mZombies.IterateNext(z)) if (!z->mDead && z->mNetId == id) return z; return nullptr; }
SeedPacket* Packet(Board* b, int who, int kind)
{
    for (int i = 0; i < b->mSeedBank->mNumPackets; ++i)
    { auto* p = &b->mSeedBank->mSeedPackets[i]; const auto* d = GetNativePacketData(p); if (d && !d->isExtra && d->owner == who && d->kind == kind) return p; }
    return nullptr;
}
bool Held(Board* b, SeedPacket* p)
{
    const auto* c = b->mCursorObject;
    return p && c && c->mCursorType == CURSOR_TYPE_PLANT_FROM_BANK && c->mSeedBankIndex >= 0 && c->mSeedBankIndex < b->mSeedBank->mNumPackets && b->mSeedBank->mSeedPackets[c->mSeedBankIndex].mNetId == p->mNetId;
}
bool Business(LawnApp* a)
{
    auto* b = a->mBoard;
    for (const auto pair : {std::pair{0,26},std::pair{0,32},std::pair{1,1},std::pair{1,28}})
    {
        const auto* p = Packet(b,pair.first,pair.second); const auto* d = GetNativePacketData(p);
        const int price = pair.second == 32 ? 300 : pair.second == 1 ? 50 : 100;
        const int used = p && s.uses.contains(p->mNetId) ? s.uses[p->mNetId] : 0;
        if (!Require(a,p && d && d->price == price && p->mRefreshTime == (used ? 750 : 0) &&
            (!a->NetIsHost() || p->mTimesUsed == used),"Corn native price/CD/identity/use counter changed")) return false;
    }
    if (b->mSunMoney != s.money)
        LawnNet::NetTrace("SMOKE_FUSION_CORN_WALLET_DIAGNOSTIC role=%s stage=%d group=%d expected=%d actual=%d selectedCoin=%u clicked=%d credited=%d",Role(a),s.stage,s.group,s.money,b->mSunMoney,s.collectedSun,s.sunClicked?1:0,s.sunCollected?1:0);
    return Require(a,b->mSunMoney == s.money,"Corn currency changed outside real purchase/one Coin5 credit");
}
bool Purchase(LawnApp* a)
{
    const auto q = buys[s.buy]; auto* b = a->mBoard; auto* packet = Packet(b,q.who,q.card); const auto* data = GetNativePacketData(packet);
    if (Slot(a) == q.who && !s.clicked)
    {
        if (!packet || !packet->mActive || packet->mRefreshing) return false;
        if (const auto* old = GetNativePlantData(At(b,q.row))) s.parent = {At(b,q.row)->mNetId,old->instanceToken};
        const auto* bank = b->mSeedBank; s.pickSamples = Samples(a);
        s.clicked = BoardClick(a,float(bank->mX+packet->mX+packet->mOffsetX+packet->mWidth/2),float(bank->mY+packet->mY+packet->mHeight/2)); return false;
    }
    if (Slot(a) == q.who && !s.placed)
    {
        if (!Held(b,packet) || (a->NetIsClient() && Samples(a) <= s.pickSamples)) return false;
        if (!Require(a,b->mSunMoney == s.money,"Corn bank pickup charged before placement")) return false;
        s.placed = Cell(a,q.row); return false;
    }
    if (!s.observed)
    {
        auto* plant = At(b,q.row); const auto* d = GetNativePlantData(plant); if (!d || d->kind != q.result) return false;
        const int first = q.parent < 0 ? kFusionNoPlant : q.parent, second = q.parent < 0 ? kFusionNoPlant : q.card;
        if (!Require(a,packet && data && plant->mPlantHealth == 300 && plant->mPlantMaxHealth == 300 && !plant->mIsAsleep && IsNativeLobber(plant) &&
            d->instanceToken && d->owner == q.who && d->packetId == packet->mNetId && !d->randomOrigin && d->firstParent == first && d->secondParent == second && b->mSunMoney == s.money-data->price,
            "Corn real fusion/native HP/provenance/currency mismatch")) return false;
        if (Slot(a) == q.who && q.parent >= 0 && !Require(a,s.parent.id && s.parent.id != plant->mNetId && s.parent.token != d->instanceToken,"Corn fusion reused old actor identity")) return false;
        s.money -= data->price; ++s.uses[packet->mNetId]; s.groups[s.group].plant = {plant->mNetId,d->instanceToken}; s.observed = true;
        LawnNet::NetTrace("SMOKE_FUSION_CORN_PURCHASE role=%s index=%d kind=%d id=%u token=%u owner=%d packet=%u parents=%d,%d cost=%d sun=%d nativeSDL=1",Role(a),s.buy,q.result,plant->mNetId,d->instanceToken,q.who,packet->mNetId,first,second,data->price,s.money);
    }
    return Business(a) && Barrier(a,"FC162_BUY_"+std::to_string(s.buy));
}
bool Setup(LawnApp* a)
{
    auto* b = a->mBoard;
    if (a->NetIsHost() && !s.setup)
    {
        if (!Require(a,!b->mPlants.mSize && !b->mZombies.mSize && b->mSunMoney == 500,"Corn requires untouched initial native world")) return false;
        b->mSunMoney = 2500; b->mSunCountDown = b->mZombieCountDown = b->mZombieCountDownStart = 100000;
        std::ostringstream ids; ids << "FC162_IDS";
        for (int i = 0; i < int(s.targets.size()); ++i)
        {
            const int row = i < 3 ? 2 : i < 6 ? 1 : 3;
            auto* z = b->AddZombieInRow(ZOMBIE_NORMAL,row,0);
            if (!Require(a,z != nullptr,"Corn initial native target allocation failed")) return false;
            z->mBodyHealth = z->mBodyMaxHealth = 20000; z->mVelX = 0;
            const auto r = z->GetZombieRect(); z->mPosX += 640-(r.mX+r.mWidth*.5f); z->mX = int(z->mPosX); z->ApplyAnimRate(0);
            s.targets[i] = z->mNetId; ids << ' ' << s.targets[i];
            LawnNet::NetTrace("SMOKE_FUSION_CORN_SETUP target=%u row=%d HP=20000 velocity=0 oneTimeInitialValues=1 sourceZombieAIUnported=1",s.targets[i],row);
        }
        Sexy::SRand(a->mCoopSmokeManifest.mSeed); // One initial controlled stream; never force an alternate shot.
        s.setupMessage = ids.str(); s.setup = true;
        LawnNet::NetTrace("SMOKE_FUSION_CORN_FIXTURE_CONTROL initialSun=2500 originalSun=500 seed=%u actorWritesAfterSetup=0 setupWalletWrites=1 ambientTimersSetupOnly=1",a->mCoopSmokeManifest.mSeed);
    }
    if (a->NetIsHost() && !Send(a,s.setupMessage)) return false;
    if (a->NetIsClient() && !s.setup)
        for (const auto& line : a->mNetManager->mChatLines) if (line.mSlot == 0 && line.mText.rfind("FC162_IDS ",0) == 0)
        {
            std::istringstream in(line.mText.substr(10));
            for (auto& id : s.targets) { unsigned n = 0; if (!(in >> n) || !n || n > 65535) return false; id = uint16_t(n); }
            s.setup = true; break;
        }
    if (!s.setup || b->mSunMoney != 2500) return false;
    for (auto id : s.targets)
    { auto* z = Target(b,id); if (!z || z->mBodyHealth != 20000 || z->mZombieType != ZOMBIE_NORMAL || !z->mHasHead) return false; }
    for (auto& g : s.groups) std::copy_n(s.targets.begin(),3,g.targets.begin());
    s.money = 2500; return Barrier(a,"FC162_SETUP");
}
bool HasLobbers(Board* b, bool foreignOnly = false)
{
    Projectile* p = nullptr;
    while (b->mProjectiles.IterateNext(p)) if (!p->mDead && IsNativeLobberProjectile(p))
    { const auto* d = GetNativeProjectileData(p); if (!foreignOnly || (d->kind != s.groups[s.group].normal && d->kind != s.groups[s.group].alternate)) return true; }
    return false;
}
bool Shovel(LawnApp* a)
{
    auto* b = a->mBoard;
    if (a->NetIsHost() && At(b,s.groups[s.group].row))
    {
        if (!s.shovelPicked) { const auto r = b->GetShovelButtonRect(); s.shovelPicked = BoardClick(a,float(r.mX+r.mWidth/2),float(r.mY+r.mHeight/2)); return false; }
        if (!s.shovelSent && b->mCursorObject->mCursorType == CURSOR_TYPE_SHOVEL) { s.shovelSent = Cell(a,s.groups[s.group].row); return false; }
    }
    return !At(b,s.groups[s.group].row) && !HasLobbers(b) && Barrier(a,"FC162_DRAIN_"+std::to_string(s.group));
}
bool ButterNegative(LawnApp* a)
{
    if (a->NetIsHost() || s.negative || !a->mBoard->mPaused) return true;
    auto* b = a->mBoard; const auto* ctx = GetNativeContext(b);
    for (const auto& pending : a->mNetManager->mPendingBoardMsgs) if (pending.mKind == 1)
    {
        auto wire = pending.mData; NativeSnapshotFrame frame; LawnNet::NetBuffer body;
        if (!DecodeNativeSnapshot(wire,ctx->eligible,ctx->epoch,frame,body) || body.mData.empty() || !(body.mData[0] & LawnNet::SNAPFLAG_PAUSED)) continue;
        auto entry = std::find_if(frame.butterZombies.begin(),frame.butterZombies.end(),[](const auto& r){ return r.data.ticks > 255; });
        if (entry == frame.butterZombies.end()) continue;
        const auto id = entry->id; entry->data.ticks = 50;
        if (!Require(a,EncodeNativeSnapshot(frame,ctx->eligible,body),"Corn CRC-valid butter negative encoding failed")) return false;
        LawnNet::NetBuffer before,after; b->NetEncodeSnapshot(before); bool applied = true; b->NetApplySnapshot(body,&applied); b->NetEncodeSnapshot(after);
        if (!Require(a,!applied && before.mData == after.mData && b->mPaused,"Corn butter/raw255 mismatch mutated native Board")) return false;
        s.negative = true;
        LawnNet::NetTrace("SMOKE_FUSION_CORN_NEGATIVE role=client target=%u FNS12=1 crcValid=1 exact50Raw255=1 rejected=1 wholeBoardBytesUnchanged=1 localDecoderOnly=1",id);
        return true;
    }
    return true;
}
bool Pause(LawnApp* a)
{
    if (s.pauseState == 4) return true;
    auto* b = a->mBoard;
    if (!s.pauseTarget)
        for (const auto id : s.groups[0].targets)
        {
            const auto* candidate = Target(b,id); const auto* data = GetNativeZombieButterData(candidate);
            if (candidate && data && data->visible && candidate->mButteredCounter > 255)
            { s.pauseTarget = id; break; }
        }
    if (!s.pauseTarget) return true;
    auto* target = Target(b,s.pauseTarget);
    if (!Require(a,target != nullptr,"Corn pause target missing")) return false;
    if (a->NetIsHost() && !s.pauseState && target->mButteredCounter > 300 && s.frames.contains("fusion_corn_butter400"))
    { if (!Key(a,SDLK_ESCAPE)) return false; s.pauseState = 1; }
    if (!b->mPaused)
    {
        if (s.pauseState == 3) s.pauseState = 4;
        return true;
    }
    if (s.pauseState < 2)
    { s.pauseState = 2; s.pauseSince = GetTickCount(); s.pauseTick = b->mMainCounter; s.pauseButter = target->mButteredCounter; }
    if (!Require(a,s.pauseButter > 255 && b->mMainCounter == s.pauseTick && target->mButteredCounter == s.pauseButter,"Corn paused native butter timer advanced/lost exact >255 value")) return false;
    if (!ButterNegative(a)) return false;
    ++s.pauseChecks;
    if (s.pauseChecks < 10 || GetTickCount()-s.pauseSince < 1200 || (a->NetIsClient() && !s.negative)) return true;
    if (a->NetIsClient() && s.pauseState == 2)
    { s.pauseState = 3; LawnNet::NetTrace("SMOKE_FUSION_CORN_PAUSED role=client tick=%d butter=%d stableMs=%u exactStable=1",s.pauseTick,s.pauseButter,GetTickCount()-s.pauseSince); }
    if (!Barrier(a,"FC162_PAUSED")) return true;
    if (a->NetIsHost() && s.pauseState == 2)
    { LawnNet::NetTrace("SMOKE_FUSION_CORN_PAUSED role=host tick=%d butter=%d stableMs=%u exactStable=1",s.pauseTick,s.pauseButter,GetTickCount()-s.pauseSince); if (!Key(a,SDLK_ESCAPE)) return false; s.pauseState = 3; }
    return true;
}
bool ObserveSunCredit(LawnApp* a)
{
    auto* b = a->mBoard;
    if (!s.sunCollected)
    {
        Coin* coin = nullptr;
        while (b->mCoins.IterateNext(coin))
            if (!coin->mDead && coin->mIsBeingCollected && coin->IsSun() && GetNativeCoinValue(coin,0) == 5)
            {
                if (!s.collectedSun) s.collectedSun = coin->mNetId;
                if (!Require(a,s.collectedSun == coin->mNetId,"Corn collection selected more than one native Coin5")) return false;
            }
    }
    if (!s.sunCollected && b->mSunMoney == s.money+5)
    {
        if (!Require(a,s.collectedSun != 0,"Corn wallet credit has no observed native Coin5 identity")) return false;
        s.money += 5; s.sunCollected = true; s.sunCreditAt = GetTickCount();
        LawnNet::NetTrace("SMOKE_FUSION_CORN_SUN_CREDIT role=%s delta=5 wallet=%d nativeCoin=1 id=%u",Role(a),s.money,s.collectedSun);
    }
    return true;
}
bool CollectSun(LawnApp* a)
{
    static LawnNet::NetSmokeCoinClickState stable;static uint32_t clickSince=0;
    if(a->NetIsClient()&&s.sunClicked&&!s.sunCollected&&clickSince&&GetTickCount()-clickSince>4000)
    {
        LawnNet::NetTrace("SMOKE_FUSION_CORN_PICKUP_GATE role=client group=%d clicked=%d credited=%d physicalCoin=%u wallet=%d expected=%d elapsedMs=%u",s.group,s.sunClicked?1:0,s.sunCollected?1:0,s.collectedSun,a->mBoard->mSunMoney,s.money,GetTickCount()-clickSince);
        return Require(a,false,"Corn single settled SDL Coin5 click received no credit within4s");
    }
    auto* b = a->mBoard;
    if (a->NetIsClient() && !s.sunClicked)
    {
        Coin* coin = nullptr;int x=0,y=0;
        while (b->mCoins.IterateNext(coin)) if (!coin->mDead && coin->IsSun() && GetNativeCoinValue(coin,0) == 5 && coin->mHitGround && !coin->mIsBeingCollected)
            if(LawnNet::NetSmokeSettledCoinPoint(b,coin,a->mCoopSmokeSnapshotsApplied,stable,x,y))
            {
                s.clickedSun = coin->mNetId; s.sunClicked = BoardClick(a,float(x),float(y));
                clickSince=GetTickCount();
                LawnNet::NetTrace("SMOKE_FUSION_CORN_SUN_CLICK role=client id=%u point=%d,%d singleSDL=1 settledSnapshots=%u margin=8 ghost=%.4f,%.4f target=%.4f,%.4f",s.clickedSun,x,y,a->mCoopSmokeSnapshotsApplied-stable.coins.at(coin->mNetId).firstSnapshot,coin->mPosX,coin->mPosY,coin->mNetTargetX,coin->mNetTargetY); return s.sunClicked;
            }
    }
    return true;
}
bool Observe(LawnApp* a)
{
    if (s.stage < 7 || s.stage > 8) return true;
    auto* b = a->mBoard; auto& g = s.groups[s.group]; const auto* ctx = GetNativeContext(b);
    if (a->NetIsHost() && g.phaseOpen && b->mMainCounter > g.fireTick)
    {
        auto* plant = At(b,g.row); const auto* data = GetNativePlantData(plant);
        if (!plant && s.stage == 8) g.phaseOpen = false; // Actual user shovel may interrupt a later shot.
        else
        {
            if (!Require(a,plant && data && data->kind == g.kind && plant->mShootingCounter == 0,"Corn source callback did not release shootingCounter to0 by next logic frame")) return false;
            if (data->shootSeconds >= 0)
            {
                if (!Require(a,data->shootSeconds >= g.phaseLast && (!g.phaseAlternate || plant->mState == STATE_KERNELPULT_BUTTER),"Corn source phase/state reset before full clip")) return false;
                g.phaseLast = data->shootSeconds;
            }
            else
            {
                if (!Require(a,g.phaseLast >= g.phaseDuration-.025f && plant->mState == STATE_NOTREADY,"Corn source shoot phase failed full-duration/NOTREADY cleanup")) return false;
                if (g.phaseAlternate) g.alternatePhase = true; else g.normalPhase = true;
                if (g.normal == g.alternate) g.normalPhase = g.alternatePhase = true;
                g.phaseOpen = false;
                LawnNet::NetTrace("SMOKE_FUSION_CORN_PHASE kind=%d alternate=%d counter=0 last=%.9g duration=%.9g state=NOTREADY fullClip=1 tick=%d",g.kind,g.phaseAlternate?1:0,g.phaseLast,g.phaseDuration,b->mMainCounter);
            }
        }
    }
    Projectile* p = nullptr;
    while (b->mProjectiles.IterateNext(p))
    {
        const auto* d = GetNativeProjectileData(p);
        if (p->mDead || !d || (d->kind != g.normal && d->kind != g.alternate)) continue;
        const auto* profile = FindFusionLobberProjectileProfile(d->kind);
        if (!Require(a,profile && p->mMotionType == MOTION_LOBBED && p->mProjectileType == profile->nativeType && d->damage == profile->damage && d->sunOnHit == (d->kind == 53 ? 5 : 0),"Corn live native payload/alias/damage mismatch")) return false;
        if (!s.flights.contains(p->mNetId))
        {
            const uint32_t uid = b->mProjectiles.DataArrayGetID(p); int limit = 1;
            if (a->NetIsHost() && ctx->lobberPierceLimits.contains(uid)) limit = ctx->lobberPierceLimits.at(uid);
            if (a->NetIsHost() && !Require(a,d->kind == 53 ? limit == 6 : (d->kind == 56 || d->kind == 57 ? limit == 3 : d->kind == 70 ? limit == 2 : limit == 1),"Corn captured host pierce limit differs")) return false;
            s.flights[p->mNetId] = {d->kind,limit,uid,p->mVelX}; g.payloads.insert(d->kind);
            if (a->NetIsHost() && p->mProjectileAge == 1)
            {
                auto* parent = At(b,g.row); const auto* parentData = GetNativePlantData(parent);
                const auto* timing = GetNativeLobberTimingProfile(parent);
                if (!Require(a,parent && parentData && timing && parent->mShootingCounter == 1 && parentData->shootSeconds >= 0,"Corn birth missing source callback epoch/counter1")) return false;
                g.phaseOpen = true; g.phaseAlternate = parent->mState == STATE_KERNELPULT_BUTTER;
                g.fireTick = b->mMainCounter; g.phaseLast = parentData->shootSeconds; g.phaseDuration = FusionShooterDuration(*timing,parentData->animationSpeed);
            }
            LawnNet::NetTrace("SMOKE_FUSION_CORN_PROJECTILE role=%s kind=%d payload=%u id=%u damage=%d nativeType=%d motion=%d limit=%d wallet=%d hostPhysics=%d",Role(a),g.kind,d->kind,p->mNetId,d->damage,int(p->mProjectileType),int(p->mMotionType),a->NetIsHost()?limit:-1,b->mSunMoney,a->NetIsHost()?1:0);
        }
        if (a->NetIsHost() && d->kind == 53 && FusionProjectileHitCount(ctx->rollingHits,b->mProjectiles.DataArrayGetID(p)) == 3) g.pierceObserved = true;
        float cx = 0, cy = 0;
        if (GetNativeLobberProjectileCenter(p,cx,cy) && cx >= 40 && cx < 760 && cy >= 85 && cy < 580) Capture(a,"fusion_corn_payload"+std::to_string(d->kind));
    }
    std::array<int,3> now{};
    for (int i = 0; i < 3; ++i)
    {
        auto* z = Target(b,g.targets[i]); if (!Require(a,z && z->mBodyHealth > 0 && z->mHasHead,"Corn controlled normal target retired/changed unexpectedly")) return false;
        now[i] = z->mBodyHealth;
    }
    if (g.baseline && a->NetIsHost() && now != g.health && !HasLobbers(b,true))
    {
        const std::array<int,3> delta{g.health[0]-now[0],g.health[1]-now[1],g.health[2]-now[2]};
        auto ordered = delta;
        std::sort(ordered.rbegin(),ordered.rend()); // Coincident native colliders do not promise an array-index victim.
        bool normal = false, alternate = false;
        if (g.kind == 28) { normal = ordered == std::array<int,3>{20,0,0}; alternate = ordered == std::array<int,3>{40,0,0}; }
        if (g.kind == 1111) normal = alternate = ordered == std::array<int,3>{50,50,50};
        if (g.kind == 1113) { normal = ordered == std::array<int,3>{50,50,50}; alternate = ordered == std::array<int,3>{70,70,70}; }
        if (g.kind == 1121) { normal = ordered == std::array<int,3>{100,33,33}; alternate = ordered == std::array<int,3>{120,40,40}; }
        if (g.kind == 1124) normal = alternate = ordered == std::array<int,3>{133,133,66};
        LawnNet::NetTrace("SMOKE_FUSION_CORN_DAMAGE_DIAGNOSTIC kind=%d ids=%u,%u,%u raw=%d,%d,%d sorted=%d,%d,%d normal=%d alternate=%d tick=%d",g.kind,g.targets[0],g.targets[1],g.targets[2],delta[0],delta[1],delta[2],ordered[0],ordered[1],ordered[2],normal?1:0,alternate?1:0,b->mMainCounter);
        if (!Require(a,normal || alternate,"Corn natural direct/splash HP delta differs from source golden")) return false;
        g.normalDamage |= normal; g.alternateDamage |= alternate;
        LawnNet::NetTrace("SMOKE_FUSION_CORN_DAMAGE role=host kind=%d delta=%d,%d,%d normal=%d alternate=%d tick=%d noHPWrites=1 permutationInvariant=1",g.kind,ordered[0],ordered[1],ordered[2],normal?1:0,alternate?1:0,b->mMainCounter);
    }
    g.health = now;
    for (const auto id : s.targets)
    {
        const auto* z = Target(b,id); const auto* data = GetNativeZombieButterData(z); if (!z || !data || z->mButteredCounter <= 0) continue;
        if (z->mButteredCounter > 255 && data->visible && ShouldDrawNativeButter(z))
        {
            if (!s.butter400) LawnNet::NetTrace("SMOKE_FUSION_CORN_BUTTER role=%s id=%u real=%d mirror=%d visible=1 overRaw255=1",Role(a),id,z->mButteredCounter,data->ticks);
            s.butter400 = true; if (!b->mPaused) Capture(a,"fusion_corn_butter400");
        }
        if (s.group == 3 && z->mRow == 1 && z->mButteredCounter <= 50 && !data->visible && !ShouldDrawNativeButter(z))
        {
            if (!s.hidden50) LawnNet::NetTrace("SMOKE_FUSION_CORN_HIDDEN50 role=%s id=%u real=%d mirror=%d visible=0 positiveTimer=1",Role(a),id,z->mButteredCounter,data->ticks);
            s.hidden50 = true; Capture(a,"fusion_corn_hidden50");
        }
    }
    if (s.group == 0 && !Pause(a)) return false;
    if (s.group == 1)
    {
        Coin* coin = nullptr;
        while (b->mCoins.IterateNext(coin)) if (!coin->mDead && coin->IsSun() && GetNativeCoinValue(coin,0) == 5)
        { s.sunCoins.insert(coin->mNetId); Capture(a,"fusion_corn_sun"); }
        if (!CollectSun(a)) return false;
    }
    return Business(a);
}
bool Evidence(LawnApp* a)
{
    auto& g = s.groups[s.group];
    static std::set<int> checkpoint;
    if(GetTickCount()-s.since>10000&&checkpoint.insert(s.group).second)
        LawnNet::NetTrace("SMOKE_FUSION_CORN_EVIDENCE_GATE role=%s group=%d kind=%d host=%d payloadNormal=%d payloadAlternate=%d damageNormal=%d damageAlternate=%d pierce=%d phaseNormal=%d phaseAlternate=%d frameNormal=%d frameAlternate=%d pause=%d butter400=%d hidden50=%d coins=%zu clicked=%d credited=%d physicalCoin=%u wallet=%d expected=%d creditAgeMs=%u",
            Role(a),s.group,g.kind,a->NetIsHost()?1:0,g.payloads.contains(g.normal)?1:0,g.payloads.contains(g.alternate)?1:0,g.normalDamage?1:0,g.alternateDamage?1:0,g.pierceObserved?1:0,g.normalPhase?1:0,g.alternatePhase?1:0,
            s.frames.contains("fusion_corn_payload"+std::to_string(g.normal))?1:0,s.frames.contains("fusion_corn_payload"+std::to_string(g.alternate))?1:0,s.pauseState,s.butter400?1:0,s.hidden50?1:0,s.sunCoins.size(),s.sunClicked?1:0,s.sunCollected?1:0,s.collectedSun,a->mBoard->mSunMoney,s.money,s.sunCreditAt?GetTickCount()-s.sunCreditAt:0);
    if (!g.payloads.contains(g.normal) || !g.payloads.contains(g.alternate)) return false;
    if (a->NetIsHost() && (!g.normalDamage || !g.alternateDamage || (g.kind == 1111 && !g.pierceObserved))) return false;
    if (a->NetIsHost() && (!g.normalPhase || !g.alternatePhase)) return false;
    if (!s.frames.contains("fusion_corn_payload"+std::to_string(g.normal)) || !s.frames.contains("fusion_corn_payload"+std::to_string(g.alternate))) return false;
    if (s.group == 0 && (s.pauseState != 4 || !s.butter400 || !s.frames.contains("fusion_corn_butter400"))) return false;
    if (s.group == 1 && (s.sunCoins.size() < 4 || !s.sunCollected || GetTickCount()-s.sunCreditAt < 500 || !s.frames.contains("fusion_corn_sun"))) return false;
    if (s.group == 3 && (!s.hidden50 || !s.frames.contains("fusion_corn_hidden50"))) return false;
    return Capture(a,"fusion_corn_plant"+std::to_string(g.kind)) && Barrier(a,"FC162_COMBAT_"+std::to_string(g.kind));
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionCorn(LawnApp* a)
{
    const char* enabled = std::getenv("PVZ_COOP_SMOKE_FUSION_CORN"); const char* native = std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if (!a || !enabled || std::strcmp(enabled,"1") || !native || std::strcmp(native,"1") || !a->CoopSmokeActive() || a->mCoopSmokeManifest.mScenario != NETSMOKE_SCENARIO_FUSION_VALIDATION) return false;
    if (a->mCoopSmokeResultWritten) return true;
    if (!s.since) s.since = GetTickCount();
    if (GetTickCount()-s.since > (s.stage == 7 ? 120000U : 45000U)) { char why[120]; sprintf_s(why,"Corn deadline stage=%d group=%d buy=%d role=%s",s.stage,s.group,s.buy,Role(a)); a->CoopSmokeFail(why); return true; }
    if (!a->mNetManager || !a->mNetManager->IsActive()) return true;
    a->mCoopSmokePeerReady = true;
    if (s.stage == 0)
    {
        if (a->NetIsHost() && !s.configured) { a->mNetManager->SetDifficulty(1); s.configured = true; }
        if (a->mNetManager->GetPlayerCount() != 2 || (a->NetIsClient() && !a->mNetManager->IsConnectedToHost())) return true;
        if (a->NetIsClient()) { Advance(a,1); Send(a,"FC162_START"); }
        else if (Has(a,1,"FC162_START")) { Advance(a,1); std::string error; if (!a->StartFusionValidation(&error)) a->CoopSmokeFail(error.c_str()); }
        return true;
    }
    if (!a->mBoard || !IsNativeFusionBoard(a->mBoard)) { if (s.stage == 1 && a->NetIsClient()) Send(a,"FC162_START"); return true; }
    auto* b = a->mBoard; const auto* ctx = GetNativeContext(b); auto* chooser = a->mSeedChooserScreen; const auto& bank = ctx->selection.State(); const int slot = Slot(a);
    if (!Require(a,ctx->validation && !a->mFusionRuntime && ctx->visuals && b->GetColumnCount() == 10,"Corn requires canonical native 10-column world/source models")) return true;
    if (!ObserveSunCredit(a) || !Observe(a)) return true;
    if (s.stage == 1)
    {
        if (!chooser || !chooser->mNativeFusionChooser || !chooser->mMouseVisible || b->mCutScene->mSeedChoosing == false || chooser->mSeedsInFlight) return true;
        if (!Require(a,bank.activeMask == 3 && bank.targetCount == 10 && bank.quotas[0] == 5 && bank.quotas[1] == 5,"Corn live fair quotas differ")) return true;
        if (!Capture(a,"fusion_corn_chooser")) return true;
        std::vector<int> choices;
        for (size_t i = 0; i < ctx->eligible.size(); ++i)
        { const auto& e = ctx->eligible[i]; if (slot == 0 ? (!e.isExtra && e.kind != 1 && e.kind != 28) : (e.isExtra || e.kind == 1 || e.kind == 28)) choices.push_back(int(i)); }
        auto priority = [&](int i) { const auto& e = ctx->eligible[i]; return slot == 0 ? (e.kind == 26 ? 0 : e.kind == 32 ? 1 : 10+e.kind) : (!e.isExtra && e.kind == 1 ? 0 : !e.isExtra && e.kind == 28 ? 1 : 10+e.kind); };
        std::stable_sort(choices.begin(),choices.end(),[&](int x,int y){ return priority(x) < priority(y); });
        if (!Require(a,choices.size() >= 5,"Corn mandatory/disjoint eligible cards missing")) return true;
        int own = 0; for (const auto& p : bank.packets) own += p.owner == slot;
        if (own < s.waitingOwn) return true;
        if (own < 5)
        { int x = 0,y = 0; chooser->GetSeedPositionInChooser(choices[own],x,y); if (Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue)))) s.waitingOwn = own+1; return true; }
        if (bank.packets.size() != 10 || !Packet(b,0,26) || !Packet(b,0,32) || !Packet(b,1,1) || !Packet(b,1,28)) return true;
        int rank = -1;
        for (int i = 0; i < b->mSeedBank->mNumPackets; ++i) { const int r = NetSeedBankOwnerRank(slot,b->mSeedBank->mSeedPackets[i].mNetOwnerSlot); if (!Require(a,r >= rank,"Corn bank lost native own-first ordering")) return true; rank = r; }
        NetTrace("SMOKE_FUSION_CORN_POOL role=%s eligible=%zu target=10 quota0=5 quota1=5 ownFirst=1",Role(a),ctx->eligible.size()); Advance(a,2);
    }
    else if (s.stage == 2) { if (bank.phase != FusionBankPhase::Choosing) Advance(a,3); else if (chooser && !chooser->mSeedsInFlight && Ready(a,chooser->mStartButton)) Advance(a,3); }
    else if (s.stage == 3)
    {
        if (bank.phase == FusionBankPhase::Intro) { if (!s.introFirst) s.introFirst = GetTickCount(); s.introLast = GetTickCount(); ++s.introChecks; return true; }
        if (bank.phase != FusionBankPhase::Playing || a->mGameScene != SCENE_PLAYING || chooser) return true;
        if (!Require(a,s.introChecks >= 10 && s.introLast-s.introFirst >= 1000 && b->mSunMoney == 500,"Corn skipped native full intro")) return true; Advance(a,4);
    }
    else if (s.stage == 4) { if (Setup(a)) Advance(a,5); }
    else if (s.stage == 5 && Purchase(a))
    {
        ++s.buy;
        const bool last = s.buy == buyEnds[s.group];
        Advance(a,last ? 6 : 5);
    }
    else if (s.stage == 6)
    {
        if (HasLobbers(b,true)) return true;
        auto& g = s.groups[s.group]; for (int i = 0; i < 3; ++i) { auto* z = Target(b,g.targets[i]); if (!Require(a,z != nullptr,"Corn activation target missing")) return true; g.health[i] = z->mBodyHealth; }
        g.baseline = true; Advance(a,7);
    }
    else if (s.stage == 7 && Evidence(a))
    { NetTrace("SMOKE_FUSION_CORN_COMBAT role=%s kind=%d normal=%d alternate=%d sourceCounterparts=1",Role(a),s.groups[s.group].kind,s.groups[s.group].normal,s.groups[s.group].alternate); Advance(a,8); }
    else if (s.stage == 8 && Shovel(a))
    {
        if (!Business(a)) return true;
        s.shovelPicked = s.shovelSent = false;
        if (++s.group == int(s.groups.size())) Advance(a,9); else Advance(a,5);
    }
    else if (s.stage == 9)
    {
        if (!Business(a) || !Capture(a,"fusion_corn_complete") || !Barrier(a,"FC162_COMPLETE")) return true;
        for (auto id : s.targets)
        { auto* z = Target(b,id); if (!Require(a,z != nullptr,"Corn final canonical target missing")) return true; NetTrace("SMOKE_FUSION_CORN_FINAL role=%s id=%u row=%d HP=%d",Role(a),id,z->mRow,z->mBodyHealth); }
        NetTrace("SMOKE_FUSION_CORN_COMPLETE role=%s kinds=28,1111,1113,1121,1124 purchases=9 initialSun=2500 totalCost=1250 collected=5 finalSun=1255 payloads=53,54,55,56,57,66,67,70 nativeBoard=1 secondWorld=0 actorWritesAfterSetup=0 rngSeeds=1 setupWalletWrites=1 sourceZombieAIUnported=1",Role(a));
        Advance(a,10); a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
