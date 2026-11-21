#include "FusionNativeZombieVisual.h"
#include "FusionAnimation.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "NativeSpecialZombie269.h"
#include "FusionNativeEmbeddedEffects269.h"
#include "FusionNativeDayZombie273.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
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
    switch(kind){case 3:return "zombie273-3";case 21:return "zombie273-21";case 22:return "zombie273-22";
    case 23:return "zombie273-23";case 100:return "zombie273-100";case 106:return "zombie273-106";
    case 107:return "zombie273-107";case 108:return "zombie273-108";}
    return nullptr;
}

const char* DayPrefab273(uint16_t kind)
{
    switch(kind)
    {
    case 3:return "Resources/zombies/_prefab/PolevaulterZombie.prefab";
    case 21:return "Resources/zombies/_prefab/DollDiamondZombie.prefab";
    case 22:return "Resources/zombies/_prefab/DollGoldZombie.prefab";
    case 23:return "Resources/zombies/_prefab/DollSilverZombie.prefab";
    case 100:return "Resources/zombies/_prefab/PeaShooterZombie.prefab";
    case 106:return "Resources/zombies/_prefab/BucketNutZombie.prefab";
    case 107:return "Resources/zombies/_prefab/CherryNutZombie.prefab";
    case 108:return "Resources/zombies/_prefab/IronPeaZombie.prefab";
    default:return nullptr;
    }
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
    std::string dayArmorPath;
    std::vector<std::string> shieldHandPaths;
    std::vector<std::string> rootChildren;
    std::map<std::string,float> durations;
};

void IndependentAnimators273(const Json& package,ZombieModel& model)
{
    if(!package.contains("sourceAnimators"))return;
    const auto& states=package.at("controller").at("states");const auto& layers=package.at("controller").at("layers");
    std::set<std::string> paths;for(const auto& node:package.at("nodes"))paths.insert(node.at("path").get<std::string>());
    std::set<int> usedLayers;int primary=0;
    for(const auto& animator:package.at("sourceAnimators"))
    {
        if(!paths.contains(animator.at("path").get<std::string>()))throw std::runtime_error("missing independent Animator root");
        const bool main=animator.at("primary").get<bool>();if(main)++primary;
        bool first=true;
        for(const auto& rawLayer:animator.at("layers"))
        {
            const int layer=rawLayer.get<int>();
            if(layer<0||layer>=int(layers.size())||!usedLayers.insert(layer).second)
                throw std::runtime_error("ambiguous independent Animator layer");
            const auto& sourceLayer=layers.at(layer);const bool effective=first||sourceLayer.at("defaultWeight").get<float>()>0;first=false;
            if(main||!animator.at("enabled").get<bool>()||!effective)continue;
            const int64_t id=sourceLayer.at("defaultState").get<int64_t>();if(!id)continue;
            const auto state=std::find_if(states.begin(),states.end(),[&](const Json& s){return s.at("id")==id;});
            if(state==states.end()||state->at("sourceAnimatorFileId")!=animator.at("fileId"))
                throw std::runtime_error("independent Animator default state identity differs");
            if(state->at("clip").is_null())continue;
            const auto& clip=package.at("clips").at(state->at("clip").get<std::string>());
            // Existing extraStates has a separate elapsed value/layer, but no
            // independent parameter/transition machine. Admit only an inert
            // source default, retaining every other source state for later use.
            for(const auto& curve:clip.at("curves"))if(paths.contains(curve.at("path").get<std::string>()))
                throw std::runtime_error("independent Animator default has effective bindings; clock contract required");
            if(!clip.at("events").empty())throw std::runtime_error("independent Animator default has unimplemented events");
            model.options.extraStates.push_back({state->at("name").get<std::string>(),0.0f,layer});
        }
    }
    if(primary!=1||usedLayers.size()!=layers.size())throw std::runtime_error("incomplete independent Animator layer ownership");
}

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
    const bool day=DayPrefab273(kind)!=nullptr;
    if(day&&package.at("sourcePrefab")!=DayPrefab273(kind))throw std::runtime_error("Day Zombie source prefab identity differs");
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
    if(day)
    {
        if(FusionDayZombie273Doll(kind)&&(!package.contains("sourceAnimators")||package.at("sourceAnimators").size()!=2))
            throw std::runtime_error("source Doll requires both independent Animators");
        IndependentAnimators273(package,*model);
    }
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
        if(day&&nodePath==(FusionDayZombie273Doll(kind)?"Zombie_head/Cone":"Zombie_head"))model->dayArmorPath=nodePath;
    }
    const bool money=kind==41||kind==42,vehicle=kind==14||kind==18,helmet=kind==2||kind==4||kind==12||kind==13;
    if (shadowPath.empty() || !foundShadow || (!money&&!vehicle&&(model->headPaths.empty() ||
        model->handPaths.empty() || (kind!=109&&model->upperArmPaths.empty()))) || (helmet && model->conePath.empty())||
        (kind==8&&(model->shieldPath.empty()||model->shieldHandPaths.size()!=3)))
        throw std::runtime_error("missing source Zombie appearance tags/Shadow/Cone");
    // The real native shadow remains in Board's separate shadow render pass.
    model->options.hiddenNodePaths.push_back(shadowPath);
    if(!money)model->damagedArm = ExtraSprite(package,kind==3?
        "Resources/zombies/zombie_polevaulter/Zombie_polevaulter_outerarm_upper2.asset":
        "Resources/zombies/image/reanim/Zombie_outerarm_upper2.asset");
    if(FusionDayZombie273Doll(kind)||kind==106||kind==107)
    {
        if(model->dayArmorPath.empty())throw std::runtime_error("missing exact Day Zombie first-armor node");
        int armorSprites=0;
        for(const auto& sprite:package.at("sprites"))
            if(package.at("nodes").at(sprite.at("node").get<size_t>()).at("path")==model->dayArmorPath)++armorSprites;
        if(armorSprites!=1)throw std::runtime_error("Day Zombie first armor requires its exact SpriteRenderer");
        const char* first=nullptr;const char* second=nullptr;
        switch(kind)
        {
        case 21:first="Resources/zombies/zombie/DiamondDoll2.asset";second="Resources/zombies/zombie/DiamondDoll3.asset";break;
        case 22:first="Resources/zombies/zombie/GoldDoll2.asset";second="Resources/zombies/zombie/GoldDoll3.asset";break;
        case 23:first="Resources/zombies/zombie/SilverDoll2.asset";second="Resources/zombies/zombie/SilverDoll3.asset";break;
        case 106:first="Resources/zombies/plantzombie/ironNut2.asset";second="Resources/zombies/plantzombie/ironNut3.asset";break;
        case 107:first="Resources/plants/_mixer/cherrynut/crack1.asset";second="Resources/plants/_mixer/cherrynut/crack2.asset";break;
        }
        model->damagedCone[0]=ExtraSprite(package,first);model->damagedCone[1]=ExtraSprite(package,second);
    }
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
    if(FindSpecialZombie269Profile(kind)||day)
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
    if(day)
    {
        required=kind==3?std::vector<const char*>{"idle","walk","jump","walk2","attack","die"}:
            FusionDayZombie273Doll(kind)?std::vector<const char*>{"idle","walk","walk2","attack","attack2","die"}:
            std::vector<const char*>{"idle","walk","attack","die"};
        if(FusionDayZombie273Shooter(kind))required.push_back("shoot");
    }
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

bool SourcePose273(const Zombie* zombie,const char*& state,float& phase)
{
    const auto* data=GetNativeSourceZombieData(zombie);
    if(data&&DayPrefab273(data->kind)&&!FusionDayZombie273Doll(data->kind))
        return GetNativeDayZombie273Presentation(zombie,state,phase);
    return GetNativeSourceZombiePresentation(zombie,state,phase);
}

bool DayBodyPose276(const Zombie* zombie,const char*& state,float& phase)
{
    if(!IsNativeDayZombie273(zombie))return false;
    auto* body=zombie->mApp->ReanimationTryToGet(zombie->mBodyReanimID);
    const auto* data=GetNativeSourceZombieData(zombie);
    if(!body||!body->mDefinition||body->mFrameStart<0||body->mFrameCount<=0||
        !std::isfinite(body->mAnimTime)||body->mAnimTime<0||body->mAnimTime>1)return false;
    // Match the real carrier span, not IsDeadOrDying/mIsEating: health and
    // phase can already be the next snapshot while this body is still walking.
    const auto match=[&](const char* track,const char* source)
    {
        if(!body->TrackExists(track))return false;
        int first=0,count=0;body->GetFramesForLayer(track,first,count);
        if(first!=body->mFrameStart||count!=body->mFrameCount)return false;
        LawnNet::NetTrace("Fusion Day limb276 birthpose kind=%u id=%u token=%u source=%s native=%s frame=%d/%d phase=%.9g bodyHP=%d",
            data->kind,zombie->mNetId,data->instanceToken,source,track,first,count,body->mAnimTime,zombie->mBodyHealth);
        state=source;phase=body->mAnimTime;return true;
    };
    if(data->kind==3)
        return match("anim_run","walk")||match("anim_jump","jump")||match("anim_walk","walk2")||
            match("anim_eat","attack")||match("anim_death","die")||match("anim_idle","idle");
    if(FusionDayZombie273Doll(data->kind))
        return match("anim_walk","walk")||match("anim_walk2","walk2")||match("anim_eat","attack")||
            match("anim_eat2","attack2")||match("anim_death","die")||match("anim_idle","idle");
    return match("anim_walk2","walk")||match("anim_eat","attack")||
        match("anim_death","die")||match("anim_idle","idle");
}

bool DayPose273(const Zombie* zombie,const ZombieModel& model,FusionAnimationDrawOptions& options)
{
    const auto* data=GetNativeSourceZombieData(zombie);if(!data||!DayPrefab273(data->kind))return true;
    const auto* day=GetNativeDayZombie273(zombie);
    if(!day||!NativeDayZombie273ControllerValid(data->kind,*day))return false;
    if(data->kind==3)options.nodeActivityOverrides.push_back({"Shadow",day->shadowEnabled});
    // These describe the current source subtree only. Day detached particles
    // are started by the actual loss callback, never inferred by the FX poll.
    options.nodeActivityOverrides.push_back({"LoseArm",!zombie->mHasArm});
    if(data->kind==3)options.nodeActivityOverrides.push_back({"LoseHead",!zombie->mHasHead});
    const char* head=nullptr;float phase=0;
    const bool headPose=GetNativeDayZombie273HeadPose(zombie,head,phase);
    if(FusionDayZombie273Shooter(data->kind)&&day->shootEvent&&!headPose)return false;
    if(headPose)
    {
        if(!head)return false;
        const auto duration=model.durations.find(head);
        if(duration==model.durations.end())return false;
        options.extraStates.push_back({head,phase*duration->second,model.animation.GetStateLayer(head)});
    }
    if(!zombie->mHasHead)options.hiddenNodePaths.insert(options.hiddenNodePaths.end(),model.headPaths.begin(),model.headPaths.end());
    if(!zombie->mHasArm)
    {
        options.hiddenNodePaths.insert(options.hiddenNodePaths.end(),model.handPaths.begin(),model.handPaths.end());
        for(const auto& path:model.upperArmPaths)
        {
            ReplaceSprite(options,path,model.damagedArm);
            // Zombie.ChangeArmSpirte380B60 writes localScale(4,4,4).
            // Pole's36EDD0 override only changes its actual Pole sprite.
            if(data->kind!=3)
            {
                auto& replacement=options.spriteReplacements.back();replacement.overrideScale=true;
                replacement.scaleX=replacement.scaleY=replacement.scaleZ=4.0f;
            }
        }
    }
    if(FusionDayZombie273Doll(data->kind))
    {
        if(zombie->mHelmHealth<=0)options.hiddenNodePaths.push_back(model.dayArmorPath);
        else if(day->armorStage)ReplaceSprite(options,model.dayArmorPath,model.damagedCone[day->armorStage-1]);
    }
    else if((data->kind==106||data->kind==107)&&day->armorStage)
        // WallNutZ clears the FirstArmor reference but retains its last head
        // sprite; helm depletion must not remove the actual Zombie_head node.
        ReplaceSprite(options,model.dayArmorPath,model.damagedCone[day->armorStage-1]);
    return true;
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
    if (!SourcePose273(zombie,state,phase) || !state)return DayPrefab273(data->kind)!=nullptr;
    if(!zombie->GetShadowCenter(groundX,groundY))return false;
    if(data->kind==8&&zombie->mShieldHealth<=0&&!zombie->IsDeadOrDying())
        state=zombie->mIsEating?"attack_":data->originSpeed>kFusionZombieWalk2Threshold?"walk1_":"walk_";
    const auto& model = *found->second;
    const auto duration = model.durations.find(state);
    if (duration == model.durations.end()) return false;
    FusionAnimationDrawOptions options = model.options;
    SpecialPose(zombie,model,options);
    if(!DayPose273(zombie,model,options))return true; // Unadmitted Day state never renders a native carrier alias.
    if (!DayPrefab273(data->kind)&&!zombie->mHasHead)
        options.hiddenNodePaths.insert(options.hiddenNodePaths.end(),model.headPaths.begin(),model.headPaths.end());
    if (!DayPrefab273(data->kind)&&!zombie->mHasArm)
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

bool NativeZombieVisuals::Node(const Zombie* zombie,const char* path,float& x,float& y,bool* active,bool actualBodyPose) const
{
    const auto* data=GetNativeSourceZombieData(zombie);
    if(!data||!path||zombie->mApp!=mImpl->app||!std::isfinite(zombie->mScaleZombie)||zombie->mScaleZombie<=0)return false;
    const auto found=mImpl->models.find(data->kind);if(found==mImpl->models.end())return false;
    const auto& model=*found->second;const char* state=nullptr;float phase=0,sx=0,sy=0;
    if(!(actualBodyPose?DayBodyPose276(zombie,state,phase):SourcePose273(zombie,state,phase))||!state||
       !const_cast<Zombie*>(zombie)->GetShadowCenter(sx,sy))return false;
    const auto duration=model.durations.find(state);if(duration==model.durations.end())return false;
    auto options=model.options;SpecialPose(zombie,model,options);if(!DayPose273(zombie,model,options))return false;
    options.rootFlipX=const_cast<Zombie*>(zombie)->IsWalkingBackwards();
    float nx=0,ny=0;
    if(!model.animation.TryGetNodeOffset(path,nx,ny,state,phase*duration->second,nullptr,-1,1,&options))return false;
    if(active&&!model.animation.TryGetNodeActivity(path,*active,state,phase*duration->second,nullptr,-1,1,&options))return false;
    const float pixels=kPixelsPerSourceUnit*zombie->mScaleZombie;
    x=sx-(options.rootFlipX?-model.shadowX:model.shadowX)*pixels+nx*pixels;
    y=sy+model.shadowY*pixels-ny*pixels;
    return std::isfinite(x)&&std::isfinite(y);
}

bool NativeZombieVisuals::CanQueryRoot(uint16_t kind) const
{
    const auto found=mImpl->models.find(kind);
    if(found==mImpl->models.end())return false;
    const auto& model=*found->second;
    if(!std::isfinite(model.shadowX)||!std::isfinite(model.shadowY)||model.durations.empty())return false;
    // Load already selected the real required states for this source model.
    // Money, armored walkers and vehicles do not universally own walk2.
    for(const auto& [state,duration]:model.durations)
    {
        float x=0,y=0;
        if(!std::isfinite(duration)||duration<=0||!model.animation.TryGetNodeOffset("",x,y,state.c_str(),0,nullptr,-1,1,&model.options)||
            !std::isfinite(x)||!std::isfinite(y))return false;
    }
    return true;
}

bool CanQueryNativeSourceZombieRoot273(Board* board,uint16_t kind)
{
    const auto* context=GetNativeContext(board);
    return context&&context->zombieVisuals&&context->zombieVisuals->CanQueryRoot(kind);
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
bool GetNativeSourceZombieLimbBirthPoint276(const Zombie* zombie,const char* path,float& x,float& y)
{
    auto* context=GetNativeContext(zombie?zombie->mBoard:nullptr);
    return context&&context->zombieVisuals&&context->zombieVisuals->Node(zombie,path,x,y,nullptr,true);
}
}
