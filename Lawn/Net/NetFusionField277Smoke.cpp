#include "NetFusionField277Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../CursorObject.h"
#include "../SeedPacket.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionAnimation.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/Image.h"
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace LawnFusion;using LawnNet::NetTrace;
constexpr std::array<uint8_t,3> backgrounds={BACKGROUND_2_NIGHT,BACKGROUND_3_POOL,BACKGROUND_1_DAY};
constexpr std::array<const char*,3> names={"night","pool","day"};
struct Ref{uint16_t id=0;uint32_t token=0,localUID=0;ReanimationID body=REANIMATIONID_NULL;};
struct Water
{
    uint32_t clock=0;unsigned emitters=0,particles=0,visible=0;float alpha=0;
    float left=0,top=0,right=0,bottom=0;
};
struct State
{
    int group=0,stage=0,waiting=0,entry=-1;uint32_t started=0,since=0,chat=0,wheel=0,epoch=0,previousEpoch=0;
    uint32_t introFirst=0,introLast=0,introChecks=0,samples=0,actionTick=0,pauseTick=0,pausedClock=0,resumeSamples=0;
    unsigned totalFrames=0;bool setup=false,hiddenClicked=false,pauseRequested=false,resumeRequested=false;
    bool clientClicked=false,hostClicked=false,resumeSeen=false,waterAdvanced=false,initialPref=false;
    uint32_t waterUID=0,canonicalGrid=0;const TodParticleDefinition* waterDefinition=nullptr;const TodParticleDefinition* previousWaterDefinition=nullptr;
    std::array<Ref,4> plants{};std::set<std::string> frames;std::map<std::string,int> draws;
}s;
bool nodePathsDone=false;
bool Enabled(const char* name){const auto* v=std::getenv(name);return v&&std::strcmp(v,"1")==0;}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool okay,const char* why){if(!okay)a->CoopSmokeFail(why);return okay;}
void Advance(LawnApp* a,int stage)
{s.stage=stage;s.since=GetTickCount();NetTrace("SMOKE_FIELD277_STAGE role=%s biome=%s stage=%d epoch=%u",Role(a),names[size_t(s.group)],stage,s.epoch);}
bool Has(LawnApp* a,int who,const std::string& value)
{for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==who&&line.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value)
{
    if(!Require(a,value.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Field277 proof exceeds native chat limit"))return false;
    if(Has(a,Slot(a),value))return true;if(s.chat&&GetTickCount()-s.chat<850)return false;
    s.chat=GetTickCount();a->mNetManager->SendChat(value);return Has(a,Slot(a),value);
}
bool Barrier(LawnApp* a,const std::string& label)
{const auto value="F277_"+std::to_string(s.group)+"_"+std::to_string(s.epoch)+"_"+label;Send(a,value);return Has(a,0,value)&&Has(a,1,value);}
bool Click(LawnApp* a,float x,float y,bool click=true)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    if(!click)return Require(a,SDL_PushEvent(&m),"Field277 owned SDL hover failed");
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Field277 owned SDL click failed");
}
bool Escape(LawnApp* a)
{
    SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;
    SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;
    return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Field277 native SDL pause/resume failed");
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_field277_"+std::string(names[size_t(s.group)])+"_"+suffix;if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requested=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requested))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Field277 fresh owned native BMP failed"))return false;
    s.frames.insert(name);++s.totalFrames;
    NetTrace("SMOKE_FIELD277_RENDER role=%s biome=%s frame=%s requestedDraw=%d capturedDraw=%d",Role(a),names[size_t(s.group)],name.c_str(),requested,a->mCoopSmokeFrameDrawCount);return true;
}
bool Preference(LawnApp* a,bool expected)
{
    LawnNet::NetConfig loaded;
    return Require(a,!a->mCoopSmokeManifest.mConfigPath.empty()&&a->mNetManager->mConfig.mFusionClassicField==expected&&
        LawnNet::NetLoadConfig(loaded)&&loaded.mFusionClassicField==expected,"Field277 local/native persisted preference changed or private readback failed");
}
bool NodePaths(LawnApp* a)
{
    if(nodePathsDone)return true;
    unsigned checks=0;std::string error;
    const auto check=[&](bool okay,const char* why)
    {++checks;if(!okay)NetTrace("SMOKE_FIELD277_NODEPATHS_FAILED role=%s check=%u reason=%s loaderError=%s",Role(a),checks,why,error.c_str());return Require(a,okay,why);};
    try
    {
        std::wstring executable(32768,L'\0');const DWORD length=GetModuleFileNameW(nullptr,executable.data(),DWORD(executable.size()));
        if(!check(length&&length<executable.size(),"Field277 loader regression cannot locate actual runtime assets"))return false;
        executable.resize(length);const auto source=std::filesystem::path(executable).parent_path()/L"fusion"/L"pilot"/L"background2"/L"animation.json";
        std::ifstream input(source);if(!check(bool(input),"Field277 actual full Night package missing"))return false;
        using Json=nlohmann::json;const auto original=Json::parse(input);
        constexpr const char* duplicate="floor0/ice/road/ice";
        unsigned copies=0;for(const auto& node:original.at("nodes"))if(node.at("path")==duplicate)++copies;
        if(!check(copies==11&&original.at("nodes").size()==90&&original.at("sprites").size()==68,
            "Field277 genuine full Night hierarchy was stripped or duplicate closure changed"))return false;
        FusionAnimation animation;
        if(!check(animation.Load(a,source.string(),error),"Field277 actual native full Night Load failed"))return false;
        const auto* state=animation.GetDefaultStateName();
        if(!check(state&&*state,"Field277 actual Night default state missing"))return false;
        std::array<float,6> matrix{};bool active=false;
        if(!check(animation.TryGetNodeTransform("bg/bg",matrix,state,0)&&std::all_of(matrix.begin(),matrix.end(),[](float v){return std::isfinite(v);}),
            "Field277 unique source bg/bg matrix unavailable"))return false;
        const auto backgroundMatrix=matrix;
        if(!check(animation.TryGetNodeActivity("bg/bg",active,state,0)&&active,"Field277 unique source bg/bg activity differs"))return false;
        constexpr std::array<float,6> sentinel={901,902,903,904,905,906};matrix=sentinel;
        if(!check(!animation.TryGetNodeTransform(duplicate,matrix,state,0)&&matrix==sentinel,"Field277 ambiguous transform query changed output"))return false;
        for(bool initial:{false,true})
        {active=initial;if(!check(!animation.TryGetNodeActivity(duplicate,active,state,0)&&active==initial,"Field277 ambiguous activity query changed output"))return false;}
        float x=901,y=902;if(!check(!animation.TryGetNodeOffset(duplicate,x,y,state,0)&&x==901&&y==902,"Field277 ambiguous offset query changed output"))return false;
        // One loaded genuine model exercises all targeted-option entry points;
        // each failure must preserve an unrelated unique query's output.
        std::array<FusionAnimationDrawOptions,9> options;
        options[0].hiddenNodePaths.push_back(duplicate);
        options[1].nodeActivityOverrides.push_back({duplicate,false});
        options[2].nodeRotations.push_back({duplicate,.5f,false});
        options[3].nodeWorldPositions.push_back({duplicate,1,2});
        options[4].spriteReplacements.push_back({duplicate,original.at("spriteVariants").begin().key()});
        options[5].spriteSortingOverrides.push_back({duplicate,0,1});
        options[6].sortingGroupOverrides.push_back({duplicate,0,1});
        options[7].maskSortingOverrides.push_back({duplicate,0,0,0,1});
        options[8].spritePathPrefix=duplicate;
        for(const auto& option:options)
        {
            matrix=sentinel;active=true;
            if(!check(!animation.TryGetNodeTransform("bg/bg",matrix,state,0,nullptr,-1,1,&option)&&matrix==sentinel,
                "Field277 ambiguous runtime override partially changed transform output"))return false;
            if(!check(!animation.TryGetNodeActivity("bg/bg",active,state,0,nullptr,-1,1,&option)&&active,
                "Field277 ambiguous runtime override partially changed activity output"))return false;
        }
        if(!check(!a->mCoopSmokeManifest.mAppDataDir.empty(),"Field277 negative loader fixtures require a private appdata directory"))return false;
        const auto directory=std::filesystem::absolute(std::filesystem::u8path(a->mCoopSmokeManifest.mAppDataDir))/L"work"/L"field277-nodepaths"/std::to_string(GetCurrentProcessId());
        std::filesystem::create_directories(directory);std::set<std::filesystem::path> textures;
        for(const auto& variant:original.at("spriteVariants"))textures.insert(std::filesystem::u8path(variant.at("texture").get<std::string>()));
        for(const auto& relative:textures)
        {
            if(relative.is_absolute()||std::any_of(relative.begin(),relative.end(),[](const auto& component){return component==L"..";}))
                throw std::runtime_error("Night fixture texture path leaves its package");
            const auto destination=directory/relative;std::filesystem::create_directories(destination.parent_path());
            std::filesystem::copy_file(source.parent_path()/relative,destination,std::filesystem::copy_options::skip_existing);
        }
        auto changed=original;auto clip=changed.at("clips").begin();
        if(!check(clip!=changed.at("clips").end()&&!clip.value().at("curves").empty(),"Field277 actual Night has no valid curve to clone"))return false;
        const auto clipName=clip.key();clip.value().at("curves").at(0)["path"]=duplicate;
        const auto ambiguous=directory/L"ambiguous-curve.json";
        {std::ofstream output(ambiguous);output<<changed.dump();if(!output)throw std::runtime_error("cannot write private ambiguous Night fixture");}
        FusionAnimation rejected;error.clear();
        if(!check(!rejected.Load(a,ambiguous.string(),error)&&!rejected.IsLoaded()&&error==std::string("ambiguous transform path: ")+duplicate,
            "Field277 ambiguous curve did not reject with its exact source-path error"))return false;
        changed.at("clips").at(clipName).at("curves").at(0)["path"]="fixture277/missing-transform";
        const auto missing=directory/L"missing-curve.json";
        {std::ofstream output(missing);output<<changed.dump();if(!output)throw std::runtime_error("cannot write private missing-curve Night fixture");}
        FusionAnimation allowed;error.clear();
        if(!check(allowed.Load(a,missing.string(),error),"Field277 existing missing-transform curve no-op was broken"))return false;
        matrix=sentinel;active=false;const auto* missingState=allowed.GetDefaultStateName();
        if(!check(missingState&&allowed.TryGetNodeTransform("bg/bg",matrix,missingState,0)&&matrix==backgroundMatrix&&
            allowed.TryGetNodeActivity("bg/bg",active,missingState,0)&&active,"Field277 missing-curve no-op damaged the genuine unique hierarchy"))return false;
        nodePathsDone=true;
        NetTrace("SMOKE_FIELD277_NODEPATHS role=%s checks=%u fullNightNodes=90 fullNightSprites=68 duplicateIndexedNodes=11 actualNativeLoads=3 uniqueMatrixActivity=1 ambiguousQueriesUnchanged=1 ambiguousOptionsUnchanged=9 ambiguousCurveExactError=1 missingCurveNoop=1 privateCopies=2 runtimeAssetsWritten=0 fakeDrawCalls=0",Role(a),checks);
        return true;
    }
    catch(const std::exception& e)
    {NetTrace("SMOKE_FIELD277_NODEPATHS_FAILED role=%s checks=%u error=%s",Role(a),checks,e.what());return Require(a,false,"Field277 private actual Night loader regression threw; inspect node-path trace");}
}
bool Choose(LawnApp* a)
{
    const auto* c=GetNativeContext(a->mBoard);auto* chooser=a->mSeedChooserScreen;
    if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!a->mBoard->mCutScene->mSeedChoosing)return false;
    const auto& bank=c->selection.State();const int slot=Slot(a);std::vector<int> normal;
    for(size_t i=0;i<c->eligible.size();++i)if(!c->eligible[i].isExtra)normal.push_back(int(i));
    if(!Require(a,bank.activeMask==3&&bank.targetCount==10&&bank.quotas[0]+bank.quotas[1]==10&&
        normal.size()>=bank.targetCount,"Field277 validation chooser has insufficient real ordinary cards/quotas"))return false;
    int own=0;for(const auto& packet:bank.packets)own+=packet.owner==slot;if(own<s.waiting)return false;
    if(own<bank.quotas[slot])
    {
        const int offset=slot?bank.quotas[0]:0;int x=0,y=0;chooser->GetSeedPositionInChooser(normal[size_t(offset+own)],x,y);
        const float cy=float(y+35-int(chooser->mScrollbar->mScrollValue));
        if(cy<158||cy>493)
        {
            if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();
            SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(chooser->mX+300);m.motion.y=float(chooser->mY+300);
            SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;
            Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Field277 native chooser scroll failed");return false;
        }
        if(std::abs(chooser->mScrollAmount)<=.05f&&Click(a,float(chooser->mX+x+25),float(chooser->mY)+cy))s.waiting=own+1;
        return false;
    }
    return bank.packets.size()==bank.targetCount&&Capture(a,"chooser")&&Barrier(a,"CHOOSER");
}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;
    if(!Require(a,a->NetIsHost()&&!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize,"Field277 setup must be initial empty validation world")||
        !Require(a,ConfigureNativeFusionBackground(b,backgrounds[size_t(s.group)]),"Field277 initial native biome configuration refused"))return false;
    const int last=GetNativeFusionRowCount(b)-1;std::string error;
    if(!Require(a,EnsureNativePlantVisuals(b,0,&error),"Field277 genuine Pea0 source resource unavailable"))return false;
    for(int i=0;i<4;++i)
    {
        NativePlantData fresh;fresh.kind=0;fresh.owner=LawnNet::NET_SHARED_OWNER;
        auto* p=b->AddPlant(i%2?9:0,i/2?last:0,GetNativeSeedForPlant(0),SEED_NONE,&fresh);const auto* d=GetNativePlantData(p);
        if(!Require(a,p&&d&&p->mNetId&&d->instanceToken&&!p->mDead,"Field277 initial typed corner factory failed"))return false;
        NetTrace("SMOKE_FIELD277_BIRTH biome=%s index=%d id=%u token=%u column=%d row=%d initialFactoryOnly=1 acquisition=NotExecuted",names[size_t(s.group)],i,p->mNetId,d->instanceToken,p->mPlantCol,p->mRow);
    }
    s.setup=true;return true;
}
bool Geometry(LawnApp* a)
{
    auto* b=a->mBoard;const int rows=s.group==1?6:5;
    if(!Require(a,unsigned(b->mBackground)==backgrounds[size_t(s.group)]&&b->GetColumnCount()==10&&GetNativeFusionRowCount(b)==rows,
        "Field277 actual native biome/ten-column geometry differs"))return false;
    for(int row=0;row<rows;++row)for(int col=0;col<10;++col)
    {
        const int x=b->GetCellCenterX(col,row),y=b->GridToPixelY(col,row)+40;const bool water=s.group==1&&(row==2||row==3);
        if(!Require(a,b->IsValidGridCell(col,row)&&b->PixelToGridX(x,y)==col&&b->PixelToGridY(x,y)==row&&
            b->mPlantRow[row]==(water?PLANTROW_POOL:PLANTROW_NORMAL)&&
            b->GetGridSquareTypeAt(col,row)==(water?GRIDSQUARE_POOL:GRIDSQUARE_GRASS),"Field277 cell roundtrip/row terrain failed, including last column/Pool row5"))return false;
    }
    return true;
}
bool Actors(LawnApp* a,bool bind=false)
{
    auto* b=a->mBoard;Plant* p=nullptr;unsigned seen=0;const int last=GetNativeFusionRowCount(b)-1;
    while(b->IteratePlants(p))
    {
        const auto* d=GetNativePlantData(p);const int i=(p->mRow==0?0:p->mRow==last?2:-4)+(p->mPlantCol==0?0:p->mPlantCol==9?1:-4);
        if(!Require(a,i>=0&&i<4&&!(seen&(1U<<i))&&d&&d->kind==0&&p->mNetId&&d->instanceToken&&
            p->mIsOnBoard&&!p->mSquished&&p->mPlantHealth==300&&p->mPlantMaxHealth==300&&p->mSeedType==SEED_PEASHOOTER&&
            p->mNetGhost==a->NetIsClient()&&p->mX==b->GetPlantOriginX(p->mPlantCol,p->mRow)&&p->mY==b->GridToPixelY(p->mPlantCol,p->mRow)&&
            d->owner==LawnNet::NET_SHARED_OWNER&&!d->packetId&&!d->randomOrigin&&
            d->firstParent==kFusionNoPlant&&d->secondParent==kFusionNoPlant&&a->ReanimationTryToGet(p->mBodyReanimID),
            "Field277 corner actor/raw carrier/provenance/HP differs"))return false;
        const uint32_t uid=b->mPlants.DataArrayGetID(p);auto& ref=s.plants[size_t(i)];
        if(bind&&!ref.id)ref={p->mNetId,d->instanceToken,uid,p->mBodyReanimID};
        if(!Require(a,(a->NetIsClient()||d->instanceToken==uid)&&ref.id==p->mNetId&&ref.token==d->instanceToken&&ref.localUID==uid&&ref.body==p->mBodyReanimID,
            "Field277 skin/pause action replaced a full native generation or body"))return false;
        seen|=1U<<i;
    }
    return seen==15&&Require(a,!b->mZombies.mSize,"Field277 validation gained an unexpected zombie");
}
bool Field(LawnApp* a,bool expectedPref,bool hidden,NativeFieldView277& view)
{
    // Background metadata is adopted after this frame's field UI update.
    // Observe resource reconciliation before asserting the resulting controls.
    if(!QueryNativeFusionField277(a->mBoard,view)||!view.artReady)return false;
    const auto* c=GetNativeContext(a->mBoard);const auto* field=c->fieldButton.get();const auto* menu=a->mBoard->mMenuButton;
    if(!Require(a,field&&menu&&field->mParentWidget==a->mBoard&&field->mDrawStoneButton&&field->mX==menu->mX&&
        field->mY==menu->mY+menu->mHeight-4&&field->mWidth==menu->mWidth&&field->mHeight==menu->mHeight,"Field277 FIELD is not the real native stone button below Menu"))return false;
    if(!Require(a,hidden?(field->mBtnNoDraw&&field->mDisabled&&!c->fieldButtonPressed&&!c->fieldButtonReleasePending):
        (a->mBoard->mPaused||(!field->mBtnNoDraw&&!field->mDisabled&&field->mLabel==_S("FIELD"))),"Field277 FIELD visibility/interaction policy differs"))return false;
    return Require(a,view.background==backgrounds[size_t(s.group)]&&view.sourceSelected==(hidden||!expectedPref),
        "Field277 actual selected source/native field ignored the per-viewer/biome policy");
}
bool FieldClick(LawnApp* a,bool hidden)
{
    auto* field=GetNativeContext(a->mBoard)->fieldButton.get();if(!field)return false;
    const auto pos=field->mParentWidget->GetAbsPos();
    const float x=float(pos.mX+field->mX)+field->mWidth*.5f,y=float(pos.mY+field->mY)+field->mHeight*.5f;
    if(!hidden&&!field->IsMouseOver()){Click(a,x,y,false);return false;}
    return Click(a,x,y);
}
uint32_t Hash(const LawnNet::NetBuffer& bytes){return LawnNet::NetFnv1a32(bytes.Data(),bytes.Size());}
bool Proof(LawnApp* a,const char* label)
{
    LawnNet::NetBuffer bytes;auto* b=a->mBoard;bytes.WriteU8(uint8_t(b->mBackground));bytes.WriteU8(uint8_t(GetNativeFusionRowCount(b)));
    for(int i=0;i<4;++i)
    {
        Plant* p=b->mPlants.DataArrayTryToGet(s.plants[size_t(i)].localUID);const auto* d=GetNativePlantData(p);if(!p||!d)return false;
        bytes.WriteU16(p->mNetId);bytes.WriteU32(d->instanceToken);bytes.WriteU16(d->kind);bytes.WriteI32(p->mPlantHealth);bytes.WriteI32(p->mPlantMaxHealth);
        bytes.WriteU8(uint8_t(p->mPlantCol));bytes.WriteU8(uint8_t(p->mRow));bytes.WriteU8(d->owner);bytes.WriteU16(d->packetId);
        bytes.WriteU16(d->firstParent);bytes.WriteU16(d->secondParent);
        bytes.WriteI32(p->mX);bytes.WriteI32(p->mY);
    }
    for(int row=0;row<GetNativeFusionRowCount(b);++row)for(int col=0;col<10;++col)bytes.WriteU8(uint8_t(b->GetGridSquareTypeAt(col,row)));
    const uint32_t hash=Hash(bytes);if(!s.canonicalGrid)s.canonicalGrid=hash;
    if(!Require(a,hash==s.canonicalGrid,"Field277 local skin/pause changed the original canonical actors/raw grid"))return false;
    char digest[64];sprintf_s(digest,"%s_%08x",label,hash);return Barrier(a,digest);
}
bool WaterState(LawnApp* a,const NativeFieldView277& view,Water& out)
{
    auto* holder=a->mEffectSystem?a->mEffectSystem->mParticleHolder:nullptr;if(!holder)return false;
    unsigned owners=0;TodParticleSystem* ps=nullptr;
    if(s.group!=1)
    {
        if(!Require(a,!view.waterResource&&!view.waterSystemId,"Field277 non-Pool biome retained its source water owner"))return false;
        while(holder->mParticleSystems.IterateNext(ps))if(!ps->mDead&&s.previousWaterDefinition&&ps->mParticleDef==s.previousWaterDefinition)++owners;
        return Require(a,!owners,"Field277 previous Pool water leaked into a fresh biome/epoch");
    }
    if(!view.waterSystemId)return false;
    ps=holder->mParticleSystems.DataArrayTryToGet(view.waterSystemId);
    if(!Require(a,view.waterResource==6000&&ps&&!ps->mDead&&!ps->mIsAttachment&&ps->mEffectType==PARTICLE_FUSION_EMBEDDED269&&
        ps->mParticleHolder==holder&&ps->mParticleDef&&std::isfinite(view.waterX)&&std::isfinite(view.waterY),"Field277 source6000 is not an actual independent native Tod owner"))return false;
    if(!s.waterUID){s.waterUID=view.waterSystemId;s.waterDefinition=ps->mParticleDef;}
    if(!Require(a,s.waterUID==view.waterSystemId&&s.waterDefinition==ps->mParticleDef,"Field277 Pool water owner restarted/replaced during pause/resume"))return false;
    TodParticleSystem* other=nullptr;while(holder->mParticleSystems.IterateNext(other))if(!other->mDead&&other->mParticleDef==s.waterDefinition)++owners;
    if(!Require(a,owners==1,"Field277 source6000 has duplicate native owners"))return false;
    LawnNet::NetBuffer clock;clock.WriteU32(s.waterUID);
    for(auto* node=ps->mEmitterList.mHead;node;node=node->mNext)
    {
        auto* e=holder->mEmitters.DataArrayTryToGet(uint32_t(node->mValue));
        if(!Require(a,e&&!e->mDead&&e->mParticleSystem==ps&&e->mEmitterDef&&e->mSystemDuration>0&&
            std::abs(e->mSystemCenter.x-view.waterX)<.1f&&std::abs(e->mSystemCenter.y-view.waterY)<.1f,"Field277 actual water emitter/anchor differs from observer"))return false;
        ++out.emitters;clock.WriteU32(uint32_t(node->mValue));clock.WriteI32(e->mSystemAge);clock.WriteI32(e->mParticlesSpawned);
        for(auto* particleNode=e->mParticleList.mHead;particleNode;particleNode=particleNode->mNext)
        {
            auto* p=holder->mParticles.DataArrayTryToGet(uint32_t(particleNode->mValue));if(!p)return Require(a,false,"Field277 native water particle generation vanished");
            ++out.particles;clock.WriteU32(uint32_t(particleNode->mValue));clock.WriteI32(p->mParticleAge);clock.WriteF32(p->mPosition.x);clock.WriteF32(p->mPosition.y);
            ParticleRenderParams render{};if(!TodParticleEmitter::GetRenderParams(p,&render))continue;
            auto* image=e->mImageOverride?e->mImageOverride:e->mEmitterDef->mImage;
            if(!image||!std::isfinite(render.mAlpha)||render.mAlpha<=0||!std::isfinite(render.mParticleScale)||render.mParticleScale==0||
                !std::isfinite(render.mParticleStretch)||!std::isfinite(render.mSpinPosition)||
                !std::isfinite(render.mPosX)||!std::isfinite(render.mPosY))continue;
            const float w=float(image->GetCelWidth())*std::abs(render.mParticleScale),h=float(image->GetCelHeight())*std::abs(render.mParticleScale*render.mParticleStretch);
            const float c=std::abs(std::cos(render.mSpinPosition)),sn=std::abs(std::sin(render.mSpinPosition));
            const float hw=(c*w+sn*h)*.5f,hh=(sn*w+c*h)*.5f;
            const float left=render.mPosX-hw,top=render.mPosY-hh,right=render.mPosX+hw,bottom=render.mPosY+hh;
            // Actual PoolSparkly is small sparks, not a wide water plane. The
            // native render AABB must intersect water rows and this viewport.
            const float ox=float(a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX),oy=float(a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);
            if(right<40||left>760||bottom<250||top>420||right+ox<0||left+ox>a->mWidth||bottom+oy<0||top+oy>a->mHeight)continue;
            if(!out.visible){out.left=left;out.top=top;out.right=right;out.bottom=bottom;}
            else{out.left=(std::min)(out.left,left);out.top=(std::min)(out.top,top);out.right=(std::max)(out.right,right);out.bottom=(std::max)(out.bottom,bottom);}
            ++out.visible;out.alpha=(std::max)(out.alpha,render.mAlpha);
        }
    }
    out.clock=Hash(clock);return out.emitters&&out.visible;
}
bool Observe(LawnApp* a,bool pref,bool bind,Water& water)
{
    if(!Geometry(a)||!Actors(a,bind)||!Preference(a,pref))return false;
    NativeFieldView277 view;if(!Field(a,pref,s.group!=2,view))return false;
    return WaterState(a,view,water);
}
void CaseDone(LawnApp* a)
{
    NetTrace("SMOKE_FIELD277_CASE role=%s biome=%s epoch=%u corners=4 fullUIDTokenBody=1 canonicalHP=1 nativeGridRoundtrips=%d source6000=%d waterOwner=%u localPauseFreeze=%d localResumeAdvance=%d acquisition=NotExecuted postFactoryActorWrites=0",Role(a),names[size_t(s.group)],s.epoch,(s.group==1?6:5)*10,s.group==1?1:0,s.waterUID,s.group!=2?1:0,s.group!=2?1:0);
    Advance(a,20);
}
}
namespace LawnNet
{
bool NetSmokeUpdateFusionField277(LawnApp* a)
{
    if(!Enabled("PVZ_COOP_SMOKE_FUSION_FIELD277"))return false;
    if(!a||!a->CoopSmokeActive()||!a->mNetManager||a->mCoopSmokeResultWritten)return true;
    if(!Require(a,Enabled("PVZ_COOP_SMOKE_FUSION_NATIVE")&&a->mCoopSmokeManifest.mScenario==NETSMOKE_SCENARIO_FUSION_VALIDATION,"Field277 requires owned native fusion_validation"))return true;
    if(!s.started)s.started=s.since=GetTickCount();
    if(GetTickCount()-s.started>240000||GetTickCount()-s.since>60000)
    {char why[160];sprintf_s(why,"Field277 bounded native UI/particle deadline biome=%s stage=%d",names[size_t(s.group)],s.stage);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(!s.stage)
    {
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(!Require(a,Slot(a)>=0&&Slot(a)<2&&a->mNetManager->GetDifficulty()==1,"Field277 requires two owned peers/difficulty1"))return true;
        s.initialPref=a->NetIsClient();if(!Preference(a,s.initialPref))return true;
        if(!NodePaths(a))return true;
        if(a->NetIsClient()){if(Send(a,"F277_BEGIN"))Advance(a,1);return true;}
        if(!Has(a,1,"F277_BEGIN"))return true;std::string error;
        if(!Require(a,a->StartFusionValidation(&error),error.empty()?"Field277 native validation start failed":error.c_str()))return true;
        NetTrace("SMOKE_FIELD277_BEGIN role=host freshEpochs=3 hostDayPreference=0 clientDayPreference=1 configWritesByFixture=0");Advance(a,1);return true;
    }
    if(s.stage==20)
    {
        if(!Barrier(a,"NEXT"))return true;
        if(s.group==2)
        {
            if(!Require(a,s.totalFrames==11,"Field277 missing mandatory fresh native frames"))return true;
            NetTrace("SMOKE_FIELD277_COMPLETE role=%s epochs=3 biomes=Night,Pool,Day freshFrames=11 geometry10columns=1 lastColumn9=1 PoolRow5=1 sameActorsAcrossLocalSkins=1 NightPoolSourceOnly=1 hiddenFIELDNoninteractive=1 PoolSource6000NativeOwner=1 actualParticleAlphaCoverage=1 localPauseFreezeResume=1 DayIndependentSDLChoices=2 nativeSaveLoadPrivateINI=1 postFactoryHPClockRNGWrites=0 fullPoolCampaign=NotExecuted FogRoof=NotExecuted processRestart=NotExecuted ClassicGameplay=NotExecuted",Role(a));
            a->CoopSmokeWriteResult(true,nullptr);return true;
        }
        State next;next.group=s.group+1;next.previousEpoch=s.epoch;next.started=s.started;next.chat=s.chat;next.initialPref=s.initialPref;next.totalFrames=s.totalFrames;
        next.previousWaterDefinition=s.waterDefinition?s.waterDefinition:s.previousWaterDefinition;s=std::move(next);Advance(a,1);
        if(a->NetIsHost()){std::string error;if(!Require(a,a->StartFusionValidation(&error),error.empty()?"Field277 next fresh native epoch failed":error.c_str()))return true;}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard))return true;auto* b=a->mBoard;const auto* context=GetNativeContext(b);
    if(context->epoch<=s.previousEpoch)return true;if(!s.epoch)s.epoch=context->epoch;
    if(!Require(a,context->epoch==s.epoch&&context->validation&&!context->adventure.enabled&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Field277 lost its fresh native validation epoch"))return true;
    const auto& bank=context->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(s.stage==1){if(Choose(a))Advance(a,2);}
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;
        if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver())Click(a,x,y,false);else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000,"Field277 skipped native chooser Ready/intro"))return true;
        if(s.entry<0){s.entry=a->mAppCounter;return true;}if(a->mAppCounter<=s.entry)return true;
        if(a->NetIsHost()&&!s.setup&&!Setup(a))return true;Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(unsigned(b->mBackground)!=backgrounds[size_t(s.group)]||b->mPlants.mSize!=4)return true;
        Water water;if(!Observe(a,s.initialPref,true,water))return true;
        if(s.group!=2&&!s.hiddenClicked){s.hiddenClicked=FieldClick(a,true);s.actionTick=GetTickCount();s.samples=Samples(a);return true;}
        if(s.group!=2&&(GetTickCount()-s.actionTick<300||Samples(a)-s.samples<2))return true;
        if(!Preference(a,s.initialPref)||!Capture(a,s.group==0?"source":s.group==1?"water":"initial")||!Proof(a,"INITIAL"))return true;
        if(s.group==1)NetTrace("SMOKE_FIELD277_WATER role=%s system=%u emitters=%u particles=%u visible=%u effectiveAlpha=%.5f renderAABB=(%.3f,%.3f,%.3f,%.3f) nativeRenderParamsOnly=1 fakeDrawCalls=0",Role(a),s.waterUID,water.emitters,water.particles,water.visible,water.alpha,water.left,water.top,water.right,water.bottom);
        Advance(a,s.group==2?8:5);
    }
    else if(s.stage==5)
    {
        Water water;if(!Observe(a,s.initialPref,false,water))return true;
        if(a->NetIsHost()&&!s.pauseRequested)s.pauseRequested=Escape(a);
        if(!b->mPaused)return true;s.samples=Samples(a);s.pauseTick=GetTickCount();s.pausedClock=water.clock;Advance(a,6);
    }
    else if(s.stage==6)
    {
        Water water;if(!b->mPaused||!Observe(a,s.initialPref,false,water))return true;
        if(!Require(a,s.group!=1||water.clock==s.pausedClock,"Field277 native source water advanced while paused"))return true;
        if(GetTickCount()-s.pauseTick<1000||Samples(a)-s.samples<2)return true;
        if((s.group==1&&!Capture(a,"paused"))||!Proof(a,"PAUSED"))return true;
        NetTrace("SMOKE_FIELD277_PAUSED role=%s biome=%s localClock=%08x frozenMs=%u crossPeerClockEquality=NotClaimed",Role(a),names[size_t(s.group)],water.clock,GetTickCount()-s.pauseTick);
        if(a->NetIsHost()&&!s.resumeRequested)s.resumeRequested=Escape(a);Advance(a,7);
    }
    else if(s.stage==7)
    {
        if(b->mPaused)return true;Water water;if(!Observe(a,s.initialPref,false,water))return true;
        if(!s.resumeSeen){s.resumeSeen=true;s.resumeSamples=Samples(a);}
        if(s.group==1&&water.clock!=s.pausedClock)s.waterAdvanced=true;
        if(Samples(a)-s.resumeSamples<2||(s.group==1&&!s.waterAdvanced))return true;
        if((s.group==1&&!Capture(a,"resumed"))||!Proof(a,"RESUMED"))return true;
        NetTrace("SMOKE_FIELD277_RESUMED role=%s biome=%s localClock=%08x nativeOwner=%u advanced=%d",Role(a),names[size_t(s.group)],water.clock,s.waterUID,s.group==1?s.waterAdvanced:1);CaseDone(a);
    }
    else if(s.stage==8||s.stage==9)
    {
        const bool acting=s.stage==8?a->NetIsClient():a->NetIsHost();bool& clicked=s.stage==8?s.clientClicked:s.hostClicked;
        const bool expected=s.stage==9&&a->NetIsHost();
        if(acting&&!clicked){clicked=FieldClick(a,false);return true;}
        if(a->mNetManager->mConfig.mFusionClassicField!=expected){if(acting)return true;Require(a,false,"Field277 other viewer's FIELD toggle changed this preference");return true;}
        Water water;if(!Observe(a,expected,false,water)||!Capture(a,s.stage==8?"client_fusion":"host_native")||!Proof(a,s.stage==8?"CLIENT_FUSION":"HOST_NATIVE"))return true;
        NetTrace("SMOKE_FIELD277_LOCAL_CHOICE role=%s actor=%s preference=%d persistedReadback=1 otherViewerIndependent=1 sameEntitiesHPGrid=1 privateINI=%s",Role(a),s.stage==8?"client":"host",expected?1:0,a->mCoopSmokeManifest.mConfigPath.c_str());Advance(a,s.stage+1);
    }
    else if(s.stage==10)
    {
        Water water;if(!Observe(a,a->NetIsHost(),false,water))return true;
        if(a->NetIsHost()&&!s.pauseRequested)s.pauseRequested=Escape(a);
        if(!b->mPaused)return true;s.samples=Samples(a);Advance(a,11);
    }
    else if(s.stage==11)
    {
        if(!b->mPaused||Samples(a)-s.samples<2)return true;Water water;
        if(!Observe(a,a->NetIsHost(),false,water)||!Capture(a,"complete")||!Proof(a,"DAY_COMPLETE"))return true;CaseDone(a);
    }
    return true;
}
}
