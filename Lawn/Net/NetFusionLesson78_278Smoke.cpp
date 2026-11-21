#include "NetFusionLesson78_278Smoke.h"
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
#include "../Fusion/FusionNativeDroppedCards269.h"
#include "../Fusion/FusionNativeEquipment.h"
#include "../Fusion/FusionNativeZombies.h"
#include "../Fusion/FusionRecipe.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/DDInterface.h"
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
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool valid,const char* why){if(!valid)a->CoopSmokeFail(why);return valid;}
FusionAdventureProfile273 Profile(int level){return {level==7?uint64_t(63):uint64_t(127),1,uint16_t(level)};}
bool Same(const FusionAdventureProfile273& x,const FusionAdventureProfile273& y)
{return x.completedLevels==y.completedLevels&&x.cycle==y.cycle&&x.frontierLevel==y.frontierLevel;}
std::string Tag(const std::string& label){return "L78278_"+std::to_string(s.epoch)+"_"+label;}
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
{s.stage=stage;s.input=0;s.since=GetTickCount();NetTrace("SMOKE_LESSON78_278_STAGE role=%s level=%d stage=%d epoch=%u",Role(a),s.level,stage,s.epoch);}
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
    const auto name="fusion_lesson78_278_"+std::to_string(s.level)+"_"+suffix;if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int draw=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(draw))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Lesson78 fresh native BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_LESSON78_278_RENDER role=%s frame=%s requestedDraw=%d capturedDraw=%d",Role(a),name.c_str(),draw,a->mCoopSmokeFrameDrawCount);return true;
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
    if(slot==0)wanted={{0,false},{2,false},{1,false},{3,false},{4,false},{5,false}};
    else wanted={{256,false},{0,true},{1,true},{2,true},{3,true},{4,true},{5,true}};
    if(!Require(a,bank.activeMask==3&&bank.targetCount==(std::min)(kNativeFusionBankCapacity,c->eligible.size())&&
        bank.quotas[0]+bank.quotas[1]==bank.targetCount&&bank.quotas[slot]>=2&&bank.quotas[slot]<=wanted.size(),
        "Lesson78 native chooser quotas/required real card identities differ"))return false;
    int own=0;for(const auto& p:bank.packets)if(p.owner==slot)
    {if(!Require(a,own<int(wanted.size())&&p.kind==wanted[size_t(own)].first&&p.isExtra==wanted[size_t(own)].second,"Lesson78 selected native packet identity differs"))return false;++own;}
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
        if(std::abs(chooser->mScrollAmount)<=.05f&&Click(a,float(chooser->mX+x+25),float(chooser->mY)+cy))s.waiting=own+1;return false;
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
    const auto* c=GetNativeContext(a->mBoard);
    if(c&&c->epoch!=s.epoch)
    {
        if(!Require(a,s.complete&&!s.checkpoint.empty()&&Has(a,0,s.checkpoint)&&Has(a,1,s.checkpoint),"Lesson78 fresh source8 epoch arrived before common source7 proof"))return true;
        s.previousEpoch=s.epoch;s.epoch=0;s.level=8;s.waiting=s.input=s.spent=0;s.lessonTick=-1;s.messages=s.introChecks=0;s.introStart=s.introLast=0;
        s.initial=s.pause=s.resume=s.dying=s.dyingLogged=s.complete=s.preparedLogged=s.bucketTransition=s.caseLogged=false;s.mission=s.enemy=s.giftResult=s.bucket=s.receiver=s.iron=s.fallback={};s.cards={};s.defenses={};s.cardCount=0;
        s.checkpoint.clear();s.caseStarted=GetTickCount();Advance(a,1);return true;
    }
    if(a->NetIsHost()&&!s.profileSetup)
    {
        s.profileSetup=true;a->mNetManager->mConfig.mFusionSourceProfile=Profile(8);
        NetTrace("SMOKE_LESSON78_278_PROFILE_SETUP hostOnly=1 level=8 completed=127 frontier=8 isolatedRuntimeConfig=1 profilePersist=0 progressionEarned=0");
        Require(a,a->StartFusionSourceAdventure(8,false),"Lesson78 isolated source8 fresh production admission failed");
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
    if(!s.started)s.started=s.caseStarted=s.since=GetTickCount();
    if(GetTickCount()-s.started>540000||GetTickCount()-s.caseStarted>240000)
    {NetTrace("SMOKE_LESSON78_278_DEADLINE role=%s level=%d stage=%d epoch=%u input=%d sourceMission=%u enemy=%u bucket=%u",Role(a),s.level,s.stage,s.epoch,s.input,s.mission.id,s.enemy.id,s.bucket.id);a->CoopSmokeFail("Lesson78 genuine native action deadline; inspect exact source mission/cursor/gift/item");return true;}
    if(!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(!s.stage)
    {
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(!Require(a,Slot(a)>=0&&Slot(a)<2&&a->mNetManager->GetDifficulty()==1&&a->mPlayerInfo,"Lesson78 requires two peers/difficulty1/profile"))return true;
        if(s.classicLevel<0){s.classicLevel=a->mPlayerInfo->mLevel;s.classicFinished=a->mPlayerInfo->mFinishedAdventure;s.localProfile=a->mNetManager->mConfig.mFusionSourceProfile;}
        if(a->NetIsClient()){if(Send(a,"L78278_BEGIN"))Advance(a,1);return true;}
        if(!Has(a,1,"L78278_BEGIN"))return true;
        if(!Require(a,Same(s.localProfile,Profile(7))&&a->StartFusionSourceAdventure(7,false),"Lesson78 Root-private source6-complete profile/source7 admission failed"))return true;
        NetTrace("SMOKE_LESSON78_278_BEGIN sourceLevel=7 completed=63 frontier=7 cycle=1 profileFixture=1 progressionEarned=0 timeoutPerCaseMs=240000");Advance(a,1);return true;
    }
    if(!a->mBoard)return true;
    if(s.stage==90)return Restart(a);
    if(const auto* next=GetNativeContext(a->mBoard);s.level==7&&s.complete&&next&&next->epoch!=s.epoch)
        return Restart(a); // START_GAME may drain before this peer's final chat echo.
    if(!World(a)||!Message(a))return true;
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
