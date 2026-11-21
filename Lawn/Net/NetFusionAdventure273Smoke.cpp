#include "NetFusionAdventure273Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Coin.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeAdventure273.h"
#include "../Fusion/FusionNativeAdventureProgress273.h"
#include "../Fusion/FusionNativeDroppedCards269.h"
#include "../Fusion/FusionNativeZombieVisual.h"
#include "../Fusion/FusionNativeEffects269.h"
#include "../Fusion/FusionRecipe.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/DDInterface.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
using namespace LawnFusion;using LawnNet::NetTrace;
struct Ref{uint16_t id=0;uint32_t token=0;};
struct State
{
    int stage=0,waiting=0,input=0,playingFrame=-1;
    uint32_t since=0,started=0,chat=0,epoch=0,pickSamples=0,initialSamples=0,waveSamples=0,pauseSamples=0;
    bool verified=false,pause=false,resourcesChecked=false;
    std::array<Ref,6> plants{};std::array<Ref,3> cards{},originalCards{};Ref wave,missionProposal;
    float firstWaveX=0;
    std::set<std::string> frames;std::map<std::string,int> draws;
    int classicLevel=-1,classicFinished=-1,farmInput=0,farmKind=0,farmCol=0,farmRow=0,farmResult=0;
    int farmPurchases=0,speedRequests=0,missionAttempts=0,missionAgreed=-1;
    uint32_t speedAt=0,sunAt=0,diagnostic=0,missionAt=0;
    bool speedLogged=false,commitObserved=false,trophyClicked=false;
    Ref trophy;std::map<uint32_t,std::pair<uint16_t,uint8_t>> enemies;std::array<unsigned,11> waveCounts{};
}s;
bool Victory(){const char* value=std::getenv("PVZ_COOP_SMOKE_FUSION_SOURCE273_VICTORY");return value&&std::strcmp(value,"1")==0;}
bool SolarShots(){const char* value=std::getenv("PVZ_COOP_SMOKE_FUSION_SOURCE273_SOLAR");return !Victory()&&value&&std::strcmp(value,"1")==0;}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool okay,const char* why){if(!okay)a->CoopSmokeFail(why);return okay;}
void Advance(LawnApp* a,int stage)
{s.stage=stage;s.since=GetTickCount();s.input=0;s.verified=false;s.missionAttempts=0;s.missionAgreed=-1;s.missionProposal={};NetTrace("SMOKE_SOURCE273_STAGE role=%s stage=%d epoch=%u",Role(a),stage,s.epoch);}
std::string Tag(const std::string& name){return "S273_"+std::to_string(s.epoch)+"_"+name;}
bool Has(LawnApp* a,int who,const std::string& text)
{for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==who&&line.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text)
{
    if(!Require(a,text.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Source273 chat proof exceeds native limit"))return false;
    if(Has(a,Slot(a),text))return true;
    if(s.chat&&GetTickCount()-s.chat<800)return false;
    s.chat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);
}
bool Barrier(LawnApp* a,const std::string& name)
{const auto text=Tag(name);Send(a,text);return Has(a,0,text)&&Has(a,1,text);}
bool Click(LawnApp* a,float x,float y,Uint8 button=SDL_BUTTON_LEFT)
{
    SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=SDL_GetWindowID(a->mSDLWindow);motion.motion.x=x;motion.motion.y=y;
    SDL_Event down{};down.type=SDL_EVENT_MOUSE_BUTTON_DOWN;down.button.windowID=motion.motion.windowID;down.button.button=button;down.button.clicks=1;down.button.down=true;down.button.x=x;down.button.y=y;
    SDL_Event up=down;up.type=SDL_EVENT_MOUSE_BUTTON_UP;up.button.down=false;
    return Require(a,SDL_PushEvent(&motion)&&SDL_PushEvent(&down)&&SDL_PushEvent(&up),"Source273 owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Escape(LawnApp* a)
{
    SDL_Event down{};down.type=SDL_EVENT_KEY_DOWN;down.key.windowID=SDL_GetWindowID(a->mSDLWindow);down.key.key=SDLK_ESCAPE;down.key.down=true;
    SDL_Event up=down;up.type=SDL_EVENT_KEY_UP;up.key.down=false;
    return Require(a,SDL_PushEvent(&down)&&SDL_PushEvent(&up),"Source273 owned SDL Escape failed");
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_source273_"+std::string(suffix);if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requested=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requested))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Source273 fresh owned frame failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_SOURCE273_RENDER role=%s frame=%s requestedDraw=%d capturedDraw=%d",Role(a),name.c_str(),requested,a->mCoopSmokeFrameDrawCount);return true;
}
Plant* At(Board* b,int col,int row,uint16_t kind)
{Plant* p=nullptr;while(b->IteratePlants(p))if(p->mPlantCol==col&&p->mRow==row)if(const auto* d=GetNativePlantData(p);d&&d->kind==kind)return p;return nullptr;}
Plant* PlantByRef(Board* b,Ref ref)
{Plant* p=nullptr;while(b->IteratePlants(p))if(p->mNetId==ref.id)if(const auto* d=GetNativePlantData(p);d&&d->instanceToken==ref.token)return p;return nullptr;}
Coin* CardByRef(Board* b,Ref ref)
{Coin* c=nullptr;while(b->IterateCoins(c))if(c->mNetId==ref.id)if(const auto* d=GetNativeDroppedCard269Data(c);d&&d->instanceToken==ref.token)return c;return nullptr;}
bool CardPickupPoint(Board* b,Coin* card,float& x,float& y)
{
    // Native hit testing returns the LAST overlapping Coin, not necessarily
    // the card whose center we clicked. Use an exposed, rounding-safe point
    // for this exact card and retain the authoritative UID/token check below.
    float best=1e9f;bool found=false;
    for(int iy=1;iy<14;++iy)for(int ix=1;ix<10;++ix)
    {
        const float px=card->mPosX+ix*5,py=card->mPosY+iy*5;
        if(FindNativeDroppedCard269At(b,px,py)!=card||
            FindNativeDroppedCard269At(b,px-3,py)!=card||FindNativeDroppedCard269At(b,px+3,py)!=card||
            FindNativeDroppedCard269At(b,px,py-3)!=card||FindNativeDroppedCard269At(b,px,py+3)!=card)continue;
        const float dx=px-card->mPosX-25,dy=py-card->mPosY-35,score=dx*dx+dy*dy;
        if(score<best){best=score;x=px;y=py;found=true;}
    }
    return found;
}
SeedPacket* Packet(Board* b,int owner,uint16_t kind,bool extra)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];if(const auto* d=GetNativePacketData(p);d&&d->owner==owner&&d->kind==kind&&d->isExtra==extra)return p;}return nullptr;}
bool World(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);if(!c||!c->adventure.enabled)return false;
    const auto& view=c->adventure;
    if(!Require(a,!c->validation&&!a->mFusionRuntime&&IsNativeSourceAdventure273(b)&&view.run.level==1&&view.run.cycle==1&&
        view.profile.cycle==1&&view.sourceDifficulty==2&&view.maximumWaves==10&&!view.schedulerFailed&&c->money==3000&&
        ((Victory()&&s.stage>=20)||(!view.run.over&&view.profile.frontierLevel==1&&!view.profile.completedLevels))&&
        b->GetColumnCount()==10&&GetNativeFusionRowCount(b)==5&&b->mBackground==BACKGROUND_1_DAY,
        "Source273 true source boot/world/profile differs"))return false;
    for(int row=0;row<5;++row)if(!Require(a,b->mPlantRow[row]==PLANTROW_NORMAL,"Source273 stock restricted lawn leaked"))return false;
    if(!s.epoch)s.epoch=c->epoch;
    return Require(a,c->epoch==s.epoch,"Source273 native epoch changed unexpectedly");
}
bool Recipes(LawnApp* a)
{
    for(const auto [existing,used,expected]:{std::array<uint16_t,3>{1,0,1000},{0,0,1030},{1030,0,1090},{1090,0,1032}})
    {const auto* r=FindFusionRecipe(existing,used);if(!Require(a,r&&r->result==expected,"Source273 actual source recipe chain differs"))return false;}
    return true;
}
bool Choose(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;
    if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return false;
    const auto& bank=c->selection.State();std::set<unsigned> eligible;
    for(const auto& card:c->eligible)eligible.insert(unsigned(card.kind)*2+(card.isExtra?1:0));
    if(!Require(a,eligible==std::set<unsigned>{0,1,2,3}&&c->eligible.size()==4&&bank.targetCount==4&&
        bank.quotas[0]==2&&bank.quotas[1]==2,"Source273 initial normal/extra0/1 pool/quotas differ"))return false;
    int own=0;for(const auto& card:bank.packets)if(card.owner==Slot(a))
    {if(!Require(a,card.kind<=1&&card.isExtra==(Slot(a)==1),"Source273 actual local chooser identity differs"))return false;++own;}
    if(own<s.waiting)return false;
    if(own<2)
    {
        const uint16_t wanted=uint16_t(own);int dense=-1;
        for(size_t i=0;i<c->eligible.size();++i)if(c->eligible[i].kind==wanted&&c->eligible[i].isExtra==(Slot(a)==1))dense=int(i);
        if(!Require(a,dense>=0,"Source273 local card missing"))return false;
        int x=0,y=0;chooser->GetSeedPositionInChooser(dense,x,y);
        if(std::abs(chooser->mScrollAmount)>.05f)return false;
        if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waiting=own+1;
        return false;
    }
    return bank.packets.size()==4&&Capture(a,"chooser")&&Barrier(a,"CHOOSER");
}
bool PlantProof(LawnApp* a,int index,int owner,uint16_t kind,uint16_t parent,uint16_t packetId,int col)
{
    auto* b=a->mBoard;auto* p=At(b,col,4,kind);const auto* d=GetNativePlantData(p);
    if(!p||!d)return false;
    if(!Require(a,p->mNetId&&d->instanceToken&&d->owner==owner&&d->packetId==packetId&&
        d->firstParent==parent&&d->secondParent==(parent==kFusionNoPlant?kFusionNoPlant:uint16_t(0))&&
        p->mPlantHealth==p->mPlantMaxHealth&&p->mPlantHealth>0,"Source273 actual native plant identity/ancestry/HP differs"))return false;
    if(index==1&&!Require(a,!PlantByRef(b,s.plants[0])&&p->mNetId!=s.plants[0].id,"Source273 Sunflower parent was not replaced"))return false;
    if(index>=3&&!Require(a,!PlantByRef(b,s.plants[index-1])&&p->mNetId!=s.plants[index-1].id,"Source273 Pea chain parent was not replaced"))return false;
    s.plants[index]={p->mNetId,d->instanceToken};
    if(!s.verified)
    {
        s.verified=true;NetTrace("SMOKE_SOURCE273_PLANT role=%s step=%d owner=%d kind=%u id=%u token=%u parent=%u packet=%u sourceInputOnly=1",
            Role(a),index,owner,kind,p->mNetId,d->instanceToken,parent,packetId);
    }
    return Barrier(a,"PLANT_"+std::to_string(index)+"_"+std::to_string(p->mNetId)+"_"+std::to_string(d->instanceToken));
}
bool PacketPlant(LawnApp* a,int index,int owner,uint16_t used,bool extra,uint16_t result,uint16_t parent,int col)
{
    auto* b=a->mBoard;auto* packet=Packet(b,owner,used,extra);
    if(!Require(a,packet!=nullptr,"Source273 actual selected native packet missing"))return false;
    if(Slot(a)==owner&&s.input==0)
    {
        if(!packet->CanPickUp()||b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL)return false;
        s.pickSamples=Samples(a);auto* bank=b->mSeedBank;
        if(BoardClick(a,float(bank->mX+packet->mX+packet->mOffsetX+25),float(bank->mY+packet->mY+35)))s.input=1;
        return false;
    }
    if(Slot(a)==owner&&s.input==1)
    {
        const auto* cursor=b->mCursorObject;
        if(cursor->mCursorType!=CURSOR_TYPE_PLANT_FROM_BANK||cursor->mSeedBankIndex<0||
            cursor->mSeedBankIndex>=b->mSeedBank->mNumPackets||
            b->mSeedBank->mSeedPackets[cursor->mSeedBankIndex].mNetId!=packet->mNetId||
            (a->NetIsClient()&&Samples(a)<=s.pickSamples))return false;
        if(BoardClick(a,float(b->GetCellCenterX(col,4)),float(b->GridToPixelY(col,4)+50)))s.input=2;
        return false;
    }
    return PlantProof(a,index,owner,result,parent,packet->mNetId,col);
}
bool Missions(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);
    if(c->adventure.run.tutorial.stage!=6||!c->adventure.run.tutorial.active)return false;
    std::vector<Ref> cards;Coin* coin=nullptr;
    while(b->IterateCoins(coin))if(const auto* d=GetNativeDroppedCard269Data(coin);d&&d->kind==0&&d->cost==0&&d->heldSlot==255)
    {if(!d->landed)return false;cards.push_back({coin->mNetId,d->instanceToken});}
    if(cards.size()!=3)return false;
    std::sort(cards.begin(),cards.end(),[](Ref a,Ref b){return a.id<b.id;});
    LawnNet::NetBuffer identity;
    for(size_t i=0;i<3;++i){s.cards[i]=s.originalCards[i]=cards[i];identity.WriteU16(cards[i].id);identity.WriteU32(cards[i].token);}
    char tag[32];sprintf_s(tag,"CARDS_%08x",LawnNet::NetFnv1a32(identity.Data(),identity.Size()));
    if(!Capture(a,"missions")||!Barrier(a,tag))return false;
    NetTrace("SMOKE_SOURCE273_MISSIONS role=%s count=3 free=1 tutorial=6 ids=%u,%u,%u actualGrant=1 sourceFullCampaign=0",Role(a),s.cards[0].id,s.cards[1].id,s.cards[2].id);
    return true;
}
void MissionGuard(LawnApp* a,int mission,const char* reason)
{
    if(s.diagnostic&&GetTickCount()-s.diagnostic<2000)return;
    s.diagnostic=GetTickCount();auto* b=a->mBoard;
    auto* held=GetNativeDroppedCard269Held(b,1);const auto* hd=GetNativeDroppedCard269Data(held);
    NetTrace("SMOKE_SOURCE273_CARD_GUARD role=%s mission=%d input=%d agreed=%d cursor=%d held=%u heldToken=%u reason=%s",
        Role(a),mission,s.input,s.missionAgreed,int(b->mCursorObject->mCursorType),held?held->mNetId:0,hd?hd->instanceToken:0,reason);
    for(int i=mission;i<3;++i)
    {
        auto* card=CardByRef(b,s.cards[i]);const auto* d=GetNativeDroppedCard269Data(card);float x=0,y=0;
        const bool point=card&&CardPickupPoint(b,card,x,y);
        NetTrace("SMOKE_SOURCE273_CARD_CANDIDATE role=%s index=%d id=%u token=%u exists=%d heldSlot=%u landed=%d pos=(%.2f,%.2f) safePoint=%d point=(%.2f,%.2f)",
            Role(a),i,s.cards[i].id,s.cards[i].token,card?1:0,d?unsigned(d->heldSlot):255,d&&d->landed?1:0,
            card?card->mPosX:0,card?card->mPosY:0,point?1:0,x,y);
    }
}
bool AgreeMissionCard(LawnApp* a,int mission)
{
    if(s.missionAgreed==mission)return true;
    auto* b=a->mBoard;const auto prefix=Tag("CARD_CHOICE_"+std::to_string(mission)+"_");
    std::string choice;
    for(const auto& line:a->mNetManager->mChatLines)
        if(line.mSlot==1&&line.mText.starts_with(prefix))
        {
            if(!Require(a,choice.empty()||choice==line.mText,"Source273 conflicting exact mission-card choices"))return false;
            choice=line.mText;
        }
    if(choice.empty())
    {
        if(Slot(a)!=1){MissionGuard(a,mission,"waiting-client-choice");return false;}
        if(s.missionProposal.id)
        {Send(a,prefix+std::to_string(s.missionProposal.id)+"_"+std::to_string(s.missionProposal.token));return false;}
        if(b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL||GetNativeDroppedCard269Held(b,1))
        {MissionGuard(a,mission,"choice-cursor-not-normal");return false;}
        for(int i=mission;i<3;++i)
        {
            auto* card=CardByRef(b,s.cards[i]);const auto* d=GetNativeDroppedCard269Data(card);float x=0,y=0;
            if(!card||!d||d->kind!=0||d->cost!=0||d->heldSlot!=255||!d->landed||!CardPickupPoint(b,card,x,y))continue;
            s.missionProposal=s.cards[i];
            Send(a,prefix+std::to_string(s.missionProposal.id)+"_"+std::to_string(s.missionProposal.token));return false;
        }
        MissionGuard(a,mission,"no-exposed-remaining-original-card");return false;
    }
    const auto tail=choice.substr(prefix.size());const auto split=tail.find('_');
    unsigned id=0,token=0;bool parsed=false;
    if(split!=std::string::npos)
    {
        const auto first=std::from_chars(tail.data(),tail.data()+split,id);
        const auto second=std::from_chars(tail.data()+split+1,tail.data()+tail.size(),token);
        parsed=first.ec==std::errc{}&&first.ptr==tail.data()+split&&second.ec==std::errc{}&&second.ptr==tail.data()+tail.size();
    }
    if(!Require(a,parsed&&id>0&&id<=65535&&token>0,"Source273 malformed exact mission-card agreement"))return false;
    int selected=-1;
    for(int i=mission;i<3;++i)if(s.cards[i].id==id&&s.cards[i].token==token)selected=i;
    if(!Require(a,selected>=mission,"Source273 choice is not an unconsumed original mission Ref"))return false;
    std::swap(s.cards[mission],s.cards[selected]); // Fixture ordering only; never move the actual Coin.
    if(!Barrier(a,"CARD_READY_"+std::to_string(mission)+"_"+std::to_string(id)+"_"+std::to_string(token)))
    {MissionGuard(a,mission,"waiting-exact-card-ready-barrier");return false;}
    s.missionAgreed=mission;
    NetTrace("SMOKE_SOURCE273_CARD_AGREED role=%s mission=%d id=%u token=%u originalSetPreserved=1",Role(a),mission,id,token);
    return true;
}
bool DropPlant(LawnApp* a,int index,int mission,uint16_t result,uint16_t parent)
{
    auto* b=a->mBoard;
    for(const auto original:s.originalCards)
    {
        const auto count=std::count_if(s.cards.begin(),s.cards.end(),[&](Ref r){return r.id==original.id&&r.token==original.token;});
        if(!Require(a,original.id&&original.token&&count==1,"Source273 initial mission Ref set was not preserved"))return false;
    }
    if(!AgreeMissionCard(a,mission))return false;
    const auto ref=s.cards[mission];auto* card=CardByRef(b,ref);
    for(int i=0;i<mission;++i)
        if(!Require(a,!CardByRef(b,s.cards[i]),"Source273 previously consumed exact mission card still exists"))return false;
    if(Slot(a)==1&&s.input==0)
    {
        if(!card||b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL)
        {MissionGuard(a,mission,"pickup-card-or-cursor-unavailable");return false;}
        const auto* d=GetNativeDroppedCard269Data(card);
        if(!Require(a,d&&d->kind==0&&d->cost==0&&d->heldSlot==255,"Source273 mission card metadata differs before pickup"))return false;
        float x=0,y=0;if(!CardPickupPoint(b,card,x,y))
        {MissionGuard(a,mission,"agreed-card-point-not-exposed");return false;}
        if(!Require(a,s.missionAttempts<4,"Source273 exact mission-card pickup exhausted real pointer retries"))return false;
        ++s.missionAttempts;s.pickSamples=Samples(a);s.missionAt=GetTickCount();
        NetTrace("SMOKE_SOURCE273_CARD_PICK role=%s mission=%d attempt=%d wanted=%u token=%u pos=(%.2f,%.2f) point=(%.2f,%.2f) hit=%u samples=%u",
            Role(a),mission,s.missionAttempts,ref.id,ref.token,card->mPosX,card->mPosY,x,y,FindNativeDroppedCard269At(b,x,y)->mNetId,s.pickSamples);
        if(BoardClick(a,x,y))s.input=1;
        return false;
    }
    if(Slot(a)==1&&s.input==1)
    {
        auto* held=GetNativeDroppedCard269Held(b,1);const auto* d=GetNativeDroppedCard269Data(held);
        if(Samples(a)<=s.pickSamples)return false;
        if(!held||!d||held->mNetId!=ref.id||d->instanceToken!=ref.token)
        {
            if(!s.diagnostic||GetTickCount()-s.diagnostic>=2000)
            {
                s.diagnostic=GetTickCount();
                NetTrace("SMOKE_SOURCE273_CARD_WAIT role=%s mission=%d wanted=%u token=%u held=%u heldToken=%u cursor=%d samples=%u pickSamples=%u",
                    Role(a),mission,ref.id,ref.token,held?held->mNetId:0,d?d->instanceToken:0,int(b->mCursorObject->mCursorType),Samples(a),s.pickSamples);
            }
            if(held&&d)
            {
                // Release the actual wrong card through its genuine native
                // pointer command; never rewrite heldSlot or accept its ID.
                s.pickSamples=Samples(a);
                if(Click(a,float(b->mX+a->mDDInterface->mWideScreenOffsetX+500),
                    float(b->mY+a->mDDInterface->mWideScreenOffsetY+500),SDL_BUTTON_RIGHT))s.input=3;
            }
            else if(b->mCursorObject->mCursorType==CURSOR_TYPE_NORMAL&&GetTickCount()-s.missionAt>=1000&&Samples(a)>s.pickSamples+2)
                s.input=0;
            return false;
        }
        NetTrace("SMOKE_SOURCE273_CARD_HELD role=%s mission=%d id=%u token=%u attempt=%d samples=%u",
            Role(a),mission,held->mNetId,d->instanceToken,s.missionAttempts,Samples(a));
        if(BoardClick(a,float(b->GetCellCenterX(3,4)),float(b->GridToPixelY(3,4)+50)))s.input=2;
        return false;
    }
    if(Slot(a)==1&&s.input==3)
    {
        if(Samples(a)>s.pickSamples&&!GetNativeDroppedCard269Held(b,1)&&b->mCursorObject->mCursorType==CURSOR_TYPE_NORMAL)
        {s.input=0;NetTrace("SMOKE_SOURCE273_CARD_RELEASED role=%s mission=%d retry=%d",Role(a),mission,s.missionAttempts);}
        return false;
    }
    if(card)return false;
    return PlantProof(a,index,1,result,parent,0,3);
}
Zombie* Wave(LawnApp* a)
{
    auto* b=a->mBoard;Zombie* z=nullptr;Zombie* found=nullptr;int count=0;
    while(b->IterateZombies(z))if(!z->mMindControlled)
    {
        const auto* d=GetNativeSourceZombieData(z);
        if(!Require(a,d&&d->kind==0&&z->mZombieType==ZOMBIE_NORMAL&&z->mFromWave==1,
            "Source273 stock FLAG/untyped/non-source-wave enemy leaked"))return nullptr;
        ++count;found=z;
    }
    if(count!=1)return nullptr;
    const auto* d=GetNativeSourceZombieData(found);
    if(!s.wave.id)
    {
        if(!Require(a,found->mBodyHealth==270&&found->mBodyMaxHealth==270&&d->health==270.f,
            "Source273 first-wave initial HP differs"))return nullptr;
        s.wave={found->mNetId,d->instanceToken};s.firstWaveX=found->mPosX;s.waveSamples=Samples(a);
    }
    if(!Require(a,found->mNetId==s.wave.id&&d->instanceToken==s.wave.token&&found->mBodyHealth>0&&
        found->mBodyMaxHealth==270&&d->health>0&&!found->IsDeadOrDying(),
        "Source273 first-wave live source identity differs"))return nullptr;
    return found;
}
bool WaveVisible(LawnApp* a,Zombie* zombie,const char* capture)
{
    auto* b=a->mBoard;float rootX=0,rootY=0,shadowX=0,shadowY=0;
    if(!Require(a,GetNativeSourceZombie269NodePoint(zombie,"",rootX,rootY)&&
        GetNativeSourceZombie269NodePoint(zombie,"Shadow",shadowX,shadowY),
        "Source273 first-wave actual source model anchors unavailable"))return false;
    const float offsetX=float(b->mX+a->mDDInterface->mWideScreenOffsetX);
    const float offsetY=float(b->mY+a->mDDInterface->mWideScreenOffsetY);
    const float screenRootX=rootX+offsetX,screenRootY=rootY+offsetY;
    const float screenShadowX=shadowX+offsetX,screenShadowY=shadowY+offsetY;
    // Carrier mPosX excludes the source pose/root offset. Both genuine model
    // anchors must enter this peer's actual viewport, leaving body-width margin.
    if((std::min)(screenRootX,screenShadowX)<80.f||
        (std::max)(screenRootX,screenShadowX)>float(a->mWidth)-80.f||
        (std::min)(screenRootY,screenShadowY)<80.f||
        (std::max)(screenRootY,screenShadowY)>float(a->mHeight)-20.f)return false;
    if(!s.draws.contains("fusion_source273_"+std::string(capture)))
        NetTrace("SMOKE_SOURCE273_VISIBLE role=%s frame=%s id=%u token=%u carrier=(%.3f,%.3f) root=(%.3f,%.3f) shadow=(%.3f,%.3f) screenRoot=(%.3f,%.3f) screenShadow=(%.3f,%.3f) offset=(%.0f,%.0f) viewport=%dx%d marginX=80 marginY=20",
            Role(a),capture,s.wave.id,s.wave.token,zombie->mPosX,zombie->mPosY,rootX,rootY,shadowX,shadowY,
            screenRootX,screenRootY,screenShadowX,screenShadowY,offsetX,offsetY,a->mWidth,a->mHeight);
    return true;
}
bool SolarEvidence(LawnApp* a)
{
    if(!SolarShots())return true;
    if(s.frames.contains("fusion_source273_solar"))return Barrier(a,"SOLAR");
    auto* b=a->mBoard;Projectile* shot=nullptr;
    while(b->IterateProjectiles(shot))
    {
        const auto* d=GetNativeProjectileData(shot);
        if(shot->mDead||!shot->mNetId||!d||d->kind!=8||d->damage!=100||d->sunOnHit!=5||
            d->fireState||d->specialType||d->lastTorchId||d->lastTorchToken||d->effectVariant||
            d->melonEnhanced||d->isZombieBullet||d->hitLatched||d->umbrellaed||d->goldAttracted||
            shot->mProjectileType!=PROJECTILE_PEA||shot->mMotionType!=MOTION_STRAIGHT)continue;
        const float x=shot->mPosX+20.f+b->mX+a->mDDInterface->mWideScreenOffsetX;
        const float y=shot->mPosY+20.f+shot->mPosZ+b->mY+a->mDDInterface->mWideScreenOffsetY;
        if(!std::isfinite(x)||!std::isfinite(y)||x<32.f||x>float(a->mWidth)-32.f||
            y<110.f||y>float(a->mHeight)-32.f)continue;
        NetTrace("SMOKE_SOURCE273_SOLAR role=%s id=%u age=%d row=%d kind=%u damage=%d sunOnHit=%d fireState=%u raw=(%.3f,%.3f,%.3f) screen=(%.3f,%.3f) viewport=%dx%d samples=%u naturallyFired=1 actorWrites=0 frame=fusion_source273_solar",
            Role(a),shot->mNetId,shot->mProjectileAge,shot->mRow,d->kind,d->damage,d->sunOnHit,unsigned(d->fireState),
            shot->mPosX,shot->mPosY,shot->mPosZ,x,y,a->mWidth,a->mHeight,Samples(a));
        return Capture(a,"solar")&&Barrier(a,"SOLAR");
    }
    return false;
}
bool Final(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);const auto& view=c->adventure;
    if(!b->mPaused||view.wave!=1||view.run.tutorial.active||!Wave(a)||
        !PlantByRef(b,s.plants[1])||!PlantByRef(b,s.plants[5]))return false;
    if(a->NetIsHost()&&!Require(a,c->adventureEnemyCount==1&&c->adventureEnemies.contains(s.wave.token),"Source273 host source enemy counter/generation differs"))return false;
    LawnNet::NetBuffer proof;proof.WriteU16(view.wave);proof.WriteU16(view.maximumWaves);proof.WriteF32(view.countdown);
    proof.WriteU32(uint32_t(view.profile.completedLevels));proof.WriteI32(b->mSunMoney);proof.WriteI32(c->money);
    for(int index:{1,5}){proof.WriteU16(s.plants[index].id);proof.WriteU32(s.plants[index].token);}
    auto* actor=Wave(a);if(!actor||!WaveVisible(a,actor,"paused"))return false;const auto* source=GetNativeSourceZombieData(actor);
    proof.WriteU16(s.wave.id);proof.WriteU32(s.wave.token);proof.WriteI32(actor->mBodyHealth);proof.WriteF32(source->health);
    char tag[32];sprintf_s(tag,"FINAL_%08x",LawnNet::NetFnv1a32(proof.Data(),proof.Size()));
    if(!Capture(a,"paused")||!Barrier(a,tag))return false;
    NetTrace("SMOKE_SOURCE273_COMPLETE role=%s epoch=%u sourceLevel=1 wave=1/10 kind=0 id=%u token=%u rows=5 columns=10 initialSun=500 money=3000 missionCards=3 clientDropFusion=1 clientPaidFusion=1 tutorialCompleted=1 paused=1 sourceFullCampaign=0 trophy=NotExecuted actorWrites=0 clockWrites=0 selectionWrites=SDLOnly",
        Role(a),s.epoch,s.wave.id,s.wave.token);return true;
}
bool Farm(LawnApp*);
bool ObserveVictory(LawnApp*);
bool VictoryTick(LawnApp*);
bool NextChooser(LawnApp*);
}
namespace LawnNet
{
bool NetSmokeUpdateFusionAdventure273(LawnApp* a)
{
    const char* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_SOURCE273"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||
        a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;
    if(!s.started)s.started=GetTickCount();
    const uint32_t totalDeadline=Victory()?600000:180000;
    const uint32_t stageDeadline=Victory()&&(s.stage==15||s.stage==20)?300000:60000;
    if(GetTickCount()-s.started>totalDeadline||(s.since&&GetTickCount()-s.since>stageDeadline))
    {a->CoopSmokeFail("Source273 bounded actual-input opening deadline; inspect stage/input traces");return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->mNetManager->GetPlayerCount()!=2)return true;
        if(!Require(a,Slot(a)>=0&&Slot(a)<2&&a->mNetManager->GetDifficulty()==1,"Source273 requires two peers at existing coop difficulty1"))return true;
        if(s.classicLevel<0)
        {s.classicLevel=a->mPlayerInfo->mLevel;s.classicFinished=a->mPlayerInfo->mFinishedAdventure;}
        if(a->NetIsClient()){Send(a,"S273_BEGIN");Advance(a,1);return true;}
        if(!Has(a,1,"S273_BEGIN"))return true;
        if(!Require(a,Recipes(a)&&a->StartFusionSourceAdventure(1,false),"Source273 production source1 start failed"))return true;
        Advance(a,1);return true;
    }
    if(!a->mBoard)return true;
    if(Victory()&&s.stage>=22)
        if(const auto* next=GetNativeContext(a->mBoard);next&&next->adventure.run.level==2)
        {NextChooser(a);return true;}
    if(!World(a))return true;
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);const auto& bank=c->selection.State();
    const char* resources=std::getenv("PVZ_COOP_SMOKE_FUSION_DAY273_RESOURCES");
    if(resources&&std::strcmp(resources,"1")==0&&!s.resourcesChecked)
    {
        std::string error;
        for(uint16_t kind:{3,21,22,23,100,106,107,108})
        {
            const bool ready=EnsureNativeSourceZombieVisuals(b,kind,&error);
            NetTrace("SMOKE_SOURCE273_DAY_RESOURCE role=%s kind=%u ready=%d error=%s",Role(a),kind,ready?1:0,error.c_str());
            if(!Require(a,ready,"Day273 complete body/particle source resource preflight failed"))return true;
        }
        for(uint16_t effect:{0,11,18,23})
        {
            const bool ready=EnsureNativeSourceEffect269(b,effect,&error);
            NetTrace("SMOKE_SOURCE273_DAY_FX role=%s kind=%u ready=%d error=%s",Role(a),effect,ready?1:0,error.c_str());
            if(!Require(a,ready,"Day273 source hit/vault effect preflight failed"))return true;
        }
        s.resourcesChecked=true;
        NetTrace("SMOKE_SOURCE273_DAY_RESOURCES_COMPLETE role=%s bodies=8 factoryFX=4 actorAdmission=0",Role(a));
    }
    if(Victory()&&s.stage>=10&&!ObserveVictory(a))return true;
    if(Victory()&&s.stage>=20)return VictoryTick(a);
    if(s.stage==1){if(Choose(a))Advance(a,2);}
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* chooser=a->mSeedChooserScreen;auto* button=chooser?chooser->mStartButton:nullptr;
        if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver())
        {SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Source273 owned Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase!=FusionBankPhase::Playing||a->mSeedChooserScreen||a->mGameScene!=SCENE_PLAYING)return true;
        if(s.playingFrame<0){s.playingFrame=a->mAppCounter;s.initialSamples=Samples(a);return true;}
        if(a->mAppCounter<=s.playingFrame||Samples(a)-s.initialSamples<2)return true;
        if(!Require(a,b->mSunMoney==500&&c->adventure.wave==0&&c->adventure.run.tutorial.active&&
            !b->mPlants.mSize&&!b->mZombies.mSize,"Source273 actual opening defaults/tutorial/empty world differ"))return true;
        if(Capture(a,"initial")&&Barrier(a,"INITIAL"))Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(c->adventure.run.tutorial.stage<4)return true;
        if(PacketPlant(a,0,0,1,false,1,kFusionNoPlant,2))Advance(a,5);
    }
    else if(s.stage==5){if(PacketPlant(a,1,0,0,false,1000,1,2))Advance(a,6);}
    else if(s.stage==6){if(Missions(a))Advance(a,7);}
    else if(s.stage==7){if(DropPlant(a,2,0,0,kFusionNoPlant))Advance(a,8);}
    else if(s.stage==8){if(DropPlant(a,3,1,1030,0))Advance(a,9);}
    else if(s.stage==9){if(DropPlant(a,4,2,1090,1030))Advance(a,Victory()?15:10);}
    else if(s.stage==15){if(Farm(a))Advance(a,10);}
    else if(s.stage==10){if(PacketPlant(a,5,1,0,true,1032,1090,3))Advance(a,11);}
    else if(s.stage==11)
    {
        if(c->adventure.run.tutorial.active||!PlantByRef(b,s.plants[5]))return true;
        if(!Capture(a,"gatling")||!Barrier(a,"GATLING"))return true;
        if(Victory()){Advance(a,20);return true;}
        if(SolarShots())
        {
            if(SolarEvidence(a))
            {
                NetTrace("SMOKE_SOURCE273_SOLAR_COMPLETE role=%s sourceLevel=1 wave=%u/10 kind=8 damage=100 sunOnHit=5 naturallyFired=1 actorWrites=0 sourceFullCampaign=0",
                    Role(a),c->adventure.wave);
                a->CoopSmokeWriteResult(true,nullptr);
            }
            return true;
        }
        if(c->adventure.wave!=1)return true;
        auto* zombie=Wave(a);if(!zombie||Samples(a)-s.waveSamples<2||std::abs(s.firstWaveX-zombie->mPosX)<2.f||!WaveVisible(a,zombie,"firstwave"))return true;
        const auto tag="WAVE_"+std::to_string(s.wave.id)+"_"+std::to_string(s.wave.token);
        if(Capture(a,"firstwave")&&Barrier(a,tag))
        {NetTrace("SMOKE_SOURCE273_MOVED role=%s id=%u token=%u xStart=%.3f xNow=%.3f samples=%u",Role(a),s.wave.id,s.wave.token,s.firstWaveX,zombie->mPosX,Samples(a)-s.waveSamples);Advance(a,12);}
    }
    else if(s.stage==12)
    {
        if(a->NetIsHost()&&!s.pause)s.pause=Escape(a);
        if(!b->mPaused)return true;
        s.pauseSamples=Samples(a);Advance(a,13);
    }
    else if(s.stage==13)
    {if(Samples(a)-s.pauseSamples>=2&&Final(a))a->CoopSmokeWriteResult(true,nullptr);}
    return true;
}
}
namespace
{
bool SpeedInput(LawnApp* a)
{
    if(!a->NetIsHost())return false;auto* b=a->mBoard;
    if(b->mAllowSpeedMod&&b->GetSpeedValue(b->mSpeedMod)<2.5f&&s.speedRequests<3)
    {
        if(s.speedAt&&GetTickCount()-s.speedAt<250)return true;
        SDL_Event text{};text.type=SDL_EVENT_TEXT_INPUT;text.text.windowID=SDL_GetWindowID(a->mSDLWindow);text.text.text="e";
        if(Require(a,SDL_PushEvent(&text),"Source273 owned native speed input failed"))
        {s.speedAt=GetTickCount();++s.speedRequests;}
        return true;
    }
    if(!s.speedLogged)
    {s.speedLogged=true;NetTrace("SMOKE_SOURCE273_SPEED actual=%.3f requests=%d nativeTextInput=e directSetter=0",b->GetSpeedValue(b->mSpeedMod),s.speedRequests);}
    return false;
}
bool CollectSun(LawnApp* a)
{
    if(!a->NetIsHost()||a->mBoard->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL)return false;
    if(s.sunAt&&GetTickCount()-s.sunAt<50)return false;
    Coin* coin=nullptr;while(a->mBoard->IterateCoins(coin))
        if(coin->IsSun()&&!coin->mIsBeingCollected&&coin->mHitGround)
        {s.sunAt=GetTickCount();return BoardClick(a,coin->mPosX+coin->mWidth*.5f,coin->mPosY+coin->mHeight*.5f);}
    return false;
}
bool FarmGoals(LawnApp* a,LawnNet::NetBuffer& proof)
{
    auto* b=a->mBoard;
    for(int row=0;row<4;++row)for(int col=0;col<5;++col)
    {
        const uint16_t kind=col<2?1:1030;auto* plant=At(b,col,row,kind);const auto* data=GetNativePlantData(plant);
        if(!plant||!data)return false;
        if(!Require(a,data->owner==0&&plant->mPlantHealth==plant->mPlantMaxHealth&&
            data->firstParent==(kind==1?kFusionNoPlant:uint16_t(0))&&data->secondParent==(kind==1?kFusionNoPlant:uint16_t(0)),
            "Source273 real farm defense ownership/ancestry/HP differs"))return false;
        proof.WriteU16(plant->mNetId);proof.WriteU32(data->instanceToken);proof.WriteU16(kind);
    }
    return true;
}
bool Farm(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);
    if(!Require(a,c->adventure.wave==0&&c->adventure.run.tutorial.active&&c->adventure.run.tutorial.stage==6,
        "Source273 farm lost the real BuildGatling tutorial gate"))return false;
    LawnNet::NetBuffer proof;
    if(!s.farmInput&&FarmGoals(a,proof))
    {
        char tag[40];sprintf_s(tag,"DEFENSES_%08x",LawnNet::NetFnv1a32(proof.Data(),proof.Size()));
        if(!Capture(a,"defenses")||!Barrier(a,tag))return false;
        NetTrace("SMOKE_SOURCE273_DEFENSES role=%s sunflowers=8 doubleShooters=12 rows=4 naturalIncome=1 purchases=%d sourceWave=0 tutorial=6 actorWrites=0",Role(a),s.farmPurchases);
        return true;
    }
    if(!a->NetIsHost())return false;
    if(SpeedInput(a))return false;
    if(s.farmInput==1)
    {
        auto* packet=Packet(b,0,uint16_t(s.farmKind),false);const auto* cursor=b->mCursorObject;
        if(!packet||cursor->mCursorType!=CURSOR_TYPE_PLANT_FROM_BANK||cursor->mSeedBankIndex<0||
            cursor->mSeedBankIndex>=b->mSeedBank->mNumPackets||
            b->mSeedBank->mSeedPackets[cursor->mSeedBankIndex].mNetId!=packet->mNetId)return false;
        if(BoardClick(a,float(b->GetCellCenterX(s.farmCol,s.farmRow)),float(b->GridToPixelY(s.farmCol,s.farmRow)+50)))s.farmInput=2;
        return false;
    }
    if(s.farmInput==2)
    {
        auto* plant=At(b,s.farmCol,s.farmRow,uint16_t(s.farmResult));const auto* data=GetNativePlantData(plant);
        if(!plant||!data)return false;
        if(!Require(a,data->owner==0&&plant->mPlantHealth==plant->mPlantMaxHealth,"Source273 actual farm placement differs"))return false;
        ++s.farmPurchases;
        NetTrace("SMOKE_SOURCE273_FARM kind=%u id=%u token=%u col=%d row=%d purchases=%d sun=%d input=SDL",
            data->kind,plant->mNetId,data->instanceToken,s.farmCol,s.farmRow,s.farmPurchases,b->mSunMoney);
        s.farmInput=0;return false;
    }
    if(b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL)
    {
        // A falling Sun may overlap a real packet before the queued mouse
        // event is processed. Recover exactly as a player cancels that cursor.
        if(!s.sunAt||GetTickCount()-s.sunAt>=250)
        {s.sunAt=GetTickCount();Click(a,float(b->mX+a->mDDInterface->mWideScreenOffsetX+500),float(b->mY+a->mDDInterface->mWideScreenOffsetY+500),SDL_BUTTON_RIGHT);}
        return false;
    }
    if(CollectSun(a))return false;
    int sunflowers=0;for(int row=0;row<4;++row)for(int col=0;col<2;++col)if(At(b,col,row,1))++sunflowers;
    auto pick=[&](int kind,int col,int row,int result)
    {
        auto* packet=Packet(b,0,uint16_t(kind),false);
        if(!packet||!packet->CanPickUp()||b->mSunMoney<b->GetSeedPacketCost(*packet)+200||b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL)return false;
        auto* bank=b->mSeedBank;
        if(!BoardClick(a,float(bank->mX+packet->mX+packet->mOffsetX+25),float(bank->mY+packet->mY+35)))return false;
        s.farmKind=kind;s.farmCol=col;s.farmRow=row;s.farmResult=result;s.farmInput=1;return true;
    };
    for(int row=0;row<4;++row)for(int col=0;col<2;++col)
        if(!At(b,col,row,1)&&pick(1,col,row,1))return false;
    if(sunflowers>=4)
        for(int row=0;row<4;++row)for(int col=2;col<5;++col)
            if(!At(b,col,row,1030)&&pick(0,col,row,At(b,col,row,0)?1030:0))return false;
    if(!s.diagnostic||GetTickCount()-s.diagnostic>=5000)
    {s.diagnostic=GetTickCount();NetTrace("SMOKE_SOURCE273_FARM_WAIT sun=%d producers=%d purchases=%d cursor=%d speed=%.2f",b->mSunMoney,sunflowers,s.farmPurchases,int(b->mCursorObject->mCursorType),b->GetSpeedValue(b->mSpeedMod));}
    return false;
}
bool ObserveVictory(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);if(!c||c->adventure.run.level!=1)return true;
    if(!Require(a,a->mGameScene!=SCENE_ZOMBIES_WON&&!c->adventure.schedulerFailed,"Source273 natural defense lost or scheduler failed"))return false;
    Zombie* zombie=nullptr;
    while(b->mZombies.IterateNext(zombie))
    {
        const auto* data=GetNativeSourceZombieData(zombie);
        if(!data)
        {if(!Require(a,zombie->mDead||zombie->mMindControlled,"Source273 untyped/stock enemy leaked into full source run"))return false;continue;}
        if(!Require(a,data->kind==0&&zombie->mZombieType==ZOMBIE_NORMAL&&zombie->mFromWave>=1&&zombie->mFromWave<=10&&
            zombie->mBodyMaxHealth==270&&!zombie->mMindControlled,"Source273 full run emitted a wrong source enemy"))return false;
        const auto [found,added]=s.enemies.emplace(data->instanceToken,std::pair<uint16_t,uint8_t>{zombie->mNetId,uint8_t(zombie->mFromWave)});
        if(!Require(a,found->second.first==zombie->mNetId&&found->second.second==zombie->mFromWave,
            "Source273 full-generation enemy identity changed"))return false;
        if(added)++s.waveCounts[size_t(zombie->mFromWave)];
    }
    return true;
}
bool AllWaves(LawnApp* a)
{
    constexpr unsigned expected[]={0,1,3,5,6,8,10,11,13,15,33};
    if(!Require(a,s.enemies.size()==105,"Source273 did not observe all105 natural source enemies"))return false;
    for(size_t wave=1;wave<11;++wave)if(!Require(a,s.waveCounts[wave]==expected[wave],"Source273 natural wave composition/count differs"))return false;
    return true;
}
Coin* Trophy(LawnApp* a)
{
    const auto& run=GetNativeContext(a->mBoard)->adventure.run;Coin* coin=nullptr;
    while(a->mBoard->IterateCoins(coin))if(IsNativeFusionAdventureTrophy273(coin,run))return coin;
    return nullptr;
}
bool VictoryTick(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);const auto& view=c->adventure;
    if(s.stage==20)
    {
        if(!s.diagnostic||GetTickCount()-s.diagnostic>=5000)
        {s.diagnostic=GetTickCount();NetTrace("SMOKE_SOURCE273_BATTLE role=%s wave=%u/10 seen=%zu sourceEnemies=%d over=%d sun=%d",Role(a),view.wave,s.enemies.size(),a->NetIsHost()?c->adventureEnemyCount:-1,view.run.over?1:0,b->mSunMoney);}
        if(view.wave>=3&&!s.frames.contains("fusion_source273_battle"))Capture(a,"battle");
        if(!view.run.over)return true;
        if(!AllWaves(a)||!Require(a,view.wave==10&&!view.profile.completedLevels&&view.profile.frontierLevel==1&&!view.run.rewardCollected,
            "Source273 progress committed before actual trophy click"))return true;
        if(a->NetIsHost()&&!Require(a,c->adventureEnemyCount==0&&c->adventureEnemies.empty(),"Source273 last natural death did not retire source enemies"))return true;
        Advance(a,21);
    }
    if(s.stage==21)
    {
        auto* coin=Trophy(a);if(!coin||!coin->mHitGround)return true;
        s.trophy={view.run.trophyNetId,view.run.trophyToken};
        if(!Require(a,s.trophy.id&&s.trophy.token&&!view.profile.completedLevels&&!view.run.rewardCollected,
            "Source273 trophy identity/preclick progress differs"))return true;
        const auto tag="TROPHY_"+std::to_string(s.trophy.id)+"_"+std::to_string(s.trophy.token);
        if(Capture(a,"trophy")&&Barrier(a,tag))Advance(a,22);
    }
    if(s.stage==22)
    {
        if(a->NetIsClient()&&!s.trophyClicked)
        {
            auto* coin=Trophy(a);if(!coin||!coin->mHitGround||b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL)return true;
            if(BoardClick(a,coin->mPosX+coin->mWidth*.5f,coin->mPosY+coin->mHeight*.5f))
            {s.trophyClicked=true;NetTrace("SMOKE_SOURCE273_TROPHY_INPUT role=client id=%u token=%u input=ownedSDL authenticatedCommand=AdventureRewardCollect",s.trophy.id,s.trophy.token);}
            return true;
        }
        if(!view.run.rewardCollected)return true;
        if(!Require(a,view.profile.completedLevels==1&&view.profile.frontierLevel==2&&view.profile.cycle==1,
            "Source273 actual trophy click did not commit source1 exactly once"))return true;
        s.commitObserved=true;
        if(!Capture(a,"trophy_clicked"))return true;
        NetTrace("SMOKE_SOURCE273_COMMIT role=%s id=%u token=%u completed=1 frontier=2 rewardTicks=%u",Role(a),s.trophy.id,s.trophy.token,view.run.rewardTicks);
        Advance(a,23);
    }
    return true;
}
bool NextChooser(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);if(!c||c->adventure.run.level!=2)return false;
    const auto& view=c->adventure;auto* chooser=a->mSeedChooserScreen;
    if(!Require(a,s.commitObserved&&c->epoch!=s.epoch&&view.profile.completedLevels==1&&view.profile.frontierLevel==2&&
        view.profile.cycle==1&&view.run.cycle==1&&!view.run.tutorial.active&&!view.run.over&&view.wave==0&&view.maximumWaves==10&&
        c->money==3000&&b->mSunMoney==500&&a->mPlayerInfo->mLevel==s.classicLevel&&a->mPlayerInfo->mFinishedAdventure==s.classicFinished,
        "Source273 source2 transition/profile/Classic preservation differs"))return false;
    if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||c->selection.State().phase!=FusionBankPhase::Choosing)return false;
    std::set<unsigned> eligible;for(const auto& card:c->eligible)eligible.insert(unsigned(card.kind)*2+(card.isExtra?1:0));
    if(!Require(a,eligible==std::set<unsigned>{0,1,2,3,4,5}&&c->eligible.size()==6,"Source273 trophy unlock did not expose native source2 Cherry cards"))return false;
    if(!Capture(a,"next2_chooser")||!Barrier(a,"NEXT2_"+std::to_string(c->epoch)))return false;
    NetTrace("SMOKE_SOURCE273_VICTORY_COMPLETE role=%s source1Victory=1 naturalWaves=10 naturalSourceNPC=105 typedKind=0 trophyClientClick=1 profileCompleted=1 frontier=2 nextSourceChooser=2 classicUnchanged=1 sourceFullCampaign=0 actorWrites=0 clockWrites=0 seedWrites=SDLOnly",
        Role(a));a->CoopSmokeWriteResult(true,nullptr);return true;
}
}
