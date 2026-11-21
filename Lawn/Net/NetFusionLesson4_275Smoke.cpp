#include "NetFusionLesson4_275Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../MessageWidget.h"
#include "../SeedPacket.h"
#include "../System/PlayerInfo.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeAdventure273.h"
#include "../Fusion/FusionNativeAdventureProgress273.h"
#include "../Fusion/FusionNativeZombies.h"
#include "../Fusion/FusionNativeDayZombie273.h"
#include "../Fusion/FusionCampaign272Rules.h"
#include "../Fusion/FusionRecipe.h"
#include "../Fusion/FusionNativeGloveUI.h"
#include "../Fusion/FusionBasePlantRules.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../SexyAppFramework/Dialog.h"
#include <algorithm>
#include <array>
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
struct Ref{uint16_t id=0;uint32_t token=0,localUID=0;ReanimationID body=REANIMATIONID_NULL;};
struct Enemy278
{uint16_t id=0,kind=0;uint8_t wave=0;uint32_t localUID=0;ReanimationID body=REANIMATIONID_NULL;unsigned poleStates=0;};
struct State
{
    int stage=0,waitingOwn=0,classicLevel=-1,classicFinished=-1,lessonStartTick=-1,preparedTick=-1;
    uint32_t started=0,since=0,chat=0,epoch=0,samples=0,introStart=0,introLast=0;
    unsigned introChecks=0,messages=0;FusionAdventureProfile273 profile;
    std::array<Ref,2> plants{};bool toolClicked=false,pickupClicked=false,placeClicked=false,pause=false;
    bool preparedLogged=false,heldLogged=false,movedLogged=false,completedLogged=false;
    int farmInput=0,farmKind=0,farmCol=0,farmRow=0,farmResult=0,farmCost=0,farmBefore=0;
    int purchases=0,spent=0,collected=0,sunBefore=0,sunValue=0,speedRequests=0,speedMode=0;
    uint32_t inputAt=0,speedAt=0,diagnostic=0,sunUID=0;
    uint16_t sunId=0;bool resume=false,commitObserved=false,trophyClicked=false,speedLogged=false,planChecked=false,sunBlockedLogged=false;
    int sunClickX=0,sunClickY=0;
    Ref trophy;std::map<uint32_t,Enemy278> enemies;
    std::set<std::string> frames;std::map<std::string,int> draws;
}s;
bool Enabled(const char* key){const auto* v=std::getenv(key);return v&&std::strcmp(v,"1")==0;}
bool Victory(){return Enabled("PVZ_COOP_SMOKE_FUSION_LESSON4_VICTORY278");}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool okay,const char* why){if(!okay)a->CoopSmokeFail(why);return okay;}
void Advance(LawnApp* a,int stage)
{s.stage=stage;s.since=GetTickCount();NetTrace("SMOKE_LESSON4_275_STAGE role=%s stage=%d epoch=%u",Role(a),stage,s.epoch);}
bool SameProfile(const FusionAdventureProfile273& x,const FusionAdventureProfile273& y)
{return x.completedLevels==y.completedLevels&&x.cycle==y.cycle&&x.frontierLevel==y.frontierLevel;}
bool Has(LawnApp* a,int slot,const std::string& value)
{for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==slot&&line.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value)
{
    if(!Require(a,value.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Lesson4 identity proof exceeds native chat limit"))return false;
    if(Has(a,Slot(a),value))return true;
    if(s.chat&&GetTickCount()-s.chat<850)return false;
    s.chat=GetTickCount();a->mNetManager->SendChat(value);return Has(a,Slot(a),value);
}
bool Barrier(LawnApp* a,const std::string& label)
{const auto tag="L4275_"+std::to_string(s.epoch)+"_"+label;Send(a,tag);return Has(a,0,tag)&&Has(a,1,tag);}
bool Click(LawnApp* a,float x,float y,bool click=true)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    if(!click)return Require(a,SDL_PushEvent(&m),"Lesson4 owned SDL hover failed");
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Lesson4 owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Cell(LawnApp* a,int column,int row)
{return BoardClick(a,float(a->mBoard->GetCellCenterX(column,row)),float(a->mBoard->GridToPixelY(column,row)+50));}
bool Escape(LawnApp* a)
{
    SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;
    SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;
    return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Lesson4 native pause key failed");
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_lesson4_275_"+std::string(suffix);if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requested=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requested))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Lesson4 fresh owned BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_LESSON4_275_RENDER role=%s frame=%s requestedDraw=%d capturedDraw=%d",Role(a),name.c_str(),requested,a->mCoopSmokeFrameDrawCount);return true;
}
bool World(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);if(!c)return false;
    const auto& v=c->adventure;
    if(!Require(a,IsNativeSourceAdventure273(b)&&!c->validation&&!a->mFusionRuntime&&v.run.level==4&&v.run.cycle==1&&
        v.sourceDifficulty==2&&!v.run.replay&&!v.run.over&&!v.schedulerFailed&&v.wave==0&&v.maximumWaves==20&&
        v.profile.completedLevels==7&&v.profile.cycle==1&&v.profile.frontierLevel==4&&
        b->mBackground==BACKGROUND_1_DAY&&b->GetColumnCount()==10&&GetNativeFusionRowCount(b)==5&&c->money==3000&&
        a->mPlayerInfo&&a->mPlayerInfo->mLevel==s.classicLevel&&a->mPlayerInfo->mFinishedAdventure==s.classicFinished&&
        SameProfile(a->mNetManager->mConfig.mFusionSourceProfile,s.profile),"Lesson4 genuine source4 world/profile/Classic preservation differs"))return false;
    for(int row=0;row<5;++row)if(!Require(a,b->mPlantRow[row]==PLANTROW_NORMAL,"Lesson4 non-Day row leaked"))return false;
    if(!s.epoch)s.epoch=c->epoch;
    return Require(a,c->epoch==s.epoch,"Lesson4 source epoch changed");
}
bool Choose(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;
    if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return false;
    const auto& bank=c->selection.State();const int slot=Slot(a);
    const size_t target=(std::min)(kNativeFusionBankCapacity,c->eligible.size());
    if(!Require(a,bank.activeMask==3&&bank.targetCount==target&&bank.quotas[0]+bank.quotas[1]==target&&
        bank.quotas[0]>=2&&bank.quotas[1]>=2,"Lesson4 native chooser quotas differ"))return false;
    std::vector<int> choices;std::set<uint16_t> kinds;
    for(size_t i=0;i<c->eligible.size();++i)
    {
        const auto& e=c->eligible[i];if(!Require(a,IsNativeBasePlantKind(e.kind)&&GetNativeSeedForPlant(e.kind)!=SEED_NONE,
            "Lesson4 chooser contains an unmapped/non-base identity"))return false;
        kinds.insert(e.kind);if(e.isExtra==(slot==1))choices.push_back(int(i));
    }
    if(!Require(a,c->eligible.size()==2*kinds.size()&&choices.size()>=bank.quotas[slot],"Lesson4 local chooser identities missing"))return false;
    int own=0;for(const auto& packet:bank.packets)if(packet.owner==slot)
    {
        if(!Require(a,packet.isExtra==(slot==1)&&kinds.contains(packet.kind),"Lesson4 chosen owner/source identity differs"))return false;
        ++own;
    }
    if(own<s.waitingOwn)return false;
    if(own<bank.quotas[slot])
    {
        int x=0,y=0;chooser->GetSeedPositionInChooser(choices[size_t(own)],x,y);
        if(std::abs(chooser->mScrollAmount)>.05f)return false;
        if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waitingOwn=own+1;
        return false;
    }
    if(bank.packets.size()!=target)return false;
    int rank=-1;for(int i=0;i<b->mSeedBank->mNumPackets;++i)
    {const int next=LawnNet::NetSeedBankOwnerRank(slot,b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,next>=rank,"Lesson4 native bank lost local-own-first order"))return false;rank=next;}
    return Capture(a,"chooser")&&Barrier(a,"CHOOSER");
}
bool Message(LawnApp* a)
{
    const auto& t=GetNativeContext(a->mBoard)->adventure.run.tutorial;SexyString text;unsigned bit=0;
    switch(t.message)
    {
    case FusionAdventureMessage273::None:return true;
    case FusionAdventureMessage273::GloveUnlocked:text=_S("Вы открыли перчатку — важный инструмент.");bit=1;break;
    case FusionAdventureMessage273::GloveMove:text=_S("Перчатка перемещает растения и позволяет объединять их.");bit=2;break;
    case FusionAdventureMessage273::MoveTwinFlower:text=_S("Переместите двойной подсолнух за орех.");bit=4;break;
    case FusionAdventureMessage273::GloveReady:text=_S("Вы освоили перчатку. Готовьте оборону от зомби!");bit=8;break;
    default:return Require(a,false,"Lesson4 unrelated tutorial message appeared");
    }
    if(!t.messageTicks)return true;
    const auto* advice=a->mBoard->mAdvice;
    const bool matches=advice&&advice->mLabel==text&&advice->mDuration>0&&
        advice->mDisplayTime==FusionAdventureMessageDuration273(t.message);
    if(!matches)NetTrace("SMOKE_LESSON4_275_ADVICE_FAILED role=%s fixtureStage=%d snapshot=%u appTick=%d boardTick=%d epoch=%u tutorialStage=%u message=%u active=%d sourceMessageTicks=%u sourceTimerTicks=%u expectedDisplay=%u advicePresent=%d labelEqual=%d expectedBytes=%zu actualBytes=%zu duration=%d displayTime=%d style=%d expectedLabel=%s actualLabel=%s",
        Role(a),s.stage,Samples(a),a->mAppCounter,a->mBoard->mMainCounter,GetNativeContext(a->mBoard)->epoch,unsigned(t.stage),unsigned(t.message),t.active?1:0,
        unsigned(t.messageTicks),unsigned(t.timerTicks),unsigned(FusionAdventureMessageDuration273(t.message)),advice?1:0,advice&&advice->mLabel==text?1:0,
        text.size(),advice?advice->mLabel.size():size_t(0),advice?advice->mDuration:-1,advice?advice->mDisplayTime:-1,advice?int(advice->mMessageStyle):-1,
        text.c_str(),advice?advice->mLabel.c_str():"<absent>");
    if(!Require(a,matches,"Lesson4 native Advice differs from replicated source lesson"))return false;
    if(!(s.messages&bit))NetTrace("SMOKE_LESSON4_275_MESSAGE role=%s stage=%u message=%u ticks=%u timer=%u boardTick=%d nativeAdvice=1",
        Role(a),unsigned(t.stage),unsigned(t.message),unsigned(t.messageTicks),unsigned(t.timerTicks),a->mBoard->mMainCounter);
    s.messages|=bit;
    if(bit==1&&s.lessonStartTick<0)s.lessonStartTick=a->mBoard->mMainCounter;
    return true;
}
Plant* Resolve(Board* b,const Ref& ref)
{
    Plant* p=nullptr;while(b->IteratePlants(p))if(p->mNetId==ref.id)
    {const auto* d=GetNativePlantData(p);if(d&&d->instanceToken==ref.token&&b->mPlants.DataArrayGetID(p)==ref.localUID)return p;}
    return nullptr;
}
bool Plants(LawnApp* a,bool moved,bool bind=false)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);Plant* p=nullptr;int count=0;
    const uint16_t kinds[]={1033,1003};
    while(b->IteratePlants(p))
    {
        const auto* d=GetNativePlantData(p);const int i=d&&d->kind==1033?0:d&&d->kind==1003?1:-1;
        if(i<0&&Victory()&&s.stage>=5)continue; // Purchased defenses are proved separately.
        if(!Require(a,i>=0,"Lesson4 gained an unexpected plant"))return false;
        const auto* def=FusionFindPlant(kinds[i]);const int column=i?5:moved?4:9;
        if(!Require(a,def&&p->mNetId&&d->instanceToken&&!p->mDead&&p->mIsOnBoard&&!p->mSquished&&
            p->mNetGhost==a->NetIsClient()&&p->mPlantCol==column&&p->mRow==2&&p->mSeedType==GetNativeSeedForPlant(kinds[i])&&
            p->mPlantHealth==def->maxHealth&&p->mPlantMaxHealth==def->maxHealth&&d->owner==LawnNet::NET_SHARED_OWNER&&
            !d->packetId&&!d->randomOrigin&&d->firstParent==kFusionNoPlant&&d->secondParent==kFusionNoPlant&&
            a->ReanimationTryToGet(p->mBodyReanimID),"Lesson4 mission factory/provenance/HP/cell differs"))return false;
        if(bind&&!s.plants[size_t(i)].id)s.plants[size_t(i)]={p->mNetId,d->instanceToken,b->mPlants.DataArrayGetID(p),p->mBodyReanimID};
        const auto& ref=s.plants[size_t(i)];
        if(!Require(a,ref.id==p->mNetId&&ref.token==d->instanceToken&&ref.localUID==b->mPlants.DataArrayGetID(p)&&
            ref.body==p->mBodyReanimID&&Resolve(b,ref)==p,"Lesson4 replaced the original full native generation/body"))return false;
        ++count;
    }
    if(!Require(a,count==2&&!b->mZombies.mSize,"Lesson4 expected exactly two mission plants and no spawned NPC"))return false;
    if(a->NetIsHost()&&c->adventure.run.tutorial.active)
        if(!Require(a,c->adventureMissionPlantUID==s.plants[0].localUID&&c->adventureMissionPlantUID==s.plants[0].token,
            "Lesson4 host mission did not bind the exact fresh generation"))return false;
    return true;
}
bool IdentityProof(LawnApp* a,const char* label)
{
    const auto& x=s.plants[0];const auto& y=s.plants[1];
    return Barrier(a,std::string(label)+"_"+std::to_string(x.id)+"_"+std::to_string(x.token)+"_"+std::to_string(y.id)+"_"+std::to_string(y.token));
}
bool Held(LawnApp* a)
{
    const auto* c=GetNativeContext(a->mBoard);const auto& h=c->gloveHolds[1];const auto& empty=c->gloveHolds[0];
    if(h.plantId!=s.plants[0].id||h.instanceToken!=s.plants[0].token)return false;
    if(!Require(a,!empty.plantId&&!empty.instanceToken&&GetNativeGloveHeldPlant(a->mBoard,1)==Resolve(a->mBoard,s.plants[0]),
        "Lesson4 canonical glove bound a different actor"))return false;
    if(!a->NetIsClient())return true;
    // FNS adoption follows the current frame's cursor synchronization. Like
    // the existing native Glove smoke, observe the next real UI update.
    if(a->mBoard->mCursorObject->mCursorType==CURSOR_TYPE_GLOVE)return false;
    return Require(a,a->mBoard->mCursorObject->mCursorType==CURSOR_TYPE_PLANT_FROM_GLOVE&&
        uint32_t(a->mBoard->mCursorObject->mGlovePlantID)==s.plants[0].localUID,
        "Lesson4 client native glove preview bound a different actor");
}
bool Released(LawnApp* a)
{
    const auto* c=GetNativeContext(a->mBoard);
    for(int slot=0;slot<2;++slot)if(c->gloveHolds[slot].plantId||c->gloveHolds[slot].instanceToken||GetNativeGloveHeldPlant(a->mBoard,slot))return false;
    return !a->NetIsClient()||a->mBoard->mCursorObject->mCursorType!=CURSOR_TYPE_PLANT_FROM_GLOVE;
}
bool FinalProof(LawnApp* a)
{
    const auto* c=GetNativeContext(a->mBoard);LawnNet::NetBuffer bytes;
    for(const auto& ref:s.plants)
    {
        const auto* p=Resolve(a->mBoard,ref);const auto* d=GetNativePlantData(p);if(!p||!d)return false;
        bytes.WriteU16(ref.id);bytes.WriteU32(ref.token);bytes.WriteU16(d->kind);bytes.WriteI32(p->mPlantHealth);
        bytes.WriteI32(p->mPlantMaxHealth);bytes.WriteU8(uint8_t(p->mPlantCol));bytes.WriteU8(uint8_t(p->mRow));
        bytes.WriteU8(d->owner);bytes.WriteU32(d->packetId);bytes.WriteU16(d->firstParent);bytes.WriteU16(d->secondParent);
    }
    if(!EncodeFusionAdventureSession273(c->adventure,bytes))return Require(a,false,"Lesson4 final source session not encodable");
    uint32_t hash=2166136261U;for(size_t i=0;i<bytes.Size();++i){hash^=bytes.Data()[i];hash*=16777619U;}
    char label[64];sprintf_s(label,"FINAL_%08x",hash);return Barrier(a,label);
}
Plant* At278(Board* b,int col,int row,uint16_t kind)
{Plant* p=nullptr;while(b->IteratePlants(p))if(p->mPlantCol==col&&p->mRow==row)if(const auto* d=GetNativePlantData(p);d&&d->kind==kind&&!p->mDead)return p;return nullptr;}
SeedPacket* Packet278(Board* b,uint16_t kind)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];if(const auto* d=GetNativePacketData(p);d&&d->owner==0&&d->kind==kind&&!d->isExtra)return p;}return nullptr;}
struct Cell278{int col,row;uint16_t kind;};
std::vector<Cell278> Defenses278()
{
    std::vector<Cell278> cells;
    for(int row=0;row<4;++row)for(int col=0;col<2;++col)cells.push_back({col,row,1});
    for(int row=0;row<5;++row)cells.push_back({7,row,3});
    for(int row=0;row<5;++row)for(int col:{2,3,4,6})if(row!=2||col!=4)cells.push_back({col,row,1030});
    cells.push_back({5,4,1030}); // Keep mission9,2, CherryNut5,2 and glove destination4,2 free.
    return cells;
}
bool Speed278(LawnApp* a)
{
    auto* b=a->mBoard;
    const auto policyPrefix="L4275_"+std::to_string(s.epoch)+"_SPEED_POLICY_";
    if(!s.speedMode)
    {
        if(a->NetIsHost())
        {
            if(!Require(a,b->GetSpeedValue(b->mSpeedMod)==1.f,"Lesson4 victory initial host speed differs before policy negotiation"))return false;
            s.speedMode=b->mAllowSpeedMod?1:2;
        }
        else if(Has(a,0,policyPrefix+"HOST2500_CLIENT1000"))s.speedMode=1;
        else if(Has(a,0,policyPrefix+"HOST1000_CLIENT1000"))s.speedMode=2;
        else return false;
    }
    const char* policy=s.speedMode==1?"HOST2500_CLIENT1000":"HOST1000_CLIENT1000";
    // Agree the host's capability before any key. A snapshot client deliberately
    // has no local gameplay acceleration; its measured native speed stays1x.
    if(!Barrier(a,std::string("SPEED_POLICY_")+policy))return false;
    if(a->NetIsHost()&&s.speedMode==1&&b->GetSpeedValue(b->mSpeedMod)<2.5f&&s.speedRequests<3)
    {
        if(s.speedAt&&GetTickCount()-s.speedAt<300)return false;
        SDL_Event event{};event.type=SDL_EVENT_TEXT_INPUT;event.text.windowID=SDL_GetWindowID(a->mSDLWindow);event.text.text="e";
        if(Require(a,SDL_PushEvent(&event),"Lesson4 victory native speed input failed")){++s.speedRequests;s.speedAt=GetTickCount();}
        return false;
    }
    const float speed=b->GetSpeedValue(b->mSpeedMod);
    const float expected=a->NetIsHost()&&s.speedMode==1?2.5f:1.f;
    if(!Require(a,speed==expected&&(a->NetIsHost()||!b->mAllowSpeedMod),"Lesson4 victory measured role-specific native speed/capability differs"))return false;
    if(!s.speedLogged)
    {s.speedLogged=true;NetTrace("SMOKE_LESSON4_VICTORY278_SPEED role=%s actual=%.3f allowSpeedMod=%d nativeOwnedText=e requests=%d hostAccelerationOnly=%d clientGhostPresentation=1 speedWireSync=NotPresent policy=%s directSetter=0",Role(a),speed,b->mAllowSpeedMod?1:0,s.speedRequests,s.speedMode==1?1:0,policy);}
    return Barrier(a,policy);
}
bool Sun278(LawnApp* a)
{
    if(!a->NetIsHost())return false;auto* b=a->mBoard;
    if(s.sunId)
    {
        Coin* coin=nullptr;Coin* candidate=nullptr;
        while(b->mCoins.IterateNext(candidate))if(candidate->mNetId==s.sunId&&b->mCoins.DataArrayGetID(candidate)==s.sunUID){coin=candidate;break;}
        if(b->mSunMoney==s.sunBefore+s.sunValue)
        {
            s.collected+=s.sunValue;
            NetTrace("SMOKE_LESSON4_VICTORY278_SUN id=%u localUID=%u value=%d before=%d after=%d collected=%d actualNativeScore=1 input=SDL",s.sunId,s.sunUID,s.sunValue,s.sunBefore,b->mSunMoney,s.collected);
            s.sunId=0;s.sunUID=0;return true;
        }
        if(!Require(a,b->mSunMoney==s.sunBefore,"Lesson4 victory Sun ledger changed by an unaccounted input"))return true;
        if(GetTickCount()-s.inputAt>5000)
        {
            if(coin&&!coin->mIsBeingCollected){s.sunId=0;s.sunUID=0;return true;}
            Require(a,false,"Lesson4 victory selected native Sun never scored its real value");
        }
        return true;
    }
    if(b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL)return false;
    Coin* coin=nullptr;while(b->IterateCoins(coin))if(coin->IsSun()&&!coin->mIsBeingCollected&&coin->mHitGround)
    {
        const int x=int(coin->mPosX+coin->mWidth*.5f),y=int(coin->mPosY+coin->mHeight*.5f);HitResult hit;
        if(!b->MouseHitTest(x,y,&hit)||hit.mObject!=coin)continue;
        const int screenX=x+b->mX+a->mDDInterface->mWideScreenOffsetX,screenY=y+b->mY+a->mDDInterface->mWideScreenOffsetY;
        auto* widget=a->mWidgetManager->GetWidgetAt(screenX,screenY,nullptr,nullptr);
        if(widget!=b)
        {
            if(!s.sunBlockedLogged){s.sunBlockedLogged=true;NetTrace("SMOKE_LESSON4_VICTORY278_SUN_OCCLUDED id=%u screen=%d,%d pause=%d slowdown=%d speedup=%d skipped=1 realWidgetRouting=1",coin->mNetId,screenX,screenY,widget==b->mPauseButton?1:0,widget==b->mSlowdownButton?1:0,widget==b->mSpeedupButton?1:0);}
            continue;
        }
        if(BoardClick(a,float(x),float(y)))
        {s.sunId=coin->mNetId;s.sunUID=b->mCoins.DataArrayGetID(coin);s.sunValue=coin->GetSunValue();s.sunBefore=b->mSunMoney;s.inputAt=GetTickCount();s.sunClickX=screenX;s.sunClickY=screenY;}
        return true;
    }
    return false;
}
bool DefenseProof278(LawnApp* a,LawnNet::NetBuffer& proof)
{
    for(const auto& cell:Defenses278())
    {
        auto* p=At278(a->mBoard,cell.col,cell.row,cell.kind);const auto* d=GetNativePlantData(p);if(!p||!d)return false;
        const auto* def=FusionFindPlant(cell.kind);
        if(!Require(a,def&&p->mPlantHealth==def->maxHealth&&p->mPlantMaxHealth==def->maxHealth&&d->owner==0&&
            d->firstParent==(cell.kind==1030?uint16_t(0):kFusionNoPlant)&&d->secondParent==(cell.kind==1030?uint16_t(0):kFusionNoPlant)&&
            p->mNetId&&d->instanceToken&&a->ReanimationTryToGet(p->mBodyReanimID),"Lesson4 victory purchased defense ancestry/owner/HP/body differs"))return false;
        proof.WriteU16(p->mNetId);proof.WriteU32(d->instanceToken);proof.WriteU16(d->kind);proof.WriteU8(uint8_t(cell.col));proof.WriteU8(uint8_t(cell.row));
    }
    return true;
}
bool Farm278(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);
    if(!Require(a,c->adventure.run.tutorial.active&&c->adventure.run.tutorial.stage==3&&c->adventure.wave==0&&
        !c->adventure.run.over&&!c->adventure.run.rewardCollected&&!b->mZombies.mSize,
        "Lesson4 victory farm lost the genuine MoveTwinFlower wave gate")||!Plants(a,false))return false;
    if(!Speed278(a))return false;
    if(!s.farmInput&&!s.sunId)
    {
        LawnNet::NetBuffer proof;
        if(DefenseProof278(a,proof))
        {
            if(a->NetIsHost()&&!Require(a,s.purchases==53&&s.spent==4650&&b->mSunMoney==500+s.collected-s.spent,
                "Lesson4 victory defense purchase count/cost/Sun ledger differs"))return false;
            char tag[40];sprintf_s(tag,"DEFENSES_%08x",LawnNet::NetFnv1a32(proof.Data(),proof.Size()));
            if(!Capture(a,"defenses")||!Barrier(a,tag))return false;
            NetTrace("SMOKE_LESSON4_VICTORY278_DEFENSES role=%s sunflowers=8 doubleShooters=20 wallNuts=5 purchasedPlants=33 purchases=%d spent=%d collected=%d sun=%d wave=0 tutorial=3 missionReserved=1 input=SDL actorWrites=0 balanceWrites=0",
                Role(a),s.purchases,s.spent,s.collected,b->mSunMoney);return true;
        }
    }
    if(!a->NetIsHost())return false;
    if(s.farmInput==1)
    {
        auto* packet=Packet278(b,uint16_t(s.farmKind));auto* cursor=b->mCursorObject;
        if(!packet||cursor->mCursorType!=CURSOR_TYPE_PLANT_FROM_BANK||cursor->mSeedBankIndex<0||cursor->mSeedBankIndex>=b->mSeedBank->mNumPackets||
            b->mSeedBank->mSeedPackets[cursor->mSeedBankIndex].mNetId!=packet->mNetId)
        {if(cursor->mCursorType==CURSOR_TYPE_NORMAL&&GetTickCount()-s.inputAt>1500)s.farmInput=0;return false;}
        if(Cell(a,s.farmCol,s.farmRow)){s.farmInput=2;s.inputAt=GetTickCount();}return false;
    }
    if(s.farmInput==2)
    {
        auto* p=At278(b,s.farmCol,s.farmRow,uint16_t(s.farmResult));const auto* d=GetNativePlantData(p);if(!p||!d)return false;
        if(!Require(a,d->owner==0&&p->mPlantHealth==p->mPlantMaxHealth&&b->mSunMoney==s.farmBefore-s.farmCost,
            "Lesson4 victory real purchase ownership/HP/Sun debit differs"))return false;
        ++s.purchases;s.spent+=s.farmCost;
        NetTrace("SMOKE_LESSON4_VICTORY278_BUY purchase=%d used=%d result=%u id=%u token=%u packetCost=%d before=%d after=%d col=%d row=%d input=SDL",s.purchases,s.farmKind,d->kind,p->mNetId,d->instanceToken,s.farmCost,s.farmBefore,b->mSunMoney,s.farmCol,s.farmRow);
        s.farmInput=0;return false;
    }
    if(Sun278(a))return false;
    if(!Require(a,b->mSunMoney==500+s.collected-s.spent,"Lesson4 victory unexplained balance outside a pending native purchase/Sun"))return false;
    if(b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL)return false;
    for(const auto& cell:Defenses278())
    {
        if(At278(b,cell.col,cell.row,cell.kind))continue;
        const uint16_t used=cell.kind==1030?uint16_t(0):cell.kind;
        const uint16_t result=cell.kind==1030&&!At278(b,cell.col,cell.row,0)?uint16_t(0):cell.kind;
        auto* packet=Packet278(b,used);if(!packet||!packet->CanPickUp())continue;
        const int cost=b->GetSeedPacketCost(*packet);if(b->mSunMoney<cost)continue;
        if(cell.kind==1030&&result==1030)
        {const auto* recipe=FindFusionRecipe(0,0);if(!Require(a,recipe&&recipe->result==1030,"Lesson4 victory real DoubleShooter recipe changed"))return false;}
        const int x=b->mSeedBank->mX+packet->mX+packet->mOffsetX+25,y=b->mSeedBank->mY+packet->mY+35;HitResult hit;
        if(!b->MouseHitTest(x,y,&hit)||hit.mObject!=packet)continue;
        if(BoardClick(a,float(x),float(y)))
        {s.farmKind=used;s.farmCol=cell.col;s.farmRow=cell.row;s.farmResult=result;s.farmCost=cost;s.farmBefore=b->mSunMoney;s.farmInput=1;s.inputAt=GetTickCount();}
        return false;
    }
    if(!s.diagnostic||GetTickCount()-s.diagnostic>=5000)
    {s.diagnostic=GetTickCount();NetTrace("SMOKE_LESSON4_VICTORY278_FARM_WAIT role=%s purchases=%d spent=%d collected=%d sun=%d cursor=%d elapsed=%u",Role(a),s.purchases,s.spent,s.collected,b->mSunMoney,int(b->mCursorObject->mCursorType),GetTickCount()-s.since);}
    return false;
}
bool Observe278(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);const auto& v=c->adventure;
    if(!Require(a,IsNativeSourceAdventure273(b)&&!c->validation&&!a->mFusionRuntime&&c->epoch==s.epoch&&
        v.run.level==4&&v.run.cycle==1&&v.sourceDifficulty==2&&!v.run.replay&&!v.schedulerFailed&&v.maximumWaves==20&&v.wave<=20&&
        b->GetColumnCount()==10&&GetNativeFusionRowCount(b)==5&&b->mBackground==BACKGROUND_1_DAY&&
        a->mGameScene!=SCENE_ZOMBIES_WON&&a->mPlayerInfo->mLevel==s.classicLevel&&a->mPlayerInfo->mFinishedAdventure==s.classicFinished,
        "Lesson4 victory natural battle lost/source world/Classic identity differs"))return false;
    const uint64_t expectedMask=v.run.rewardCollected?15:7;const int expectedFrontier=v.run.rewardCollected?5:4;
    if(!Require(a,v.profile.completedLevels==expectedMask&&v.profile.frontierLevel==expectedFrontier&&v.profile.cycle==1&&
        (a->NetIsHost()?SameProfile(a->mNetManager->mConfig.mFusionSourceProfile,v.profile):SameProfile(a->mNetManager->mConfig.mFusionSourceProfile,s.profile)),
        "Lesson4 victory source progress committed early or client local profile changed"))return false;
    if(a->NetIsHost())
    {
        const auto& scheduler=c->adventureScheduler;
        if(!Require(a,scheduler.initialized&&!scheduler.failed&&scheduler.waves.size()==20,"Lesson4 victory actual host wave plan missing"))return false;
        if(!s.planChecked)
        {
            for(const auto& wave:scheduler.waves)for(auto kind:wave)if(!Require(a,FusionSourceAdventureAllowsZombie272(4,kind),"Lesson4 victory plan includes a non-source4 identity"))return false;
            s.planChecked=true;
        }
        if(v.wave<20||c->adventureEnemyCount>0)
            if(!Require(a,!v.run.over&&!v.run.trophyNetId&&!v.run.rewardCollected,"Lesson4 victory trophy/over appeared with real source enemies or incomplete waves"))return false;
    }
    Zombie* z=nullptr;while(b->mZombies.IterateNext(z))
    {
        const auto* d=GetNativeSourceZombieData(z);
        if(!d){if(!Require(a,z->mDead,"Lesson4 victory untyped live native zombie leaked"))return false;continue;}
        const auto* profile=FindFusionZombieProfile(d->kind);
        if(!Require(a,profile&&FusionSourceAdventureAllowsZombie272(4,d->kind)&&!z->mMindControlled&&z->mNetId&&d->instanceToken&&
            z->mZombieType==GetNativeSourceZombieCarrier(d->kind)&&z->mBodyMaxHealth==profile->maxHealth&&z->mFromWave>=1&&z->mFromWave<=20&&
            (z->mDead||a->ReanimationTryToGet(z->mBodyReanimID)),"Lesson4 victory actual typed kind/carrier/HP/body/wave differs"))return false;
        const Enemy278 fresh{z->mNetId,d->kind,uint8_t(z->mFromWave),b->mZombies.DataArrayGetID(z),z->mBodyReanimID,0};
        auto [it,added]=s.enemies.emplace(d->instanceToken,fresh);auto& old=it->second;
        if(!Require(a,old.id==fresh.id&&old.kind==fresh.kind&&old.wave==fresh.wave&&old.localUID==fresh.localUID&&old.body==fresh.body,
            "Lesson4 victory full source generation changed native identity/body"))return false;
        if(d->kind==3&&!z->mDead)
        {
            const auto* day=GetNativeDayZombie273(z);
            if(!Require(a,day&&NativeDayZombie273ControllerValid(3,*day),"Lesson4 victory Pole has no real validated source controller"))return false;
            old.poleStates|=1u<<day->status;
        }
        if(added)NetTrace("SMOKE_LESSON4_VICTORY278_ENEMY role=%s id=%u token=%u localUID=%u kind=%u wave=%u maxHP=%d nativeBody=%u sourceCounter=%d",Role(a),old.id,d->instanceToken,old.localUID,old.kind,old.wave,z->mBodyMaxHealth,unsigned(old.body),a->NetIsHost()?c->adventureEnemyCount:-1);
    }
    return true;
}
bool AllWaves278(LawnApp* a)
{
    const auto* c=GetNativeContext(a->mBoard);LawnNet::NetBuffer proof;unsigned poles=0,poleStates=0;
    for(const auto& [token,e]:s.enemies)
    {proof.WriteU32(token);proof.WriteU16(e.id);proof.WriteU16(e.kind);proof.WriteU8(e.wave);if(e.kind==3){++poles;poleStates|=e.poleStates;}}
    if(!Require(a,poles>0,"Lesson4 victory did not observe a genuine sourcePole3"))return false;
    for(int wave=1;wave<=20;++wave)
    {
        std::map<uint16_t,unsigned> actual,expected;for(const auto& [token,e]:s.enemies)if(e.wave==wave)++actual[e.kind];
        if(!Require(a,!actual.empty(),"Lesson4 victory missed an entire natural wave"))return false;
        if(a->NetIsHost())
        {
            for(auto kind:c->adventureScheduler.waves[size_t(wave-1)])++expected[kind];
            if(!Require(a,actual==expected,"Lesson4 victory actual native wave multiset differs from source RNG plan"))return false;
        }
    }
    char tag[60];sprintf_s(tag,"ALL20_%zu_%08x",s.enemies.size(),LawnNet::NetFnv1a32(proof.Data(),proof.Size()));
    if(!Barrier(a,tag))return false;
    NetTrace("SMOKE_LESSON4_VICTORY278_ALLWAVES role=%s naturalWaves=20 naturalSourceNPC=%zu kinds=0,2,3 exactHostPlan=1 fullPeerIdentityDigest=1 poles=%u observedPoleStatusBits=%u forcedWaves=0",Role(a),s.enemies.size(),poles,poleStates);return true;
}
Coin* Trophy278(LawnApp* a)
{Coin* coin=nullptr;const auto& run=GetNativeContext(a->mBoard)->adventure.run;while(a->mBoard->IterateCoins(coin))if(IsNativeFusionAdventureTrophy273(coin,run))return coin;return nullptr;}
bool Next5_278(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);const auto& v=c->adventure;auto* chooser=a->mSeedChooserScreen;
    if(!Require(a,s.commitObserved&&c->epoch!=s.epoch&&v.run.level==5&&v.run.cycle==1&&v.profile.completedLevels==15&&v.profile.frontierLevel==5&&v.profile.cycle==1&&
        !v.run.over&&!v.run.rewardCollected&&!v.run.tutorial.active&&v.wave==0&&v.maximumWaves==20&&v.sourceDifficulty==2&&
        b->mSunMoney==500&&c->money==3000&&a->mPlayerInfo->mLevel==s.classicLevel&&a->mPlayerInfo->mFinishedAdventure==s.classicFinished&&
        (a->NetIsHost()?SameProfile(a->mNetManager->mConfig.mFusionSourceProfile,v.profile):SameProfile(a->mNetManager->mConfig.mFusionSourceProfile,s.profile)),
        "Lesson4 victory next5/source profile/Classic preservation differs"))return false;
    if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||c->selection.State().phase!=FusionBankPhase::Choosing)return false;
    std::set<unsigned> eligible;for(const auto& card:c->eligible)eligible.insert(unsigned(card.kind)*2+(card.isExtra?1:0));
    if(!Require(a,c->eligible.size()==10&&eligible==std::set<unsigned>{0,1,2,3,4,5,6,7,8,9},"Lesson4 victory real unlock did not expose source5 SnowPea native cards"))return false;
    if(!Capture(a,"next5_chooser")||!Barrier(a,"NEXT5_"+std::to_string(c->epoch)))return false;
    NetTrace("SMOKE_LESSON4_VICTORY278_COMPLETE role=%s source4Victory=1 naturalWaves=20 naturalSourceNPC=%zu typedKinds=0,2,3 trophyClientClick=1 earnedCompletedMask=15 frontier=5 nextSourceChooser=5 ClassicUnchanged=1 clientLocalProfileUnchanged=1 purchasesHost=53 directActorWrites=0 clockWrites=0 RNGWrites=0 balanceWrites=0 source5FullVictory=NotExecuted source6FinalDoll=NotExecuted",Role(a),s.enemies.size());
    a->CoopSmokeWriteResult(true,nullptr);return true;
}
bool Victory278(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);if(!c)return true;
    if(c->adventure.run.level==5){Next5_278(a);return true;}
    if(!Observe278(a))return true;const auto& v=c->adventure;
    if(s.stage==17)
    {
        if(!Barrier(a,"RESUME_READY"))return true;
        if(a->NetIsHost()&&b->mPaused&&!s.resume)s.resume=Escape(a);
        if(!b->mPaused){s.samples=Samples(a);Advance(a,20);}return true;
    }
    if(s.stage==20)
    {
        if(b->mPaused||a->GetDialogCount())
        {
            NetTrace("SMOKE_LESSON4_VICTORY278_UNEXPECTED_PAUSE role=%s paused=%d dialogs=%d pendingSun=%u lastSunClick=%d,%d",Role(a),b->mPaused?1:0,a->GetDialogCount(),s.sunId,s.sunClickX,s.sunClickY);
            for(auto* dialog:a->mDialogList)NetTrace("SMOKE_LESSON4_VICTORY278_DIALOG role=%s id=%d",Role(a),dialog->mId);
            Require(a,false,"Lesson4 victory natural battle unexpectedly paused or opened a modal");return true;
        }
        if(a->NetIsHost()&&!v.run.over)Sun278(a);
        if(v.wave>=3&&!s.frames.contains("fusion_lesson4_275_battle"))Capture(a,"battle");
        if(v.wave==20&&!s.frames.contains("fusion_lesson4_275_finalwave"))Capture(a,"finalwave");
        if(!s.diagnostic||GetTickCount()-s.diagnostic>=5000)
        {s.diagnostic=GetTickCount();NetTrace("SMOKE_LESSON4_VICTORY278_BATTLE role=%s wave=%u/20 seen=%zu sourceEnemies=%d over=%d sun=%d elapsed=%u",Role(a),v.wave,s.enemies.size(),a->NetIsHost()?c->adventureEnemyCount:-1,v.run.over?1:0,b->mSunMoney,GetTickCount()-s.since);}
        if(!v.run.over)return true;
        if(!Require(a,v.wave==20&&!v.run.rewardCollected&&(!a->NetIsHost()||(c->adventureEnemyCount==0&&c->adventureEnemies.empty())),
            "Lesson4 victory ended before all20 waves/real source retirements")||!AllWaves278(a))return true;
        Advance(a,21);
    }
    if(s.stage==21)
    {
        auto* coin=Trophy278(a);if(!coin||!coin->mHitGround)return true;
        s.trophy={v.run.trophyNetId,v.run.trophyToken};
        if(!Require(a,s.trophy.id&&s.trophy.token&&!v.run.rewardCollected&&v.profile.completedLevels==7,
            "Lesson4 victory actual trophy identity/preclick progress differs"))return true;
        if(Capture(a,"trophy")&&Barrier(a,"TROPHY_"+std::to_string(s.trophy.id)+"_"+std::to_string(s.trophy.token)))Advance(a,22);
    }
    if(s.stage==22)
    {
        if(a->NetIsClient()&&!s.trophyClicked)
        {
            auto* coin=Trophy278(a);if(!coin||!coin->mHitGround||b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL)return true;
            const int x=int(coin->mPosX+coin->mWidth*.5f),y=int(coin->mPosY+coin->mHeight*.5f);HitResult hit;
            if(!coin->MouseHitTest(x,y,&hit)||hit.mObject!=coin)return true;
            if(BoardClick(a,float(x),float(y)))
            {s.trophyClicked=true;NetTrace("SMOKE_LESSON4_VICTORY278_TROPHY_INPUT role=client id=%u token=%u input=ownedSDL authenticatedCommand=AdventureRewardCollect",s.trophy.id,s.trophy.token);}return true;
        }
        if(!v.run.rewardCollected)return true;
        if(!Require(a,v.profile.completedLevels==15&&v.profile.frontierLevel==5,"Lesson4 victory client trophy did not earn source4 exactly once"))return true;
        s.commitObserved=true;if(!Capture(a,"trophy_clicked"))return true;
        NetTrace("SMOKE_LESSON4_VICTORY278_COMMIT role=%s id=%u token=%u completedBefore=7 completedAfter=15 frontier=5 rewardTicks=%u",Role(a),s.trophy.id,s.trophy.token,v.run.rewardTicks);Advance(a,23);
    }
    return true;
}
}
namespace LawnNet
{
bool NetSmokeUpdateFusionLesson4_275(LawnApp* a)
{
    if(!Enabled("PVZ_COOP_SMOKE_FUSION_LESSON4_275"))return false;
    if(!a||!a->CoopSmokeActive()||!a->mNetManager||a->mCoopSmokeResultWritten)return true;
    if(!Require(a,Enabled("PVZ_COOP_SMOKE_FUSION_NATIVE")&&a->mCoopSmokeManifest.mScenario==NETSMOKE_SCENARIO_FUSION_VALIDATION,
        "Lesson4 requires owned native fusion_validation"))return true;
    if(!s.started)s.started=s.since=GetTickCount();
    const uint32_t totalLimit=Victory()?(s.speedMode==2?1800000:900000):180000;
    const uint32_t stageLimit=Victory()&&(s.stage==15||s.stage==20)?(s.speedMode==2?800000:400000):60000;
    if(GetTickCount()-s.started>totalLimit||GetTickCount()-s.since>stageLimit)
    {a->CoopSmokeFail("Lesson4 actual native input deadline; inspect stage/message/mission/glove identities");return true;}
    if(!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(!s.stage)
    {
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(!Require(a,Slot(a)>=0&&Slot(a)<2&&a->mNetManager->GetDifficulty()==1&&a->mPlayerInfo,"Lesson4 requires two peers/difficulty1/local profile"))return true;
        if(s.classicLevel<0)
        {s.classicLevel=a->mPlayerInfo->mLevel;s.classicFinished=a->mPlayerInfo->mFinishedAdventure;s.profile=a->mNetManager->mConfig.mFusionSourceProfile;}
        if(a->NetIsClient()){if(Send(a,"L4275_BEGIN"))Advance(a,1);return true;}
        if(!Has(a,1,"L4275_BEGIN"))return true;
        if(!Require(a,s.profile.completedLevels==7&&s.profile.cycle==1&&s.profile.frontierLevel==4&&
            a->StartFusionSourceAdventure(4,false),"Lesson4 isolated profile/source4 production admission failed"))return true;
        NetTrace("SMOKE_LESSON4_275_BEGIN sourceLevel=4 cycle=1 completedMask=7 frontier=4 profileWrites=0 ClassicWrites=0");
        Advance(a,1);return true;
    }
    if(!a->mBoard)return true;
    if(Victory()&&(s.stage==17||s.stage>=20))return Victory278(a);
    if(!World(a))return true;
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);const auto& bank=c->selection.State();
    if(!Message(a))return true;
    if(s.stage==1){if(Choose(a))Advance(a,2);}
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* chooser=a->mSeedChooserScreen;auto* button=chooser?chooser->mStartButton:nullptr;
        if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver())Click(a,x,y,false);else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro)
        {if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;Capture(a,"opening");return true;}
        if(bank.phase!=FusionBankPhase::Playing||a->mSeedChooserScreen||a->mGameScene!=SCENE_PLAYING)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introStart>=1000&&s.frames.contains("fusion_lesson4_275_opening")&&
            !b->mPlants.mSize&&!b->mZombies.mSize&&c->adventure.run.tutorial.active,"Lesson4 skipped native intro or initial empty lesson"))return true;
        Advance(a,4);
    }
    else if(s.stage==4)
    {
        const auto& t=c->adventure.run.tutorial;if(t.stage<3)return true;
        if(!Require(a,t.active&&t.stage==3&&t.message==FusionAdventureMessage273::MoveTwinFlower&&(s.messages&7)==7,
            "Lesson4 skipped its genuine source4/5 second messages"))return true;
        if(!Plants(a,false,true))return true;
        if(s.preparedTick<0)s.preparedTick=b->mMainCounter;
        if(a->NetIsHost()&&!Require(a,s.lessonStartTick>=0&&s.preparedTick-s.lessonStartTick>=899,
            "Lesson4 preparation bypassed the source nine-second timer"))return true;
        if(!s.preparedLogged)
        {
            s.preparedLogged=true;NetTrace("SMOKE_LESSON4_275_PREPARED role=%s mission=%u token=%u localUID=%u hostMissionUID=%u other=%u otherToken=%u kind0=1033 col0=9 kind1=1003 col1=5 row=2 sourceElapsedTicks=%d sharedDefault=1 postBirthWrites=0",
                Role(a),s.plants[0].id,s.plants[0].token,s.plants[0].localUID,c->adventureMissionPlantUID,s.plants[1].id,s.plants[1].token,s.preparedTick-s.lessonStartTick);
        }
        if(Capture(a,"prepared")&&IdentityProof(a,"PREPARED"))Advance(a,Victory()?15:5);
    }
    else if(s.stage==15){if(Farm278(a))Advance(a,5);}
    else if(s.stage==5)
    {
        if(!Plants(a,false))return true;
        if(a->NetIsClient())
        {
            if(!s.toolClicked)
            {
                Sexy::Rect rect=b->GetShovelButtonRect();b->GetZenButtonRect(OBJECT_TYPE_GLOVE,rect);
                if(!Require(a,rect.mWidth>0&&rect.mHeight>0,"Lesson4 actual native glove button missing"))return true;
                s.toolClicked=BoardClick(a,float(rect.mX+rect.mWidth/2),float(rect.mY+rect.mHeight/2));return true;
            }
            if(!s.pickupClicked)
            {if(b->mCursorObject->mCursorType!=CURSOR_TYPE_GLOVE)return true;s.pickupClicked=Cell(a,9,2);return true;}
        }
        if(!Held(a))return true;
        if(!s.heldLogged){s.heldLogged=true;NetTrace("SMOKE_LESSON4_275_HELD role=%s operator=1 id=%u token=%u nativeGloveUI=1 sourceMissionExact=1",Role(a),s.plants[0].id,s.plants[0].token);}
        if(Capture(a,"held")&&IdentityProof(a,"HELD"))Advance(a,6);
    }
    else if(s.stage==6)
    {
        if(a->NetIsClient()&&!s.placeClicked){if(!Held(a))return true;s.placeClicked=Cell(a,4,2);return true;}
        auto* mission=Resolve(b,s.plants[0]);if(!Require(a,mission,"Lesson4 exact mission generation vanished during glove move"))return true;
        if(mission->mPlantCol!=4)return true;
        if(!Plants(a,true)||!Released(a))return true;
        if(!s.movedLogged){s.movedLogged=true;NetTrace("SMOKE_LESSON4_275_MOVED role=%s operator=1 id=%u token=%u localUID=%u col=4 row=2 sameGeneration=1 sameBody=1 canonicalHP=1 sharedDefault=1 released=1 nativeGloveUI=1",Role(a),s.plants[0].id,s.plants[0].token,s.plants[0].localUID);}
        const auto& v=c->adventure;if(v.run.tutorial.active)return true;
        if(!Require(a,v.run.tutorial.stage==3&&v.run.tutorial.message==FusionAdventureMessage273::GloveReady&&(s.messages&15)==15&&
            v.countdown>4.5f&&v.countdown<=5.f&&(!a->NetIsHost()||!c->adventureMissionPlantUID),"Lesson4 completion/message/five-second countdown differs"))return true;
        if(!s.completedLogged){s.completedLogged=true;NetTrace("SMOKE_LESSON4_275_COMPLETED role=%s sourceLevel=4 tutorialStage=3 active=0 countdown=%.6f wave=%u message=%u hostMissionCleared=%d fullLevelVictory=%s",Role(a),v.countdown,unsigned(v.wave),unsigned(v.run.tutorial.message),c->adventureMissionPlantUID?0:1,Victory()?"Pending":"NotExecuted");}
        if(a->NetIsHost()&&!s.pause)s.pause=Escape(a);
        if(!b->mPaused)return true;
        s.samples=Samples(a);Advance(a,7);
    }
    else if(s.stage==7&&Samples(a)-s.samples>=2)
    {
        if(!b->mPaused||!Plants(a,true)||!Released(a)||!Capture(a,"moved")||!Capture(a,"complete")||!FinalProof(a)||!Barrier(a,"DONE"))return true;
        if(Victory()){Advance(a,17);return true;}
        NetTrace("SMOKE_LESSON4_275_COMPLETE role=%s sourceLevel=4 nativeChooser=1 nativeReady=1 nativeIntro=1 messages=4 genuineNineSecondPreparation=1 sourceFactories=2 sharedDefault=1 clientGloveMove=1 exactFullGenerations=2 unchangedBody=1 canonicalHP=1 released=1 tutorialCompleted=1 fiveSecondCountdown=1 profileUnchanged=1 ClassicUnchanged=1 commonPausedProof=1 actorWrites=0 clockWrites=0 RNGWrites=0 source4Victory=NotExecuted dirtyCellCallbacks=NotExecuted allocatorExhaustion=NotExecuted",Role(a));
        a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
