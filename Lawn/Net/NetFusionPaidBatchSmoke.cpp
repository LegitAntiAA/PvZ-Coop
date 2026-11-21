#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/GameButton.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionRecipe.h"
#include "../Fusion/FusionDefenderRules.h"
#include "../Fusion/FusionBatchActionRules.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/DDInterface.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>

namespace
{
using namespace LawnFusion;
using LawnNet::NetTrace;
constexpr std::array<uint16_t,10> kinds={8,9,11,13,14,16,17,24,27,31};
constexpr int recipeRegressionGroup = 2;
constexpr uint16_t recipeDeck[][5] = {{6, 0, 3, 2, 4}, {9, 7, 1, 5, 8}};
struct RecipeStep { uint16_t incoming, result; int owner, column, replacedStep; };
constexpr RecipeStep recipeSteps[] = {
    {6, 6, 0, 2, -1}, {9, 6, 1, 2, 0}, {7, 6, 1, 2, 1},
    {0, 0, 0, 4, -1}, {1, 1000, 1, 4, 3},
};
struct Ref { uint16_t id=0;uint32_t token=0,localGeneration=0;int birthTick=0; };
struct State
{
    int group=0,stage=0,buy=0,waitingOwn=0,selectedOwn=0,choiceDense=-1,introChecks=0,sun=500,playingFrame=-1;
    uint32_t epoch=0,oldEpoch=0,since=0,chat=0,wheel=0,diagnostic=0,introFirst=0,introLast=0,pickSample=0,pauseSample=0;
    bool configured=false,budget=false,picked=false,placed=false,purchased=false,setup=false,before=false,resumed=false,resumeObserved=false,resumeAcknowledged=false,finalPause=false,finalLogged=false;
    std::array<Ref,10> actors{};std::array<bool,10> retired{};uint16_t target=0;uint32_t targetGeneration=0;
    float targetStartX=0;
    std::set<std::string> frames;std::map<std::string,int> firstDraw;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool yes,const char* why){if(!yes)a->CoopSmokeFail(why);return yes;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();s.diagnostic=0;NetTrace("SMOKE_PAID233_STAGE role=%s group=%d epoch=%u stage=%d buy=%d",Role(a),s.group,s.epoch,stage,s.buy);}
std::string Tag(const char* value){return "P233_"+std::string(value)+"_"+std::to_string(s.group)+"_"+std::to_string(s.epoch);}
bool Has(LawnApp* a,int slot,const std::string& value){for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==slot&&l.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value)
{if(Has(a,Slot(a),value))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(value);return Has(a,Slot(a),value);}
bool Barrier(LawnApp* a,const char* value){const auto tag=Tag(value);Send(a,tag);return Has(a,0,tag)&&Has(a,1,tag);}
bool Key(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Paid batch owned SDL Escape failed");}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Paid batch owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y){return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(s.choiceDense!=index){s.choiceDense=index;NetTrace("SMOKE_PAID233_CHOICE_TARGET role=%s group=%d dense=%d scroll=%.6g range=%.6g centerY=%.6g",Role(a),s.group,index,c->mScrollbar->mScrollValue,c->mScrollbar->mScrollRange,cy);}
    if(cy<158||cy>493)
    {
        if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();
        SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);
        SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;
        Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Paid batch chooser owned SDL wheel failed");return false;
    }
    if(std::abs(c->mScrollAmount)>.05f)return false;
    return Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_paid_"+std::to_string(s.group)+"_"+suffix;if(s.frames.contains(name))return true;
    if(!s.firstDraw.contains(name)){s.firstDraw[name]=a->mDrawCount;return false;}if(a->mDrawCount-s.firstDraw[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Paid batch actual native BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_PAID233_RENDER role=%s group=%d frame=%s freshDraws=%d",Role(a),s.group,name.c_str(),a->mDrawCount-s.firstDraw[name]);return true;
}
Plant* Actor(Board* b,const Ref& ref)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mNetId==ref.id&&d&&d->instanceToken==ref.token)return p;}return nullptr;}
Plant* At(Board* b,int col,int row,int kind)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&d&&d->kind==kind&&p->mPlantCol==col&&p->mRow==row)return p;}return nullptr;}
Zombie* Target(Board* b){Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(z->mNetId==s.target)return z;return nullptr;}
SeedPacket* Card(Board* b,int owner,int kind)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(p);if(d&&d->owner==owner&&d->kind==kind&&!d->isExtra)return p;}return nullptr;}
int Layer(Board* b,Plant* p)
{PlantsOnLawn cell{};b->GetPlantsOnLawn(p->mPlantCol,p->mRow,&cell);return cell.mNormalPlant==p?0:cell.mUnderPlant==p?1:cell.mPumpkinPlant==p?2:-1;}
void Retirement(LawnApp* a)
{
    if(s.group)return;for(int i:{2,5})
    {
        const auto ref=s.actors[i];if(!ref.id||s.retired[i])continue;auto* p=a->mBoard->mPlants.DataArrayTryToGet(ref.localGeneration);
        if(!p||p->mDead){s.retired[i]=true;NetTrace("SMOKE_PAID233_RETIRED role=%s group=0 epoch=%u kind=%u id=%u token=%u localGeneration=%u elapsed=%d natural=1",Role(a),s.epoch,kinds[i],ref.id,ref.token,ref.localGeneration,a->NetIsHost()?a->mBoard->mMainCounter-ref.birthTick:-1);}
    }
}
bool Purchase(LawnApp* a)
{
    auto* b=a->mBoard;
    const auto* step=s.group==recipeRegressionGroup ? &recipeSteps[s.buy] : nullptr;
    const int incoming=step?step->incoming:s.group?(s.buy?21:24):kinds[s.buy];
    const int result=step?step->result:s.group&&s.buy?1088:incoming;
    const int owner=step?step->owner:s.group?s.buy:s.buy/5;
    const int col=step?step->column:s.group?5:2*(s.buy%5),row=s.group?2:(s.buy<5?0:4);
    const int replaced=step?step->replacedStep:s.group&&s.buy?0:-1;
    const auto* definition=FusionFindPlant(incoming);const auto* child=FusionFindPlant(result);auto* card=Card(b,owner,incoming);const auto* packet=GetNativePacketData(card);
    const int price=definition?definition->sunCost:-1,cd=definition?int(std::lround(definition->rechargeSeconds*100)):-1;
    if(!Require(a,definition&&child&&card&&packet&&packet->price==price&&packet->rechargeSeconds==definition->rechargeSeconds&&price>=0&&cd>0,"Paid batch typed source card cost/CD missing"))return false;
    if(Slot(a)==owner&&!s.picked)
    {if(!card->mActive||card->mRefreshing)return false;auto* bank=b->mSeedBank;s.pickSample=Samples(a);s.picked=BoardClick(a,float(bank->mX+card->mX+card->mOffsetX+25),float(bank->mY+card->mY+35));return false;}
    if(Slot(a)==owner&&!s.placed)
    {
        const auto* c=b->mCursorObject;const bool held=c&&c->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&c->mSeedBankIndex>=0&&c->mSeedBankIndex<b->mSeedBank->mNumPackets&&b->mSeedBank->mSeedPackets[c->mSeedBankIndex].mNetId==card->mNetId;
        if(!held||(a->NetIsClient()&&Samples(a)<=s.pickSample))return false;if(!Require(a,b->mSunMoney==s.sun,"Paid batch pickup charged before actual placement"))return false;
        NetTrace("SMOKE_PAID233_HELD role=%s group=%d index=%d incoming=%d packet=%u heldEcho=1 sun=%d",Role(a),s.group,s.buy,incoming,card->mNetId,b->mSunMoney);
        s.placed=BoardClick(a,float(b->GetCellCenterX(col,row)),float(b->GridToPixelY(col,row)+50));return false;
    }
    if(!s.purchased)
    {
        auto* p=At(b,col,row,result);const auto* d=GetNativePlantData(p);if(!d)return false;
        // A same-kind child must be located by its ancestry, not the outgoing parent.
        if (step && replaced>=0 && d->firstParent==kFusionNoPlant) return false;
        if (step && replaced>=0 && p->mNetId==s.actors[replaced].id) return false;
        const int first=replaced<0?kFusionNoPlant:step?recipeSteps[replaced].result:24;
        const int second=replaced<0?kFusionNoPlant:incoming;
        const auto* defender=FindFusionDefenderProfile(result);const int layer=defender?int(defender->layer):0;
        if(!Require(a,d->instanceToken&&d->kind==result&&d->owner==owner&&d->packetId==card->mNetId&&!d->randomOrigin&&d->firstParent==first&&d->secondParent==second&&
            p->mPlantHealth==child->maxHealth&&p->mPlantMaxHealth==child->maxHealth&&p->mSeedType==GetNativeSeedForPlant(result)&&Layer(b,p)==layer&&
            b->mSunMoney==s.sun-price&&card->mRefreshTime==cd&&card->mRefreshing&&!card->mActive&&std::isfinite(d->idleSeconds)&&std::isfinite(d->shootSeconds)&&std::isfinite(d->animationSpeed),
            "Paid batch actual source identity/HP/cost/CD/layer differs"))return false;
        if(replaced>=0&&!Require(a,p->mNetId!=s.actors[replaced].id&&d->instanceToken!=s.actors[replaced].token&&!Actor(b,s.actors[replaced]),"Paid recipe did not create a fresh child and retire parent"))return false;
        s.actors[s.buy]={p->mNetId,d->instanceToken,b->mPlants.DataArrayGetID(p),b->mMainCounter};s.purchased=true;
        if(!s.group&&(incoming==11||incoming==16)&&a->NetIsHost())
        {int ticks=0;if(!Require(a,TryGetFusionBatchInstantTicks(uint16_t(incoming),d->animationSpeed,ticks)&&p->mDoSpecialCountdown>0&&p->mDoSpecialCountdown<=ticks,"Paid instant callback counter is not finite/natural"))return false;NetTrace("SMOKE_PAID233_INSTANT kind=%d id=%u token=%u remaining=%d sourceTicks=%d speed=%.9g natural=1",incoming,p->mNetId,d->instanceToken,p->mDoSpecialCountdown,ticks,d->animationSpeed);}
        NetTrace("SMOKE_PAID233_PURCHASE role=%s group=%d epoch=%u index=%d incoming=%d kind=%d id=%u token=%u packet=%u owner=%d parents=%u,%u HP=%d cost=%d sun=%d cooldown=%d column=%d row=%d layer=%d nativeSDL=1",
            Role(a),s.group,s.epoch,s.buy,incoming,result,p->mNetId,d->instanceToken,card->mNetId,owner,d->firstParent,d->secondParent,p->mPlantHealth,price,b->mSunMoney,cd,col,row,Layer(b,p));
    }
    const char* frame=step?(s.buy==1?"scaredy":s.buy==2?"fume":s.buy==4?"hybrid":nullptr):
        s.group?(s.buy?"fused":"parent"):incoming==11?"paid_11":incoming==16?"paid_16":nullptr;
    if(frame&&!Capture(a,frame))return false;
    // Cursor clearing is an observed successful input result, not a gate on
    // the chat acknowledgement; both peers can always finish this barrier.
    const std::string tag="PAID_"+std::to_string(s.buy);if(!Barrier(a,tag.c_str()))return false;
    s.sun-=price;++s.buy;s.picked=s.placed=s.purchased=false;return true;
}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;auto* p=Actor(b,s.actors[1]);if(!Require(a,p&&b->mPaused,"Paid CactusPumpkin setup requires paused actual child"))return false;
    const auto prefix=Tag("NPC")+" ";
    if(a->NetIsHost()&&!s.setup)
    {
        if(!Require(a,!b->mZombies.mSize,"Paid CactusPumpkin initial world has unexpected NPCs"))return false;
        auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,p->mRow,0);auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;
        if(!Require(a,z&&z->mNetId&&body&&body->mAnimRate>0&&z->mVelX>0,"Paid CactusPumpkin real walking biter allocation failed"))return false;
        z->mBodyHealth=z->mBodyMaxHealth=3;z->mPosY=z->GetPosYBasedOnRow(z->mRow);z->mY=int(z->mPosY);bool found=false;
        for(int dx=80;dx>=-80;--dx){z->mPosX=float(p->mX+dx);z->mX=int(z->mPosX);if(z->FindPlantTarget(ATTACKTYPE_CHEW)==p){found=true;break;}}
        if(!Require(a,found,"Paid CactusPumpkin initial actual chew geometry failed"))return false;
        s.target=z->mNetId;s.targetGeneration=b->mZombies.DataArrayGetID(z);s.setup=true;
        NetTrace("SMOKE_PAID233_NPC_SETUP id=%u generation=%u row=%d body=3 max=3 x=%.6g y=%.6g vx=%.9g initialOnly=1",z->mNetId,s.targetGeneration,z->mRow,z->mPosX,z->mPosY,z->mVelX);
    }
    if(a->NetIsHost())return Send(a,prefix+std::to_string(s.target));
    if(!s.setup)for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==0&&l.mText.rfind(prefix,0)==0)
    {std::istringstream in(l.mText.substr(prefix.size()));unsigned id=0;if(!(in>>id)||!id||id>65535)return false;s.target=uint16_t(id);s.setup=true;break;}
    return s.setup&&Target(b);
}
bool Outcome(LawnApp* a)
{
    auto* b=a->mBoard;auto* p=Actor(b,s.actors[1]);auto* z=Target(b);
    if(!Require(a,p&&p->mPlantMaxHealth==4000&&p->mPlantHealth>=3996&&p->mPlantHealth<=4000&&Layer(b,p)==2&&b->mSunMoney==250&&GetNativeContext(b)->money==3000,"Paid CactusPumpkin child/ledger changed outside its real bite"))return false;
    return p->mPlantHealth==3996&&(!z||z->mDead||z->IsDeadOrDying()||z->mBodyHealth<=0);
}
bool Final(LawnApp* a)
{
    auto* b=a->mBoard;if(!Require(a,b->mSunMoney==s.sun&&GetNativeContext(b)->money==3000,"Paid batch final balances differ"))return false;
    if(s.group)return Outcome(a);
    if(!s.retired[2]||!s.retired[5])return false;
    for(size_t i=0;i<kinds.size();++i)if(i!=2&&i!=5)
    {auto* p=Actor(b,s.actors[i]);const auto* d=FusionFindPlant(kinds[i]);if(!Require(a,p&&d&&p->mPlantHealth==d->maxHealth&&p->mPlantMaxHealth==d->maxHealth,"Paid base actor retired or changed HP without a target"))return false;}
    return Require(a,b->GetCraterAt(4,0)&&!b->GetCraterAt(0,4),"Paid Doom/Jalapeno natural callback world result differs");
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionPaidBatch(LawnApp* a)
{
    const auto* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_PAID_BATCH"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;
    if(!s.since)
    {
        s.since=GetTickCount();
        const auto* regression=std::getenv("PVZ_COOP_SMOKE_FUSION_RECIPE271");
        if(regression&&std::strcmp(regression,"1")==0) s.group=recipeRegressionGroup;
    }
    if(GetTickCount()-s.since>60000){char why[100];sprintf_s(why,"Paid batch deadline group=%d stage=%d buy=%d selected=%d target=%u",s.group,s.stage,s.buy,s.selectedOwn,s.target);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
     if(a->NetIsClient()){Advance(a,1);Send(a,"P233_BEGIN");}else if(Has(a,1,"P233_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;}
    if(s.stage==10)
    {
        if(!Barrier(a,"DONE"))return true;
        if(s.group){NetTrace("SMOKE_PAID233_COMPLETE role=%s activities=2 baseKinds=10 legitimate1088=1 actualPurchases=12 nativeSDL=1 naturalInstants=2 fullReflection=4 childHP=3996 finalSun=250 postSetupActorWrites=0 sourceNPC=NotExecuted gold=NotExecuted equipment=NotExecuted",Role(a));a->CoopSmokeWriteResult(true,nullptr);return true;}
        const auto old=s.epoch;State next;next.group=1;next.oldEpoch=old;next.chat=s.chat;s=std::move(next);Advance(a,1);
        if(a->NetIsHost()){std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient()&&!s.oldEpoch)Send(a,"P233_BEGIN");return true;}
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);if(ctx->epoch<=s.oldEpoch)return true;
    if(!s.epoch){s.epoch=ctx->epoch;NetTrace("SMOKE_PAID233_EPOCH role=%s group=%d epoch=%u previous=%u",Role(a),s.group,s.epoch,s.oldEpoch);}
    if(!Require(a,ctx->epoch==s.epoch&&ctx->validation&&ctx->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Paid batch requires the real native validation epoch"))return true;
    const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(s.stage>=5)Retirement(a);
    if(!s.diagnostic||GetTickCount()-s.diagnostic>=10000)
    {s.diagnostic=GetTickCount();NetTrace("SMOKE_PAID233_GATE role=%s group=%d epoch=%u stage=%d buy=%d selected=%d waiting=%d sun=%d expected=%d retired11=%d retired16=%d target=%u paused=%d samples=%u",Role(a),s.group,s.epoch,s.stage,s.buy,s.selectedOwn,s.waitingOwn,b->mSunMoney,s.sun,s.retired[2]?1:0,s.retired[5]?1:0,s.target,b->mPaused?1:0,Samples(a));}
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
        if(!Require(a,bank.activeMask==3&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5,"Paid batch chooser quotas differ"))return true;
        const std::array<uint16_t,5> h={24,0,1,2,3},c={21,4,5,6,7};int own=0;
        const auto wantedCard=[&](int index)
        {
            return s.group==recipeRegressionGroup?recipeDeck[Slot(a)][index]:
                s.group?(Slot(a)?c[index]:h[index]):kinds[5*Slot(a)+index];
        };
        for(const auto& p:bank.packets)if(p.owner==Slot(a))
        {if(!Require(a,own<5,"Paid batch received too many owned selections"))return true;if(!Require(a,p.kind==wantedCard(own)&&!p.isExtra,"Paid batch actual selected card identity differs"))return true;++own;}
        if(own>s.selectedOwn){s.selectedOwn=own;NetTrace("SMOKE_PAID233_CHOICE_ACK role=%s group=%d own=%d dense=%d scroll=%.6g",Role(a),s.group,own,s.choiceDense,chooser->mScrollbar->mScrollValue);}
        if(own<s.waitingOwn)return true;
        if(own<5){const int wanted=wantedCard(own);int dense=-1;for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].kind==wanted&&!ctx->eligible[i].isExtra)dense=int(i);if(!Require(a,dense>=0,"Paid batch requested source base is not eligible"))return true;if(Choice(a,chooser,dense))s.waitingOwn=own+1;return true;}
        if(bank.packets.size()==10&&Capture(a,"chooser")){NetTrace("SMOKE_PAID233_POOL role=%s group=%d epoch=%u distinct=10 normal=10 target=10 own=5",Role(a),s.group,s.epoch);Advance(a,2);}
    }
    else if(s.stage==2)
    {if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
     if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Paid batch Ready hover failed");}else if(Click(a,x,y))Advance(a,3);}
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000,"Paid batch skipped the actual full intro"))return true;
        if(s.playingFrame<0){s.playingFrame=a->mAppCounter;NetTrace("SMOKE_PAID233_INTRO_RETIRE_WAIT role=%s group=%d appFrame=%d allocatedPlants=%u allocatedZombies=%u",Role(a),s.group,s.playingFrame,b->mPlants.mSize,b->mZombies.mSize);return true;}if(a->mAppCounter<=s.playingFrame)return true;
        if(a->NetIsHost()){if(!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize&&b->mSunMoney==500,"Paid batch initial world is not empty after natural intro cleanup"))return true;if(!s.group)b->mSunMoney=2500;}
        s.sun=s.group?500:2500;NetTrace("SMOKE_PAID233_ENTRY role=%s group=%d epoch=%u initialSun=%d initialSunOverride=%d elapsedFrames=%d naturalIntroCleanup=1 otherWorldWrites=0",Role(a),s.group,s.epoch,s.sun,s.group?0:1,a->mAppCounter-s.playingFrame);Advance(a,4);
    }
    else if(s.stage==4)
    {if(b->mSunMoney!=s.sun)return true;if(!s.budget){s.budget=true;NetTrace("SMOKE_PAID233_BUDGET role=%s group=%d epoch=%u sun=%d money=%d",Role(a),s.group,s.epoch,b->mSunMoney,ctx->money);}if(Barrier(a,"BUDGET"))Advance(a,5);}
    else if(s.stage==5)
    {
        const int total=s.group==recipeRegressionGroup?int(std::size(recipeSteps)):s.group?2:10;
        if(s.buy<total){Purchase(a);return true;}
        Advance(a,6);
    }
    else if(s.stage==6)
    {
        if(s.group==recipeRegressionGroup)
        {
            if(a->NetIsHost()&&!s.setup)
            {
                s.setup=true;
                if(!Require(a,b->AddZombieInRow(ZOMBIE_NORMAL,4,-1)!=nullptr,"Recipe smoke could not spawn native walker"))return true;
            }
            Zombie* z=s.target?Target(b):nullptr;
            if(!s.target)
            {
                while(b->IterateZombies(z))
                    if(z->mRow==4&&z->mZombieType==ZOMBIE_NORMAL)
                    {
                        s.target=z->mNetId;
                        s.targetStartX=z->mPosX;
                        s.pauseSample=Samples(a);
                        break;
                    }
                return true;
            }
            // Wait until the moving zombie is inside both peers' visible lawns.
            if(!z||Samples(a)-s.pauseSample<16||z->mPosX>640.0f)return true;
            if(!Capture(a,"moving")||!Barrier(a,"MOVING"))return true;
            NetTrace("SMOKE_RECIPE271_COMPLETE role=%s recipes=3 walker=%u xStart=%.3f xNow=%.3f newSnapshots=%u",
                Role(a),s.target,s.targetStartX,z->mPosX,Samples(a)-s.pauseSample);
            a->CoopSmokeWriteResult(true,nullptr);
            return true;
        }
        if(!s.group){if(Final(a))Advance(a,9);return true;}
        const auto* recipe=FindFusionRecipe(24,21);if(!Require(a,recipe&&recipe->result==1088,"Paid source recipe24+21 does not produce1088"))return true;
        if(a->NetIsHost()&&!s.finalPause){s.finalPause=Key(a);return true;}if(!b->mPaused)return true;if(Setup(a)){s.pauseSample=0;Advance(a,7);}
    }
    else if(s.stage==7)
    {
        if(!b->mPaused){if(s.before){s.finalPause=false;s.pauseSample=0;Advance(a,8);}return true;}
        if(!s.pauseSample){s.pauseSample=Samples(a);return true;}if(Samples(a)-s.pauseSample<2)return true;
        if(!s.before){auto* z=Target(b);auto* p=Actor(b,s.actors[1]);if(!Require(a,z&&z->mHasHead&&z->mBodyHealth==3&&z->mBodyMaxHealth==3&&p&&p->mPlantHealth==4000,"Paid paused biter/child baseline differs"))return true;s.before=true;NetTrace("SMOKE_PAID233_BEFORE role=%s group=1 epoch=%u child=%u token=%u target=%u body=3 max=3 childHP=4000 freshSnapshots=2",Role(a),s.epoch,p->mNetId,s.actors[1].token,z->mNetId);}
        if(!Barrier(a,"BEFORE"))return true;if(a->NetIsHost()&&!s.resumed){s.resumed=Key(a);return true;}
    }
    else if(s.stage==8)
    {
        if(!s.resumeObserved){if(b->mPaused)return true;s.resumeObserved=true;NetTrace("SMOKE_PAID233_RESUMED role=%s group=1 epoch=%u paused=0 samples=%u",Role(a),s.epoch,Samples(a));}
        // Both peers must enter this unpaused phase before the host can pause
        // again. A first-tick lethal reflection may precede a client snapshot.
        if(!Barrier(a,"RESUMED"))return true;
        if(!s.resumeAcknowledged){s.resumeAcknowledged=true;NetTrace("SMOKE_PAID233_RESUME_ACK role=%s group=1 epoch=%u peers=2",Role(a),s.epoch);}
        if(!Outcome(a))return true;if(a->NetIsHost()&&!s.finalPause){s.finalPause=Key(a);return true;}
        if(Barrier(a,"REFLECT")){NetTrace("SMOKE_PAID233_REFLECT role=%s group=1 epoch=%u child=%u token=%u target=%u reflected=4 initialBody=3 dead=1 childHP=3996 originalBiteContinued=1 nativeCombat=1",Role(a),s.epoch,s.actors[1].id,s.actors[1].token,s.target);s.pauseSample=0;Advance(a,9);}
    }
    else if(s.stage==9)
    {
        if(a->NetIsHost()&&!s.finalPause){s.finalPause=Key(a);return true;}if(!b->mPaused)return true;
        if(!s.pauseSample){s.pauseSample=Samples(a);return true;}if(Samples(a)-s.pauseSample<2)return true;if(!Final(a)||!Capture(a,"complete")||!Barrier(a,"FINAL"))return true;
        NetTrace("SMOKE_PAID233_CASE role=%s group=%d epoch=%u purchases=%d sun=%d money=%d naturalRetirements=%d childHP=%d postSetupActorWrites=0",Role(a),s.group,s.epoch,s.buy,b->mSunMoney,ctx->money,s.group?0:2,s.group?3996:0);Advance(a,10);
    }
    return true;
}
}
