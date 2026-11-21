#include "../../LawnApp.h"
#include "../../Resources.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeGarlic.h"
#include "../Fusion/FusionNativeGloveUI.h"
#include "../Fusion/FusionRecipe.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/DDInterface.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace
{
using namespace LawnFusion;
using LawnNet::NetTrace;
struct Ref { uint16_t id=0;uint32_t token=0; };
struct Buy { int owner,kind;bool extra;int column,row,result,cost; };
constexpr Buy buys[]={{1,27,false,5,3,27,25},{0,29,false,2,1,29,50},{1,29,true,5,3,1130,100},
                     {0,29,false,5,2,29,50},{0,1,false,2,1,1,50}};
struct Track
{
    GarlicActionState previous{};int cycle=0,waitTick=-1,moveTick=-1,faceTick=-1,lastTick=-1;
    int moves=0;float moveX=0;bool wait=false,moving=false,firstDone=false,repeat=false,stall=false,face=false,secondDone=false;
};
struct State
{
    int stage=0,buy=0,waiting=0,selectedOwn=0,choiceDense=-1,sun=500,introChecks=0,gloveStep=0,pauseStep=0;
    uint32_t since=0,chat=0,introFirst=0,introLast=0,pickSample=0,lastSample=0,diagnostic=0,pauseSample=0,pauseSince=0,lastWheel=0,chooserDiagnostic=0;
    bool configured=false,ambient=false,picked=false,placed=false,observed=false,setup=false,idsRead=false,initialRecorded=false,resumed=false;
    bool moved=false,retarget=false,icePicked=false,icePlaced=false,iceObserved=false,finalKey=false,finalRecorded=false;
    std::array<Ref,5> plants{};std::array<uint16_t,2> targets{};std::array<Track,2> tracks{};
    std::array<GarlicActionState,2> paused{};std::array<float,2> pauseX{},pauseY{};int pausedTick=0;
    std::set<std::string> frames;std::map<std::string,int> draw;std::string ids;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool yes,const char* why){if(!yes)a->CoopSmokeFail(why);return yes;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();NetTrace("SMOKE_GARLIC225_STAGE role=%s stage=%d",Role(a),stage);}
bool Has(LawnApp* a,int who,const std::string& value){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==who&&line.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value)
{if(Has(a,Slot(a),value))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(value);return Has(a,Slot(a),value);}
bool Barrier(LawnApp* a,const std::string& value){Send(a,value);return Has(a,0,value)&&Has(a,1,value);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Garlic owned SDL click failed");
}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    // Same owned SDL wheel/settled-row path as the paired Recipient224 fixture.
    // Chooser mX already includes the viewport's native widescreen offset.
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(s.choiceDense!=index)
    {s.choiceDense=index;NetTrace("SMOKE_GARLIC225_CHOICE_TARGET role=%s own=%d dense=%d scroll=%.6g centerY=%.6g origin=%d,%d viewport=%d,%d",Role(a),s.selectedOwn,index,c->mScrollbar->mScrollValue,cy,c->mX,c->mY,a->mWidth,a->mHeight);}
    if(cy<158||cy>493)
    {
        if(s.lastWheel&&GetTickCount()-s.lastWheel<250)return false;s.lastWheel=GetTickCount();
        SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);
        SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;
        Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Garlic chooser owned SDL wheel failed");return false;
    }
    if(std::abs(c->mScrollAmount)>.05f)return false;
    if(!Click(a,float(c->mX+x+25),float(c->mY)+cy))return false;
    NetTrace("SMOKE_GARLIC225_CHOICE_SENT role=%s own=%d dense=%d scroll=%.6g centerY=%.6g x=%.6g y=%.6g nativeSDL=1",Role(a),s.selectedOwn,index,c->mScrollbar->mScrollValue,cy,float(c->mX+x+25),float(c->mY)+cy);
    return true;
}
bool BoardClick(LawnApp* a,float x,float y){return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Cell(LawnApp* a,int col,int row){return BoardClick(a,float(a->mBoard->GetCellCenterX(col,row)),float(a->mBoard->GridToPixelY(col,row)+50));}
bool Escape(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Garlic owned SDL Escape failed");}
bool Capture(LawnApp* a,const char* suffix)
{
    const std::string name="fusion_garlic_"+std::string(suffix);if(s.frames.contains(name))return true;
    if(!s.draw.contains(name)){s.draw[name]=a->mDrawCount;return false;}if(a->mDrawCount-s.draw[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Garlic native BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_GARLIC225_RENDER role=%s frame=%s freshDraws=%d",Role(a),name.c_str(),a->mDrawCount-s.draw[name]);return true;
}
Plant* Actor(Board* b,Ref ref)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mNetId==ref.id&&d&&d->instanceToken==ref.token)return p;}return nullptr;}
Plant* At(Board* b,int col,int row,int kind)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mPlantCol==col&&p->mRow==row&&d&&d->kind==kind)return p;}return nullptr;}
Zombie* Enemy(Board* b,uint16_t id){Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(!z->mDead&&z->mNetId==id)return z;return nullptr;}
SeedPacket* Card(Board* b,int who,int kind,bool extra)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(p);if(d&&d->owner==who&&d->kind==kind&&d->isExtra==extra)return p;}return nullptr;}
bool Held(Board* b,SeedPacket* packet)
{const auto* c=b->mCursorObject;return c&&packet&&c->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&c->mSeedBankIndex>=0&&c->mSeedBankIndex<b->mSeedBank->mNumPackets&&b->mSeedBank->mSeedPackets[c->mSeedBankIndex].mNetId==packet->mNetId;}
bool Pick(LawnApp* a,SeedPacket* p)
{if(!p||!p->mActive||p->mRefreshing)return false;auto* bank=a->mBoard->mSeedBank;return BoardClick(a,float(bank->mX+p->mX+p->mOffsetX+p->mWidth/2),float(bank->mY+p->mY+p->mHeight/2));}
bool Purchase(LawnApp* a)
{
    const auto q=buys[s.buy];auto* b=a->mBoard;auto* card=Card(b,q.owner,q.kind,q.extra);const auto* packet=GetNativePacketData(card);
    if(!Require(a,card&&packet&&packet->price==q.cost&&packet->rechargeSeconds==7.5f,"Garlic source packet cost/CD missing"))return false;
    if(Slot(a)==q.owner&&!s.picked){if(Pick(a,card)){s.picked=true;s.pickSample=Samples(a);}return false;}
    if(Slot(a)==q.owner&&!s.placed){if(!Held(b,card)||(a->NetIsClient()&&Samples(a)<=s.pickSample))return false;if(!Require(a,b->mSunMoney==s.sun,"Garlic pickup charged prematurely"))return false;s.placed=Cell(a,q.column,q.row);return false;}
    if(!s.observed)
    {
        auto* plant=At(b,q.column,q.row,q.result);const auto* data=GetNativePlantData(plant);if(!data)return false;
        const int parent=q.result==1130?27:kFusionNoPlant,used=q.result==1130?29:kFusionNoPlant;
        if(!Require(a,data->owner==q.owner&&data->packetId==card->mNetId&&!data->randomOrigin&&data->firstParent==parent&&data->secondParent==used&&
            data->instanceToken&&plant->mPlantHealth==300&&plant->mPlantMaxHealth==300&&b->mSunMoney==s.sun-q.cost&&card->mRefreshTime==750&&card->mRefreshing&&!card->mActive,
            "Garlic paid native identity/ancestry/HP/SUN/CD differs"))return false;
        if(q.result==1130&&!Require(a,plant->mNetId!=s.plants[0].id&&!Actor(b,s.plants[0]),"GarlicPot must replace the actual original Pot"))return false;
        s.plants[s.buy]={plant->mNetId,data->instanceToken};s.observed=true;
        NetTrace("SMOKE_GARLIC225_PURCHASE role=%s index=%d kind=%d id=%u token=%u packet=%u owner=%d extra=%d parents=%u,%u cost=%d sun=%d recharge=750 HP=300 nativeSDL=1",
            Role(a),s.buy,q.result,plant->mNetId,data->instanceToken,card->mNetId,q.owner,q.extra?1:0,data->firstParent,data->secondParent,q.cost,b->mSunMoney);
    }
    if(!Barrier(a,"G225_PAID_"+std::to_string(s.buy)))return false;
    s.sun-=q.cost;++s.buy;s.picked=s.placed=s.observed=false;s.since=GetTickCount();return true;
}
bool Same(const GarlicActionState& a,const GarlicActionState& b)
{return a.phase==b.phase&&a.certainRow==b.certainRow&&a.waitTicks==b.waitTicks&&a.moveElapsedTicks==b.moveElapsedTicks&&a.faceTicks==b.faceTicks&&a.originRow==b.originRow&&a.targetRow==b.targetRow&&a.startY==b.startY&&a.targetY==b.targetY;}
bool Setup(LawnApp* a)
{
    if(a->NetIsHost()&&!s.setup)
    {
        if(!Require(a,!a->mBoard->mZombies.mSize,"Garlic initial NPC setup needs empty world"))return false;
        std::ostringstream ids;ids<<"G225_IDS";
        for(int i=0;i<2;++i)
        {
            auto* plant=Actor(a->mBoard,s.plants[i?2:1]);auto* z=plant?a->mBoard->AddZombieInRow(ZOMBIE_NORMAL,plant->mRow,0):nullptr;
            auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;
            if(!Require(a,plant&&z&&body&&body->mAnimRate>0&&z->mVelX>0,"Garlic native walking biter factory failed"))return false;
            z->mBodyHealth=z->mBodyMaxHealth=5000;z->mPosY=z->GetPosYBasedOnRow(z->mRow);z->mY=int(z->mPosY);bool found=false;
            for(int dx=80;dx>=-80;--dx){z->mPosX=float(plant->mX+dx);z->mX=int(z->mPosX);if(z->FindPlantTarget(ATTACKTYPE_CHEW)==plant){found=true;break;}}
            if(!Require(a,found,"Garlic initial chew geometry failed"))return false;
            s.targets[i]=z->mNetId;ids<<' '<<z->mNetId;
            NetTrace("SMOKE_GARLIC225_NPC_SETUP index=%d id=%u row=%d HP=5000 x=%.6g y=%.6g vx=%.9g initialOnly=1",i,z->mNetId,z->mRow,z->mPosX,z->mPosY,z->mVelX);
        }
        s.ids=ids.str();s.setup=true;if(!Escape(a))return false;
    }
    if(a->NetIsHost())return Send(a,s.ids);
    for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.rfind("G225_IDS ",0)==0)
    {std::istringstream ids(line.mText.substr(9));unsigned x=0,y=0;if(!(ids>>x>>y)||!x||!y||x>65535||y>65535)return false;s.targets={uint16_t(x),uint16_t(y)};return true;}return false;
}
bool Observe(LawnApp* a)
{
    auto* b=a->mBoard;if(a->NetIsClient()&&Samples(a)==s.lastSample)return true;s.lastSample=Samples(a);
    for(int i=0;i<2;++i)
    {
        auto* z=Enemy(b,s.targets[i]);if(!Require(a,z&&z->mHasHead&&!z->IsDeadOrDying(),"Garlic control retired unexpectedly"))return false;
        GarlicActionState empty;const auto* stored=GetNativeZombieGarlicData(z);const auto& g=stored?*stored:empty;auto& t=s.tracks[i];
        if(a->NetIsHost()&&t.lastTick==b->mMainCounter)continue;
        if(!Require(a,NativeGarlicActionStateValid(g)&&FusionGarlicLogicalRow(g,z->mRow)==z->mRow&&!z->mYuckyFace&&!z->mYuckyFaceCounter,
            "Garlic canonical state/row activated a Classic Yuck clock"))return false;
        auto* body=a->ReanimationTryToGet(z->mBodyReanimID);const bool gross=body&&body->GetImageOverride("anim_head1")==IMAGE_REANIM_ZOMBIE_HEAD_GROSSOUT;
        if(!Require(a,body&&gross==(g.faceTicks!=0),"Garlic native head override differs from canonical face"))return false;
        const bool wasMoving=t.previous.phase==GarlicActionPhase::Moving,nowMoving=g.phase==GarlicActionPhase::Moving;
        if(wasMoving&&!nowMoving)
        {
            if(a->NetIsHost()&&!Require(a,t.previous.moveElapsedTicks==99&&z->mRow==t.previous.targetRow&&std::abs(z->mPosY-NativeGarlicRowY(z->mRow))<.01f&&z->mPosX<t.moveX-.5f,
                "Garlic actual100-move progress/final row/X walk differs"))return false;
            if(a->NetIsHost()&&t.cycle==1&&!Require(a,b->mMainCounter-t.moveTick==100,"Garlic first natural row move was not100 unpaused ticks"))return false;
            t.firstDone=true;if(t.cycle>=2)t.secondDone=true;
            NetTrace("SMOKE_GARLIC225_MOVED role=%s index=%d cycle=%d elapsed=%d row=%d Xwalk=%d face=%u",Role(a),i,t.cycle,a->NetIsHost()?b->mMainCounter-t.moveTick:-1,z->mRow,a->NetIsHost()?1:0,g.faceTicks);
        }
        if(g.phase==GarlicActionPhase::Waiting&&(t.previous.phase!=GarlicActionPhase::Waiting||g.certainRow!=t.previous.certainRow))
        {
            ++t.cycle;t.wait=true;t.waitTick=b->mMainCounter;
            auto* victim=Actor(b,s.plants[i?(t.cycle==1?2:3):1]);
            if(t.cycle==1&&!Require(a,victim&&victim->mPlantHealth==(i?290:285)&&g.certainRow==(i==1),"Garlic actual first source15/10 bite differs"))return false;
            if(a->NetIsHost()&&!Require(a,g.waitTicks==100,"Garlic initial wait must be100 actual ticks"))return false;
            if(i==1&&t.cycle==2)
            {if(!Require(a,victim&&victim->mPlantHealth==285&&g.faceTicks>0&&!g.certainRow,"Garlic repeat must begin during independent old face recovery"))return false;t.repeat=true;Capture(a,"repeat");}
            NetTrace("SMOKE_GARLIC225_WAIT role=%s index=%d cycle=%d remaining=%u certain=%d face=%u tick=%d",Role(a),i,t.cycle,g.waitTicks,g.certainRow?1:0,g.faceTicks,b->mMainCounter);
            Capture(a,"wait");
        }
        if(g.phase==GarlicActionPhase::Waiting&&i==0)
        {
            auto* replacement=Actor(b,s.plants[4]);if(replacement&&!Require(a,replacement->mPlantHealth==300,"Garlic Waiting allowed damage to a replacement ordinary target"))return false;
            if(a->NetIsHost()&&replacement&&!s.retarget&&z->FindPlantTarget(ATTACKTYPE_CHEW)==replacement)
            {s.retarget=true;NetTrace("SMOKE_GARLIC225_RETARGET actualOrdinarySunflower=%u HP=300 waitRemaining=%u globalEatBlocked=1 tick=%d",replacement->mNetId,g.waitTicks,b->mMainCounter);}
        }
        if(g.phase==GarlicActionPhase::Waiting)Capture(a,"wait");
        if(i==1&&t.repeat&&g.phase==GarlicActionPhase::Waiting&&g.faceTicks)Capture(a,"repeat");
        if(nowMoving&&!wasMoving)
        {
            if(a->NetIsHost()&&!Require(a,b->mMainCounter-t.waitTick==100&&g.moveElapsedTicks==0&&g.faceTicks==150,"Garlic wait100/rowstart/face150 ordering differs"))return false;
            t.moving=true;t.moveTick=b->mMainCounter;t.moveX=z->mPosX;t.faceTick=b->mMainCounter;
            NetTrace("SMOKE_GARLIC225_MOVE role=%s index=%d cycle=%d from=%d target=%d logical=%d sort=%d elapsed=%u face=%u waitElapsed=%d",Role(a),i,t.cycle,g.originRow,g.targetRow,z->mRow,FusionGarlicSortingRow(g,z->mRow),g.moveElapsedTicks,g.faceTicks,a->NetIsHost()?b->mMainCounter-t.waitTick:-1);Capture(a,"move");
        }
        if(nowMoving)
        {
            Capture(a,"move");
            const float y=float(double(g.startY)+(double(g.targetY)-g.startY)*g.moveElapsedTicks/100);
            const float actual=a->NetIsHost()?z->mPosY:z->mNetTargetY;
            if(!Require(a,std::abs(actual-y)<(a->NetIsHost()?.01f:.26f)&&z->mRenderOrder==Board::MakeRenderOrder(RENDER_LAYER_ZOMBIE,FusionGarlicSortingRow(g,z->mRow),4),"Garlic snapshot worldY/logicalrow/sorting mismatch"))return false;
            if(i==1&&t.cycle>=2&&z->mIceTrapCounter>0&&wasMoving&&g.moveElapsedTicks==t.previous.moveElapsedTicks)
            {if(!t.stall){NetTrace("SMOKE_GARLIC225_STALL role=%s index=1 elapsed=%u frozen=%d face=%u",Role(a),g.moveElapsedTicks,z->mIceTrapCounter,g.faceTicks);Capture(a,"stall");}t.stall=true;}
            if(i==1&&t.stall&&z->mIceTrapCounter>0)Capture(a,"stall");
            if(i==1&&t.cycle>=2&&t.stall&&!g.faceTicks&&z->mIceTrapCounter>0)
            {if(!t.face){if(a->NetIsHost()&&!Require(a,b->mMainCounter-t.faceTick==150,"Garlic face recovery was not independent150 ticks"))return false;NetTrace("SMOKE_GARLIC225_FACE role=%s index=1 elapsed=%u frozen=%d face=0 imageRestored=1 independent=1",Role(a),g.moveElapsedTicks,z->mIceTrapCounter);Capture(a,"face_recovered");}t.face=true;}
            if(i==1&&t.face&&z->mIceTrapCounter>0)Capture(a,"face_recovered");
        }
        t.previous=g;t.lastTick=b->mMainCounter;
    }
    const int expectedSun=s.sun-((s.buy==4&&s.observed)?buys[4].cost:0);
    return Require(a,b->mSunMoney==expectedSun&&GetNativeContext(b)->money==3000,"Garlic unexpected SUN/cash mutation");
}
bool MoveOriginal(LawnApp* a)
{
    if(!a->NetIsHost()||s.moved)return true;if(!s.tracks[0].wait)return true;
    auto* b=a->mBoard;auto* p=Actor(b,s.plants[1]);if(!Require(a,p!=nullptr,"Garlic original move source vanished"))return false;
    if(s.gloveStep==0){Sexy::Rect rect;b->GetZenButtonRect(OBJECT_TYPE_GLOVE,rect);if(!Require(a,rect.mWidth>0&&rect.mHeight>0,"Garlic native glove tool rectangle absent"))return false;if(BoardClick(a,float(rect.mX+rect.mWidth/2),float(rect.mY+rect.mHeight/2)))++s.gloveStep;return true;}
    if(s.gloveStep==1){if(b->mCursorObject->mCursorType!=CURSOR_TYPE_GLOVE)return true;if(Cell(a,2,1))++s.gloveStep;return true;}
    if(s.gloveStep==2){if(GetNativeGloveHeldPlant(b,0)!=p||b->mCursorObject->mCursorType!=CURSOR_TYPE_PLANT_FROM_GLOVE)return true;if(Cell(a,8,4))++s.gloveStep;return true;}
    if(p->mPlantCol==8&&p->mRow==4)
    {if(!Require(a,p->mPlantHealth==285&&!At(b,2,1,29),"Garlic actual glove resetHP/left original body"))return false;s.moved=true;NetTrace("SMOKE_GARLIC225_GLOVE id=%u token=%u from=2,1 to=8,4 sameGeneration=1 HP=285 sourceCellFreed=1 nativeSDL=1",p->mNetId,s.plants[1].token);}
    return true;
}
bool Ice(LawnApp* a)
{
    if(s.iceObserved)return true;auto* b=a->mBoard;auto* card=Card(b,0,10,false);
    if(auto* p=At(b,8,0,10))
    {const auto* d=GetNativePlantData(p);if(!Require(a,card&&d->owner==0&&d->packetId==card->mNetId&&b->mSunMoney==150&&card->mRefreshTime==5000,"Garlic paid natural Ice setup differs"))return false;s.iceObserved=true;s.sun=150;NetTrace("SMOKE_GARLIC225_ICE role=%s id=%u cost=75 sun=150 sourceFuse=151 nativeSDL=1 postSetupTimerWrites=0",Role(a),p->mNetId);return true;}
    if(!s.tracks[1].repeat)return true;
    if(a->NetIsHost()&&!s.icePicked){if(Pick(a,card))s.icePicked=true;return true;}
    if(a->NetIsHost()&&!s.icePlaced){if(!Held(b,card))return true;s.icePlaced=Cell(a,8,0);return true;}
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionGarlic(LawnApp* a)
{
    const auto* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_GARLIC"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>90000){char why[100];sprintf_s(why,"Garlic deadline stage=%d buy=%d pause=%d moved=%d retarget=%d",s.stage,s.buy,s.pauseStep,s.moved?1:0,s.retarget?1:0);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}if(a->mNetManager->GetPlayerCount()!=2)return true;
     if(a->NetIsClient()){Send(a,"G225_START");Advance(a,1);}else if(Has(a,1,"G225_START")){std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());Advance(a,1);}return true;}
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard))return true;auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;const auto& bank=ctx->selection.State();
    if(!Require(a,ctx->validation&&ctx->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Garlic lost real native world"))return true;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
        const std::array<std::pair<int,bool>,5> host={{{29,false},{10,false},{0,false},{1,false},{2,false}}},client={{{27,false},{29,true},{0,true},{1,true},{2,true}}};const auto& wanted=Slot(a)?client:host;
        int own=0;for(const auto& p:bank.packets)if(p.owner==Slot(a))
        {if(!Require(a,own<5&&p.kind==wanted[own].first&&p.isExtra==wanted[own].second,"Garlic actual chosen identity differs from requested card"))return true;++own;}
        if(own>s.selectedOwn){NetTrace("SMOKE_GARLIC225_CHOICE_ACK role=%s own=%d kind=%d extra=%d dense=%d scroll=%.6g",Role(a),own,wanted[own-1].first,wanted[own-1].second?1:0,s.choiceDense,chooser->mScrollbar->mScrollValue);s.selectedOwn=own;}
        if(!s.chooserDiagnostic||GetTickCount()-s.chooserDiagnostic>=10000)
        {NetTrace("SMOKE_GARLIC225_CHOOSER_GATE role=%s own=%d waiting=%d dense=%d scroll=%.6g range=%.6g inertia=%.6g total=%zu visible=1",Role(a),own,s.waiting,s.choiceDense,chooser->mScrollbar->mScrollValue,chooser->mScrollbar->mScrollRange,chooser->mScrollAmount,bank.packets.size());s.chooserDiagnostic=GetTickCount();}
        if(own<s.waiting)return true;
        if(own<5){int dense=-1;for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].kind==wanted[own].first&&ctx->eligible[i].isExtra==wanted[own].second)dense=int(i);if(!Require(a,dense>=0,"Garlic requested source eligible card absent"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;}
        if(bank.packets.size()==10)Advance(a,2);
    }
    else if(s.stage==2)
    {if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
     if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Garlic Ready hover failed");}else if(Click(a,x,y))Advance(a,3);}
    else if(s.stage==3)
    {if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;
     if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000&&b->mSunMoney==500,"Garlic full intro skipped"))return true;const auto* recipe=FindFusionRecipe(27,29);if(!Require(a,recipe&&recipe->result==1130,"Garlic source recipe27+29 missing"))return true;
     if(a->NetIsHost()&&!s.ambient){b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;s.ambient=true;NetTrace("SMOKE_GARLIC225_AMBIENT oneTimeInitialOnly=1");}Advance(a,4);}
    else if(s.stage==4){if(s.buy<4){Purchase(a);return true;}if(Capture(a,"paid")&&Barrier(a,"G225_PAID_DONE"))Advance(a,5);}
    else if(s.stage==5){if(Setup(a))Advance(a,6);}
    else if(s.stage==6)
    {if(!b->mPaused){if(s.initialRecorded&&Has(a,0,"G225_BASELINE")&&Has(a,1,"G225_BASELINE")){s.pauseSample=0;Advance(a,7);}return true;}if(!s.pauseSample){s.pauseSample=Samples(a);return true;}if(Samples(a)-s.pauseSample<2)return true;
     if(!s.initialRecorded){for(int i=0;i<2;++i){auto* victim=Actor(b,s.plants[i?2:1]);if(!Require(a,Enemy(b,s.targets[i])&&victim&&victim->mPlantHealth==300,"Garlic setup acted before paused baseline"))return true;}s.initialRecorded=true;NetTrace("SMOKE_GARLIC225_BASELINE role=%s targets=%u,%u actualNative=1 sourceHPFull=1 initialNPCWritesOnly=1",Role(a),s.targets[0],s.targets[1]);}
     if(!Barrier(a,"G225_BASELINE"))return true;if(a->NetIsHost()&&!s.resumed){s.resumed=Escape(a);return true;}}
    else if(s.stage==7)
    {
        if(!s.diagnostic||GetTickCount()-s.diagnostic>=10000)
        {NetTrace("SMOKE_GARLIC225_GATE role=%s pause=%d moved=%d retarget=%d ice=%d Acycle=%d Adone=%d Bcycle=%d Brepeat=%d Bstall=%d Bface=%d Bdone=%d frames=%zu samples=%u",Role(a),s.pauseStep,s.moved?1:0,s.retarget?1:0,s.iceObserved?1:0,s.tracks[0].cycle,s.tracks[0].firstDone?1:0,s.tracks[1].cycle,s.tracks[1].repeat?1:0,s.tracks[1].stall?1:0,s.tracks[1].face?1:0,s.tracks[1].secondDone?1:0,s.frames.size(),Samples(a));s.diagnostic=GetTickCount();}
        // Ice placement can change the ledger in this input frame; adopt its
        // actual authoritative actor before the ordinary economy assertion.
        auto* original=Actor(b,s.plants[1]);
        if(s.buy==4&&original&&original->mPlantCol==8&&original->mRow==4)Purchase(a);
        if(!Ice(a)||!Observe(a)||!MoveOriginal(a))return true;
        auto* z=Enemy(b,s.targets[1]);const auto* garlic=GetNativeZombieGarlicData(z);
        if(s.pauseStep==0&&garlic&&garlic->phase==GarlicActionPhase::Moving&&s.tracks[1].cycle==1&&garlic->moveElapsedTicks>=10)
        {if(a->NetIsHost()){if(Escape(a))s.pauseStep=1;}else if(b->mPaused)s.pauseStep=1;}
        if(s.pauseStep==1&&b->mPaused)
        {
            if(!s.pauseSample){s.pauseSample=Samples(a);s.pauseSince=GetTickCount();return true;}if(Samples(a)-s.pauseSample<2)return true;
            for(int i=0;i<2;++i){auto* e=Enemy(b,s.targets[i]);GarlicActionState empty;const auto* p=GetNativeZombieGarlicData(e);s.paused[i]=p?*p:empty;s.pauseX[i]=a->NetIsHost()?e->mPosX:e->mNetTargetX;s.pauseY[i]=a->NetIsHost()?e->mPosY:e->mNetTargetY;}s.pausedTick=b->mMainCounter;s.pauseSample=Samples(a);s.pauseStep=2;
        }
        if(s.pauseStep==2)
        {
            if(Samples(a)-s.pauseSample<2||GetTickCount()-s.pauseSince<250)return true;
            for(int i=0;i<2;++i){auto* e=Enemy(b,s.targets[i]);GarlicActionState empty;const auto* p=GetNativeZombieGarlicData(e);if(!Require(a,b->mPaused&&b->mMainCounter==s.pausedTick&&Same(s.paused[i],p?*p:empty)&&std::abs((a->NetIsHost()?e->mPosX:e->mNetTargetX)-s.pauseX[i])<.01f&&std::abs((a->NetIsHost()?e->mPosY:e->mNetTargetY)-s.pauseY[i])<.01f,"Garlic paused state/world advanced"))return true;}
            if(!Capture(a,"pause")||!Barrier(a,"G225_PAUSE"))return true;NetTrace("SMOKE_GARLIC225_PAUSE role=%s freshSnapshots=4 unchanged=1 nativeSDL=1",Role(a));s.pauseStep=3;
        }
        if(s.pauseStep==3){if(a->NetIsHost()){Escape(a);s.pauseStep=4;}else if(!b->mPaused)s.pauseStep=4;return true;}
        if(s.pauseStep==4&&!b->mPaused&&s.iceObserved&&s.tracks[0].firstDone&&s.tracks[1].repeat&&s.tracks[1].stall&&s.tracks[1].face&&s.tracks[1].secondDone)
        {if(!Send(a,"G225_DONE"))return true;if(Has(a,0,"G225_DONE")&&Has(a,1,"G225_DONE"))Advance(a,8);}
    }
    else if(s.stage==8)
    {if(a->NetIsHost()&&!s.finalKey){s.finalKey=Escape(a);return true;}if(!b->mPaused)return true;
     if(a->NetIsHost()&&!Require(a,s.moved&&s.retarget,"Garlic global replacement-target regression not observed"))return true;
     auto* moved=Actor(b,s.plants[1]);auto* replacement=Actor(b,s.plants[4]);if(!Require(a,s.buy==5&&moved&&moved->mPlantCol==8&&moved->mRow==4&&moved->mPlantHealth==285&&replacement&&replacement->mPlantHealth==300,"Garlic canonical movedactor/untouched ordinarySunflower mismatch"))return true;
     for(int i=0;i<2;++i){auto* z=Enemy(b,s.targets[i]);if(!Require(a,z&&z->mBodyHealth==4980&&z->mBodyMaxHealth==5000,"Garlic natural sourceIce must damage both controls exactly20"))return true;}
     if(!Capture(a,"complete")||!Barrier(a,"G225_COMPLETE"))return true;
     NetTrace("SMOKE_GARLIC225_GLOVE_ADOPT role=%s id=%u token=%u column=8 row=4 HP=285 replacementHP=300",Role(a),moved->mNetId,s.plants[1].token);
     NetTrace("SMOKE_GARLIC225_COMPLETE role=%s paidPlants=5 realRecipe=27,29,1130 firstDamage=15,10 wait=100 move=100 face=150 repeatWhileFace=1 freezeStall=1 independentFace=1 coldDamage20=1 pause=1 sun=150 money=3000 postSetupActorWrites=0 nativeBoard=1 lateObserver=NotExecuted malformedRuntime=NotExecuted",Role(a));Advance(a,9);a->CoopSmokeWriteResult(true,nullptr);}
    return true;
}
}
