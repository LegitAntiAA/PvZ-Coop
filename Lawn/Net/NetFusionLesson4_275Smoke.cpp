#include "NetFusionLesson4_275Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../MessageWidget.h"
#include "../SeedPacket.h"
#include "../System/PlayerInfo.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeAdventure273.h"
#include "../Fusion/FusionNativeGloveUI.h"
#include "../Fusion/FusionBasePlantRules.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/DDInterface.h"
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
struct State
{
    int stage=0,waitingOwn=0,classicLevel=-1,classicFinished=-1,lessonStartTick=-1,preparedTick=-1;
    uint32_t started=0,since=0,chat=0,epoch=0,samples=0,introStart=0,introLast=0;
    unsigned introChecks=0,messages=0;FusionAdventureProfile273 profile;
    std::array<Ref,2> plants{};bool toolClicked=false,pickupClicked=false,placeClicked=false,pause=false;
    bool preparedLogged=false,heldLogged=false,movedLogged=false,completedLogged=false;
    std::set<std::string> frames;std::map<std::string,int> draws;
}s;
bool Enabled(const char* key){const auto* v=std::getenv(key);return v&&std::strcmp(v,"1")==0;}
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
    if(GetTickCount()-s.started>180000||GetTickCount()-s.since>60000)
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
    if(!a->mBoard||!World(a))return true;
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
        if(Capture(a,"prepared")&&IdentityProof(a,"PREPARED"))Advance(a,5);
    }
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
        if(!s.completedLogged){s.completedLogged=true;NetTrace("SMOKE_LESSON4_275_COMPLETED role=%s sourceLevel=4 tutorialStage=3 active=0 countdown=%.6f wave=%u message=%u hostMissionCleared=%d fullLevelVictory=NotExecuted",Role(a),v.countdown,unsigned(v.wave),unsigned(v.run.tutorial.message),c->adventureMissionPlantUID?0:1);}
        if(a->NetIsHost()&&!s.pause)s.pause=Escape(a);
        if(!b->mPaused)return true;
        s.samples=Samples(a);Advance(a,7);
    }
    else if(s.stage==7&&Samples(a)-s.samples>=2)
    {
        if(!b->mPaused||!Plants(a,true)||!Released(a)||!Capture(a,"moved")||!Capture(a,"complete")||!FinalProof(a)||!Barrier(a,"DONE"))return true;
        NetTrace("SMOKE_LESSON4_275_COMPLETE role=%s sourceLevel=4 nativeChooser=1 nativeReady=1 nativeIntro=1 messages=4 genuineNineSecondPreparation=1 sourceFactories=2 sharedDefault=1 clientGloveMove=1 exactFullGenerations=2 unchangedBody=1 canonicalHP=1 released=1 tutorialCompleted=1 fiveSecondCountdown=1 profileUnchanged=1 ClassicUnchanged=1 commonPausedProof=1 actorWrites=0 clockWrites=0 RNGWrites=0 source4Victory=NotExecuted dirtyCellCallbacks=NotExecuted allocatorExhaustion=NotExecuted",Role(a));
        a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
