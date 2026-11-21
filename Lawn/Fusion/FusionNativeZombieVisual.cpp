#include "FusionNativeZombieVisual.h"
#include "FusionAnimation.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "NativeSpecialZombie269.h"
#include "FusionNativeEmbeddedEffects269.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../SexyAppFramework/Graphics.h"
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>
#include <Windows.h>

namespace LawnFusion
{
namespace
{
using Json = nlohmann::json;
constexpr float kPixelsPerSourceUnit = 60.0f;

const char* PackageName(uint16_t kind)
{
    if (kind == 0) return "zombie";
    if (kind == 2) return "zombie-cone";
    switch(kind){case 4:return "zombie-batch-4";case 8:return "zombie-batch-8";
    case 11:return "zombie-batch-11";case 12:return "zombie-batch-12";case 13:return "zombie-batch-13";
    case 41:return "zombie-batch-41";case 42:return "zombie-batch-42";}
    if(kind==111)return "zombie-batch-111-native22";
    if(kind==109)return "zombie-batch-109-native22";
    switch(kind){case 14:return "zombie270-14";case 15:return "zombie270-15";
    case 18:return "zombie270-18";case 104:return "zombie270-104";}
    return nullptr;
}

std::filesystem::path PackagePath(const char* name)
{
    std::wstring executable(32768,L'\0');
    const DWORD length = GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
    if (!length || length >= executable.size()) throw std::runtime_error("cannot locate source Zombie assets");
    executable.resize(length);
    return std::filesystem::path(executable).parent_path()/L"fusion"/L"pilot"/name/L"animation.json";
}

struct ZombieModel
{
    FusionAnimation animation;
    FusionAnimationDrawOptions options;
    float shadowX = 0, shadowY = 0;
    std::vector<std::string> headPaths, handPaths, upperArmPaths;
    std::string damagedArm, conePath, damagedCone[2],shieldPath,damagedShield[2];
    std::string damagedPaper[2],damagedDriver[5];
    std::vector<std::string> shieldHandPaths;
    std::vector<std::string> rootChildren;
    std::map<std::string,float> durations;
};

std::string ExtraSprite(const Json& package,const char* path)
{
    const std::string guid = package.at("extraSpriteGuids").at(path).get<std::string>();
    if (guid.empty() || !package.at("spriteVariants").contains(guid))
        throw std::runtime_error(std::string("missing source Zombie replacement sprite: ")+path);
    return guid;
}

std::unique_ptr<ZombieModel> LoadModel(Board* board,uint16_t kind,const std::filesystem::path& path)
{
    auto* app=board->mApp;
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open source Zombie package");
    const Json package = Json::parse(input);
    if (package.at("schema").get<int>() != 1 || !package.at("ignoredBindings").empty() ||
        !package.at("omittedInactiveNodes").empty())
        throw std::runtime_error("incomplete source Zombie hierarchy/animation bindings");
    if(kind==111||kind==109)
    {
        const auto& missing=package.at(kind==111?"source111MissingTargetProof":"source109MissingTargetProof");
        const auto& dormant=package.at(kind==111?"source111DormantParticleProof":"source109DormantParticleProof");
        const char* missingPath=kind==111?"Zombie_tie":"Zombie_jaw";
        std::vector<std::string> actualPaths;for(const auto& node:package.at("nodes"))actualPaths.push_back(node.at("path").get<std::string>());
        std::sort(actualPaths.begin(),actualPaths.end());
        Json retained=Json::array();for(auto clip=package.at("clips").begin();clip!=package.at("clips").end();++clip)
            for(const auto& curve:clip.value().at("curves"))if(curve.at("path")==missingPath)retained.push_back({{"clip",clip.key()},{"curve",curve}});
        // Compare binding records as sets because JSON object-key ordering is
        // not part of a Unity controller's semantics.
        auto expected=missing.at("bindingsRetained");const auto byDump=[](const Json& a,const Json& b){return a.dump()<b.dump();};
        std::sort(retained.begin(),retained.end(),byDump);std::sort(expected.begin(),expected.end(),byDump);
        const bool prefabHash=std::any_of(package.at("sources").begin(),package.at("sources").end(),[&](const Json& s)
            {return s.at("path")==package.at("sourcePrefab")&&s.at("sha256")==missing.at("prefabSha256");});
        if(package.at("sourcePrefab")!=(kind==111?"Resources/zombies/_prefab/TallIceNutZombie.prefab":"Resources/zombies/_prefab/TallNutFootballZombie.prefab")||
           missing.at("path")!=missingPath||missing.at("reason")!="absent-from-complete-flat-source-transform-tree"||
           actualPaths.size()!=(kind==111?18U:22U)||std::find(actualPaths.begin(),actualPaths.end(),missingPath)!=actualPaths.end()||
           missing.at("nodePaths")!=Json(actualPaths)||!missing.at("allOriginalCurveKeysRetained").get<bool>()||retained.empty()||retained!=expected||!prefabHash||
           dormant.at("runtimeBound")!=(kind==111?"born-withEffect-false-and-intact-head-arm":"born-and-intact-head-arm")||!package.at("deferredComponents").empty())
            throw std::runtime_error("source111 missing-target/dormant-particle proof differs");
        size_t systems=0,renderers=0;for(const auto& component:package.at("nonRenderingComponents"))
        {
            if(component.at("typeId")==199)++renderers;
            if(component.at("typeId")!=198)continue;++systems;
            const auto& proof=dormant.at("proofs").at(0);
            if(proof.at("componentFileId")!=component.at("fileId")||proof.at("serializedComponent")!=component.at("serializedComponent")||
               proof.at("nodePath")!="LoseArm"||proof.at("reason")!="inactive-until-source-arm-loss"||
               !std::any_of(package.at("nodes").begin(),package.at("nodes").end(),[](const Json& n){return n.at("path")=="LoseArm"&&!n.at("active").get<bool>();}))
                throw std::runtime_error("source111 dormant LoseArm proof differs");
        }
        for(const auto& clip:package.at("clips"))for(const auto& curve:clip.at("curves"))
            if(curve.at("path")=="LoseArm"&&curve.value("attribute",std::string())=="m_IsActive")
                throw std::runtime_error("source111 clip activates unsupported LoseArm particle");
        if(systems!=1||renderers!=1||dormant.at("proofs").size()!=1)
            throw std::runtime_error("source111 incomplete particle proof");
    }
    auto model = std::make_unique<ZombieModel>();
    const auto& shadow = package.at("anchors").at("shadow");
    if (!shadow.at("relativeToRuntimeRoot").get<bool>())
        throw std::runtime_error("source Zombie Shadow must be root-relative");
    model->shadowX = shadow.at("x").get<float>();
    model->shadowY = shadow.at("y").get<float>();
    if (!std::isfinite(model->shadowX) || !std::isfinite(model->shadowY))
        throw std::runtime_error("invalid source Zombie Shadow anchor");
    const std::string shadowPath = shadow.at("path").get<std::string>();
    bool foundShadow = false;
    for (const auto& node : package.at("nodes"))
    {
        const std::string nodePath = node.at("path").get<std::string>();
        const std::string tag = node.value("tag",std::string());
        if(node.at("parent")==0)model->rootChildren.push_back(nodePath);
        if (nodePath == shadowPath) foundShadow = true;
        if (tag == "ZombieHead") model->headPaths.push_back(nodePath);
        else if (tag == "ZombieHand") model->handPaths.push_back(nodePath);
        else if (tag == "ZombieArmUpper") model->upperArmPaths.push_back(nodePath);
        if ((kind == 2||kind==12) && nodePath == "Zombie_head/Cone") model->conePath = nodePath;
        if ((kind==4||kind==13)&&nodePath=="Zombie_head/Bucket") model->conePath=nodePath;
        if(kind==8&&nodePath=="Zombie_screendoor1")model->shieldPath=nodePath;
        if(kind==8&&(nodePath=="Zombie_outerarm_screendoor"||nodePath=="Zombie_innerarm_screendoor_hand"||nodePath=="Zombie_innerarm_screendoor"))model->shieldHandPaths.push_back(nodePath);
    }
    const bool money=kind==41||kind==42,vehicle=kind==14||kind==18,helmet=kind==2||kind==4||kind==12||kind==13;
    if (shadowPath.empty() || !foundShadow || (!money&&!vehicle&&(model->headPaths.empty() ||
        model->handPaths.empty() || (kind!=109&&model->upperArmPaths.empty()))) || (helmet && model->conePath.empty())||
        (kind==8&&(model->shieldPath.empty()||model->shieldHandPaths.size()!=3)))
        throw std::runtime_error("missing source Zombie appearance tags/Shadow/Cone");
    // The real native shadow remains in Board's separate shadow render pass.
    model->options.hiddenNodePaths.push_back(shadowPath);
    if(!money)model->damagedArm = ExtraSprite(package,"Resources/zombies/image/reanim/Zombie_outerarm_upper2.asset");
    if (kind == 2||kind==12)
    {
        model->damagedCone[0] = ExtraSprite(package,"Resources/zombies/image/reanim/Zombie_cone2.asset");
        model->damagedCone[1] = ExtraSprite(package,"Resources/zombies/image/reanim/Zombie_cone3.asset");
    }
    if(kind==4||kind==13)
    {model->damagedCone[0]=ExtraSprite(package,"Resources/zombies/image/reanim/Zombie_bucket2.asset");model->damagedCone[1]=ExtraSprite(package,"Resources/zombies/image/reanim/Zombie_bucket3.asset");}
    if(kind==111)
    {model->damagedCone[0]=ExtraSprite(package,"Resources/zombies/plantzombie/tallicenutz/TallIceCracked1.asset");model->damagedCone[1]=ExtraSprite(package,"Resources/zombies/plantzombie/tallicenutz/TallIceCracked2.asset");}
    if(kind==109)
    {model->damagedCone[0]=ExtraSprite(package,"Resources/zombies/zombie_football/tallnutfootballz/tnf2.asset");model->damagedCone[1]=ExtraSprite(package,"Resources/zombies/zombie_football/tallnutfootballz/tnf3.asset");}
    if(kind==8)
    {model->damagedShield[0]=ExtraSprite(package,"Resources/zombies/door/Zombie_screendoor2.asset");model->damagedShield[1]=ExtraSprite(package,"Resources/zombies/door/Zombie_screendoor3.asset");}
    if(kind==15||kind==104)
    {model->damagedPaper[0]=ExtraSprite(package,"Resources/Zombies/PlantZombie/Paper/CherryPaper1.asset");model->damagedPaper[1]=ExtraSprite(package,"Resources/Zombies/PlantZombie/Paper/CherryPaper2.asset");}
    if(kind==18)
    {
        const char* paths[]={"body_dmg1.asset","lower_dmg1.asset","body_dmg2.asset","lower_dmg2.asset","below_dmg.asset"};
        for(size_t n=0;n<5;++n)model->damagedDriver[n]=ExtraSprite(package,(std::string("Resources/Zombies/Zombie_Driver/SuperDriver/")+paths[n]).c_str());
    }
    std::string error;
    std::vector<int64_t> particleComponents;
    if(FindSpecialZombie269Profile(kind))
    {
        for(const auto& component:package.at("nonRenderingComponents"))
            if(component.at("typeId")==198||component.at("typeId")==199)particleComponents.push_back(component.at("fileId").get<int64_t>());
        if(!particleComponents.empty()&&!EnsureNativeEmbedded269ZombieModel(board,kind,path.string(),&error))throw std::runtime_error(error);
    }
    if (!model->animation.Load(app,path.string(),error,particleComponents)) throw std::runtime_error(error);
    std::vector<const char*> required=money?std::vector<const char*>{"idle","walk"}:std::vector<const char*>{"idle","walk","walk2","attack","die"};
    if(kind==111||kind==109)required={"idle","walk","attack","die"};
    if(kind==14)required={"idle","walk"};
    if(kind==18)required={"idle","die","shake"};
    if(kind==15||kind==104)required={"idle","walk","walk2","attack","attack2","losePaper","die"};
    if(kind==104)required.push_back("shoot");
    if(kind==11||kind==12||kind==13)required.insert(required.end(),{"swim","attackinwater","drowning"});
    if(kind==8)required.insert(required.end(),{"walk_","walk1_","attack_"});
    if(kind==0||kind==2||kind==4)required.push_back("attack2");
    for (const char* state : required)
    {
        const float speed = model->animation.GetStateSpeed(state);
        const float duration = model->animation.GetStateDurationSeconds(state,1.0f);
        if (!std::isfinite(speed) || speed <= 0 || !std::isfinite(duration) || duration <= 0)
            throw std::runtime_error(std::string("missing/unsupported source Zombie state: ")+state);
        model->durations.emplace(state,duration);
    }
    return model;
}

void ReplaceSprite(FusionAnimationDrawOptions& options,const std::string& path,const std::string& guid)
{
    FusionAnimationDrawOptions::SpriteReplacement replacement;
    replacement.nodePath = path;
    replacement.spriteGuid = guid;
    replacement.overrideScale = false;
    options.spriteReplacements.push_back(std::move(replacement));
}

void SpecialPose(const Zombie* zombie,const ZombieModel& model,FusionAnimationDrawOptions& options)
{
    const auto* data=GetNativeSourceZombieData(zombie);
    if(!data||!FindSpecialZombie269Profile(data->kind))return;
    const char* addon=nullptr;float phase=0;
    if(GetNativeSpecialZombie269AddonPose(zombie,addon,phase))
    {
        const auto duration=model.durations.find(addon);
        if(duration!=model.durations.end())options.extraStates.push_back({addon,phase*duration->second,model.animation.GetStateLayer(addon)});
    }
    if(data->kind==14)
    {
        uint8_t stage=0;
        if(model.rootChildren.size()>=6&&GetNativeSpecialZombie269BodyStage(zombie,stage))
            for(size_t n=0;n<6;++n)options.nodeActivityOverrides.push_back({model.rootChildren[n],n%3==stage});
    }
    if(data->kind==15||data->kind==104)
    {
        options.nodeActivityOverrides.push_back({"LoseHead",!zombie->mHasHead});
        options.nodeActivityOverrides.push_back({"LoseArm",!zombie->mHasArm});
        if(data->firstArmorStage>0&&data->firstArmorStage<=2)
            ReplaceSprite(options,"Zombie_paper_paper1",model.damagedPaper[data->firstArmorStage-1]);
        options.nodeActivityOverrides.push_back({"LosePaper",zombie->mShieldHealth<=0});
    }
    if(data->kind==18)
    {
        uint8_t stage=0;
        if(GetNativeSpecialZombie269BodyStage(zombie,stage)&&stage>0)
        {
            ReplaceSprite(options,"Zombie_zamboni_1",model.damagedDriver[(stage-1)*2]);
            ReplaceSprite(options,"Zombie_zamboni_2",model.damagedDriver[(stage-1)*2+1]);
            if(stage==2)
            {ReplaceSprite(options,"Zombie_zamboni_2/lower_front",model.damagedDriver[4]);ReplaceSprite(options,"Zombie_zamboni_2/lower_back",model.damagedDriver[4]);}
        }
        options.nodeActivityOverrides.push_back({"Zombie_zamboni_2/SmokePos",stage==2||zombie->mFlatTires});
    }
}
}

struct NativeZombieVisuals::Impl
{
    LawnApp* app = nullptr;
    std::map<uint16_t,std::unique_ptr<ZombieModel>> models;
};

NativeZombieVisuals::NativeZombieVisuals() : mImpl(std::make_unique<Impl>()) {}
NativeZombieVisuals::~NativeZombieVisuals() = default;

bool NativeZombieVisuals::Ensure(LawnApp* app,uint16_t kind,std::string& error)
{
    std::string location;
    try
    {
        const char* name = PackageName(kind);
        if (!app || !name || (mImpl->app && mImpl->app != app))
            throw std::runtime_error("unsupported source Zombie resource request");
        if (mImpl->models.contains(kind)) { error.clear(); return true; }
        const auto path = PackagePath(name);
        location = path.string()+": ";
        // Allocate the map node and fully load/validate before publishing it.
        std::map<uint16_t,std::unique_ptr<ZombieModel>> pending;
        pending.emplace(kind,LoadModel(app->mBoard,kind,path));
        mImpl->models.merge(pending);
        mImpl->app = app;
        error.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        // Candidate animation refs have already unwound; live models retain theirs.
        if (app) app->CleanSharedImages();
        error = location+exception.what();
        return false;
    }
}

void NativeZombieVisuals::ReleaseUnused(Board* board)
{
    const auto* context = GetNativeContext(board);
    if (!context || board->mApp != mImpl->app) return;
    std::set<uint16_t> keep;
    for (const auto& [uid,data] : context->sourceZombies)
        if (board->mZombies.DataArrayTryToGet(uid))
        {
            // Allocated native death tails still need their source body resources.
            keep.insert(data.kind);
        }
    bool changed = false;
    for(auto it=mImpl->models.begin();it!=mImpl->models.end();)
        if(!keep.contains(it->first)){it=mImpl->models.erase(it);changed=true;}else ++it;
    if (changed) mImpl->app->CleanSharedImages();
}

bool NativeZombieVisuals::Draw(Sexy::Graphics* graphics,Zombie* zombie) const
{
    const auto* data = GetNativeSourceZombieData(zombie);
    if (!graphics || !data || zombie->mApp != mImpl->app ||
        int(zombie->mZombieType)!=GetNativeSourceZombieCarrier(data->kind)) return false;
    const auto found = mImpl->models.find(data->kind);
    if (found == mImpl->models.end()) return false;
    if((data->kind==111||data->kind==109)&&(!zombie->mHasArm||!zombie->mHasHead))
    {
        if(zombie->mZombieAge%100==0)LawnNet::NetTrace("Fusion source nutZombie visual bounded refusal id=%u kind=%u arm=%d head=%d reason=activeLoseArmParticleNotCompiled",zombie->mNetId,data->kind,zombie->mHasArm?1:0,zombie->mHasHead?1:0);
        return true; // Explicit unsupported active-FX state; never draw a Pail alias.
    }
    // This slice has source ground walk/attack/die poses. Native special terminal
    // effects, water/clipping and additive hit flashes keep their existing body.
    const bool swimming=data->kind==11||data->kind==12||data->kind==13||data->kind==14;
    if ((!swimming&&zombie->mInPool) || (zombie->mZombieHeight != HEIGHT_ZOMBIE_NORMAL&&
        !(swimming&&zombie->mZombieHeight==HEIGHT_DRAGGED_UNDER)) ||
        (zombie->mZombiePhase != PHASE_ZOMBIE_NORMAL && zombie->mZombiePhase != PHASE_ZOMBIE_DYING&&
         !FindSpecialZombie269Profile(data->kind)) ||
        !std::isfinite(zombie->mScaleZombie) ||
        zombie->mScaleZombie <= 0 || zombie->mScaleZombie > 8) return false;
    ZombieDrawPosition nativePosition;
    zombie->GetDrawPos(nativePosition);
    // Source swim clips hide the legs and animate the actual duck/whitewater.
    // Applying the native body clip a second time would crop this source pose.
    if (!swimming&&nativePosition.mClipHeight > CLIP_HEIGHT_LIMIT) return false;
    const char* state = nullptr;
    float phase = 0, groundX = 0, groundY = 0;
    if (!GetNativeSourceZombiePresentation(zombie,state,phase) || !state ||
        !zombie->GetShadowCenter(groundX,groundY)) return false;
    if(data->kind==8&&zombie->mShieldHealth<=0&&!zombie->IsDeadOrDying())
        state=zombie->mIsEating?"attack_":data->originSpeed>kFusionZombieWalk2Threshold?"walk1_":"walk_";
    const auto& model = *found->second;
    const auto duration = model.durations.find(state);
    if (duration == model.durations.end()) return false;
    FusionAnimationDrawOptions options = model.options;
    SpecialPose(zombie,model,options);
    if (!zombie->mHasHead)
        options.hiddenNodePaths.insert(options.hiddenNodePaths.end(),model.headPaths.begin(),model.headPaths.end());
    if (!zombie->mHasArm)
    {
        options.hiddenNodePaths.insert(options.hiddenNodePaths.end(),model.handPaths.begin(),model.handPaths.end());
        for (const auto& path : model.upperArmPaths) ReplaceSprite(options,path,model.damagedArm);
    }
    if (data->kind == 2||data->kind==4||data->kind==12||data->kind==13)
    {
        const auto expected=data->kind==2||data->kind==12?HELMTYPE_TRAFFIC_CONE:HELMTYPE_BUCKET;
        if (zombie->mHelmType != HELMTYPE_NONE && zombie->mHelmType != expected) return false;
        if (zombie->mHelmHealth <= 0 || zombie->mHelmType == HELMTYPE_NONE)
            options.hiddenNodePaths.push_back(model.conePath);
        else
        {
            const int damage = zombie->GetHelmDamageIndex();
            if (damage < 0 || damage > 2) return false;
            if (damage) ReplaceSprite(options,model.conePath,model.damagedCone[damage-1]);
        }
    }
    if(data->kind==8)
    {
        if(zombie->mShieldHealth<=0||zombie->mShieldType==SHIELDTYPE_NONE)
        {options.hiddenNodePaths.push_back(model.shieldPath);options.hiddenNodePaths.insert(options.hiddenNodePaths.end(),model.shieldHandPaths.begin(),model.shieldHandPaths.end());}
        else
        {const int stage=zombie->mShieldHealth<zombie->mShieldMaxHealth/3?2:zombie->mShieldHealth<int(int64_t(zombie->mShieldMaxHealth)*2/3)?1:0;
        if(stage)ReplaceSprite(options,model.shieldPath,model.damagedShield[stage-1]);}
    }
    if(data->kind==111||data->kind==109)
    {
        if(data->firstArmorStage>2)return true;
        // FirstArmor is the head111 or body109. Source lethal armor removal
        // leaves its last sprite visible, independently of armor HP/type.
        if(data->firstArmorStage)ReplaceSprite(options,data->kind==111?"Zombie_head":"Zombie_football_upperbody3",model.damagedCone[data->firstArmorStage-1]);
    }
    const int alpha = zombie->mZombieFade < 0 ? 255 :
        static_cast<int>((std::clamp)(255LL*zombie->mZombieFade/10,0LL,255LL));
    if (zombie->mMindControlled) options.colorMultiplierRGBA = {128,64,192,alpha};
    else if (zombie->mChilledCounter > 0 || zombie->mIceTrapCounter > 0)
        options.colorMultiplierRGBA = {75,75,255,alpha};
    else options.colorMultiplierRGBA = {255,255,255,alpha};
    options.rootFlipX = zombie->IsWalkingBackwards();
    const float pixels = kPixelsPerSourceUnit*zombie->mScaleZombie;
    const float shadowX = options.rootFlipX ? -model.shadowX : model.shadowX;
    // BeginDraw already supplied entity/camera translation; source root .31 is
    // in the package. Reflect the root and its ground anchor together, once.
    model.animation.DrawPose(graphics,groundX-zombie->mX-shadowX*pixels,
        groundY-zombie->mY+model.shadowY*pixels,pixels/100.0f,
        state,phase*duration->second,nullptr,-1.0f,1.0f,&options);
    return true;
}

bool NativeZombieVisuals::Node(const Zombie* zombie,const char* path,float& x,float& y,bool* active) const
{
    const auto* data=GetNativeSourceZombieData(zombie);
    if(!data||!path||zombie->mApp!=mImpl->app||!std::isfinite(zombie->mScaleZombie)||zombie->mScaleZombie<=0)return false;
    const auto found=mImpl->models.find(data->kind);if(found==mImpl->models.end())return false;
    const auto& model=*found->second;const char* state=nullptr;float phase=0,sx=0,sy=0;
    if(!GetNativeSourceZombiePresentation(zombie,state,phase)||!state||
       !const_cast<Zombie*>(zombie)->GetShadowCenter(sx,sy))return false;
    const auto duration=model.durations.find(state);if(duration==model.durations.end())return false;
    auto options=model.options;SpecialPose(zombie,model,options);
    options.rootFlipX=const_cast<Zombie*>(zombie)->IsWalkingBackwards();
    float nx=0,ny=0;
    if(!model.animation.TryGetNodeOffset(path,nx,ny,state,phase*duration->second,nullptr,-1,1,&options))return false;
    if(active&&!model.animation.TryGetNodeActivity(path,*active,state,phase*duration->second,nullptr,-1,1,&options))return false;
    const float pixels=kPixelsPerSourceUnit*zombie->mScaleZombie;
    x=sx-(options.rootFlipX?-model.shadowX:model.shadowX)*pixels+nx*pixels;
    y=sy+model.shadowY*pixels-ny*pixels;
    return std::isfinite(x)&&std::isfinite(y);
}

bool EnsureNativeSourceZombieVisuals(Board* board,uint16_t kind,std::string* error)
{
    auto* context = GetNativeContext(board);
    if (!context)
    { if (error) *error = "native Fusion Zombie context is unavailable"; return false; }
    std::string failure;
    if (context->zombieVisuals)
    {
        const bool ok = context->zombieVisuals->Ensure(board->mApp,kind,failure);
        if (error) *error = failure;
        return ok;
    }
    try
    {
        auto pending = std::make_unique<NativeZombieVisuals>();
        if (!pending->Ensure(board->mApp,kind,failure))
        { if (error) *error = failure; return false; }
        context->zombieVisuals = std::move(pending);
        if (error) error->clear();
        return true;
    }
    catch (const std::exception& exception)
    { if (error) *error = exception.what(); return false; }
}

void ReleaseUnusedNativeSourceZombieVisuals(Board* board)
{
    auto* context = GetNativeContext(board);
    if (context && context->zombieVisuals) context->zombieVisuals->ReleaseUnused(board);
}

bool DrawNativeSourceZombie(Sexy::Graphics* graphics,Zombie* zombie)
{
    auto* context = GetNativeContext(zombie ? zombie->mBoard : nullptr);
    return context && context->zombieVisuals && context->zombieVisuals->Draw(graphics,zombie);
}
bool GetNativeSourceZombie269NodePoint(const Zombie* zombie,const char* path,float& x,float& y)
{
    auto* context=GetNativeContext(zombie?zombie->mBoard:nullptr);
    return context&&context->zombieVisuals&&context->zombieVisuals->Node(zombie,path,x,y);
}
bool GetNativeSourceZombie269NodeActivity(const Zombie* zombie,const char* path,bool& active)
{
    float x=0,y=0;auto* context=GetNativeContext(zombie?zombie->mBoard:nullptr);
    return context&&context->zombieVisuals&&context->zombieVisuals->Node(zombie,path,x,y,&active);
}
}
