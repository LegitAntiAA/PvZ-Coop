#include "NetFusionLesson78_278Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
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
#include "../Fusion/FusionNativeDroppedCards269.h"
#include "../Fusion/FusionNativeEquipment.h"
#include "../Fusion/FusionNativeZombies.h"
#include "../Fusion/FusionRecipe.h"
#include "../Fusion/FusionNativeRemainingLeaves269.h"
#include "../Fusion/FusionNativeTools268.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativeZombieVisual.h"
#include "../Fusion/FusionNativeCold.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../SexyAppFramework/Image.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
using namespace LawnFusion;using LawnNet::NetTrace;
struct Ref {uint16_t id=0;uint32_t token=0,localUID=0;};
struct State
{
    int level=7,stage=0,waiting=0,input=0,classicLevel=-1,classicFinished=-1,spent=0,lessonTick=-1;
    uint32_t started=0,caseStarted=0,since=0,chat=0,wheel=0,epoch=0,previousEpoch=0,samples=0,diagnostic=0;
    uint32_t introStart=0,introLast=0;unsigned introChecks=0,messages=0;
    bool initial=false,pause=false,resume=false,dying=false,dyingLogged=false,profileSetup=false,complete=false,preparedLogged=false,bucketTransition=false,caseLogged=false;
    Ref mission,enemy,giftResult,bucket,receiver,iron,fallback;
    std::array<Ref,3> cards{};std::array<Ref,4> defenses{};
    int cardCount=0;uint16_t giftKind=0;FusionAdventureProfile273 localProfile;
    std::string checkpoint;std::set<std::string> frames;std::map<std::string,int> draws;
}s;
bool Enabled(const char* key){const auto* v=std::getenv(key);return v&&std::strcmp(v,"1")==0;}
bool Lesson1315(){return Enabled("PVZ_COOP_SMOKE_FUSION_LESSON1315_288");}
struct NightLesson288
{
    Ref nut,tall,chomper;std::array<Ref,2> items{};std::array<Ref,6> plants{};std::array<Ref,3> enemies{};
    std::array<bool,2> held{},used{};
    std::array<int,9> messageAt={-1,-1,-1,-1,-1,-1,-1,-1,-1};
    bool hpPreference=false,fxSeen=false,coldSeen=false,shotSeen=false,cohort=false,countdownSeen=false;
    float countdown=0;uint32_t effectUID=0;
}night;
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool valid,const char* why){if(!valid)a->CoopSmokeFail(why);return valid;}
FusionAdventureProfile273 Profile(int level){return {(uint64_t{1}<<(level-1))-1,1,uint16_t(level)};}
bool Same(const FusionAdventureProfile273& x,const FusionAdventureProfile273& y)
{return x.completedLevels==y.completedLevels&&x.cycle==y.cycle&&x.frontierLevel==y.frontierLevel;}
std::string Tag(const std::string& label){return std::string(Lesson1315()?"L1315_288_":"L78278_")+std::to_string(s.epoch)+"_"+label;}
bool Has(LawnApp* a,int slot,const std::string& value)
{for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==slot&&line.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value)
{
    if(!Require(a,value.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Lesson78 identity proof exceeds native chat limit"))return false;
    if(Has(a,Slot(a),value))return true;
    if(s.chat&&GetTickCount()-s.chat<850)return false;s.chat=GetTickCount();
    a->mNetManager->SendChat(value);return Has(a,Slot(a),value);
}
bool Barrier(LawnApp* a,const std::string& label)
{const auto tag=Tag(label);Send(a,tag);return Has(a,0,tag)&&Has(a,1,tag);}
std::string Identity(const Ref& r){return std::to_string(r.id)+"_"+std::to_string(r.token);}
void Advance(LawnApp* a,int stage)
{s.stage=stage;s.input=0;s.since=GetTickCount();NetTrace(Lesson1315()?"SMOKE_LESSON1315_288_STAGE role=%s level=%d stage=%d epoch=%u":"SMOKE_LESSON78_278_STAGE role=%s level=%d stage=%d epoch=%u",Role(a),s.level,stage,s.epoch);}
bool Click(LawnApp* a,float x,float y,bool click=true)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    if(!click)return Require(a,SDL_PushEvent(&m),"Lesson78 native SDL hover failed");
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Lesson78 native SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Cell(LawnApp* a,int col,int row)
{return BoardClick(a,float(a->mBoard->GetCellCenterX(col,row)),float(a->mBoard->GridToPixelY(col,row)+50));}
bool Escape(LawnApp* a)
{
    SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;
    SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;
    return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Lesson78 native SDL pause/resume failed");
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name=std::string(Lesson1315()?"fusion_lesson1315_288_":"fusion_lesson78_278_")+std::to_string(s.level)+"_"+suffix;if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int draw=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(draw))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Lesson78 fresh native BMP failed"))return false;
    s.frames.insert(name);NetTrace(Lesson1315()?"SMOKE_LESSON1315_288_RENDER role=%s frame=%s requestedDraw=%d capturedDraw=%d":"SMOKE_LESSON78_278_RENDER role=%s frame=%s requestedDraw=%d capturedDraw=%d",Role(a),name.c_str(),draw,a->mCoopSmokeFrameDrawCount);return true;
}
Plant* PlantBy(Board* b,const Ref& ref)
{
    auto* p=b->mPlants.DataArrayTryToGet(ref.localUID);const auto* d=GetNativePlantData(p);
    return p&&p->mNetId==ref.id&&(!d||d->instanceToken==ref.token)?p:nullptr;
}
Plant* At(Board* b,int col,int row,uint16_t kind)
{
    Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mPlantCol==col&&p->mRow==row)
        if(const auto* d=GetNativePlantData(p);d&&d->kind==kind)return p;return nullptr;
}
Zombie* Enemy(Board* b)
{
    auto* z=b->mZombies.DataArrayTryToGet(s.enemy.localUID);const auto* d=GetNativeSourceZombieData(z);
    // Source7 observes the allocated reference, including a dead actor awaiting
    // DataArray retirement. Metadata pruning must not masquerade as destruction.
    return z&&z->mNetId==s.enemy.id&&(!d||d->instanceToken==s.enemy.token)?z:nullptr;
}
Coin* Card(Board* b,const Ref& ref)
{
    Coin* c=nullptr;while(b->IterateCoins(c))if(c->mNetId==ref.id&&!c->mDead)
        if(const auto* d=GetNativeDroppedCard269Data(c);d&&d->instanceToken==ref.token&&b->mCoins.DataArrayGetID(c)==ref.localUID)return c;return nullptr;
}
Ref PlantRef(Board* b,Plant* p){return {p->mNetId,GetNativePlantData(p)->instanceToken,b->mPlants.DataArrayGetID(p)};}
bool FreshPlant(LawnApp* a,Plant* p,uint16_t kind,int owner,uint16_t packet,bool shared)
{
    const auto* d=GetNativePlantData(p);const auto* def=FusionFindPlant(kind);
    return Require(a,p&&d&&def&&p->mNetId&&d->instanceToken&&!p->mDead&&p->mIsOnBoard&&!p->mSquished&&
        p->mNetGhost==a->NetIsClient()&&p->mSeedType==GetNativeSeedForPlant(kind)&&d->kind==kind&&
        p->mPlantHealth==def->maxHealth&&p->mPlantMaxHealth==def->maxHealth&&d->owner==owner&&d->packetId==packet&&
        !d->randomOrigin&&(!shared||(d->firstParent==kFusionNoPlant&&d->secondParent==kFusionNoPlant))&&
        a->ReanimationTryToGet(p->mBodyReanimID)&&(!a->NetIsHost()||d->instanceToken==a->mBoard->mPlants.DataArrayGetID(p)),
        "Lesson78 actual plant factory/HP/provenance differs");
}
bool World(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);if(!c||!c->adventure.enabled)return false;const auto& v=c->adventure;
    if(Lesson1315())
    {
        int waves=0;
        if(!Require(a,TryGetFusionSourceAdventureWaves272(s.level,1,waves)&&IsNativeSourceAdventure273(b)&&!c->validation&&!a->mFusionRuntime&&
            v.run.level==s.level&&v.run.cycle==1&&v.sourceDifficulty==2&&!v.run.replay&&!v.run.over&&!v.schedulerFailed&&v.wave==0&&v.maximumWaves==waves&&
            Same(v.profile,Profile(s.level))&&Same(a->mNetManager->mConfig.mFusionSourceProfile,a->NetIsHost()?Profile(s.level):s.localProfile)&&
            b->mBackground==BACKGROUND_2_NIGHT&&b->GetColumnCount()==10&&GetNativeFusionRowCount(b)==5&&b->mGridItems.mSize==0&&
            c->money==3000&&b->mSunMoney==150&&a->mShowHealthBar==night.hpPreference&&
            a->mPlayerInfo&&a->mPlayerInfo->mLevel==s.classicLevel&&a->mPlayerInfo->mFinishedAdventure==s.classicFinished,
            "Lesson1315 source Night world/profile/economy/HP preference differs"))return false;
        if(!s.epoch)s.epoch=c->epoch;
        return Require(a,c->epoch==s.epoch&&c->epoch!=s.previousEpoch,"Lesson1315 fresh source epoch differs");
    }
    if(!Require(a,IsNativeSourceAdventure273(b)&&!c->validation&&!a->mFusionRuntime&&v.run.level==s.level&&v.run.cycle==1&&
        v.sourceDifficulty==2&&!v.run.replay&&!v.run.over&&!v.schedulerFailed&&v.wave==0&&v.maximumWaves==30&&
        Same(v.profile,Profile(s.level))&&Same(a->mNetManager->mConfig.mFusionSourceProfile,a->NetIsHost()?Profile(s.level):s.localProfile)&&
        b->mBackground==BACKGROUND_1_DAY&&b->GetColumnCount()==10&&GetNativeFusionRowCount(b)==5&&c->money==3000&&
        a->mPlayerInfo&&a->mPlayerInfo->mLevel==s.classicLevel&&a->mPlayerInfo->mFinishedAdventure==s.classicFinished,
        "Lesson78 genuine source world/profile/Classic preservation differs"))return false;
    if(!s.epoch)s.epoch=c->epoch;
    return Require(a,c->epoch==s.epoch&&c->epoch!=s.previousEpoch,"Lesson78 fresh source epoch/full identity differs");
}
bool Message(LawnApp* a)
{
    const auto& t=GetNativeContext(a->mBoard)->adventure.run.tutorial;SexyString text;unsigned bit=0;
    if(Lesson1315())
    {
        const SexyString text13[]={_S("Вы открыли удобрение. Оно периодически появляется в левом верхнем углу поля."),
            _S("Удобрение мгновенно выращивает солнечный гриб."),_S("Хищник может сглотнуть немедленно"),
            _S("Удобрение всегда переводит растение на следующую стадию"),_S("Попробуйте применить удобрение к этому ореху."),
            _S("Удобрение работает как фиолетовая карта — улучшает растение"),_S("Теперь примените удобрение к этому ореху-пожирателю."),
            _S("Удобрение полностью восстанавливает здоровье растения."),_S("Высаживайте растения и защищайтесь от зомби!")};
        const SexyString text15[]={_S("Познакомимся с новой механикой ледяных растений — шкалой заморозки."),
            _S("Ледяные атаки заполняют шкалу заморозки. Полная шкала замораживает зомби и обнуляется. Замороженный зомби получает вчетверо больше урона от ледяных снарядов."),
            _S("Предел заморозки растёт с каждым замораживанием"),_S("Высаживайте растения и защищайтесь от зомби!")};
        if(t.message==FusionAdventureMessage273::None)return true;
        const int index=int(t.message)-int(s.level==13?FusionAdventureMessage273::FertilizeIntro:FusionAdventureMessage273::IceIntro);
        if(!Require(a,index>=0&&index<(s.level==13?9:4),"Lesson1315 unrelated source Advice message"))return false;
        if(!t.messageTicks)return true;text=s.level==13?text13[index]:text15[index];bit=1U<<index;
        auto* w=a->mBoard->mAdvice;
        if(!Require(a,w&&w->mLabel==text&&w->mDuration>0&&w->mDisplayTime==FusionAdventureMessageDuration273(t.message),
            "Lesson1315 source Advice/native duration differs"))return false;
        if(!(s.messages&bit))
        {
            night.messageAt[size_t(index)]=a->mBoard->mMainCounter;
            NetTrace("SMOKE_LESSON1315_288_MESSAGE role=%s level=%d stage=%u message=%u ticks=%u timer=%u nativeTick=%d nativeAdvice=1",
                Role(a),s.level,unsigned(t.stage),unsigned(t.message),unsigned(t.messageTicks),unsigned(t.timerTicks),a->mBoard->mMainCounter);
        }
        s.messages|=bit;return true;
    }
    switch(t.message)
    {
    case FusionAdventureMessage273::None:return true;
    case FusionAdventureMessage273::PresentIntro:text=_S("Подарок может дать любое базовое растение.");bit=1;break;
    case FusionAdventureMessage273::UsePresent:text=_S("Положите подарок на этого повторителя.");bit=2;break;
    case FusionAdventureMessage273::PresentReady:text=_S("Вы освоили подарок. Продолжим!");bit=4;break;
    case FusionAdventureMessage273::CherryPaperWarning:text=_S("Вишнёвый газетчик на дорожке 1! Поставьте вишнёвый орех.");bit=8;break;
    case FusionAdventureMessage273::PresentDefenseReady:text=_S("Отлично! Высаживайте растения и защищайтесь от зомби!");bit=16;break;
    case FusionAdventureMessage273::EquipmentIntro:text=_S("Можно ли надеть зомбиное снаряжение на растение?");bit=1;break;
    case FusionAdventureMessage273::BucketNutWarning:text=_S("Убейте орехозомби с ведром на дорожке 1. Без вишни и пожирателя!");bit=2;break;
    case FusionAdventureMessage273::UseBucket:text=_S("Подбери выпавшее ведро и надень на горохострел или орех");bit=4;break;
    case FusionAdventureMessage273::IronPlants:text=_S("Железные растения — лучшие друзья ранней игры!");bit=8;break;
    case FusionAdventureMessage273::EquipmentReady:text=_S("Высаживайте растения и защищайтесь от зомби!");bit=16;break;
    default:return Require(a,false,"Lesson78 unrelated source Advice appeared");
    }
    if(!t.messageTicks)return true;const auto* w=a->mBoard->mAdvice;
    if(!Require(a,w&&w->mLabel==text&&w->mDuration>0&&w->mDisplayTime==FusionAdventureMessageDuration273(t.message),
        "Lesson78 native Advice label/duration differs from source FNS"))return false;
    if(!(s.messages&bit))NetTrace("SMOKE_LESSON78_278_MESSAGE role=%s level=%d tutorialStage=%u message=%u ticks=%u timer=%u nativeAdvice=1",
        Role(a),s.level,unsigned(t.stage),unsigned(t.message),unsigned(t.messageTicks),unsigned(t.timerTicks));
    if(bit==1&&s.lessonTick<0)s.lessonTick=a->mBoard->mMainCounter;s.messages|=bit;return true;
}
bool Choose(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;
    if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return false;
    const auto& bank=c->selection.State();const int slot=Slot(a);std::vector<std::pair<uint16_t,bool>> wanted;
    // Mouse visibility is authoritative before the client's interpolated panel
    // reaches that position. Never queue an SDL pick against a moving widget.
    const int wideX=a->mDDInterface->mWideScreenOffsetX,wideY=a->mDDInterface->mWideScreenOffsetY;
    const auto& intro=b->mNetIntroTargets;
    const int targetX=a->NetIsClient()&&intro.mValid?intro.mChooserX+wideX:wideX;
    const int targetY=a->NetIsClient()&&intro.mValid?intro.mChooserY+wideY:wideY;
    if((a->NetIsClient()&&!intro.mValid)||targetX!=wideX||targetY!=wideY||chooser->mX!=targetX||chooser->mY!=targetY)
    {
        if(!s.diagnostic||GetTickCount()-s.diagnostic>=1000)
        {s.diagnostic=GetTickCount();NetTrace("SMOKE_LESSON_CHOOSER_MOVING role=%s level=%d actual=(%d,%d) authoritativeTarget=(%d,%d) cutscene=%d",Role(a),s.level,chooser->mX,chooser->mY,targetX,targetY,b->mCutScene->mCutsceneTime);}
        return false;
    }
    if(chooser->mNativeFusionEpoch!=bank.epoch||chooser->mNativeFusionRevision!=bank.revision)return false;
    if(Lesson1315())
    {
        const size_t first=slot?bank.quotas[0]:0;
        for(size_t i=first;i<first+bank.quotas[slot]&&i<c->eligible.size();++i)wanted.emplace_back(c->eligible[i].kind,c->eligible[i].isExtra);
    }
    else if(slot==0)wanted={{0,false},{2,false},{1,false},{3,false},{4,false},{5,false}};
    else wanted={{256,false},{0,true},{1,true},{2,true},{3,true},{4,true},{5,true}};
    if(!Require(a,bank.activeMask==3&&bank.targetCount==(std::min)(kNativeFusionBankCapacity,c->eligible.size())&&
        bank.quotas[0]+bank.quotas[1]==bank.targetCount&&bank.quotas[slot]>=2&&bank.quotas[slot]<=wanted.size(),
        "Lesson78 native chooser quotas/required real card identities differ"))return false;
    int own=0;for(const auto& p:bank.packets)if(p.owner==slot)
    {
        const bool matches=own<int(wanted.size())&&p.kind==wanted[size_t(own)].first&&p.isExtra==wanted[size_t(own)].second;
        if(!matches)NetTrace("SMOKE_LESSON_CHOOSER_MISMATCH role=%s level=%d ownIndex=%d actual=%u actualExtra=%d expected=%u expectedExtra=%d quota0=%u quota1=%u waiting=%d eligible=%zu",Role(a),s.level,own,p.kind,p.isExtra?1:0,own<int(wanted.size())?wanted[size_t(own)].first:65535,own<int(wanted.size())&&wanted[size_t(own)].second?1:0,unsigned(bank.quotas[0]),unsigned(bank.quotas[1]),s.waiting,c->eligible.size());
        if(!Require(a,matches,"Lesson78 selected native packet identity differs"))return false;++own;
    }
    if(own<s.waiting)return false;
    if(own<bank.quotas[slot])
    {
        const auto [kind,extra]=wanted[size_t(own)];int dense=-1;
        for(size_t i=0;i<c->eligible.size();++i)if(c->eligible[i].kind==kind&&c->eligible[i].isExtra==extra)dense=int(i);
        if(!Require(a,dense>=0,"Lesson78 source-unlocked normal Present256/required card absent"))return false;
        int x=0,y=0;chooser->GetSeedPositionInChooser(dense,x,y);const float cy=float(y+35-int(chooser->mScrollbar->mScrollValue));
        if(cy<158||cy>493)
        {
            if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();
            SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(chooser->mX+300);m.motion.y=float(chooser->mY+300);
            SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;
            Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Lesson78 native chooser scroll failed");return false;
        }
        if(std::abs(chooser->mScrollAmount)>.05f)return false;
        const int screenX=chooser->mX+x+25,screenY=chooser->mY+int(cy);
        int localX=0,localY=0;
        auto* widget=a->mWidgetManager->GetWidgetAt(screenX,screenY,&localX,&localY);
        // NativeFusionMouseDown uses these same dense eligible identities and
        // native rectangles. The Classic semantic cursor helper rejects Fusion.
        if(widget!=chooser||!Rect(x,y-int(chooser->mScrollbar->mScrollValue),50,70).Contains(localX,localY))return false;
        if(a->mWidgetManager->mLastMouseX!=screenX||a->mWidgetManager->mLastMouseY!=screenY)
        {Click(a,float(screenX),float(screenY),false);return false;}
        NetTrace("SMOKE_LESSON_CHOOSER_INPUT role=%s level=%d ownIndex=%d kind=%u extra=%d dense=%d screen=(%d,%d) local=(%d,%d) panel=(%d,%d) scroll=%.4f revision=%u realWidgetHit=1",Role(a),s.level,own,kind,extra?1:0,dense,screenX,screenY,localX,localY,chooser->mX,chooser->mY,chooser->mScrollbar->mScrollValue,bank.revision);
        if(Click(a,float(screenX),float(screenY)))s.waiting=own+1;return false;
    }
    if(bank.packets.size()!=bank.targetCount)return false;
    int rank=-1;for(int i=0;i<b->mSeedBank->mNumPackets;++i)
    {const int next=LawnNet::NetSeedBankOwnerRank(slot,b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,next>=rank,"Lesson78 native bank own-first order differs"))return false;rank=next;}
    return Capture(a,"chooser")&&Barrier(a,"CHOOSER");
}
SeedPacket* Packet(Board* b,int owner,uint16_t kind,bool extra)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];if(const auto* d=GetNativePacketData(p);d&&d->owner==owner&&d->kind==kind&&d->isExtra==extra)return p;}return nullptr;}
bool PacketHeld(Board* b,const SeedPacket* packet)
{const auto* c=b->mCursorObject;return c&&c->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&c->mSeedBankIndex>=0&&c->mSeedBankIndex<b->mSeedBank->mNumPackets&&b->mSeedBank->mSeedPackets[c->mSeedBankIndex].mNetId==packet->mNetId;}
bool PacketInput(LawnApp* a,int owner,uint16_t kind,bool extra,int col,int row,int price,bool gift=false)
{
    auto* b=a->mBoard;auto* p=Packet(b,owner,kind,extra);const auto* d=GetNativePacketData(p);
    if(!Require(a,p&&d&&d->price==price&&(!gift||(!d->isExtra&&std::abs(d->rechargeSeconds-15.f)<.001f)),"Lesson78 real native packet price/CD/owner differs"))return false;
    if(Slot(a)==owner&&s.input==0)
    {
        if(!p->CanPickUp()||b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL)return false;
        if(BoardClick(a,float(b->mSeedBank->mX+p->mX+p->mOffsetX+25),float(b->mSeedBank->mY+p->mY+35)))s.input=1;return false;
    }
    if(gift)
    {
        if(Slot(a)==owner&&PacketHeld(b,p))Send(a,Tag("PRESENT_HELD_"+std::to_string(p->mNetId)));
        const auto marker=Tag("PRESENT_HELD_"+std::to_string(p->mNetId));if(!Has(a,owner,marker))return false;
        if(!Capture(a,"gift_held")||!Barrier(a,"GIFT_HELD"))return false;
    }
    if(Slot(a)==owner&&s.input==1)
    {if(!PacketHeld(b,p))return false;if(Cell(a,col,row))s.input=2;return false;}
    return true;
}
bool CardPoint(Board* b,Coin* card,float& x,float& y)
{
    float best=1e9f;bool found=false;
    for(int iy=1;iy<14;++iy)for(int ix=1;ix<10;++ix)
    {
        const float px=card->mPosX+ix*5,py=card->mPosY+iy*5;
        if(FindNativeDroppedCard269At(b,px,py)!=card||FindNativeDroppedCard269At(b,px-3,py)!=card||
            FindNativeDroppedCard269At(b,px+3,py)!=card||FindNativeDroppedCard269At(b,px,py-3)!=card||FindNativeDroppedCard269At(b,px,py+3)!=card)continue;
        const float dx=px-card->mPosX-25,dy=py-card->mPosY-35,score=dx*dx+dy*dy;
        if(score<best){best=score;x=px;y=py;found=true;}
    }
    return found;
}
bool BindCards(LawnApp* a)
{
    auto* b=a->mBoard;const int count=s.level==7?1:3;std::vector<Coin*> found;Coin* card=nullptr;
    while(b->IterateCoins(card))if(!card->mDead)if(const auto* d=GetNativeDroppedCard269Data(card))
    {
        if(d->kind==256)continue; // Genuine Present fallback is observed separately.
        if(!Require(a,d->cost==0&&d->heldSlot==255,"Lesson78 source free-card cost/lease differs"))return false;
        if(!d->landed)return false;found.push_back(card);
    }
    if(int(found.size())!=count)return false;
    // Fresh Board IDs preserve the production batch's birth order. Flight has
    // real RNG/physics; never classify twins by their later moving screen X.
    std::sort(found.begin(),found.end(),[](const Coin* x,const Coin* y){return x->mNetId<y->mNetId;});
    for(int i=0;i<count;++i)
    {
        auto* coin=found[size_t(i)];const auto* d=GetNativeDroppedCard269Data(coin);
        if(!Require(a,d->kind==(s.level==7?1003:i==2?1010:1007)&&std::isfinite(coin->mPosX)&&std::isfinite(coin->mPosY),
            "Lesson78 genuine source free-card batch kind/finite flight differs"))return false;
        Ref r{coin->mNetId,d->instanceToken,b->mCoins.DataArrayGetID(coin)};
        if(!Require(a,r.id&&r.token&&(!s.cards[size_t(i)].id||(s.cards[size_t(i)].id==r.id&&s.cards[size_t(i)].token==r.token&&s.cards[size_t(i)].localUID==r.localUID)),"Lesson78 source card full generation changed"))return false;
        s.cards[size_t(i)]=r;
    }
    s.cardCount=count;
    std::string proof="CARDS";for(int i=0;i<count;++i)proof+="_"+Identity(s.cards[size_t(i)]);
    return Barrier(a,proof);
}
bool BindEnemy(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);const uint16_t kind=s.level==7?104:106;Zombie* z=nullptr;
    if(s.enemy.id)z=Enemy(b);else
    {Zombie* candidate=nullptr;while(b->mZombies.IterateNext(candidate))if(const auto* data=GetNativeSourceZombieData(candidate);data&&data->kind==kind){z=candidate;break;}}
    const auto* d=GetNativeSourceZombieData(z);if(!z||!d)return false;
    if(!s.enemy.id)
    {
        if(!Require(a,z->mNetId&&d->instanceToken&&d->kind==kind&&z->mRow==0&&!z->mMindControlled&&z->mHasHead&&
            z->mNetGhost==a->NetIsClient()&&z->mFromWave==0&&z->mBodyHealth==(s.level==7?1500:270)&&z->mBodyMaxHealth==(s.level==7?1500:270)&&
            z->mHelmHealth==(s.level==7?0:2200)&&z->mShieldHealth==(s.level==7?200:0)&&a->ReanimationTryToGet(z->mBodyReanimID),
            "Lesson78 genuine mission NPC initial source HP/armor/carrier differs"))return false;
        s.enemy={z->mNetId,d->instanceToken,b->mZombies.DataArrayGetID(z)};
        NetTrace("SMOKE_LESSON78_278_NPC role=%s level=%d kind=%u id=%u token=%u localUID=%u sourceHealth=%.9g body=%d helm=%d shield=%d fromWave=0 actorWrites=0",Role(a),s.level,kind,s.enemy.id,s.enemy.token,s.enemy.localUID,d->health,z->mBodyHealth,z->mHelmHealth,z->mShieldHealth);
    }
    if(a->NetIsHost()&&!Require(a,c->adventureMissionZombieUID==s.enemy.localUID&&s.enemy.token==s.enemy.localUID,"Lesson78 mission NPC binds wrong source full generation"))return false;
    return Barrier(a,"ENEMY_"+Identity(s.enemy));
}
bool DropPlant(LawnApp* a,int index,int col,uint16_t kind)
{
    auto* b=a->mBoard;const auto ref=s.cards[size_t(index)];auto* card=Card(b,ref);
    if(Slot(a)==1&&s.input==0)
    {
        if(!card)return false;const auto* d=GetNativeDroppedCard269Data(card);float x=0,y=0;
        if(!d||!d->landed||d->heldSlot!=255||b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL||!CardPoint(b,card,x,y))return false;
        if(BoardClick(a,x,y))s.input=1;return false;
    }
    if(Slot(a)==1&&s.input==1)
    {auto* held=GetNativeDroppedCard269Held(b,1);if(!held||held!=card)return false;if(Cell(a,col,0))s.input=2;return false;}
    auto* p=At(b,col,0,kind);if(!p)return false;const auto* d=GetNativePlantData(p);
    if(!s.defenses[size_t(index)].id)
    {
        if(!FreshPlant(a,p,kind,1,0,false))return false;s.defenses[size_t(index)]=PlantRef(b,p);
        NetTrace("SMOKE_LESSON78_278_CARD_USED role=%s level=%d index=%d card=%u token=%u plant=%u plantToken=%u kind=%u column=%d row=0 cost=0",Role(a),s.level,index,ref.id,ref.token,p->mNetId,d->instanceToken,kind,col);
    }
    if(card)return false;
    return Require(a,d->instanceToken==s.defenses[size_t(index)].token&&p->mNetId==s.defenses[size_t(index)].id,"Lesson78 free card spawned duplicate/replaced actor")&&Barrier(a,"DROP_"+std::to_string(index)+"_"+Identity(s.defenses[size_t(index)]));
}
bool PaidPea(LawnApp* a,int owner,int col)
{
    auto* b=a->mBoard;if(!PacketInput(a,owner,0,owner==1,col,0,owner?200:100))return false;
    auto* p=At(b,col,0,0);if(!p)return false;const auto* packet=Packet(b,owner,0,owner==1);
    Ref& ref=owner?s.receiver:s.defenses[3];
    if(!ref.id){if(!FreshPlant(a,p,0,owner,packet->mNetId,false))return false;ref=PlantRef(b,p);}
    return Require(a,PlantBy(b,ref)==p,"Lesson78 purchased receiver/shooter generation changed")&&Barrier(a,"PEA_"+std::to_string(owner)+"_"+Identity(ref));
}
void ObserveBucket(LawnApp* a)
{
    if(s.level!=8||s.bucket.id)return;auto* b=a->mBoard;Coin* coin=nullptr;
    while(b->IterateCoins(coin))if(!coin->mDead)if(const auto* d=GetNativeEquipmentData(coin);d&&d->factoryType==FusionEquipmentType::Bucket&&d->type==FusionEquipmentType::Bucket)
    {s.bucket={coin->mNetId,d->instanceToken,b->mCoins.DataArrayGetID(coin)};NetTrace("SMOKE_LESSON78_278_BUCKET_DROP role=%s id=%u token=%u sourceNpc=%u sourceToken=%u actualCoin=1 fixtureFactoryCalls=0",Role(a),s.bucket.id,s.bucket.token,s.enemy.id,s.enemy.token);return;}
}
bool DyingCheckpoint(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);auto* z=Enemy(b);
    if(z&&(!z->mHasHead||GetNativeFusionZombieStatus(z)==1))s.dying=true;
    if(!s.dying)return false;
    if(!Require(a,z&&c->adventure.run.tutorial.active&&(s.level!=7||c->adventure.run.tutorial.stage==4),
        "Lesson78 destroyed mission NPC too early or source7 advanced on beforeDying"))return false;
    if(a->NetIsHost()&&!s.pause)s.pause=Escape(a);
    if(!b->mPaused)return false;
    if(!s.dyingLogged)
    {s.dyingLogged=true;NetTrace("SMOKE_LESSON78_278_DYING role=%s level=%d id=%u token=%u localUID=%u allocated=1 hasHead=%d status=%d sourceStage=%u sourceActive=1 source7NeedsRetirement=%d",Role(a),s.level,s.enemy.id,s.enemy.token,s.enemy.localUID,z->mHasHead?1:0,GetNativeFusionZombieStatus(z),unsigned(c->adventure.run.tutorial.stage),s.level==7?1:0);}
    return Capture(a,"dying")&&Barrier(a,"DYING_"+Identity(s.enemy));
}
Coin* Fertilize288(Board* b,const Ref& ref)
{
    auto* coin=b->mCoins.DataArrayTryToGet(ref.localUID);const auto* d=GetNativeFertilize269Data(coin);
    return coin&&!coin->mDead&&coin->mNetId==ref.id&&d&&d->instanceToken==ref.token?coin:nullptr;
}
bool BindFertilize288(LawnApp* a,Plant* source,int index)
{
    auto* b=a->mBoard;auto& ref=night.items[size_t(index)];if(ref.id)return Require(a,Fertilize288(b,ref),"Lesson1315 real Fertilize generation vanished before use");
    float x=0,y=0;if(!GetNativeSourcePlantNodePoint(source,"",x,y))return false;
    Coin* coin=nullptr;while(b->IterateCoins(coin))if(const auto* d=GetNativeFertilize269Data(coin);d&&!coin->mDead&&
        d->phase==NativeFertilize269Phase::Falling&&std::abs(d->anchorX-x)<1&&std::abs(d->anchorY-y)<1)
    {
        if(index&&coin->mNetId==night.items[0].id)continue;
        if(!Require(a,coin->mNetId&&d->instanceToken&&d->heldSlot==255&&coin->mNetGhost==a->NetIsClient(),"Lesson1315 genuine script item identity differs"))return false;
        ref={coin->mNetId,d->instanceToken,b->mCoins.DataArrayGetID(coin)};
        NetTrace("SMOKE_LESSON1315_288_FERTILIZE role=%s index=%d id=%u token=%u localUID=%u plant=%u anchorX=%.6f anchorY=%.6f sourceRootGrant=1 fixtureFactoryCalls=0",
            Role(a),index,ref.id,ref.token,ref.localUID,source->mNetId,d->anchorX,d->anchorY);return true;
    }
    return false;
}
bool Uncovered288(LawnApp* a,float x,float y)
{
    const int sx=int(x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX),sy=int(y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);
    return a->mWidgetManager&&a->mWidgetManager->GetWidgetAt(sx,sy,nullptr,nullptr)==a->mBoard;
}
void ObserveFertilize288(LawnApp* a,int index,const Ref& target)
{
    if(auto* coin=Fertilize288(a->mBoard,night.items[size_t(index)]))if(const auto* d=GetNativeFertilize269Data(coin))
    {
        if(d->phase==NativeFertilize269Phase::Cursor&&d->heldSlot==1&&!night.held[size_t(index)])
        {night.held[size_t(index)]=true;NetTrace("SMOKE_LESSON1315_288_HELD role=%s index=%d id=%u token=%u owner=1 nativeCursor=1",Role(a),index,coin->mNetId,d->instanceToken);}
        if(d->phase==NativeFertilize269Phase::Using&&!night.used[size_t(index)])
        {
            if(!Require(a,d->heldSlot==255&&d->targetId==target.id&&d->targetToken==target.token,"Lesson1315 Fertilize use targeted a different generation"))return;
            night.used[size_t(index)]=true;NetTrace("SMOKE_LESSON1315_288_USING role=%s index=%d id=%u token=%u target=%u targetToken=%u genuineInput=1",Role(a),index,coin->mNetId,d->instanceToken,target.id,target.token);
        }
    }
}
void UseFertilize288(LawnApp* a,int index,const Ref& target,int col)
{
    auto* b=a->mBoard;auto* coin=Fertilize288(b,night.items[size_t(index)]);const auto* d=GetNativeFertilize269Data(coin);
    ObserveFertilize288(a,index,target);if(!coin||!d||night.used[size_t(index)])return;
    if(a->NetIsClient()&&s.input==0)
    {
        if(d->phase!=NativeFertilize269Phase::World||b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL||
            b->mCursorObject->mCoinID!=COINID_NULL||!Uncovered288(a,coin->mPosX+20,coin->mPosY+20))return;
        if(BoardClick(a,coin->mPosX+20,coin->mPosY+20))s.input=1;
    }
    if(!night.held[size_t(index)])return;
    if(!Barrier(a,"FERT_HELD_"+Identity(night.items[size_t(index)])))return;
    if(a->NetIsClient()&&s.input==1)
    {
        // FNS ownership can arrive before the normal client cursor reconcile.
        // Only real local UI ownership dispatches the following native click.
        if(GetNativeFertilize269ClaimedBy(b,1)!=coin||uint32_t(b->mCursorObject->mCoinID)!=night.items[size_t(index)].localUID)return;
        if(Cell(a,col,2))s.input=2;
    }
}
bool RecoverEffect288(LawnApp* a)
{
    auto* b=a->mBoard;auto* plant=PlantBy(b,night.chomper);if(!plant||!a->mEffectSystem||!a->mEffectSystem->mParticleHolder)return false;
    float x=0,y=0;if(!plant->GetShadowCenter(x,y))return false;y-=float(.7*GetNativeFusionPixelsY(b));
    auto* holder=a->mEffectSystem->mParticleHolder;TodParticleSystem* system=nullptr;
    while(holder->mParticleSystems.IterateNext(system))if(!system->mDead&&system->mEffectType==PARTICLE_FUSION_SOURCE16&&system->mParticleDef)
    for(auto* node=system->mEmitterList.mHead;node;node=node->mNext)
    {
        auto* emitter=holder->mEmitters.DataArrayTryToGet(uint32_t(node->mValue));
        if(!emitter||emitter->mDead||!emitter->mEmitterDef||emitter->mParticleSystem!=system||std::abs(emitter->mSystemCenter.x-x)>.2f||std::abs(emitter->mSystemCenter.y-y)>.2f)continue;
        for(auto* particleNode=emitter->mParticleList.mHead;particleNode;particleNode=particleNode->mNext)
        {
            auto* p=holder->mParticles.DataArrayTryToGet(uint32_t(particleNode->mValue));if(!p)continue;
            ParticleRenderParams render{};if(!TodParticleEmitter::GetRenderParams(p,&render))continue;
            auto* image=emitter->mImageOverride?emitter->mImageOverride:emitter->mEmitterDef->mImage;
            if(!image||!std::isfinite(render.mAlpha)||render.mAlpha<=0||!std::isfinite(render.mPosX)||!std::isfinite(render.mPosY)||!std::isfinite(render.mSpinPosition)||
                !std::isfinite(render.mParticleScale)||!std::isfinite(render.mParticleStretch))continue;
            const float w=std::abs(render.mParticleScale)*image->GetCelWidth(),h=std::abs(render.mParticleScale*render.mParticleStretch)*image->GetCelHeight();
            const float cr=std::abs(std::cos(render.mSpinPosition)),sr=std::abs(std::sin(render.mSpinPosition));
            const float hw=(cr*w+sr*h)*.5f,hh=(sr*w+cr*h)*.5f;
            const float sx=render.mPosX+b->mX+a->mDDInterface->mWideScreenOffsetX,sy=render.mPosY+b->mY+a->mDDInterface->mWideScreenOffsetY;
            if(sx+hw<0||sx-hw>a->mWidth||sy+hh<0||sy-hh>a->mHeight)continue;
            if(!night.fxSeen)
            {
                night.fxSeen=true;night.effectUID=holder->mParticleSystems.DataArrayGetID(system);
                NetTrace("SMOKE_LESSON1315_288_RECOVER16 role=%s plant=%u token=%u source16=1 systemUID=%u emitterUID=%u particleUID=%u age=%d alpha=%.6f anchorX=%.6f anchorY=%.6f actualTodRender=1 callbackForced=0",
                    Role(a),night.chomper.id,night.chomper.token,night.effectUID,uint32_t(node->mValue),uint32_t(particleNode->mValue),emitter->mSystemAge,render.mAlpha,x,y);
            }
            return true;
        }
    }
    return false;
}
int LiveEnemies288(Board* b)
{
    int count=0;Zombie* z=nullptr;
    while(b->IterateZombies(z))if(!z->mDead&&!z->IsDeadOrDying()&&!z->mMindControlled)++count;
    return count;
}
bool NightComplete288(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);const auto& t=c->adventure.run.tutorial;
    if(t.active||!b->mPaused)return false;
    if(!Require(a,night.countdownSeen&&night.countdown>4.f&&night.countdown<=5.f&&s.messages==(s.level==13?511U:15U)&&
        !FusionAdventureTutorialShowsZombieHealth284(c->adventure.run)&&!LiveEnemies288(b)&&
        (a->NetIsClient()||!c->adventureEnemyCount)&&
        t.stage==(s.level==13?8:3)&&t.timerTicks==0&&
        t.message==(s.level==13?FusionAdventureMessage273::FertilizeReady:FusionAdventureMessage273::IceReady),
        "Lesson1315 completion/Advice/countdown/temporary HP state differs"))return false;
    if(!s.complete){s.complete=true;s.samples=Samples(a);}if(Samples(a)-s.samples<2)return false;
    if(!Capture(a,"complete"))return false;
    s.checkpoint=Tag("COMPLETE_"+std::to_string(s.level)+"_"+(s.level==13?Identity(night.chomper):Identity(night.enemies[0])));
    if(!s.caseLogged)
    {
        s.caseLogged=true;
        NetTrace("SMOKE_LESSON1315_288_CASE_COMPLETE role=%s level=%d epoch=%u sourceMessages=%u nativeChooser=1 nativeReady=1 fullIntro=1 initialSun=150 sourceMoney=3000 nativeInputOnly=1 fixtureFactoryCalls=0 profileSetup=FixtureProfileOnly progressionEarned=0 ClassicUnchanged=1 HPPrefUnchanged=1 sourceTutorOver=1 countdown=%.6f frames=6 actorWrites=0 HPWrites=0 clockWrites=0 RNGWrites=0 localProof=1 peerBarrierPending=1 FullLevelVictory=NotExecuted",
            Role(a),s.level,s.epoch,s.level==13?9:4,night.countdown);
        if(s.level==15)NetTrace("SMOKE_LESSON1315_288_COMPLETE role=%s cases=2 sourceLevels=13,15 frames=12 freshTallNut1027=1 sameGeneration1012Repair=1 genuineRecover16=1 source15SixIceThreeCone=1 naturalColdCombat=1 allSourceEnemiesZero=1 sourceSixCellsEmpty=1 temporaryHPNotINI=1 actualClientFertilizeInput=1 Sound66Receipt=RunnerLog profileSetup=FixtureProfileOnly progressionEarned=0 fixtureFactoryCalls=0 actorWrites=0 HPWrites=0 clockWrites=0 RNGWrites=0 localProof=1 peerBarrierPending=1 Full13Victory=NotExecuted Full15Victory=NotExecuted",Role(a));
    }
    Send(a,s.checkpoint);return Has(a,0,s.checkpoint)&&Has(a,1,s.checkpoint);
}
bool BindIceCohort288(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);
    constexpr uint16_t kinds[]={1034,1037,1038,1034,1037,1038};constexpr int cols[]={3,5,3,4,6,4},rows[]={1,2,3,1,2,3};
    for(size_t i=0;i<6;++i)
    {
        auto* p=At(b,cols[i],rows[i],kinds[i]);if(!p)return false;
        if(!night.plants[i].id)
        {
            if(!FreshPlant(a,p,kinds[i],LawnNet::NET_SHARED_OWNER,0,true))return false;night.plants[i]=PlantRef(b,p);
            NetTrace("SMOKE_LESSON1315_288_ICE_PLANT role=%s index=%zu id=%u token=%u localUID=%u kind=%u column=%d row=%d hp=%d sharedDefault=1",Role(a),i,p->mNetId,night.plants[i].token,night.plants[i].localUID,kinds[i],cols[i],rows[i],p->mPlantHealth);
        }
        if(!Require(a,PlantBy(b,night.plants[i])==p,"Lesson1315 source Ice actor generation changed before cohort"))return false;
    }
    for(size_t i=0;i<3;++i)
    {
        Zombie* found=nullptr;Zombie* z=nullptr;while(b->IterateZombies(z))if(const auto* d=GetNativeSourceZombieData(z);d&&d->kind==2&&z->mRow==int(i)+1&&z->mFromWave==0){found=z;break;}
        if(!found)return false;const auto* d=GetNativeSourceZombieData(found);
        if(!Require(a,found->mNetId&&d->instanceToken&&found->mNetGhost==a->NetIsClient()&&a->ReanimationTryToGet(found->mBodyReanimID),"Lesson1315 genuine Cone source/native identity differs"))return false;
        if(!night.enemies[i].id)
        {
            float x=0,y=0;const float expected=float(b->GetCellCenterX(b->GetColumnCount()-1,int(i)+1))+1.7f*(float(b->GetCellPitchX())/1.35f);
            if(!Require(a,GetNativeSourceZombie269NodePoint(found,"",x,y)&&std::abs(x-expected)<=8,"Lesson1315 source Cone ROOT9 birth position differs"))return false;
            night.enemies[i]={found->mNetId,d->instanceToken,b->mZombies.DataArrayGetID(found)};
            NetTrace("SMOKE_LESSON1315_288_CONE role=%s index=%zu id=%u token=%u localUID=%u kind=2 row=%d fromWave=0 rootX=%.6f expectedRootX=%.6f",Role(a),i,found->mNetId,d->instanceToken,night.enemies[i].localUID,found->mRow,x,expected);
        }
        if(!Require(a,night.enemies[i].id==found->mNetId&&night.enemies[i].token==d->instanceToken&&night.enemies[i].localUID==b->mZombies.DataArrayGetID(found),"Lesson1315 Cone generation changed"))return false;
    }
    // The scheduler counter is host-private. Ghosts prove the actual complete
    // native actor set rather than expecting an unreplicated host counter.
    if(!Require(a,LiveEnemies288(b)==3&&(a->NetIsClient()||c->adventureEnemyCount==3)&&FusionAdventureTutorialShowsZombieHealth284(c->adventure.run)&&!a->mShowHealthBar,"Lesson1315 whole enemy count or automatic HP override differs"))return false;
    if(!night.cohort)
    {
        night.cohort=true;NetTrace("SMOKE_LESSON1315_288_ICE_COHORT role=%s sixPlants=1 threeCones=1 sourceRootX=9 rows=1,2,3 npc=%u,%u,%u tokens=%u,%u,%u fullNativeGenerations=1 sharedDefault=1 sourceHPOverride=1 persistedHPPref=0 fixtureFactoryCalls=0",
            Role(a),night.enemies[0].id,night.enemies[1].id,night.enemies[2].id,night.enemies[0].token,night.enemies[1].token,night.enemies[2].token);
    }
    return true;
}
void ObserveIce288(LawnApp* a)
{
    auto* b=a->mBoard;
    Projectile* p=nullptr;while(b->IterateProjectiles(p))if(IsNativeColdProjectile(p))if(const auto* d=GetNativeProjectileData(p);d&&p->mNetId&&!night.shotSeen)
    {night.shotSeen=true;NetTrace("SMOKE_LESSON1315_288_COLD_SHOT role=%s id=%u kind=%u actualNatural=1",Role(a),p->mNetId,d->kind);}
    for(const auto& ref:night.enemies)if(auto* z=b->mZombies.DataArrayTryToGet(ref.localUID))
    if(const auto* d=GetNativeZombieColdData(static_cast<const Zombie*>(z));d&&(d->level>0||d->chilledTicks>0||d->frozenTicks>0)&&!night.coldSeen)
    {night.coldSeen=true;NetTrace("SMOKE_LESSON1315_288_COLD_HIT role=%s id=%u token=%u level=%d threshold=%d chilled=%d frozen=%d nativeEffect=1",Role(a),ref.id,ref.token,d->level,d->threshold,d->chilledTicks,d->frozenTicks);}
}
bool HPActorVisible288(LawnApp* a)
{
    for(const auto& ref:night.enemies)if(auto* z=a->mBoard->mZombies.DataArrayTryToGet(ref.localUID);z&&!z->mDead&&!z->IsDeadOrDying())
    {
        float rx=0,ry=0,sx=0,sy=0;
        if(!GetNativeSourceZombie269NodePoint(z,"",rx,ry)||!GetNativeSourceZombie269NodePoint(z,"Shadow",sx,sy))continue;
        const float ox=float(a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX),oy=float(a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);
        if(rx+ox>=0&&rx+ox<a->mWidth&&ry+oy>=0&&ry+oy<a->mHeight&&sx+ox>=0&&sx+ox<a->mWidth&&sy+oy>=0&&sy+oy<a->mHeight)return true;
    }
    return false;
}
bool UpdateNight288(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);const auto& t=c->adventure.run.tutorial;const auto phase=c->selection.State().phase;
    if(s.level==15)ObserveIce288(a);
    if(s.level==13&&night.chomper.id)ObserveFertilize288(a,1,night.chomper);
    if(!t.active&&t.stage==(s.level==13?8:3))
    {
        if(!night.countdownSeen){night.countdownSeen=true;night.countdown=c->adventure.countdown;}
        // Pause at TutorOver, independently of subsequent BMP/chat barriers.
        if(a->NetIsHost()&&!s.pause)s.pause=Escape(a);
    }
    if(s.stage==3)
    {
        if(phase==FusionBankPhase::Intro){if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(phase!=FusionBankPhase::Playing||a->mSeedChooserScreen||a->mGameScene!=SCENE_PLAYING)return true;
        NativeFieldView277 view;if(!QueryNativeFusionField277(b,view)||!view.artReady)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introStart>=1000&&view.sourceSelected&&view.background==BACKGROUND_2_NIGHT&&t.active,"Lesson1315 full native intro/source Night backdrop differs"))return true;
        Advance(a,4);return true;
    }
    if(s.level==13)
    {
        if(s.stage==4)
        {
            if(t.stage<5)return true;auto* p=At(b,2,2,3);if(!p)return true;
            if(!night.nut.id){if(!FreshPlant(a,p,3,LawnNet::NET_SHARED_OWNER,0,true))return true;night.nut=PlantRef(b,p);}
            if(!BindFertilize288(a,p,0))return true;
            if(a->NetIsHost())for(size_t i=1;i<5;++i)if(!Require(a,night.messageAt[i-1]>=0&&night.messageAt[i]-night.messageAt[i-1]>=399,"Lesson1315 source13 skipped a four-second introduction stage"))return true;
            if(Capture(a,"nut")&&Barrier(a,"NUT_"+Identity(night.nut)+"_"+Identity(night.items[0])))Advance(a,5);
        }
        else if(s.stage==5)
        {
            UseFertilize288(a,0,night.nut,2);auto* p=At(b,2,2,1027);if(!p)return true;
            if(!Require(a,night.used[0],"Lesson1315 TallNut appeared without the observed real item use"))return true;
            if(!night.tall.id)
            {
                if(!FreshPlant(a,p,1027,LawnNet::NET_SHARED_OWNER,0,true)||!Require(a,p->mNetId!=night.nut.id&&GetNativePlantData(p)->instanceToken!=night.nut.token,"Lesson1315 Fertilize did not create a fresh default TallNut"))return true;
                night.tall=PlantRef(b,p);NetTrace("SMOKE_LESSON1315_288_TALLNUT role=%s old=%u oldToken=%u fresh=%u token=%u kind=1027 sharedDefault=1 actualClientItem=1",Role(a),night.nut.id,night.nut.token,night.tall.id,night.tall.token);
            }
            if(b->mPlants.DataArrayTryToGet(night.nut.localUID)||b->mCoins.DataArrayTryToGet(night.items[0].localUID))return true;
            if(Capture(a,"tallnut")&&Barrier(a,"TALL_"+Identity(night.tall)))Advance(a,6);
        }
        else if(s.stage==6)
        {
            if(t.stage<7)return true;auto* p=At(b,3,2,1012);const auto* d=GetNativePlantData(p);if(!p||!d)return true;
            if(!Require(a,t.stage==7&&t.active&&p->mPlantHealth==1&&p->mPlantMaxHealth==4000&&d->owner==LawnNet::NET_SHARED_OWNER&&!d->packetId&&!d->randomOrigin&&
                d->firstParent==kFusionNoPlant&&d->secondParent==kFusionNoPlant&&(!a->NetIsHost()||c->adventureMissionPlantUID==b->mPlants.DataArrayGetID(p)),"Lesson1315 source3999 damage/tracked1012 differs"))return true;
            if(!night.chomper.id)night.chomper=PlantRef(b,p);
            if(!BindFertilize288(a,p,1))return true;
            if(a->NetIsHost()&&!Require(a,night.messageAt[6]-night.messageAt[5]>=399,"Lesson1315 source13 skipped the post-upgrade four-second timer"))return true;
            if(Capture(a,"damaged")&&Barrier(a,"DAMAGED_"+Identity(night.chomper)+"_"+Identity(night.items[1])))Advance(a,7);
        }
        else if(s.stage==7)
        {
            UseFertilize288(a,1,night.chomper,3);auto* p=PlantBy(b,night.chomper);const auto* d=GetNativePlantData(p);
            if(!p||!d){Require(a,false,"Lesson1315 tracked1012 was replaced or destroyed during repair");return true;}
            if(p->mPlantHealth!=p->mPlantMaxHealth)return true;
            if(!Require(a,night.used[1]&&d->kind==1012&&d->instanceToken==night.chomper.token&&p->mNetId==night.chomper.id&&p->mPlantMaxHealth==4000,"Lesson1315 repair did not preserve the exact1012 generation"))return true;
            // Capture locally while the genuine .75s effect is alive. A peer
            // receipt or an early global pause must not replace this evidence.
            if(!s.frames.contains("fusion_lesson1315_288_13_recover_fx"))
            {if(!RecoverEffect288(a)||!Capture(a,"recover_fx"))return true;}
            if(b->mCoins.DataArrayTryToGet(night.items[1].localUID))return true;
            if(Barrier(a,"REPAIRED_"+Identity(night.chomper)))Advance(a,8);
        }
        else if(s.stage==8)
        {
            if(t.active)return true;
            if(!Require(a,night.fxSeen&&night.held[0]&&night.held[1]&&night.used[0]&&night.used[1]&&
                (!a->NetIsHost()||night.messageAt[8]-night.messageAt[7]>=399),"Lesson1315 source13 exact input/FX/timer proof incomplete"))return true;
            if(NightComplete288(a))Advance(a,90);
        }
    }
    else if(s.stage==4)
    {
        if(t.stage<2)return true;if(!BindIceCohort288(a))return true;
        if(a->NetIsHost()&&!Require(a,night.messageAt[1]-night.messageAt[0]>=399,"Lesson1315 source15 skipped its four-second preparation"))return true;
        if(Capture(a,"prepared")&&Barrier(a,"ICE_COHORT_"+Identity(night.enemies[0])))Advance(a,5);
    }
    else if(s.stage==5)
    {if(night.shotSeen&&Capture(a,"combat")&&Barrier(a,"NATURAL_ICE_SHOTS"))Advance(a,6);}
    else if(s.stage==6)
    {
        if(!night.coldSeen||!HPActorVisible288(a))return true;
        if(!Require(a,t.active&&t.stage==2&&FusionAdventureTutorialShowsZombieHealth284(c->adventure.run)&&!a->mShowHealthBar,"Lesson1315 HP override did not accompany the actual cold combat"))return true;
        if(Capture(a,"hp")&&Barrier(a,"SOURCE_HP_OVERRIDE"))Advance(a,7);
    }
    else if(s.stage==7)
    {
        if(t.active||LiveEnemies288(b)||(a->NetIsHost()&&c->adventureEnemyCount))return true;
        for(const auto& ref:night.enemies)if(b->mZombies.DataArrayTryToGet(ref.localUID))return true;
        constexpr int cols[]={3,5,3,4,6,4},rows[]={1,2,3,1,2,3};
        for(size_t i=0;i<6;++i)
        {if(b->mPlants.DataArrayTryToGet(night.plants[i].localUID))return true;
         Plant* p=nullptr;while(b->IteratePlants(p))if(p->mPlantCol==cols[i]&&p->mRow==rows[i]){Require(a,false,"Lesson1315 final source six-cell clear left a live actor");return true;}}
        if(a->NetIsHost()&&!Require(a,night.messageAt[3]-night.messageAt[2]>=399,"Lesson1315 source15 skipped its final four-second timer"))return true;
        if(Capture(a,"cleared")&&Barrier(a,"ALL_SOURCE_ENEMIES_ZERO_SIX_CELLS_EMPTY"))Advance(a,8);
    }
    else if(s.stage==8&&NightComplete288(a))a->CoopSmokeWriteResult(true,nullptr);
    return true;
}
bool Complete(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);const auto& v=c->adventure;
    if(v.run.tutorial.active)return false;
    if(!Require(a,s.messages==31&&v.run.tutorial.stage==4&&v.countdown>4.f&&v.countdown<=5.f&&
        v.run.tutorial.message==(s.level==7?FusionAdventureMessage273::PresentDefenseReady:FusionAdventureMessage273::EquipmentReady)&&
        b->mSunMoney==500-s.spent&&(!a->NetIsHost()||(!c->adventureMissionPlantUID&&!c->adventureMissionZombieUID)),
        "Lesson78 source completion/advice/economy/countdown differs"))return false;
    if(a->NetIsHost()&&!s.pause)s.pause=Escape(a);if(!b->mPaused)return false;
    if(!s.complete){s.complete=true;s.samples=Samples(a);}if(Samples(a)-s.samples<2)return false;
    if(!Capture(a,"complete"))return false;
    s.checkpoint=Tag("COMPLETE_"+Identity(s.enemy)+"_"+std::to_string(s.level==7?s.giftKind:1020));
    if(!s.caseLogged)
    {
        s.caseLogged=true;
        NetTrace("SMOKE_LESSON78_278_CASE_COMPLETE role=%s level=%d epoch=%u nativeChooser=1 nativeReady=1 nativeIntro=1 sourceAdviceMessages=5 exactMissionGeneration=1 nativeActionsOnly=1 spent=%d remainingSun=%d money=%d profileFixture=1 progressionEarned=0 ClassicUnchanged=1 actorWrites=0 HPWrites=0 clockWrites=0 RNGWrites=0 localProof=1 peerBarrierPending=1 fullLevelVictory=NotExecuted",Role(a),s.level,s.epoch,s.spent,b->mSunMoney,c->money);
    }
    Send(a,s.checkpoint);if(!Has(a,0,s.checkpoint)||!Has(a,1,s.checkpoint))return false;
    return true;
}
bool Restart(LawnApp* a)
{
    const int nextLevel=Lesson1315()?15:8;
    const auto* c=GetNativeContext(a->mBoard);
    if(c&&c->epoch!=s.epoch)
    {
        if(!Require(a,s.complete&&!s.checkpoint.empty()&&Has(a,0,s.checkpoint)&&Has(a,1,s.checkpoint),"Lesson78 fresh source8 epoch arrived before common source7 proof"))return true;
        s.previousEpoch=s.epoch;s.epoch=0;s.level=nextLevel;s.waiting=s.input=s.spent=0;s.lessonTick=-1;s.messages=s.introChecks=0;s.introStart=s.introLast=0;
        s.initial=s.pause=s.resume=s.dying=s.dyingLogged=s.complete=s.preparedLogged=s.bucketTransition=s.caseLogged=false;s.mission=s.enemy=s.giftResult=s.bucket=s.receiver=s.iron=s.fallback={};s.cards={};s.defenses={};s.cardCount=0;
        if(Lesson1315()){const bool hp=night.hpPreference;night={};night.hpPreference=hp;}
        s.checkpoint.clear();s.caseStarted=GetTickCount();Advance(a,1);return true;
    }
    if(a->NetIsHost()&&!s.profileSetup)
    {
        s.profileSetup=true;a->mNetManager->mConfig.mFusionSourceProfile=Profile(nextLevel);
        if(Lesson1315())NetTrace("SMOKE_LESSON1315_288_PROFILE_SETUP hostOnly=1 level=15 completed=16383 frontier=15 setup=FixtureProfileOnly isolatedRuntimeConfig=1 profilePersist=0 progressionEarned=0");
        else NetTrace("SMOKE_LESSON78_278_PROFILE_SETUP hostOnly=1 level=8 completed=127 frontier=8 isolatedRuntimeConfig=1 profilePersist=0 progressionEarned=0");
        Require(a,a->StartFusionSourceAdventure(nextLevel,false),"Lesson78 isolated next source lesson fresh production admission failed");
    }
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionLesson78_278(LawnApp* a)
{
    if(!Enabled("PVZ_COOP_SMOKE_FUSION_LESSON78_278"))return false;
    if(!a||!a->CoopSmokeActive()||!a->mNetManager||a->mCoopSmokeResultWritten)return true;
    if(!Require(a,Enabled("PVZ_COOP_SMOKE_FUSION_NATIVE")&&a->mCoopSmokeManifest.mScenario==NETSMOKE_SCENARIO_FUSION_VALIDATION,
        "Lesson78 requires owned native fusion_validation"))return true;
    if(!s.started){s.started=s.caseStarted=s.since=GetTickCount();if(Lesson1315()){s.level=13;night.hpPreference=a->mShowHealthBar;}}
    if(GetTickCount()-s.started>540000||GetTickCount()-s.caseStarted>240000)
    {NetTrace("SMOKE_LESSON78_278_DEADLINE role=%s level=%d stage=%d epoch=%u input=%d sourceMission=%u enemy=%u bucket=%u",Role(a),s.level,s.stage,s.epoch,s.input,s.mission.id,s.enemy.id,s.bucket.id);a->CoopSmokeFail("Lesson78 genuine native action deadline; inspect exact source mission/cursor/gift/item");return true;}
    if(!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(!s.stage)
    {
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(!Require(a,Slot(a)>=0&&Slot(a)<2&&a->mNetManager->GetDifficulty()==1&&a->mPlayerInfo,"Lesson78 requires two peers/difficulty1/profile"))return true;
        if(s.classicLevel<0){s.classicLevel=a->mPlayerInfo->mLevel;s.classicFinished=a->mPlayerInfo->mFinishedAdventure;s.localProfile=a->mNetManager->mConfig.mFusionSourceProfile;}
        const char* begin=Lesson1315()?"L1315_288_BEGIN":"L78278_BEGIN";
        if(a->NetIsClient()){if(Send(a,begin))Advance(a,1);return true;}
        if(!Has(a,1,begin))return true;
        if(!Require(a,Same(s.localProfile,Profile(s.level))&&a->StartFusionSourceAdventure(s.level,false),"Lesson78 Root-private source profile/first lesson admission failed"))return true;
        if(Lesson1315())NetTrace("SMOKE_LESSON1315_288_BEGIN sourceLevel=13 completed=4095 frontier=13 cycle=1 setup=FixtureProfileOnly progressionEarned=0 timeoutPerCaseMs=240000");
        else NetTrace("SMOKE_LESSON78_278_BEGIN sourceLevel=7 completed=63 frontier=7 cycle=1 profileFixture=1 progressionEarned=0 timeoutPerCaseMs=240000");Advance(a,1);return true;
    }
    if(!a->mBoard)return true;
    if(s.stage==90)return Restart(a);
    if(const auto* next=GetNativeContext(a->mBoard);s.level==(Lesson1315()?13:7)&&s.complete&&next&&next->epoch!=s.epoch)
        return Restart(a); // START_GAME may drain before this peer's final chat echo.
    if(!World(a)||!Message(a))return true;
    if(Lesson1315()&&s.stage>=3)return UpdateNight288(a);
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);const auto& t=c->adventure.run.tutorial;const auto phase=c->selection.State().phase;
    ObserveBucket(a);
    if(s.stage>=4&&(!s.diagnostic||GetTickCount()-s.diagnostic>=5000))
    {
        s.diagnostic=GetTickCount();auto* z=Enemy(b);const auto* zd=GetNativeSourceZombieData(z);
        auto* item=ResolveNativeEquipment(b,s.bucket.id,s.bucket.token);const auto* ed=GetNativeEquipmentData(item);
        NetTrace("SMOKE_LESSON78_278_OBSERVE role=%s level=%d stage=%d elapsed=%u sourceStage=%u sourceActive=%d message=%u sun=%d cursor=%d input=%d mission=%u enemy=%u allocated=%d health=%.9g head=%d status=%d bucket=%u phase=%d claim=%u samples=%u",
            Role(a),s.level,s.stage,GetTickCount()-s.since,unsigned(t.stage),t.active?1:0,unsigned(t.message),b->mSunMoney,int(b->mCursorObject->mCursorType),s.input,
            s.mission.id,s.enemy.id,z?1:0,zd?zd->health:-1,z&&z->mHasHead?1:0,z?GetNativeFusionZombieStatus(z):-1,s.bucket.id,ed?int(ed->phase):-1,ed?unsigned(ed->claimSlot):255,Samples(a));
    }
    if(s.stage==1){if(Choose(a))Advance(a,2);}
    else if(s.stage==2)
    {
        if(phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* chooser=a->mSeedChooserScreen;auto* button=chooser?chooser->mStartButton:nullptr;
        if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver())Click(a,x,y,false);else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(phase==FusionBankPhase::Intro){if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(phase!=FusionBankPhase::Playing||a->mSeedChooserScreen||a->mGameScene!=SCENE_PLAYING)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introStart>=1000&&t.active&&b->mSunMoney==500,"Lesson78 native intro/source initial sun differs"))return true;
        s.initial=true;Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(t.stage<2)return true;
        if(a->NetIsHost()&&!Require(a,s.lessonTick>=0&&b->mMainCounter-s.lessonTick>=(s.level==7?599:399),
            "Lesson78 production preparation bypassed source six/four second lesson timer"))return true;
        if(!s.preparedLogged)
        {s.preparedLogged=true;NetTrace("SMOKE_LESSON78_278_PREPARED role=%s level=%d elapsedNativeTicks=%d sourceExpectedSeconds=%d tutorialStage=%u postFactoryWrites=0",Role(a),s.level,b->mMainCounter-s.lessonTick,s.level==7?6:4,unsigned(t.stage));}
        if(s.level==7)
        {
            auto* p=At(b,1,2,1030);if(!p)return true;
            if(!s.mission.id){if(!FreshPlant(a,p,1030,NET_SHARED_OWNER,0,true))return true;s.mission=PlantRef(b,p);}
            if(!Require(a,t.active&&t.stage==2&&PlantBy(b,s.mission)==p&&(!a->NetIsHost()||c->adventureMissionPlantUID==s.mission.localUID),"Lesson78 exact source7 mission generation differs"))return true;
            if(Capture(a,"prepared")&&Barrier(a,"PREPARED_"+Identity(s.mission)))Advance(a,5);
        }
        else if(BindEnemy(a)&&BindCards(a)&&Capture(a,"prepared")&&Barrier(a,"PREPARED"))Advance(a,20);
    }
    else if(s.stage==5)
    {
        if(!PacketInput(a,1,256,false,1,2,100,true))return true;
        Plant* result=nullptr;for(const uint16_t kind:{uint16_t(1090),uint16_t(1008),uint16_t(1105)})if(auto* p=At(b,1,2,kind))result=p;
        if(!result)
        {
            // Source fallback returns a real free Present card after a failed
            // factory attempt. Retry that exact native item, never a free grant.
            Coin* fallback=nullptr;Coin* coin=nullptr;while(b->IterateCoins(coin))if(!coin->mDead)
                if(const auto* d=GetNativeDroppedCard269Data(coin);d&&d->kind==256&&d->cost==0)fallback=coin;
            if(a->NetIsClient()&&fallback)
            {
                const auto* d=GetNativeDroppedCard269Data(fallback);float x=0,y=0;
                if(!s.fallback.id||s.fallback.id!=fallback->mNetId||s.fallback.token!=d->instanceToken)
                {s.fallback={fallback->mNetId,d->instanceToken,b->mCoins.DataArrayGetID(fallback)};s.input=2;NetTrace("SMOKE_LESSON78_278_GIFT_RETRY role=client actualDroppedPresent=%u token=%u freeGrant=0",s.fallback.id,s.fallback.token);}
                if(d->heldSlot==1&&s.input==3){if(Cell(a,1,2))s.input=4;}
                else if(s.input==2&&d->heldSlot==255&&d->landed&&b->mCursorObject->mCursorType==CURSOR_TYPE_NORMAL&&CardPoint(b,fallback,x,y))
                {if(BoardClick(a,x,y))s.input=3;}
            }
            return true;
        }
        const auto* d=GetNativePlantData(result);const auto* recipe=FindFusionRecipe(1030,d->secondParent);
        if(!s.giftResult.id)
        {
            if(!Require(a,recipe&&recipe->result==d->kind&&d->firstParent==1030&&d->owner==NET_SHARED_OWNER&&!d->packetId&&!d->randomOrigin&&
                d->instanceToken!=s.mission.token&&result->mNetId!=s.mission.id&&result->mPlantHealth==FusionFindPlant(d->kind)->maxHealth,
                "Lesson78 genuine random Gift outcome/recipe/fresh shared provenance differs"))return true;
            s.giftKind=d->kind;s.giftResult=PlantRef(b,result);s.spent=100;
            NetTrace("SMOKE_LESSON78_278_GIFT role=%s mission=%u missionToken=%u result=%u token=%u kind=%u selectedBase=%u sourceRoster=33 normalPacket256=1 spent=100 forcedRNG=0",Role(a),s.mission.id,s.mission.token,result->mNetId,d->instanceToken,d->kind,d->secondParent);
        }
        if(a->NetIsHost()&&b->mPlants.DataArrayTryToGet(s.mission.localUID))
        {Require(a,t.active&&t.stage==2,"Lesson78 source7 advanced before exact mission DataArray retirement");return true;}
        if(t.stage<3||PlantBy(b,s.mission))return true;
        if(Capture(a,"gift_used")&&Barrier(a,"GIFT_"+Identity(s.giftResult)))Advance(a,6);
    }
    else if(s.stage==6)
    {if(t.stage>=4&&BindEnemy(a)&&BindCards(a)&&Capture(a,"enemy"))Advance(a,7);}
    else if(s.stage==7){if(DropPlant(a,0,9,1003))Advance(a,8);}
    else if(s.stage==8)
    {
        if(!Require(a,PlantBy(b,s.defenses[0]),"Lesson78 genuine free CherryNut vanished before defense proof"))return true;
        if(!Capture(a,"defense")||!Barrier(a,"DEFENSE_"+Identity(s.enemy)))return true;
        NetTrace("SMOKE_LESSON78_278_DEFENSE role=%s source104=%u cherryNut=%u token=%u kind=1003 column=9 row=0 cost=0 actualSourceCard=1 extraPaidDefense=0",Role(a),s.enemy.id,s.defenses[0].id,s.defenses[0].token);
        Advance(a,9);
    }
    else if(s.stage==9)
    {
        if(auto* z=Enemy(b))
        {
            if(!z->mHasHead||GetNativeFusionZombieStatus(z)==1)s.dying=true;
            if(a->NetIsHost())Require(a,t.active&&t.stage==4&&c->adventureMissionZombieUID==s.enemy.localUID,
                "Lesson78 source7 advanced while exact NPC reference still allocated");
            return true;
        }
        if(t.active)
            return true;
        if(!Require(a,!Enemy(b)&&!PlantBy(b,s.defenses[0]),"Lesson78 source7 completion lacks exact NPC/CherryNut retirement"))return true;
        if(!s.complete)NetTrace("SMOKE_LESSON78_278_RETIRED role=%s id=%u token=%u localUID=%u actualDataArrayAbsent=1 beforeDyingObserved=%d sourceCherryNutRetired=1",Role(a),s.enemy.id,s.enemy.token,s.enemy.localUID,s.dying?1:0);
        if(Complete(a))Advance(a,90);
    }
    else if(s.stage>=20&&s.stage<=22)
    {
        const int index=s.stage-20;const int columns[]={0,1,8};const uint16_t kinds[]={1007,1007,1010};
        if(DropPlant(a,index,columns[index],kinds[index]))
        {if(index==2){if(Capture(a,"cards"))Advance(a,23);}else Advance(a,s.stage+1);}
    }
    else if(s.stage==23){if(PaidPea(a,0,2)){s.spent=100;Advance(a,24);}}
    else if(s.stage==24){if(PaidPea(a,1,3)){s.spent=300;Advance(a,25);}}
    else if(s.stage==25){if(DyingCheckpoint(a))Advance(a,26);}
    else if(s.stage==26)
    {
        if(a->NetIsHost()&&!s.resume)s.resume=Escape(a);if(b->mPaused)return true;s.pause=false;
        if(t.stage<3||!s.bucket.id)return true;
        if(!s.bucketTransition)
        {
            auto* z=Enemy(b);if(!Require(a,z&&(!z->mHasHead||GetNativeFusionZombieStatus(z)==1),
                "Lesson78 source8 UseBucket transition did not preserve the nonnull beforeDying mission reference"))return true;
            s.bucketTransition=true;
        }
        if(Barrier(a,"BUCKET_"+Identity(s.bucket)))Advance(a,27);
    }
    else if(s.stage==27)
    {
        auto* coin=ResolveNativeEquipment(b,s.bucket.id,s.bucket.token);const auto* d=GetNativeEquipmentData(coin);
        if(!Require(a,coin&&d&&b->mCoins.DataArrayGetID(coin)==s.bucket.localUID&&d->type==FusionEquipmentType::Bucket&&d->factoryType==FusionEquipmentType::Bucket,"Lesson78 genuine dropped Bucket identity/type vanished"))return true;
        if(a->NetIsClient()&&s.input==0)
        {if(d->phase!=NativeEquipmentPhase::World||b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL)return true;if(BoardClick(a,coin->mPosX+20,coin->mPosY+20))s.input=1;return true;}
        if(d->phase!=NativeEquipmentPhase::Cursor||d->claimSlot!=1)return true;
        if(!Require(a,!GetNativeDroppedCard269Held(b,1),"Lesson78 equipment cursor overlaps dropped-card lease"))return true;
        if(Capture(a,"bucket_held")&&Barrier(a,"HELD_BUCKET_"+Identity(s.bucket)))Advance(a,28);
    }
    else if(s.stage==28)
    {
        if(a->NetIsClient()&&s.input==0){if(Cell(a,3,0))s.input=1;return true;}
        auto* p=At(b,3,0,1020);if(!p)return true;
        if(!s.iron.id)
        {
            if(!FreshPlant(a,p,1020,NET_SHARED_OWNER,0,true)||!Require(a,p->mNetId!=s.receiver.id&&GetNativePlantData(p)->instanceToken!=s.receiver.token,
                "Lesson78 Bucket receiver did not use fresh source SetPlant default"))return true;s.iron=PlantRef(b,p);
            NetTrace("SMOKE_LESSON78_278_IRON role=%s sourceNpc=%u sourceToken=%u bucket=%u bucketToken=%u oldPea=%u oldToken=%u fresh=%u token=%u hp=%d originalOwner=1 actingSlot=1 sharedDefault=1 itemClicks=1 consumedOnce=1",Role(a),s.enemy.id,s.enemy.token,s.bucket.id,s.bucket.token,s.receiver.id,s.receiver.token,s.iron.id,s.iron.token,p->mPlantHealth);
        }
        if(b->mPlants.DataArrayTryToGet(s.receiver.localUID))return true;
        if(ResolveNativeEquipment(b,s.bucket.id,s.bucket.token))return true;
        if(t.active)return true;
        if(a->NetIsHost()&&!s.pause)s.pause=Escape(a);if(!b->mPaused)return true;
        if(Capture(a,"iron")&&Barrier(a,"IRON_"+Identity(s.iron)))Advance(a,29);
    }
    else if(s.stage==29)
    {
        if(!Require(a,PlantBy(b,s.iron)&&!ResolveNativeEquipment(b,s.bucket.id,s.bucket.token),"Lesson78 consumed Bucket/replacement generation regressed"))return true;
        if(Complete(a))
        {NetTrace("SMOKE_LESSON78_278_COMPLETE role=%s cases=2 sourceLevels=7,8 frames=14 nativeGift256=1 actualRandomRecipe=1 exactSource104Retirement=1 source106BeforeDying=1 realBucketDrop=1 clientBucketUse=1 freshIron1020=1 sourceLessonsCompleted=2 progressionEarned=0 fullSource7Victory=NotExecuted fullSource8Victory=NotExecuted Puff6BirthAndProjectile9=NotExecuted actorWrites=0 HPWrites=0 clockWrites=0 RNGWrites=0",Role(a));a->CoopSmokeWriteResult(true,nullptr);}
    }
    return true;
}
}
