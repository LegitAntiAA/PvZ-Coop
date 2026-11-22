#include "FusionQuickJackson367Rules.h"
#include "FusionRandomZombie360Rules.h"
#include "FusionNativeZombieVisual.h"
#include "FusionAnimation.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "NativeSpecialZombie269.h"
#include "FusionNativeEmbeddedEffects269.h"
#include "FusionNativeDayZombie273.h"
#include "FusionNativeNightZombie281.h"
#include "FusionNativeJackbox300.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeWaterZombie313.h"
#include "FusionNativeWaterShoot343.h"
#include "FusionNativeSourceGraph313.h"
#include "FusionNativeHeavyZombie313B.h"
#include "FusionNativeExtendedZombie313.h"
#include "FusionExtendedZombie347Rules.h"
#include "FusionNextZombie343Rules.h"
#include "FusionGroundShooter343Rules.h"
#include "FusionSnowZombie363Rules.h"
#include "FusionNativeProjection313.h"
#include "FusionNativeBoss361Bridge.h"

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
#include <functional>
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


const char* CohortPrefab316(uint16_t kind)
{
    switch(kind)
    {
    case 115:return "Resources/zombies/_prefab/SquashZombie.prefab";
    case 116:return "Resources/zombies/_prefab/JalaSquashZombie.prefab";
    case 117:return "Resources/zombies/_prefab/JalapenoZombie.prefab";
    case 201:return "Resources/zombies/_prefab/JacksonDriverZombie.prefab";
    case 209:return "Resources/zombies/_prefab/JackboxJumpZombie.prefab";
    case 222:return "Resources/zombies/_prefab/UltimateJacksonDriver.prefab";
    case 208:return "Resources/zombies/_prefab/QingZombie.prefab";
    case 44:return "Resources/zombies/_prefab/ZombieBoss.prefab";
    case 46:return "Resources/zombies/_prefab/ZombieBoss2.prefab";
    case 202:return "Resources/zombies/_prefab/FootballDrownZombie.prefab";
    case 204:return "Resources/zombies/_prefab/BlackFootballZombie.prefab";
    case 205:return "Resources/zombies/_prefab/KirovZombie.prefab";
    case 206:return "Resources/zombies/_prefab/BombThrowerZombie.prefab";
    case 210:return "Resources/zombies/_prefab/SuperMachineNutZombie.prefab";
    case 218:return "Resources/zombies/_prefab/UltimateFootballDrown.prefab";
    case 219:return "Resources/zombies/_prefab/UltimateMachineNutZombie.prefab";
    case 220:return "Resources/zombies/_prefab/UltimateFootballZombie.prefab";
    case 221:return "Resources/zombies/_prefab/MachineSpiderZombie.prefab";
    case 224:return "Resources/zombies/_prefab/UltimateJackboxZombie.prefab";
    case 113:return "Resources/zombies/_prefab/DolphinPeaZombie.prefab";
    case 214:return "Resources/zombies/_prefab/DolphinGatlingZombie.prefab";
    case 49:return "Resources/zombies/_prefab/SnowGunZombie.prefab";
    case 47:return "Resources/zombies/_prefab/SnowZombie.prefab";
    case 48:return "Resources/zombies/_prefab/NewYearZombie.prefab";
    case 114:return "Resources/zombies/_prefab/IronPeaDoorZombie.prefab";
    case 118:return "Resources/zombies/_prefab/GatlingFootballZombie.prefab";
    case 119:return "Resources/zombies/_prefab/IronBalloonZombie2.prefab";
    case 112:return "Resources/zombies/_prefab/CherryCatapultZombie.prefab";
    case 216:return "Resources/zombies/_prefab/DrownpultZombie.prefab";
    case 16:return "Resources/zombies/_prefab/DriverZombie.prefab";
    case 17:return "Resources/zombies/_prefab/SnorkleZombie.prefab";
    case 19:return "Resources/zombies/_prefab/DolphinriderZombie.prefab";
    case 20:return "Resources/zombies/_prefab/DrownZombie.prefab";
    case 25:return "Resources/zombies/_prefab/BalloonZombie.prefab";
    case 26:return "Resources/zombies/_prefab/AirshipZombie.prefab";
    case 27:return "Resources/zombies/_prefab/SnowDolphinriderZombie.prefab";
    case 28:return "Resources/zombies/_prefab/MinerZombie.prefab";
    case 29:return "Resources/zombies/_prefab/IronBalloonZombie.prefab";
    case 31:return "Resources/zombies/_prefab/CatapultZombie.prefab";
    case 32:return "Resources/zombies/_prefab/PogoZombie.prefab";
    case 33:return "Resources/zombies/_prefab/LadderZombie.prefab";
    case 34:return "Resources/zombies/_prefab/SuperPogoZombie.prefab";
    case 35:return "Resources/zombies/_prefab/Gargantuar.prefab";
    case 36:return "Resources/zombies/_prefab/RedGargantuar.prefab";
    case 37:return "Resources/zombies/_prefab/ImpZombie.prefab";
    case 38:return "Resources/zombies/_prefab/IronGargantuar.prefab";
    case 39:return "Resources/zombies/_prefab/RedIronGargantuar.prefab";
    case 40:return "Resources/zombies/_prefab/MachineNutZombie.prefab";
    case 43:return "Resources/zombies/_prefab/SuperGargantuar.prefab";
    case 45:return "Resources/zombies/_prefab/BungiZombie.prefab";
    case 203:return "Resources/zombies/_prefab/EliteCherryPaperZombie.prefab";
    case 212:return "Resources/zombies/_prefab/UltimateGargantuar.prefab";
    case 213:return "Resources/zombies/_prefab/ObsidianImpZombie.prefab";
    case 223:return "Resources/zombies/_prefab/UltimatePaperZombie.prefab";
    default:return nullptr;
    }
}

const char* CohortPackage316(uint16_t kind)
{
    switch(kind)
    {
    case 115:return "zombie363-115";
    case 116:return "zombie363-116";
    case 117:return "zombie363-117";
    case 201:return "zombie363-201";
    case 209:return "zombie363-209";
    case 222:return "zombie363-222";
    case 208:return "zombie360-208";
    case 44:return "zombie361-44";
    case 46:return "zombie361-46";
    case 202:return "zombie351-202";
    case 204:return "zombie351-204";
    case 205:return "zombie351-205";
    case 206:return "zombie351-206";
    case 210:return "zombie351-210";
    case 218:return "zombie351-218";
    case 219:return "zombie351-219";
    case 220:return "zombie351-220";
    case 221:return "zombie351-221";
    case 224:return "zombie351-224";
    case 113:return "zombie343-113";
    case 214:return "zombie343-214";
    case 49:return "zombie363-49";
    case 47:return "zombie363-47";
    case 48:return "zombie363-48";
    case 114:return "zombie343-114";
    case 118:return "zombie343-118";
    case 119:return "zombie343-119";
    case 112:return "zombie351-112";
    case 216:return "zombie351-216";
    case 16:return "zombie313-16";
    case 17:return "zombie313-17";
    case 19:return "zombie313-19";
    case 20:return "zombie313-20";
    case 25:return "zombie313-25";
    case 26:return "zombie313-26";
    case 27:return "zombie313-27";
    case 28:return "zombie313-28";
    case 29:return "zombie313-29";
    case 31:return "zombie313-31";
    case 32:return "zombie313-32";
    case 33:return "zombie313-33";
    case 34:return "zombie313-34";
    case 35:return "zombie313-35";
    case 36:return "zombie313-36";
    case 37:return "zombie313-37";
    case 38:return "zombie313-38";
    case 39:return "zombie313-39";
    case 40:return "zombie313-40";
    case 43:return "zombie313-43";
    case 45:return "zombie313-45";
    case 203:return "zombie313-203";
    case 212:return "zombie313-212";
    case 213:return "zombie313-213";
    case 223:return "zombie313-223";
    default:return nullptr;
    }
}

const char* PackageName(uint16_t kind)
{
    if(const auto* name=CohortPackage316(kind))return name;
    if(kind==105)return "zombie360-105";
    if(kind==110)return "zombie360-110";
    if(kind==215)return "zombie360-215";
    if(kind==207)return "zombie343-207";
    if(kind==200)return "zombie351-200";
    if(kind==211)return "zombie351-211";
    if(kind==217)return "zombie351-217";
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
    case 23:return "zombie273-23";case 100:return "zombie273-100";
    case 101:return "zombie313-101";case 102:return "zombie313-102";case 103:return "zombie313-103";case 106:return "zombie273-106";
    case 107:return "zombie273-107";case 108:return "zombie273-108";}
    switch(kind){case 5:return "zombie282-5";case 6:return "zombie282-6";
    case 7:return "zombie282-7";case 9:return "zombie282-9";case 10:return "zombie282-10";}
    if(kind==24)return "zombie307-24";
    if(kind==30)return "zombie307-30";
    return nullptr;
}

const char* BatchPrefab282(uint16_t kind)
{
    switch(kind)
    {
    case 105:return "Resources/zombies/_prefab/RandomZombie.prefab";
    case 110:return "Resources/zombies/_prefab/RandomPlusZombie.prefab";
    case 215:return "Resources/zombies/_prefab/DiamondRandomZombie.prefab";
    case 217:return "Resources/zombies/_prefab/SuperDancePolZombie.prefab";
    case 5:return "Resources/zombies/_prefab/PaperZombie.prefab";
    case 6:return "Resources/zombies/_prefab/DancePolevaulterZombie.prefab";
    case 7:return "Resources/zombies/_prefab/DancePolevaulter2Zombie.prefab";
    case 9:return "Resources/zombies/_prefab/FootballZombie.prefab";
    case 10:return "Resources/zombies/_prefab/JacksonZombie.prefab";
    case 207:return "Resources/zombies/_prefab/QuickJacksonZombie.prefab";
    default:return nullptr;
    }
}

const char* JackboxPrefab300(uint16_t kind)
{
    if(kind==24)return "Resources/zombies/_prefab/JackboxZombie.prefab";
    if(kind==30)return "Resources/zombies/_prefab/SuperJackboxZombie.prefab";
    return nullptr;
}

const char* DayPrefab273(uint16_t kind)
{
    switch(kind)
    {
    case 217:return "Resources/zombies/_prefab/SuperDancePolZombie.prefab";
    case 3:return "Resources/zombies/_prefab/PolevaulterZombie.prefab";
    case 6:case 7:return BatchPrefab282(kind);
    case 21:return "Resources/zombies/_prefab/DollDiamondZombie.prefab";
    case 22:return "Resources/zombies/_prefab/DollGoldZombie.prefab";
    case 23:return "Resources/zombies/_prefab/DollSilverZombie.prefab";
    case 100:return "Resources/zombies/_prefab/PeaShooterZombie.prefab";
    case 101:return "Resources/zombies/_prefab/CherryShooterZombie.prefab";
    case 102:return "Resources/zombies/_prefab/SuperCherryZombie.prefab";
    case 103:return "Resources/zombies/_prefab/WallNutZombie.prefab";
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
    int64_t rootBox313=0;
    std::map<std::string,std::string,std::less<>> extras316;
    std::vector<std::string> unique316;
    std::map<std::string,std::string,std::less<>> child316,sprite316;
    std::map<std::string,std::vector<std::string>,std::less<>> children316;
    std::vector<std::string> spriteNodes316;
    std::set<std::string,std::less<>> nodePaths316;
    std::map<std::string,std::vector<std::string>,std::less<>> fields351;

    FusionAnimationDrawOptions options;
    float shadowX = 0, shadowY = 0;
    std::vector<std::string> headPaths, handPaths, upperArmPaths;
    std::string damagedArm, conePath, damagedCone[2],shieldPath,damagedShield[2];
    std::string damagedPaper[2],damagedDriver[5];
    std::string dayArmorPath;
    std::vector<std::string> shieldHandPaths;
    std::vector<std::string> rootChildren;
    std::map<std::string,float> durations;
    std::map<std::string,std::string,std::less<>> stateAliases;
};

const char* PresentationState(const ZombieModel& model,const char* source)
{
    if(!source)return nullptr;
    const auto alias=model.stateAliases.find(source);
    if(alias!=model.stateAliases.end())return alias->second.c_str();
    return model.durations.contains(source)?source:nullptr;
}

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


void Fields351(uint16_t kind,ZombieModel& model)
{
    switch(kind)
    {
    case 44:
        model.fields351["ballPosition"]={"Zombie_boss_jaw"};
        model.fields351["spawnPosition"]={"Zombie_boss_outerarm_finger1_2"};
        model.fields351["eye"]={"Zombie_boss_eyeglow"};
        model.fields351["head"]={"Zombie_boss_head"};
        model.fields351["thumb"]={"Zombie_boss_outerarm_thumb2"};
        model.fields351["jaw"]={"Zombie_boss_jaw"};
        model.fields351["hand"]={"Zombie_boss_outerarm_hand"};
        model.fields351["foot_inner"]={"Zombie_boss_foot_1"};
        model.fields351["foot_outter"]={"Zombie_boss_foot"};
        model.fields351["eyes"]={"Zombie_boss_eyeglow"};
        model.fields351["animRV"]={"BossRv"};
        break;
    case 46:
        model.fields351["ballPosition"]={"Zombie_boss_jaw"};
        model.fields351["spawnPosition"]={"Zombie_boss_outerarm_finger1_2"};
        model.fields351["eye"]={"Zombie_boss_eyeglow"};
        model.fields351["head"]={"Zombie_boss_head"};
        model.fields351["thumb"]={"Zombie_boss_outerarm_thumb2"};
        model.fields351["jaw"]={"Zombie_boss_jaw"};
        model.fields351["hand"]={"Zombie_boss_outerarm_hand"};
        model.fields351["foot_inner"]={"Zombie_boss_foot_1"};
        model.fields351["foot_outter"]={"Zombie_boss_foot"};
        model.fields351["eyes"]={"Zombie_boss_eyeglow"};
        model.fields351["animRV"]={"BossRv"};
        break;


    case 202:
        model.fields351["theFirstArmor"]={"Zombie_head/head1"};
        break;
    case 204:
        model.fields351["theFirstArmor"]={"Zombie_football_head"};
        break;
    case 218:
        model.fields351["theFirstArmor"]={"Naodai"};
        model.fields351["w"]={"组 4 拷贝"};
        break;
    case 220:
        model.fields351["theFirstArmor"]={"Zombie_football_head/head"};
        model.fields351["flags"]={"Zombie_football_upperbody/Zombie_football_upperbody/flag1/1_1","Zombie_football_upperbody/Zombie_football_upperbody/flag2/2_1","Zombie_football_upperbody/Zombie_football_upperbody/flag3/3_1"};
        break;
    case 221:
        model.fields351["changeSprites"]={"bodytop","body","bLS","Fei yi 1","Fei yi 2","Fei yi 3","K_2","zhong bu shen ti bone/Paoguanguge/eye0","sbjiyu","guanzi","front","head"};
        break;
    case 224:
        model.fields351["changeSprites"]={"ZombieHead","ZombieHead/hairfront","ZombieHead/hairmid","ZombieHead/hairmid/yellowball3-A","ZombieHead/hairmid/yellowball4-A","ZombieHead/hairmid/yellowball5-A","ZombieHead/hairmid/yellowball1","ZombieHead/hairmid/yellowball2-A","ZombieHead/leafbunch2_2/hairback1","ZombieHead/leafbunch2_2/hairback2","ZombieHead/leafbunch2_2/hairback3","ZombieHead/leafbunch2_2/hairback4","ZombieHead/eye-A","LeftArmLower","LeftArmLower/armleft1top","LeftArmUpper","body","body/color1","body/color2","body/color5","body/color6","body/color7","RightArm/RightArm/rightarm1top","RightArm/RightArm/hand1"};
        break;
    case 48:
        model.fields351["theFirstArmor"]={"Zombie_yeti_head/head1"};
        break;
    case 114:
        model.fields351["theFirstArmor"]={"Zombie_head/Bucket"};
        model.fields351["theSecondArmor"]={"Zombie_screendoor1"};
        model.fields351["doorHands"]={"Zombie_outerarm_screendoor","Zombie_innerarm_screendoor_hand","Zombie_innerarm_screendoor"};
        break;
    case 118:
        model.fields351["theFirstArmor"]={"Zombie_football_head/HeadPos/GatlingPea_head/GatlingPea_helmet"};
        break;
    case 214:
        model.fields351["head1"]={"Zombie_dolphinrider_head/Pos"};
        model.fields351["head2"]={"Zombie_dolphinrider_head/head2"};
        break;
    case 216:
        model.fields351["theFirstArmor"]={"Zombie_catapult_driver_head"};
        model.fields351["weapons"]={"1","2","3","4","5","6","7"};
        model.fields351["weapons2"]={"1_2","2_2","3_2","4_2","5_2","6_2","7_2","1_3","2_3","3_3","4_3","5_3","6_3","7_3"};
        model.fields351["damaged"]={"back2_1","Zombie_catapult_basket_overlay1_1","back2","Zombie_catapult_basket_overlay1","Zombie_catapult_siding"};
        break;
    }
    for(const auto& [field,nodes]:model.fields351)for(const auto& node:nodes)
        if(!model.nodePaths316.contains(node))throw std::runtime_error("source351 serialized field target missing: "+field+"/"+node);
    const auto armor=model.fields351.find("theFirstArmor");
    if(armor!=model.fields351.end())
    {if(armor->second.size()!=1)throw std::runtime_error("source351 first armor identity differs");model.conePath=armor->second.front();}
}

std::unique_ptr<ZombieModel> LoadCohort316(Board* board,uint16_t kind,const std::filesystem::path& path,const Json& package)
{
    if(package.at("sourcePrefab")!=CohortPrefab316(kind))throw std::runtime_error("source313 prefab identity differs");
    auto model=std::make_unique<ZombieModel>();
    const auto& shadow=package.at("anchors").at("shadow");
    if(!shadow.at("relativeToRuntimeRoot").get<bool>())throw std::runtime_error("source313 Shadow is not root relative");
    model->shadowX=shadow.at("x").get<float>();model->shadowY=shadow.at("y").get<float>();
    if(!std::isfinite(model->shadowX)||!std::isfinite(model->shadowY))throw std::runtime_error("source313 invalid Shadow");
    const auto shadowPath=shadow.at("path").get<std::string>();bool foundShadow=false;
    std::map<std::string,size_t,std::less<>> pathCounts;
    for(const auto& node:package.at("nodes"))++pathCounts[node.at("path").get<std::string>()];
    const auto indexedNode=[](size_t index){return "@node:"+std::to_string(index);};
    const auto nodeReference=[&](size_t index)
    {
        const auto nodePath=package.at("nodes").at(index).at("path").get<std::string>();
        return pathCounts.at(nodePath)==1?nodePath:indexedNode(index);
    };
    size_t sourceNodeIndex=0;
    for(const auto& node:package.at("nodes"))
    {
        const auto nodePath=node.at("path").get<std::string>();const auto tag=node.value("tag",std::string());
        model->nodePaths316.insert(nodePath);
        if(nodePath==shadowPath)foundShadow=true;
        if(node.at("parent")==0)model->rootChildren.push_back("@node:"+std::to_string(sourceNodeIndex));
        ++sourceNodeIndex;
        if(tag=="ZombieHead")model->headPaths.push_back(nodePath);
        if(tag=="ZombieHand")model->handPaths.push_back(nodePath);
        if(tag=="ZombieArmUpper")model->upperArmPaths.push_back(nodePath);
        const int parent=node.at("parent").get<int>();
        if(parent>=0)
        {
            const auto parentPath=package.at("nodes").at(parent).at("path").get<std::string>();
            const auto child=nodeReference(sourceNodeIndex-1);
            // Source GetChild(n) identifies an imported node, including genuine
            // siblings with equal names. Plain-path aliases remain unique only.
            model->child316.try_emplace(indexedNode(parent),child);
            model->children316[indexedNode(parent)].push_back(child);
            if(pathCounts.at(parentPath)==1)
            {model->child316.try_emplace(parentPath,child);model->children316[parentPath].push_back(child);}
        }
    }
    for(const auto& sprite:package.at("sprites"))
    {
        const auto index=sprite.at("node").get<size_t>();
        const auto node=package.at("nodes").at(index).at("path").get<std::string>();
        const auto guid=sprite.at("guid").is_null()?std::string():sprite.at("guid").get<std::string>();
        model->spriteNodes316.push_back(nodeReference(index));
        if(!model->sprite316.emplace(indexedNode(index),guid).second)
            throw std::runtime_error("source313 ambiguous node SpriteRenderer");
        if(pathCounts.at(node)==1)model->sprite316.emplace(node,guid);
    }
    if(!foundShadow)throw std::runtime_error("source313 missing Shadow");
    Fields351(kind,*model);
    if(kind==29)model->conePath="Zombie_balloon_head/ironNut1";
    if(kind==43||kind==212)model->conePath="Zombie_gargantuar_head/SuperGargantuar_helmet";
    model->options.hiddenNodePaths.push_back(shadowPath);
    for(auto it=package.at("extraSpriteGuids").begin();it!=package.at("extraSpriteGuids").end();++it)
        model->extras316.emplace(it.key(),ExtraSprite(package,it.key().c_str()));
    const auto requireExtra=[&](const char* asset)
    {if(!model->extras316.contains(asset))throw std::runtime_error(std::string("source313 missing genuine runtime replacement: ")+asset);};
    switch(kind)
    {
    case 201:requireExtra("Resources/Zombies/InTravel/JacksonDriver/damage1.asset");requireExtra("Resources/Zombies/InTravel/JacksonDriver/Zombie_zamboni_2_damage1.asset");requireExtra("Resources/Zombies/InTravel/JacksonDriver/damage2.asset");requireExtra("Resources/Zombies/InTravel/JacksonDriver/Zombie_zamboni_2_damage2.asset");requireExtra("Resources/zombies/image/reanim/Zombie_outerarm_upper2.asset");break;
    case 115:case 116:case 117:requireExtra("Resources/zombies/image/reanim/Zombie_outerarm_upper2.asset");break;
    case 49:requireExtra("Resources/zombies/image/reanim/Zombie_outerarm_upper2.asset");break;
    case 114:
        requireExtra("Resources/Zombies/Image/reanim/Zombie_outerarm_upper2.asset");
        requireExtra("Resources/Zombies/Image/reanim/Zombie_bucket2.asset");requireExtra("Resources/Zombies/Image/reanim/Zombie_bucket3.asset");
        requireExtra("Resources/Zombies/Door/Zombie_screendoor2.asset");requireExtra("Resources/Zombies/Door/Zombie_screendoor3.asset");break;
    case 118:requireExtra("Resources/Zombies/Image/reanim/Zombie_outerarm_upper2.asset");break;
    case 119:requireExtra("Resources/Zombies/BalloonZombie/upperarm2.asset");break;
    case 202:requireExtra("Resources/zombies/zombie_drown/losearm.asset");
        requireExtra("Resources/zombies/zombie_football/tallnutfootballz/tnf2.asset");requireExtra("Resources/zombies/zombie_football/tallnutfootballz/tnf3.asset");break;
    case 204:requireExtra("Resources/zombies/intravel/blackfootball/head1.asset");requireExtra("Resources/zombies/intravel/blackfootball/head2.asset");break;
    case 16:requireExtra("Resources/zombies/zombie_driver/Zombie_zamboni_1_damage1.asset");
        requireExtra("Resources/zombies/zombie_driver/Zombie_zamboni_1_damage2.asset");requireExtra("Resources/zombies/zombie_driver/Zombie_zamboni_2_damage2.asset");break;
    case 17:requireExtra("Resources/zombies/zombie_snorkle/Zombie_snorkle_outerarm_upper2.asset");break;
    case 19:requireExtra("Resources/zombies/zombie_dolphinrider/Zombie_dolphinrider_outerarm_upper2.asset");break;
    case 20:requireExtra("Resources/zombies/zombie_drown/losearm.asset");break;
    case 113:case 214:case 27:requireExtra("Resources/zombies/zombie_dolphinrider/snowdolphinrider/arm.asset");break;
    case 25:case 29:requireExtra("Resources/zombies/balloonzombie/upperarm2.asset");
        if(kind==29){requireExtra("Resources/zombies/plantzombie/ironNut2.asset");requireExtra("Resources/zombies/plantzombie/ironNut3.asset");}break;
    case 43:case 212:requireExtra("Resources/zombies/jackbox/superjackboxzombie/SuperJackbox_cone2.asset");
        requireExtra("Resources/zombies/jackbox/superjackboxzombie/SuperJackbox_cone3.asset");break;
    case 28:requireExtra("Resources/zombies/zombie_digger/Zombie_digger_outerarm_upper2.asset");break;
    case 203:requireExtra("Resources/zombies/paperzombie/paper_bone1.asset");
        requireExtra("Resources/Zombies/PlantZombie/Paper/CherryPaper1.asset");requireExtra("Resources/Zombies/PlantZombie/Paper/CherryPaper2.asset");break;
    }
    // Read the actual root collider component identity; never infer a collider
    // from sprite bounds (the genuine Airship26 intentionally has none).
    for(const auto& box:package.at("colliders"))if(box.at("type")=="box"&&box.at("node")==0)
    {
        if(model->rootBox313)throw std::runtime_error("source313 ambiguous root BoxCollider2D");
        model->rootBox313=box.at("fileId").get<int64_t>();
    }
    if((kind==26||kind==205)!=(!model->rootBox313))throw std::runtime_error("source313 root BoxCollider2D identity missing");
    std::vector<int64_t> particles;
    for(const auto& c:package.at("nonRenderingComponents"))if(c.at("typeId")==198||c.at("typeId")==199)particles.push_back(c.at("fileId").get<int64_t>());
    std::string error;
    if(!particles.empty()&&!EnsureNativeEmbedded269ZombieModel(board,kind,path.string(),&error))throw std::runtime_error(error);
    if(!model->animation.Load(board->mApp,path.string(),error,particles))throw std::runtime_error(error);
    const auto* graph=FindFusionSourceGraph313(kind);
    const auto& states=package.at("controller").at("states");
    const auto layerOf=[&](const Json& state)
    {
        int found=-1;const auto& layers=package.at("controller").at("layers");
        for(size_t i=0;i<layers.size();++i)for(const auto& id:layers.at(i).at("states"))if(id==state.at("id"))
        {if(found!=-1)throw std::runtime_error("source313 ambiguous state layer");found=int(i);}
        return found;
    };
    for(const auto& state:states)
    {
        const auto name=state.at("name").get<std::string>();
        if(layerOf(state)==0)
        {
            const float duration=state.at("clip").is_null()?0:model->animation.GetStateDurationSeconds(name.c_str(),1);
            if(!std::isfinite(duration)||duration<0)throw std::runtime_error("source313 invalid base state duration");
            if(!model->durations.emplace(name,duration).second)throw std::runtime_error("source313 ambiguous base state name");
        }
    }
    if(graph)
    {
        if(package.at("controller").at("layers").size()!=graph->layerCount)throw std::runtime_error("source313 graph/model layer count differs");
        for(uint8_t layer=0;layer<graph->layerCount;++layer)for(uint8_t n=0;n<graph->layers[layer].stateCount;++n)
        {
            if(package.at("controller").at("layers").at(layer).at("blendingMode").get<int>()!=int(graph->layers[layer].additive))
                throw std::runtime_error("source313 graph/model layer blend differs");
            const auto& expected=graph->states[graph->layers[layer].states[n]];const Json* found=nullptr;
            for(const auto& state:states)if(state.at("name")==expected.name&&layerOf(state)==layer)
            {if(found)throw std::runtime_error("source313 duplicate graph state");found=&state;}
            if(!found||found->at("clip").is_null()!=expected.nullMotion||
               std::abs(found->at("speed").get<float>()-expected.speed)>1e-6f||
               found->at("speedParameterActive").get<bool>()!=(expected.speedParameter!=0)||
               (expected.speedParameter&&found->at("speedParameter")!=(expected.speedParameter==1?"Speed":"jumpSpeed")))
                throw std::runtime_error("source313 graph/model state differs");
            if(!expected.nullMotion&&(std::abs(package.at("clips").at(found->at("clip").get<std::string>()).at("duration").get<float>()-expected.duration)>1e-5f||
               package.at("clips").at(found->at("clip").get<std::string>()).at("loop").get<bool>()!=expected.loop))
                throw std::runtime_error("source313 graph/model clip duration differs");
            float x=0,y=0;auto options=model->options;
            if(layer)options.extraStates.push_back({expected.name,0,layer});
            const auto& base=graph->states[graph->layers[0].defaultState];
            if(!model->animation.TryGetNodeOffset("",x,y,layer?base.name:expected.name,0,nullptr,-1,1,&options))
                throw std::runtime_error("source313 graph/model pose cannot sample");
        }
    }
    else if(!FusionWaterZombie313Kind(kind))throw std::runtime_error("source313 absent controller");
    else for(uint8_t phase=0;phase<=uint8_t(FusionWaterZombie313Phase::DrownDeath);++phase)
    {
        FusionWaterZombie313Clip expected;if(!TryGetFusionWaterZombie313Clip(kind,FusionWaterZombie313Phase(phase),expected))continue;
        const Json* found=nullptr;for(const auto& state:states)if(state.at("name")==expected.state&&layerOf(state)==0)
        {if(found)throw std::runtime_error("source313 ambiguous water state");found=&state;}
        if(!found||found->at("clip").is_null()||std::abs(found->at("speed").get<float>()-expected.multiplier)>1e-6f||
           found->at("speedParameterActive").get<bool>()!=expected.speedParameter||
           (expected.speedParameter&&found->at("speedParameter")!="Speed"))throw std::runtime_error("source313 water state differs");
        const auto& clip=package.at("clips").at(found->at("clip").get<std::string>());
        if(std::abs(clip.at("duration").get<float>()-expected.seconds)>1e-5f||clip.at("loop").get<bool>()!=expected.loop)
            throw std::runtime_error("source313 water clip differs");
    }
    return model;
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
    if(CohortPrefab316(kind))return LoadCohort316(board,kind,path,package);
    const bool day=DayPrefab273(kind)!=nullptr,jackbox=FusionJackbox300Kind(kind);
    const char* expectedPrefab=kind==200?"Resources/zombies/_prefab/SuperSubmarineZombie.prefab":kind==211?"Resources/zombies/_prefab/LandSubmarineZombie.prefab":day?DayPrefab273(kind):jackbox?JackboxPrefab300(kind):BatchPrefab282(kind);
    if(expectedPrefab&&package.at("sourcePrefab").get<std::string>()!=expectedPrefab)
        throw std::runtime_error("source Zombie prefab identity differs");
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
    for(const auto& box:package.at("colliders"))if(box.at("type")=="box"&&box.at("node")==0)
    {if(model->rootBox313)throw std::runtime_error("ambiguous root source collider");model->rootBox313=box.at("fileId").get<int64_t>();}
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
    size_t sourceNodeIndex=0;
    for (const auto& node : package.at("nodes"))
    {
        const std::string nodePath = node.at("path").get<std::string>();
        const std::string tag = node.value("tag",std::string());
        if(node.at("parent")==0)model->rootChildren.push_back("@node:"+std::to_string(sourceNodeIndex));
        ++sourceNodeIndex;
        if (nodePath == shadowPath) foundShadow = true;
        if (tag == "ZombieHead") model->headPaths.push_back(nodePath);
        else if (tag == "ZombieHand") model->handPaths.push_back(nodePath);
        else if (tag == "ZombieArmUpper") model->upperArmPaths.push_back(nodePath);
        if ((kind == 2||kind==12||FusionRandomZombie360Kind(kind)) && nodePath == "Zombie_head/Cone") model->conePath = nodePath;
        if ((kind==4||kind==13)&&nodePath=="Zombie_head/Bucket") model->conePath=nodePath;
        if(kind==9&&nodePath=="Zombie_football_helmet")model->conePath=nodePath;
        if(kind==8&&nodePath=="Zombie_screendoor1")model->shieldPath=nodePath;
        if(kind==8&&(nodePath=="Zombie_outerarm_screendoor"||nodePath=="Zombie_innerarm_screendoor_hand"||nodePath=="Zombie_innerarm_screendoor"))model->shieldHandPaths.push_back(nodePath);
        if(day&&nodePath==(FusionDayZombie273Doll(kind)?"Zombie_head/Cone":"Zombie_head"))model->dayArmorPath=nodePath;
    }
    const bool money=kind==41||kind==42,vehicle=FusionSubmarine343Kind(kind)||kind==18,helmet=kind==2||kind==4||kind==9||kind==12||kind==13||FusionRandomZombie360Kind(kind);
    if (shadowPath.empty() || !foundShadow || (!money&&!vehicle&&(model->headPaths.empty() ||
        (!jackbox&&model->handPaths.empty()) || (kind!=109&&kind!=9&&model->upperArmPaths.empty()))) || (helmet && model->conePath.empty())||
        (kind==8&&(model->shieldPath.empty()||model->shieldHandPaths.size()!=3)))
        throw std::runtime_error("missing source Zombie appearance tags/Shadow/Cone");
    if(jackbox)
    {
        // Jackbox.ChangeArmSpirte3686C0 replaces sprite58 on the actual lower
        // arm tag; this family has NO ZombieHand and changes NO Transform scale.
        if(model->headPaths!=std::vector<std::string>{"Zombie_jackbox_jaw","Zombie_jackbox_head"}||
           !model->handPaths.empty()||model->upperArmPaths!=std::vector<std::string>{"Zombie_jackbox_outerarm_lower"})
            throw std::runtime_error("Jackbox source part tags differ");
        for(const char* node:{"Zombie_jackbox_box","Zombie_jackbox_handle","LoseHead","LoseArm"})
            if(std::count_if(package.at("nodes").begin(),package.at("nodes").end(),[&](const Json& n){return n.at("path")==node;})!=1)
                throw std::runtime_error("missing/ambiguous Jackbox source node");
        if(kind==30)
        {
            model->conePath="Zombie_jackbox_head/SuperJackbox_cone";
            for(const auto& node:{model->conePath,std::string("LoseCone")})
                if(std::count_if(package.at("nodes").begin(),package.at("nodes").end(),[&](const Json& n){return n.at("path")==node;})!=1)
                    throw std::runtime_error("missing/ambiguous SuperJackbox armor node");
        }
    }
    // The real native shadow remains in Board's separate shadow render pass.
    model->options.hiddenNodePaths.push_back(shadowPath);
    // Football.ChangeArmSpirte is a no-op and has no ZombieArmUpper tag.
    if(!money&&kind!=9&&kind!=200&&kind!=211&&kind!=207)model->damagedArm = ExtraSprite(package,jackbox?
        "Resources/zombies/jackbox/Zombie_jackbox_outerarm_lower2.asset":FusionDayZombie273Pole(kind)?
        "Resources/zombies/zombie_polevaulter/Zombie_polevaulter_outerarm_upper2.asset":
        kind==5?"Resources/zombies/paperzombie/paper_bone.asset":
        FusionJacksonFamily367Kind(kind)?"Resources/zombies/zombie_jackson/Zombie_Jackson_outerarm_upper2.asset":
        "Resources/zombies/image/reanim/Zombie_outerarm_upper2.asset");
    if(kind==207){for(const char* path:{"Trail_1/Trail_1","Zombie_Jackson_outerarm_upper","Zombie_Jackson_outerarm_upper/Zombie_Jackson_outerarm_upper_1"})
        if(std::count_if(package.at("nodes").begin(),package.at("nodes").end(),[&](const Json& n){return n.at("path").get<std::string>()==path;})!=1)throw std::runtime_error("QuickJackson207 exact source override node missing/ambiguous");}
    if(FusionDayZombie273Doll(kind)||FusionDayZombie273NutArmor(kind))
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
        case 103:first="Resources/plants/WallNut/crackedA.asset";second="Resources/plants/WallNut/crackedB.asset";break;
        case 106:first="Resources/zombies/plantzombie/ironNut2.asset";second="Resources/zombies/plantzombie/ironNut3.asset";break;
        case 107:first="Resources/plants/_mixer/cherrynut/crack1.asset";second="Resources/plants/_mixer/cherrynut/crack2.asset";break;
        }
        model->damagedCone[0]=ExtraSprite(package,first);model->damagedCone[1]=ExtraSprite(package,second);
    }
    if(kind==105||kind==110)
    {
        model->damagedCone[0]=ExtraSprite(package,kind==105?"Resources/zombies/zombie/Zombie_cone2.asset":"Resources/zombies/zombie/Gold2.asset");
        model->damagedCone[1]=ExtraSprite(package,kind==105?"Resources/zombies/zombie/Zombie_cone3.asset":"Resources/zombies/zombie/Gold3.asset");
    }
    if(kind==30)
    {
        model->damagedCone[0]=ExtraSprite(package,"Resources/zombies/jackbox/superjackboxzombie/SuperJackbox_cone2.asset");
        model->damagedCone[1]=ExtraSprite(package,"Resources/zombies/jackbox/superjackboxzombie/SuperJackbox_cone3.asset");
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
    if(kind==9)
    {model->damagedCone[0]=ExtraSprite(package,"Resources/zombies/zombie_football/Zombie_football_helmet2.asset");model->damagedCone[1]=ExtraSprite(package,"Resources/zombies/zombie_football/Zombie_football_helmet3.asset");}
    if(kind==8)
    {model->damagedShield[0]=ExtraSprite(package,"Resources/zombies/door/Zombie_screendoor2.asset");model->damagedShield[1]=ExtraSprite(package,"Resources/zombies/door/Zombie_screendoor3.asset");}
    if(SpecialZombie269PaperKind(kind))
    {
        const char* first=kind==5?"Resources/zombies/paperzombie/paper2.asset":"Resources/Zombies/PlantZombie/Paper/CherryPaper1.asset";
        const char* second=kind==5?"Resources/zombies/paperzombie/paper3.asset":"Resources/Zombies/PlantZombie/Paper/CherryPaper2.asset";
        model->damagedPaper[0]=ExtraSprite(package,first);model->damagedPaper[1]=ExtraSprite(package,second);
    }
    if(kind==18)
    {
        const char* paths[]={"body_dmg1.asset","lower_dmg1.asset","body_dmg2.asset","lower_dmg2.asset","below_dmg.asset"};
        for(size_t n=0;n<5;++n)model->damagedDriver[n]=ExtraSprite(package,(std::string("Resources/Zombies/Zombie_Driver/SuperDriver/")+paths[n]).c_str());
    }
    std::string error;
    std::vector<int64_t> particleComponents;
    if(FindSpecialZombie269Profile(kind)||day||BatchPrefab282(kind)||jackbox)
    {
        for(const auto& component:package.at("nonRenderingComponents"))
            if(component.at("typeId")==198||component.at("typeId")==199)particleComponents.push_back(component.at("fileId").get<int64_t>());
        if(!particleComponents.empty()&&!EnsureNativeEmbedded269ZombieModel(board,kind,path.string(),&error))throw std::runtime_error(error);
    }
    if (!model->animation.Load(app,path.string(),error,particleComponents)) throw std::runtime_error(error);
    std::vector<const char*> required=money?std::vector<const char*>{"idle","walk"}:std::vector<const char*>{"idle","walk","walk2","attack","die"};
    if(kind==111||kind==109||kind==9)required={"idle","walk","attack","die"};
    if(FusionSubmarine343Kind(kind))required={"idle","walk"};
    if(kind==18)required={"idle","die","shake"};
    if(SpecialZombie269PaperKind(kind))required={"idle","walk","walk2","attack","attack2","losePaper","die"};
    if(kind==104)required.push_back("shoot");
    if(kind==11||kind==12||kind==13)required.insert(required.end(),{"swim","attackinwater","drowning"});
    if(kind==8)required.insert(required.end(),{"walk_","walk1_","attack_"});
    if(kind==0||kind==2||kind==4)required.push_back("attack2");
    if(day)
    {
        required=FusionDayZombie273Pole(kind)?std::vector<const char*>{"idle","walk","jump","walk2","attack","die"}:
            FusionDayZombie273Doll(kind)?std::vector<const char*>{"idle","walk","walk2","attack","attack2","die"}:
            std::vector<const char*>{"idle","walk","attack","die"};
        if(FusionDayZombie273Shooter(kind))required.push_back("shoot");
    }
    if(jackbox)required={"idle","walk","eat","pop","death"};
    if(jackbox)
    {
        for(const auto phase:{FusionJackbox300Phase::Walk,FusionJackbox300Phase::Eat,FusionJackbox300Phase::Pop,FusionJackbox300Phase::Death})
        {
            FusionJackbox300Clip expected;if(!TryGetFusionJackbox300Clip(phase,expected))throw std::runtime_error("missing Jackbox source clip contract");
            const Json* found=nullptr;
            for(const auto& state:package.at("controller").at("states"))if(state.at("name")==expected.state)
            {if(found)throw std::runtime_error("ambiguous Jackbox source state");found=&state;}
            if(!found||found->at("clip")!=expected.state||!found->at("speedParameterActive").get<bool>()||
               found->at("speedParameter")!="Speed"||!std::isfinite(found->at("speed").get<float>())||
               std::abs(found->at("speed").get<float>()-expected.multiplier)>1e-6f||
               std::abs(package.at("clips").at(expected.state).at("duration").get<float>()-expected.seconds)>1e-6f)
                throw std::runtime_error("Jackbox source clip/speed differs");
        }
    }
    if(FusionJacksonFamily367Kind(kind))required={"idle","walk","point","walk1","armraise","eatfromwalk","eatfromraise","death"};
    if(FusionJacksonFamily367Kind(kind))
    {
        struct SourceState {const char* name;const char* clip;float speed;};
        const SourceState expected[]={{"idle","idle",1},{"walk","moonwalk",1.5f},{"point","point",.9f},
            {"walk1","walk",.8f},{"armraise","armraise",.9f},{"eatfromwalk","eat",2},
            {"eatfromraise","eat",2},{"death","death",1.6f}};
        for(const auto& state:expected)
        {
            const Json* found=nullptr;
            for(const auto& record:package.at("controller").at("states"))if(record.at("name").get<std::string>()==state.name)
            {if(found)throw std::runtime_error("ambiguous Jackson source state");found=&record;}
            if(!found||found->at("clip").get<std::string>()!=state.clip||
                !found->at("speedParameterActive").get<bool>()||found->at("speedParameter").get<std::string>()!="Speed"||
                !std::isfinite(found->at("speed").get<float>())||std::abs(found->at("speed").get<float>()-state.speed)>1e-6f)
                throw std::runtime_error("Jackson source state/clip/Speed differs");
        }
    }
    for (const char* state : required)
    {
        const float speed = model->animation.GetStateSpeed(state);
        const float duration = model->animation.GetStateDurationSeconds(state,1.0f);
        if (!std::isfinite(speed) || speed <= 0 || !std::isfinite(duration) || duration <= 0)
            throw std::runtime_error(std::string("missing/unsupported source Zombie state: ")+state);
        model->durations.emplace(state,duration);
    }
    if(BatchPrefab282(kind))
    {
        // Controllers expose state names while source APIs can return their
        // original clip names (Pole run/jump and Football eat). Bind both to
        // the same unique real state; never rename raw source clips or guess.
        const auto addAlias=[&](const std::string& alias,const std::string& state)
        {
            const auto [entry,inserted]=model->stateAliases.emplace(alias,state);
            if(!inserted&&entry->second!=state)throw std::runtime_error("ambiguous source Zombie state/clip alias");
        };
        for(const char* state:required)addAlias(state,state);
        for(const char* state:required)
        {
            const Json* found=nullptr;
            for(const auto& record:package.at("controller").at("states"))if(record.at("name").get<std::string>()==state)
            {if(found)throw std::runtime_error("ambiguous source Zombie controller state");found=&record;}
            if(!found||!found->at("clip").is_string())throw std::runtime_error("missing source Zombie controller clip");
            // Jackson walk/clipwalk and its two eat states intentionally
            // overlap names. Its canonical controller view never uses clip aliases.
            if(!FusionJacksonFamily367Kind(kind))addAlias(found->at("clip").get<std::string>(),state);
        }
        for(const auto& [state,duration]:model->durations)
            for(float phase:{0.f,.5f,1.f})for(const auto& node:package.at("nodes"))
            {
                const std::string nodePath=node.at("path").get<std::string>();float x=0,y=0;bool active=false;
                auto options=model->options;
                // Even a same-state transition validates exact WD1 metadata.
                // The genuine source idle and every phase must be statelessly sampleable.
                if(FusionJacksonFamily367Kind(kind))options.baseBlend=FusionAnimationDrawOptions::BaseBlend{state,phase*duration,.5f};
                if(!model->animation.TryGetNodeOffset(nodePath.c_str(),x,y,state.c_str(),phase*duration,nullptr,-1,1,&options)||
                    !std::isfinite(x)||!std::isfinite(y)||
                    !model->animation.TryGetNodeActivity(nodePath.c_str(),active,state.c_str(),phase*duration,nullptr,-1,1,&options))
                    throw std::runtime_error("source Zombie original node/state probe failed: "+state+"/"+nodePath);
            }
    }
    if(jackbox)
    {
        // All three source phase transitions are fixed .25s. Validate the same
        // WD1/common sampler used by Draw, Node and synchronous part callbacks.
        for(const char* from:{"walk","eat","pop","death"})for(const char* to:{"walk","eat","pop","death"})
            for(float weight:{0.f,.5f,1.f})for(const auto& node:package.at("nodes"))
            {
                const auto path=node.at("path").get<std::string>();float x=0,y=0;bool active=false;
                auto options=model->options;
                options.baseBlend=FusionAnimationDrawOptions::BaseBlend{from,.125f,weight};
                if(!model->animation.TryGetNodeOffset(path.c_str(),x,y,to,.125f,nullptr,-1,1,&options)||
                   !std::isfinite(x)||!std::isfinite(y)||
                   !model->animation.TryGetNodeActivity(path.c_str(),active,to,.125f,nullptr,-1,1,&options))
                    throw std::runtime_error("Jackbox source blended node probe failed");
            }
    }
    if(FusionJacksonFamily367Kind(kind))
    {
        struct TransitionProbe {const char* from;const char* to;float exit,duration;bool fixed;};
        std::vector<TransitionProbe> probes={{"walk","point",.5f,.25f,true},
            {"point","armraise",.8235294f,.25f,true},{"walk1","armraise",.925f,.25f,true},
            {"walk1","eatfromwalk",.5f,.25f,true},{"walk1","point",.9f,.1f,false},
            {"armraise","walk1",1,.02f,true},{"armraise","eatfromraise",.5f,.25f,true},
            {"eatfromwalk","walk1",.5f,.25f,true},{"eatfromraise","armraise",.5f,.25f,true}};
        // AnyState GoDie can start in each source phase, including idle/self.
        for(const char* state:required)probes.push_back({state,"death",.5f,.25f,true});
        for(const auto& probe:probes)
        {
            const float outgoing=model->durations.at(probe.from);
            const float half=.5f*probe.duration*(probe.fixed?1.f:outgoing);
            auto options=model->options;
            options.baseBlend=FusionAnimationDrawOptions::BaseBlend{probe.from,probe.exit*outgoing+half,.5f};
            for(const auto& node:package.at("nodes"))
            {
                const auto nodePath=node.at("path").get<std::string>();float x=0,y=0;bool active=false;
                if(!model->animation.TryGetNodeOffset(nodePath.c_str(),x,y,probe.to,half,nullptr,-1,1,&options)||
                    !std::isfinite(x)||!std::isfinite(y)||
                    !model->animation.TryGetNodeActivity(nodePath.c_str(),active,probe.to,half,nullptr,-1,1,&options))
                    throw std::runtime_error(std::string("Jackson source transition node probe failed: ")+probe.from+"/"+probe.to+"/"+nodePath);
            }
        }
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
    if(!data||(!FindSpecialZombie269Profile(data->kind)&&data->kind!=9))return;
    const char* addon=nullptr;float phase=0;
    if(GetNativeSpecialZombie269AddonPose(zombie,addon,phase))
    {
        const auto duration=model.durations.find(addon);
        if(duration!=model.durations.end())options.extraStates.push_back({addon,phase*duration->second,model.animation.GetStateLayer(addon)});
    }
    if(FusionSubmarine343Kind(data->kind))
    {
        uint8_t stage=0;
        if(model.rootChildren.size()>=6&&GetNativeSpecialZombie269BodyStage(zombie,stage))
            for(size_t n=0;n<6;++n)options.nodeActivityOverrides.push_back({model.rootChildren[n],n%3==stage});
    }
    if(SpecialZombie269PaperKind(data->kind))
    {
        options.nodeActivityOverrides.push_back({"LoseHead",!zombie->mHasHead});
        options.nodeActivityOverrides.push_back({"LoseArm",!zombie->mHasArm});
        if(data->firstArmorStage>0&&data->firstArmorStage<=2)
            ReplaceSprite(options,"Zombie_paper_paper1",model.damagedPaper[data->firstArmorStage-1]);
        options.nodeActivityOverrides.push_back({"LosePaper",zombie->mShieldHealth<=0});
    }
    if(data->kind==9)
    {
        options.nodeActivityOverrides.push_back({"LoseHead",!zombie->mHasHead});
        options.nodeActivityOverrides.push_back({"LoseArm",!zombie->mHasArm});
        options.nodeActivityOverrides.push_back({"LoseCone",zombie->mHelmType==HELMTYPE_NONE||zombie->mHelmHealth<=0});
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

const NativeNightZombie281State* JacksonState292(const Zombie* zombie)
{
    const auto* context=GetNativeContext(zombie?zombie->mBoard:nullptr);
    const auto* data=GetNativeSourceZombieData(zombie);
    if(!context||!data||!FusionJacksonFamily367Kind(data->kind))return nullptr;
    const uint32_t uid=zombie->mBoard->mZombies.DataArrayGetID(const_cast<Zombie*>(zombie));
    const auto found=context->nightZombies281.find(uid);
    return uid&&found!=context->nightZombies281.end()?&found->second:nullptr;
}

bool JacksonPose292(const Zombie* zombie,const ZombieModel& model,FusionAnimationDrawOptions& options)
{
    const auto* data=GetNativeSourceZombieData(zombie);if(!data||!FusionJacksonFamily367Kind(data->kind))return true;
    const auto* state=JacksonState292(zombie);if(!state)return false;
    const char* outgoing=nullptr;float elapsed=0,weight=1;
    if(!GetNativeJackson281BlendPresentation(zombie,*state,outgoing,elapsed,weight))return false;
    if(outgoing)options.baseBlend=FusionAnimationDrawOptions::BaseBlend{outgoing,elapsed,weight};
    else options.baseBlend.reset();
    options.rootFlipX=zombie->mMindControlled!=state->lookBack;
    if(data->kind==207)options.nodeActivityOverrides.push_back({"Trail_1/Trail_1",true}); // Genuine Quick.Start SetActive1; emission is separate canonical state.
    // The reliable part callback owns the detached PS. These flags describe
    // only the current source subtree, never a cosmetic birth inferred by polling.
    options.nodeActivityOverrides.push_back({"LoseHead",!zombie->mHasHead});
    options.nodeActivityOverrides.push_back({"LoseArm",!zombie->mHasArm});
    if(!zombie->mHasHead)options.hiddenNodePaths.insert(options.hiddenNodePaths.end(),model.headPaths.begin(),model.headPaths.end());
    if(!zombie->mHasArm)
    {
        options.hiddenNodePaths.insert(options.hiddenNodePaths.end(),model.handPaths.begin(),model.handPaths.end());
        for(const auto& path:model.upperArmPaths)
        {
            // ZombieJackson.ChangeArmSpirte37DD30 replaces spritePrefab[26]
            // without changing the current source Transform/animation scale.
            if(data->kind==207){if(path=="Zombie_Jackson_outerarm_upper"){
                options.spriteEnabledOverrides.push_back({path,false});
                options.nodeActivityOverrides.push_back({path+"/Zombie_Jackson_outerarm_upper_1",true});}}
            else ReplaceSprite(options,path,model.damagedArm);
        }
    }
    return true;
}

const NativeJackbox300State* JackboxState300(const Zombie* zombie)
{
    const auto* context=GetNativeContext(zombie?zombie->mBoard:nullptr);
    const auto* data=GetNativeSourceZombieData(zombie);
    if(!context||!data||!FusionJackbox300Kind(data->kind))return nullptr;
    const uint32_t uid=zombie->mBoard->mZombies.DataArrayGetID(const_cast<Zombie*>(zombie));
    const auto found=context->jackboxZombies300.find(uid);
    // Ghost lookup is keyed by the local full generation. The adopted source
    // instanceToken is opaque; it is never compared with or replaced by uid.
    return uid&&found!=context->jackboxZombies300.end()?&found->second:nullptr;
}

bool JackboxPose300(const Zombie* zombie,const ZombieModel& model,FusionAnimationDrawOptions& options)
{
    const auto* data=GetNativeSourceZombieData(zombie);if(!data||!FusionJackbox300Kind(data->kind))return true;
    const auto* controller=JackboxState300(zombie);if(!controller)return false;
    const char* outgoing=nullptr;float elapsed=0,weight=1;
    if(!GetNativeJackbox300Blend(zombie,*controller,outgoing,elapsed,weight))return false;
    if(outgoing)options.baseBlend=FusionAnimationDrawOptions::BaseBlend{outgoing,elapsed,weight};
    else options.baseBlend.reset();
    if(!controller->boxPresent)
    {
        options.nodeActivityOverrides.push_back({"Zombie_jackbox_box",false});
        options.nodeActivityOverrides.push_back({"Zombie_jackbox_handle",false});
    }
    // These describe source subtrees only; reliable host semantic callbacks
    // own detached PS birth. Neither Draw nor a baseline sample emits a PS.
    options.nodeActivityOverrides.push_back({"LoseHead",!zombie->mHasHead});
    options.nodeActivityOverrides.push_back({"LoseArm",!zombie->mHasArm});
    if(!zombie->mHasHead)options.hiddenNodePaths.insert(options.hiddenNodePaths.end(),model.headPaths.begin(),model.headPaths.end());
    if(!zombie->mHasArm)for(const auto& path:model.upperArmPaths)ReplaceSprite(options,path,model.damagedArm);
    if(data->kind==30)
    {
        if(data->firstArmorStage>2)return false;
        const bool lost=zombie->mHelmType==HELMTYPE_NONE||zombie->mHelmHealth<=0;
        options.nodeActivityOverrides.push_back({"LoseCone",lost});
        if(lost)options.hiddenNodePaths.push_back(model.conePath);
        else if(data->firstArmorStage)ReplaceSprite(options,model.conePath,model.damagedCone[data->firstArmorStage-1]);
    }
    return true;
}

bool SourcePose273(const Zombie* zombie,const char*& state,float& phase)
{
    const auto* data=GetNativeSourceZombieData(zombie);
    if(data&&FusionJackbox300Kind(data->kind))
    {
        const auto* controller=JackboxState300(zombie);
        return controller&&GetNativeJackbox300Presentation(zombie,*controller,state,phase);
    }
    if(data&&DayPrefab273(data->kind)&&!FusionDayZombie273Doll(data->kind))
        return GetNativeDayZombie273Presentation(zombie,state,phase);
    return GetNativeSourceZombiePresentation(zombie,state,phase);
}

bool SourceBodyPose276(const Zombie* zombie,const char*& state,float& phase)
{
    const auto* data=GetNativeSourceZombieData(zombie);
    if(!data)return false;
    if(FusionJacksonFamily367Kind(data->kind))
    {
        const auto* controller=JacksonState292(zombie);
        return controller&&GetNativeJackson281Presentation(zombie,*controller,state,phase);
    }
    if(FusionJackbox300Kind(data->kind))
    {
        const auto* controller=JackboxState300(zombie);
        return controller&&GetNativeJackbox300Presentation(zombie,*controller,state,phase);
    }
    if(!IsNativeDayZombie273(zombie)&&!SpecialZombie269PaperKind(data->kind)&&data->kind!=9)return false;
    auto* body=zombie->mApp->ReanimationTryToGet(zombie->mBodyReanimID);
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
    if(FusionDayZombie273Pole(data->kind))
        return match("anim_run","walk")||match("anim_jump","jump")||match("anim_walk","walk2")||
            match("anim_eat","attack")||match("anim_death","die")||match("anim_idle","idle");
    if(SpecialZombie269PaperKind(data->kind))
        return match("anim_walk","walk")||match("anim_walk_nopaper","walk2")||match("anim_eat","attack")||
            match("anim_eat_nopaper","attack2")||match("anim_gasp","losePaper")||match("anim_death","die")||match("anim_idle","idle");
    if(data->kind==9)
        return match("anim_walk","walk")||match("anim_eat","attack")||match("anim_death","die")||match("anim_idle","idle");
    if(FusionDayZombie273Doll(data->kind))
        return match("anim_walk","walk")||match("anim_walk2","walk2")||match("anim_eat","attack")||
            match("anim_eat2","attack2")||match("anim_death","die")||match("anim_idle","idle");
    return match("anim_walk2","walk")||match("anim_eat","attack")||
        match("anim_death","die")||match("anim_idle","idle");
}

bool DayPose273(const Zombie* zombie,const ZombieModel& model,FusionAnimationDrawOptions& options)
{
    const auto* data=GetNativeSourceZombieData(zombie);
    if(data&&FusionRandomZombie360Kind(data->kind))
    {
        if(data->firstArmorStage>2)return false;
        if(!zombie->mHasHead)options.hiddenNodePaths.insert(options.hiddenNodePaths.end(),model.headPaths.begin(),model.headPaths.end());
        if(!zombie->mHasArm)
        {
            options.hiddenNodePaths.insert(options.hiddenNodePaths.end(),model.handPaths.begin(),model.handPaths.end());
            for(const auto& path:model.upperArmPaths)ReplaceSprite(options,path,model.damagedArm);
        }
        options.nodeActivityOverrides.push_back({"LoseArm",!zombie->mHasArm});
        if(zombie->mHelmHealth<=0||zombie->mHelmType==HELMTYPE_NONE)options.hiddenNodePaths.push_back(model.conePath);
        else if(data->kind==215)
        {
            options.nodeActivityOverrides.push_back({"Zombie_head/Cone/Cone_1",data->firstArmorStage==1});
            options.nodeActivityOverrides.push_back({"Zombie_head/Cone/Cone_2",data->firstArmorStage==2});
            options.spriteEnabledOverrides.push_back({model.conePath,data->firstArmorStage==0});
        }
        else if(data->firstArmorStage)ReplaceSprite(options,model.conePath,model.damagedCone[data->firstArmorStage-1]);
        return true;
    }
    if(!data||!DayPrefab273(data->kind))return true;
    const auto* day=GetNativeDayZombie273(zombie);
    if(!day||!NativeDayZombie273ControllerValid(data->kind,*day))return false;
    if(FusionDayZombie273Pole(data->kind))options.nodeActivityOverrides.push_back({"Shadow",day->shadowEnabled});
    // These describe the current source subtree only. Day detached particles
    // are started by the actual loss callback, never inferred by the FX poll.
    options.nodeActivityOverrides.push_back({"LoseArm",!zombie->mHasArm});
    if(FusionDayZombie273Pole(data->kind))options.nodeActivityOverrides.push_back({"LoseHead",!zombie->mHasHead});
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
            if(!FusionDayZombie273Pole(data->kind))
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
    else if(FusionDayZombie273NutArmor(data->kind)&&day->armorStage)
        // WallNutZ clears the FirstArmor reference but retains its last head
        // sprite; helm depletion must not remove the actual Zombie_head node.
        ReplaceSprite(options,model.dayArmorPath,model.damagedCone[day->armorStage-1]);
    return true;
}
void ExtendedMutations316(uint16_t kind,const FusionExtendedZombie313View& state,FusionAnimationDrawOptions& options)
{
    switch(kind)
    {
    case 35:
        if(state.bodyStage==1)
        {
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/Zombie_gargantuar_body1_2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/Zombie_gargantuar_body1_3",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_lower/Zombie_gargantuar_outerarm_lower2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerleg_foot/Zombie_gargantuar_foot2",true});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_body1",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_outerarm_lower",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_outerleg_foot",false});
        }
        if(state.bodyStage==2)
        {
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/Zombie_gargantuar_body1_2",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/Zombie_gargantuar_body1_3",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_head/Zombie_gargantuar_head2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_lower/Zombie_gargantuar_outerarm_lower2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerleg_foot/Zombie_gargantuar_foot2",true});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_body1",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_head",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_outerarm_lower",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_outerleg_foot",false});
        }
        if(state.weapon==0)
        {
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_telephonepole/Zombie_gargantuar_zombie",true});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_telephonepole",false});
        }
        if(state.weapon==1)
        {
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_telephonepole/Zombie_gargantuar_duckxing",true});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_telephonepole",false});
        }
        if(state.carriedImpRemoved)
        {
            options.nodeActivityOverrides.push_back({"Zombie_imp_outerarm_lower",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_outerarm_upper",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_innerarm_lower",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_whiterope",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_jaw",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_head",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_body1",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_body2",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_outerleg_upper",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_outerleg_lower",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_outerleg_foot",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_innerleg_upper",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_innerleg_lower",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_innerleg_foot",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_innerarm_upper",false});
        }
        break;
    case 36:
        if(state.bodyStage==1)
        {
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/Zombie_gargantuar_body1_2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/Zombie_gargantuar_body1_3",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_lower/Zombie_gargantuar_outerarm_lower2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerleg_foot/Zombie_gargantuar_foot2",true});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_body1",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_outerarm_lower",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_outerleg_foot",false});
        }
        if(state.bodyStage==2)
        {
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/Zombie_gargantuar_body1_2",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/Zombie_gargantuar_body1_3",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_head/Zombie_gargantuar_head2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_lower/Zombie_gargantuar_outerarm_lower2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerleg_foot/Zombie_gargantuar_foot2",true});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_body1",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_head",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_outerarm_lower",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_outerleg_foot",false});
        }
        if(state.weapon==0)
        {
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_telephonepole/Zombie_gargantuar_zombie",true});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_telephonepole",false});
        }
        if(state.weapon==1)
        {
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_telephonepole/Zombie_gargantuar_duckxing",true});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_telephonepole",false});
        }
        if(state.carriedImpRemoved)
        {
            options.nodeActivityOverrides.push_back({"Zombie_imp_outerarm_lower",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_outerarm_upper",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_innerarm_lower",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_whiterope",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_jaw",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_head",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_body1",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_body2",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_outerleg_upper",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_outerleg_lower",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_outerleg_foot",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_innerleg_upper",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_innerleg_lower",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_innerleg_foot",false});
            options.nodeActivityOverrides.push_back({"Zombie_imp_innerarm_upper",false});
        }
        break;
    case 38:
        if(state.bodyStage>=1||state.uniqueType==0)
        {
            options.spriteEnabledOverrides.push_back({"Zombie_robotgargantuar_head(2)",false});
            options.spriteEnabledOverrides.push_back({"Zombie_robotgargantuar_body1",false});
            options.spriteEnabledOverrides.push_back({"Zombie_robotgargantuar_outerarm_lower",false});
            options.spriteEnabledOverrides.push_back({"Zombie_robotgargantuar_foot-1",false});
            options.spriteEnabledOverrides.push_back({"Zombie_robotgargantuar_head(2)/head_damage",true});
            options.spriteEnabledOverrides.push_back({"Zombie_robotgargantuar_body1/body_damage",true});
            options.spriteEnabledOverrides.push_back({"Zombie_robotgargantuar_outerarm_lower/arm_damage",true});
            options.spriteEnabledOverrides.push_back({"Zombie_robotgargantuar_foot-1/footdamage",true});
        }
        break;
    case 39:
        if(state.bodyStage>=1||state.uniqueType==0)
        {
            options.spriteEnabledOverrides.push_back({"Zombie_robotgargantuar_head(2)",false});
            options.spriteEnabledOverrides.push_back({"Zombie_robotgargantuar_body1",false});
            options.spriteEnabledOverrides.push_back({"Zombie_robotgargantuar_outerarm_lower",false});
            options.spriteEnabledOverrides.push_back({"Zombie_robotgargantuar_foot-1",false});
            options.spriteEnabledOverrides.push_back({"Zombie_robotgargantuar_head(2)/head_damage",true});
            options.spriteEnabledOverrides.push_back({"Zombie_robotgargantuar_body1/body_damage",true});
            options.spriteEnabledOverrides.push_back({"Zombie_robotgargantuar_outerarm_lower/arm_damage",true});
            options.spriteEnabledOverrides.push_back({"Zombie_robotgargantuar_foot-1/footshadow",true});
        }
        break;
    case 43:
        if(state.bodyStage==1)
        {
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/Zombie_gargantuar_body1_2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/Zombie_gargantuar_body1_3",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_lower/Zombie_gargantuar_outerarm_lower_1",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_lower/Zombie_gargantuar_outerarm_lower_2",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerleg_foot/Zombie_gargantuar_foot2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_upper/Zombie_gargantuar_outerarm_upper_1",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_upper/Zombie_gargantuar_outerarm_upper_2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_upper/Zombie_gargantuar_outerarm_upper_3",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_body1",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_outerarm_lower",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_outerleg_foot",false});
        }
        if(state.bodyStage==2)
        {
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/Zombie_gargantuar_body1_2",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/Zombie_gargantuar_body1_3",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_head/Zombie_gargantuar_head2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_lower/Zombie_gargantuar_outerarm_lower_1",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_lower/Zombie_gargantuar_outerarm_lower_2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerleg_foot/Zombie_gargantuar_foot2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_upper/Zombie_gargantuar_outerarm_upper_1",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_upper/Zombie_gargantuar_outerarm_upper_2",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_upper/Zombie_gargantuar_outerarm_upper_3",true});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_body1",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_head",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_outerarm_lower",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_outerleg_foot",false});
        }
        if(state.ballRemoved)
        {
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/SuperGargantuar_ball1",false});
        }
        break;
    case 37:
        break;
    case 40:
        if(state.bodyStage==1)
        {
            options.nodeActivityOverrides.push_back({"yuan1",false});
            options.nodeActivityOverrides.push_back({"yuan2",true});
            options.nodeActivityOverrides.push_back({"yuan3",false});
        }
        if(state.bodyStage==2)
        {
            options.nodeActivityOverrides.push_back({"yuan1",false});
            options.nodeActivityOverrides.push_back({"yuan2",false});
            options.nodeActivityOverrides.push_back({"yuan3",true});
        }
        if(state.machineType==1)
        {
            options.nodeActivityOverrides.push_back({"yuan1/yuan1_2",true});
            options.nodeActivityOverrides.push_back({"yuan2/yuan2_2",true});
            options.nodeActivityOverrides.push_back({"yuan3/yuan3_2",true});
            options.spriteEnabledOverrides.push_back({"yuan1",false});
            options.spriteEnabledOverrides.push_back({"yuan2",false});
            options.spriteEnabledOverrides.push_back({"yuan3",false});
        }
        if(state.machineType==2)
        {
            options.nodeActivityOverrides.push_back({"yuan1/yuan1_1",true});
            options.nodeActivityOverrides.push_back({"yuan2/yuan2_1",true});
            options.nodeActivityOverrides.push_back({"yuan3/yuan3_1",true});
            options.spriteEnabledOverrides.push_back({"yuan1",false});
            options.spriteEnabledOverrides.push_back({"yuan2",false});
            options.spriteEnabledOverrides.push_back({"yuan3",false});
        }
        break;
    case 212:
        if(state.bodyStage==1)
        {
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/Zombie_gargantuar_body1_2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/Zombie_gargantuar_body1_3",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_lower/Zombie_gargantuar_outerarm_lower_1",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_lower/Zombie_gargantuar_outerarm_lower_2",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerleg_foot/Zombie_gargantuar_foot2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_upper/Zombie_gargantuar_outerarm_upper_1",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_upper/Zombie_gargantuar_outerarm_upper_2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_upper/Zombie_gargantuar_outerarm_upper_3",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_body1",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_outerarm_lower",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_outerleg_foot",false});
        }
        if(state.bodyStage==2)
        {
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/Zombie_gargantuar_body1_2",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/Zombie_gargantuar_body1_3",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_head/Zombie_gargantuar_head2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_lower/Zombie_gargantuar_outerarm_lower_1",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_lower/Zombie_gargantuar_outerarm_lower_2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerleg_foot/Zombie_gargantuar_foot2",true});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_upper/Zombie_gargantuar_outerarm_upper_1",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_upper/Zombie_gargantuar_outerarm_upper_2",false});
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_outerarm_upper/Zombie_gargantuar_outerarm_upper_3",true});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_body1",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_head",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_outerarm_lower",false});
            options.spriteEnabledOverrides.push_back({"Zombie_gargantuar_outerleg_foot",false});
        }
        if(state.ballRemoved)
        {
            options.nodeActivityOverrides.push_back({"Zombie_gargantuar_body1/SuperGargantuar_ball1",false});
        }
        break;
    case 213:
        break;
    case 25:
        break;
    case 29:
        break;
    case 26:
        if(state.bodyStage==1)
        {
            options.nodeActivityOverrides.push_back({"11",false});
            options.nodeActivityOverrides.push_back({"21",true});
            options.nodeActivityOverrides.push_back({"31",false});
        }
        if(state.bodyStage==2)
        {
            options.nodeActivityOverrides.push_back({"11",false});
            options.nodeActivityOverrides.push_back({"21",false});
            options.nodeActivityOverrides.push_back({"31",true});
        }
        break;
    }
}


struct Pose316
{
    FusionAnimationDrawOptions options;const char* state=nullptr;float seconds=0;
    float rootX=0,rootY=0,pixels=0;bool visible=true,collider=true;
};
template<class Map>const typename Map::mapped_type* State316(const Zombie* z,const Map& map)
{
    const uint32_t uid=z->mBoard->mZombies.DataArrayGetID(const_cast<Zombie*>(z));
    const auto it=map.find(uid);return uid&&it!=map.end()?&it->second:nullptr;
}
Reanimation* GraphClock316(Zombie* z,uint8_t layer)
{
    const auto* graph=GetNativeSourceGraph313(z);if(!graph||layer>2)return nullptr;
    return z->mApp->ReanimationTryToGet(layer?ReanimationID(graph->auxiliaryUIDs[layer-1]):z->mBodyReanimID);
}
bool GraphPose316(const Zombie* z,Pose316& out)
{
    const auto* data=GetNativeSourceZombieData(z);const auto* graph=GetNativeSourceGraph313(z);
    const auto* source=data?FindFusionSourceGraph313(data->kind):nullptr;
    if(!source||!graph||!graph->initialized||graph->failed)return false;
    NativeSourceGraph313Hooks hooks;hooks.clock=GraphClock316;
    for(uint8_t layer=0;layer<source->layerCount;++layer)
    {
        if(data->kind==117&&layer==1)continue; // Genuine zero-weight/null/no-edge source layer.
        const auto& view=graph->layers[layer];const char* incoming=nullptr,*outgoing=nullptr;float seconds=0,oldSeconds=0,weight=1;
        if(!GetNativeSourceGraph313Presentation(const_cast<Zombie*>(z),*graph,layer,hooks,incoming,seconds,outgoing,oldSeconds,weight))return false;
        // Source non-loop motions hold their last sample even while their
        // real native callback clock exceeds the clip end. Legacy one-shot
        // action overlays keep their existing Apply() expiry semantics.
        const auto clampSample=[&](uint8_t index,float value)
        {
            const auto& state=source->states[index];
            return !state.nullMotion&&!state.loop?(std::min)(value,state.duration/state.speed):value;
        };
        seconds=clampSample(view.state,seconds);
        if(view.fadeDuration)oldSeconds=clampSample(view.outgoing,oldSeconds);
        // A null-motion state retains the real baseline. Its genuine state
        // name selects the sampler's no-op; no substitute clip or clock.
        if(!incoming)incoming=source->states[view.state].name;
        if(view.fadeDuration&&!outgoing)outgoing=source->states[view.outgoing].name;
        if(!layer)
        {out.state=incoming;out.seconds=seconds;if(outgoing)out.options.baseBlend=FusionAnimationDrawOptions::BaseBlend{outgoing,oldSeconds,weight};}
        else
        {
            FusionAnimationDrawOptions::ExtraState extra{incoming,seconds,layer};
            if(outgoing)extra.baseBlend=FusionAnimationDrawOptions::BaseBlend{outgoing,oldSeconds,weight};
            out.options.extraStates.push_back(std::move(extra));
        }
    }
    return out.state!=nullptr;
}
bool Extra316(const ZombieModel& model,FusionAnimationDrawOptions& options,const std::string& node,const char* asset,float scale=0)
{
    const auto found=model.extras316.find(asset);if(found==model.extras316.end()||node.empty())return false;
    ReplaceSprite(options,node,found->second);
    if(scale){auto& r=options.spriteReplacements.back();r.overrideScale=true;r.scaleX=r.scaleY=r.scaleZ=scale;}
    return true;
}
bool BrokenChildren316(const ZombieModel& model,FusionAnimationDrawOptions& options,const std::string& parent,int stage)
{
    if(!stage)return true;
    const auto children=model.children316.find(parent);
    if(stage<1||stage>2||children==model.children316.end()||children->second.size()<2)return false;
    options.spriteEnabledOverrides.push_back({parent,false});
    options.nodeActivityOverrides.push_back({children->second[0],stage==1});
    options.nodeActivityOverrides.push_back({children->second[1],stage==2});return true;
}

bool FieldChildren351(const ZombieModel& m,FusionAnimationDrawOptions& o,const char* field,int stage,bool spriteChildren=false)
{
    const auto f=m.fields351.find(field);if(f==m.fields351.end())return false;
    for(const auto& parent:f->second)
    {
        const auto c=m.children316.find(parent);if(c==m.children316.end()||c->second.size()<2)return false;
        o.spriteEnabledOverrides.push_back({parent,stage==0});
        if(spriteChildren)
        {o.spriteEnabledOverrides.push_back({c->second[0],stage==1});o.spriteEnabledOverrides.push_back({c->second[1],stage==2});}
        else
        {o.nodeActivityOverrides.push_back({c->second[0],stage==1});o.nodeActivityOverrides.push_back({c->second[1],stage==2});}
    }
    return true;
}
bool VariantPose351(const Zombie* z,const ZombieModel& m,const FusionExtendedZombie313View& v,FusionAnimationDrawOptions& o)
{
    const auto* d=GetNativeSourceZombieData(z);if(!d)return false;const auto kind=d->kind;
    if(kind==205||kind==210||kind==219)
    {
        const char* a=kind==205?"11":"yuan1";const char* b=kind==205?"21":"yuan2";const char* c=kind==205?"31":"yuan3";
        for(const auto* name:{a,b,c})if(!m.nodePaths316.contains(name))return false;
        if(v.bodyStage){o.nodeActivityOverrides.push_back({a,false});o.nodeActivityOverrides.push_back({b,v.bodyStage==1});o.nodeActivityOverrides.push_back({c,v.bodyStage==2});}
    }
    if(kind==221&&v.bodyStage&&!FieldChildren351(m,o,"changeSprites",v.bodyStage,true))return false;
    if(kind==224&&!FieldChildren351(m,o,"changeSprites",v.bodyStage))return false;
    if(kind==218)
    {
        const auto held=m.fields351.find("w");if(held==m.fields351.end()||held->second.size()!=1)return false;
        o.nodeActivityOverrides.push_back({held->second.front(),v.bombPresent});
    }
    if(kind==202||kind==204||kind==218||kind==220)
    {
        if(!z->mHasHead)o.hiddenNodePaths.insert(o.hiddenNodePaths.end(),m.headPaths.begin(),m.headPaths.end());
        if(!z->mHasArm)
        {
            o.hiddenNodePaths.insert(o.hiddenNodePaths.end(),m.handPaths.begin(),m.handPaths.end());
            // TallNut/UltimateFootball/Drown218 ChangeArmSpirte30F780 is empty.
            if(kind==202)for(const auto& arm:m.upperArmPaths)if(!Extra316(m,o,arm,"Resources/zombies/zombie_drown/losearm.asset",1.5f))return false;
        }
        if(kind==202||kind==204)
        {
            if(v.firstArmorStage&&!Extra316(m,o,m.conePath,kind==202?(v.firstArmorStage==1?
                "Resources/zombies/zombie_football/tallnutfootballz/tnf2.asset":"Resources/zombies/zombie_football/tallnutfootballz/tnf3.asset"):
                (v.firstArmorStage==1?"Resources/zombies/intravel/blackfootball/head1.asset":"Resources/zombies/intravel/blackfootball/head2.asset")))return false;
        }
        else if((kind==220||v.firstArmorStage)&&!FieldChildren351(m,o,"theFirstArmor",v.firstArmorStage))return false;
        if(kind==220)
        {
            const auto flags=m.fields351.find("flags");if(flags==m.fields351.end())return false;
            for(const auto& parent:flags->second)
            {
                const auto child=m.child316.find(parent);if(child==m.child316.end())return false;
                o.spriteEnabledOverrides.push_back({parent,v.firstArmorStage==0});o.nodeActivityOverrides.push_back({child->second,v.firstArmorStage!=0});
            }
        }
        // WallNutZ.FirstArmorTakeDamage356B70 clears the reference/HP/type
        // without destroying this GameObject. Keep its last damaged art.
        // Actual base head removal independently removes only tagged nodes.
    }
    return true;
}
bool CatapultPose351(const ZombieModel& m,uint16_t kind,const NativeHeavyZombie313BState& v,FusionAnimationDrawOptions& o)
{
    if(kind==112)
    {
        for(size_t n=3;n<=8;++n)
        {
            const unsigned threshold=n==3?24:n==4?23:n==5?22:n==6?21:20;
            if(v.ammo>threshold)continue;
            if(m.rootChildren.size()<=n)return false;const auto& parent=m.rootChildren[n];const auto c=m.child316.find(parent);if(c==m.child316.end())return false;
            o.spriteEnabledOverrides.push_back({parent,false});o.nodeActivityOverrides.push_back({c->second,true});
        }
        for(size_t n=3;n<=6;++n)if(v.ammo<=7-n)
        {if(m.rootChildren.size()<=n)return false;o.hiddenNodePaths.push_back(m.rootChildren[n]);}
    }
    if(kind==216)
    {
        if(v.catapultFlags343&1)
        {
            const auto fields=m.fields351.find("damaged");if(fields==m.fields351.end())return false;
            for(const auto& parent:fields->second){const auto c=m.child316.find(parent);if(c==m.child316.end())return false;o.spriteEnabledOverrides.push_back({parent,false});o.nodeActivityOverrides.push_back({c->second,true});}
        }
        for(const auto& pair:{std::pair<const char*,uint8_t>{"weapons",uint8_t(2)},{"weapons2",uint8_t(4)}})if(v.catapultFlags343&pair.second)
        {const auto fields=m.fields351.find(pair.first);if(fields==m.fields351.end())return false;o.hiddenNodePaths.insert(o.hiddenNodePaths.end(),fields->second.begin(),fields->second.end());}
        if(v.armorStage&&!FieldChildren351(m,o,"theFirstArmor",v.armorStage))return false;
    }
    return true;
}

bool BossPose366(const Zombie* z,const ZombieModel& m,Pose316& pose)
{
 const auto* s=GetNativeBoss361State(z);if(!s||!s->initialized||s->failed||!FusionBoss361ViewValid(s->view))return false;
 const auto& v=s->view;pose.collider=v.colliderEnabled;pose.options.rootFlipX=z->mMindControlled;
 const auto child=[&](const char* field,size_t n,bool active)
 {
  const auto f=m.fields351.find(field);if(f==m.fields351.end()||f->second.size()!=1)return false;
  const auto c=m.children316.find(f->second.front());if(c==m.children316.end()||n>=c->second.size())return false;
  pose.options.nodeActivityOverrides.push_back({c->second[n],active});return true;
 };
 const auto renderer=[&](const char* field)
 {pose.options.spriteEnabledOverrides.push_back({m.fields351.at(field).front(),false});};
 const bool low=(v.bodyHistoryMask&2)!=0,mid=(v.bodyHistoryMask&1)!=0;
 if(v.bodyStage)
 {
  for(const char* f:{"thumb","jaw","hand"})
  {renderer(f);if(!child(f,0,v.bodyStage==1)||!child(f,1,v.bodyStage==2))return false;}
  if(v.kind==44)
  {
   renderer("head");if(!child("head",0,v.bodyStage==1)||!child("head",1,v.bodyStage==2))return false;
   if(mid){renderer("foot_inner");if(!child("foot_inner",0,true))return false;}
   if(low){renderer("foot_outter");if(!child("foot_outter",0,true)||!child("head",2,true)||!child("head",3,true))return false;}
  }
  else
  {
   if(!child("head",0,false)||!child("head",1,v.bodyStage==1)||!child("head",2,v.bodyStage==2))return false;
   if(mid&&!child("foot_inner",0,false))return false;
   if(mid&&!child("foot_inner",1,true))return false;
   if(low){renderer("foot_outter");if(!child("foot_outter",0,true)||!child("head",3,true)||!child("head",4,true))return false;}
  }
 }
 if(!child("eye",0,(v.eyeMask&1)!=0)||!child("eye",1,(v.eyeMask&2)!=0))return false;
 pose.options.nodeActivityOverrides.push_back({m.fields351.at("animRV").front(),v.rvVisible});
 if(v.rvAligned)pose.options.nodeWorldPositions.push_back({m.fields351.at("animRV").front(),v.rvOffsetX,v.rvOffsetY});
 // Source eyes start white and no source clip changes their color. ColoredEye
 // writes Color.Lerp(white,black,elapsed/.75), alpha remains one.
 if(v.dying){const float white=1-v.eyeBlackSeconds/.75f;pose.options.spriteColorOverrides.push_back({m.fields351.at("eye").front(),{white,white,white,1}});}
 return true;
}

bool NewPose316(const Zombie* z,const ZombieModel& model,Pose316& out)
{
    const auto* data=GetNativeSourceZombieData(z);const auto* context=GetNativeContext(z?z->mBoard:nullptr);
    if(!data||!context||!CohortPrefab316(data->kind)||!std::isfinite(z->mScaleZombie)||z->mScaleZombie<=0||z->mScaleZombie>8)return false;
    Pose316 pose;pose.options=model.options;pose.pixels=kPixelsPerSourceUnit*z->mScaleZombie;
    float sx=0,sy=0;if(!NativeSourceGroundAnchor313(z,sx,sy)||!std::isfinite(sx)||!std::isfinite(sy)||!std::isfinite(z->mAltitude))return false;
    const auto kind=data->kind;
    if(FusionWaterZombie313Kind(kind))
    {
        const auto* state=State316(z,context->waterZombies313);const char* incoming=nullptr,*outgoing=nullptr;float phase=0,time=0,weight=1;
        if(!state||!GetNativeWaterZombie313Presentation(z,*state,incoming,phase)||!GetNativeWaterZombie313Blend(z,*state,outgoing,time,weight))return false;
        const auto duration=model.durations.find(incoming);if(duration==model.durations.end())return false;
        pose.state=incoming;pose.seconds=phase*duration->second;pose.options.rootFlipX=state->facingRight;pose.collider=state->colliderEnabled;
        if(outgoing)pose.options.baseBlend=FusionAnimationDrawOptions::BaseBlend{outgoing,time,weight};
        if(kind!=20)
        {
            const char* aux=nullptr;float seconds=0,w=0;int layer=-1;
            if(!GetNativeWaterZombie313Auxiliary(z,*state,aux,seconds,w,layer))return false;
            pose.options.extraStates.push_back({aux,seconds,layer,w});
        }

        if(FusionWaterShooter343Kind(kind))
        {
            const char* shot=nullptr,*old=nullptr;float elapsed=0,oldElapsed=0,w=1;
            if(!GetNativeWaterShoot343Presentation(z,shot,elapsed,old,oldElapsed,w))return false;
            const auto* source=FindFusionSourceGraph313(kind);if(!source||source->layerCount!=3)return false;
            // The genuine null state is a no-op sampled from the same baseline.
            // Its state name is retained so additive null<->shoot fades share
            // the established two-pose sampler; no synthetic zero-length clip.
            if(!shot)shot=source->states[state->shoot343.state].name;
            if(state->shoot343.fadeDuration&&!old)old=source->states[state->shoot343.outgoing].name;
            FusionAnimationDrawOptions::ExtraState extra{shot,elapsed,2};
            if(old)extra.baseBlend=FusionAnimationDrawOptions::BaseBlend{old,oldElapsed,w};
            pose.options.extraStates.push_back(std::move(extra));
            if(kind==214)
            {pose.options.nodeActivityOverrides.push_back({model.fields351.at("head1").front(),state->gatlingReady343});
             pose.options.nodeActivityOverrides.push_back({model.fields351.at("head2").front(),!state->gatlingReady343});}
        }
        if(kind==16)
        {
            pose.options.nodeActivityOverrides.push_back({"Zombie_zamboni_2/SmokePos",state->driverStage==2});
            if(state->driverStage>=1&&!Extra316(model,pose.options,"Zombie_zamboni_1","Resources/zombies/zombie_driver/Zombie_zamboni_1_damage1.asset"))return false;
            if(state->driverStage>=2)
            {
                pose.options.spriteReplacements.clear();
                if(!Extra316(model,pose.options,"Zombie_zamboni_1","Resources/zombies/zombie_driver/Zombie_zamboni_1_damage2.asset")||
                   !Extra316(model,pose.options,"Zombie_zamboni_2","Resources/zombies/zombie_driver/Zombie_zamboni_2_damage2.asset"))return false;
            }
        }
        else
        {
            if(!z->mHasHead)pose.options.hiddenNodePaths.insert(pose.options.hiddenNodePaths.end(),model.headPaths.begin(),model.headPaths.end());
            const bool arm=!z->mHasArm&&(!FusionDolphin343Kind(kind)||(state->sourceStatus!=8&&state->sourceStatus!=9));
            if(arm)
            {
                pose.options.hiddenNodePaths.insert(pose.options.hiddenNodePaths.end(),model.handPaths.begin(),model.handPaths.end());
                const char* asset=kind==17?"Resources/zombies/zombie_snorkle/Zombie_snorkle_outerarm_upper2.asset":kind==19?
                    "Resources/zombies/zombie_dolphinrider/Zombie_dolphinrider_outerarm_upper2.asset":kind==20?
                    "Resources/zombies/zombie_drown/losearm.asset":"Resources/zombies/zombie_dolphinrider/snowdolphinrider/arm.asset";
                for(const auto& node:model.upperArmPaths)if(!Extra316(model,pose.options,node,asset,kind==20?1.5f:(kind==27||FusionWaterShooter343Kind(kind))?1.f:0.f))return false;
            }
        }
    }
    else
    {
        if(!GraphPose316(z,pose))return false;
        if(FusionBoss361Kind(kind))
        {if(!BossPose366(z,model,pose))return false;}
        else if(const auto* state=State316(z,context->extendedZombies313))
        {
            pose.options.rootFlipX=state->rootFlip;pose.visible=state->visible;
            ExtendedMutations316(kind,*state,pose.options);
            if(FusionExtendedZombie347Kind(kind)&&!VariantPose351(z,model,*state,pose.options))return false;
            if(kind==37)
            {
                // The genuine one-source-frame yield then enables every
                // renderer except the retained bone child. No new clock.
                for(const auto& node:model.spriteNodes316)
                    pose.options.spriteEnabledOverrides.push_back({node,node!="Zombie_imp_outerarm_upper/bone"});
                if(!z->mHasHead)pose.options.hiddenNodePaths.insert(pose.options.hiddenNodePaths.end(),model.headPaths.begin(),model.headPaths.end());
                if(!z->mHasArm)
                {
                    pose.options.hiddenNodePaths.insert(pose.options.hiddenNodePaths.end(),model.handPaths.begin(),model.handPaths.end());
                    for(const auto& node:model.upperArmPaths)
                    {
                        pose.options.spriteEnabledOverrides.push_back({node,false});
                        const auto child=model.child316.find(node);if(child==model.child316.end())return false;
                        pose.options.spriteEnabledOverrides.push_back({child->second,true});
                    }
                }
            }
            else if(FusionSnowZombie363Kind(kind))
            {
                if(!z->mHasHead)pose.options.hiddenNodePaths.insert(pose.options.hiddenNodePaths.end(),model.headPaths.begin(),model.headPaths.end());
                if(!z->mHasArm)
                {
                    pose.options.hiddenNodePaths.insert(pose.options.hiddenNodePaths.end(),model.handPaths.begin(),model.handPaths.end());
                    for(const auto& arm:model.upperArmPaths)
                    {
                        const auto child=model.child316.find(arm);if(child==model.child316.end())return false;
                        pose.options.spriteEnabledOverrides.push_back({arm,false});pose.options.nodeActivityOverrides.push_back({child->second,true});
                    }
                }
            }
            else if(kind==208){
                if(!z->mHasHead)pose.options.hiddenNodePaths.insert(pose.options.hiddenNodePaths.end(),model.headPaths.begin(),model.headPaths.end());
                if(!z->mHasArm){pose.options.hiddenNodePaths.insert(pose.options.hiddenNodePaths.end(),model.handPaths.begin(),model.handPaths.end());
                    for(const auto& arm:model.upperArmPaths){pose.options.spriteEnabledOverrides.push_back({arm,false});pose.options.nodeActivityOverrides.push_back({arm+"/arm1_1",true});}}}
            else if(kind==115||kind==116||kind==117||kind==49||kind==114||kind==118)
            {
                if(!z->mHasHead)pose.options.hiddenNodePaths.insert(pose.options.hiddenNodePaths.end(),model.headPaths.begin(),model.headPaths.end());
                if(!z->mHasArm)
                {
                    pose.options.hiddenNodePaths.insert(pose.options.hiddenNodePaths.end(),model.handPaths.begin(),model.handPaths.end());
                    for(const auto& arm:model.upperArmPaths)if(!Extra316(model,pose.options,arm,(kind==115||kind==116||kind==117||kind==49)?"Resources/zombies/image/reanim/Zombie_outerarm_upper2.asset":"Resources/Zombies/Image/reanim/Zombie_outerarm_upper2.asset",4.f))return false;
                }
            }
            else if(kind==25||kind==29||kind==119||kind==213)
            {
                if(!z->mHasHead)pose.options.hiddenNodePaths.insert(pose.options.hiddenNodePaths.end(),model.headPaths.begin(),model.headPaths.end());
                if(!z->mHasArm)
                {
                    pose.options.hiddenNodePaths.insert(pose.options.hiddenNodePaths.end(),model.handPaths.begin(),model.handPaths.end());
                    for(const auto& node:model.upperArmPaths)
                        if(!Extra316(model,pose.options,node,kind==213?"Resources/zombies/image/reanim/Zombie_outerarm_upper2.asset":
                            kind==119?"Resources/Zombies/BalloonZombie/upperarm2.asset":"Resources/zombies/balloonzombie/upperarm2.asset",kind==213?4.f:0.f))return false;
                }
            }
            if(kind==201)
            {
                const auto stage=state->bodyStage;
                if(stage&&!Extra316(model,pose.options,"Zombie_zamboni_1",stage==1?"Resources/Zombies/InTravel/JacksonDriver/damage1.asset":"Resources/Zombies/InTravel/JacksonDriver/damage2.asset"))return false;
                if(stage&&!Extra316(model,pose.options,"Zombie_zamboni_2",stage==1?"Resources/Zombies/InTravel/JacksonDriver/Zombie_zamboni_2_damage1.asset":"Resources/Zombies/InTravel/JacksonDriver/Zombie_zamboni_2_damage2.asset"))return false;
                if(stage==2&&!Extra316(model,pose.options,"Zombie_zamboni_2/lower_front","Resources/zombies/image/reanim/Zombie_outerarm_upper2.asset"))return false;
                if(stage==2&&!Extra316(model,pose.options,"Zombie_zamboni_2/lower_back","Resources/zombies/image/reanim/Zombie_outerarm_upper2.asset"))return false;
                pose.options.nodeActivityOverrides.push_back({"Zombie_zamboni_2/SmokePos",stage==2});
            }
            if(kind==222)
            {
                pose.options.nodeLocalRotations.push_back({"QuickJacksonZombie",(uint32_t(data->sourceStatusLevel)&1U)?std::array<float,4>{0,1,0,0}:std::array<float,4>{0,0,0,1}});
                if(state->bodyStage){pose.options.spriteEnabledOverrides.push_back({"cartop1",false});pose.options.nodeActivityOverrides.push_back({"cartop1/cartop1_1",state->bodyStage==1});pose.options.nodeActivityOverrides.push_back({"cartop1/cartop1_2",state->bodyStage==2});}
                if(state->bodyStage){pose.options.spriteEnabledOverrides.push_back({"back",false});pose.options.nodeActivityOverrides.push_back({"back/back_1",state->bodyStage==1});pose.options.nodeActivityOverrides.push_back({"back/back_2",state->bodyStage==2});}
                if(state->bodyStage){pose.options.spriteEnabledOverrides.push_back({"HAT",false});pose.options.nodeActivityOverrides.push_back({"HAT/HAT_1",state->bodyStage==1});pose.options.nodeActivityOverrides.push_back({"HAT/HAT_2",state->bodyStage==2});}
                if(state->bodyStage){pose.options.spriteEnabledOverrides.push_back({"Zombie_zamboni_2_damage1_2",false});pose.options.nodeActivityOverrides.push_back({"Zombie_zamboni_2_damage1_2/Zombie_zamboni_2_damage1_3",state->bodyStage==1});pose.options.nodeActivityOverrides.push_back({"Zombie_zamboni_2_damage1_2/Zombie_zamboni_2_damage1_4",state->bodyStage==2});}
                if(state->bodyStage){pose.options.spriteEnabledOverrides.push_back({"hand",false});pose.options.nodeActivityOverrides.push_back({"hand/hand_1",state->bodyStage==1});pose.options.nodeActivityOverrides.push_back({"hand/hand_2",state->bodyStage==2});}
                if(state->bodyStage){pose.options.spriteEnabledOverrides.push_back({"3_2",false});pose.options.nodeActivityOverrides.push_back({"3_2/3_3",state->bodyStage==1});pose.options.nodeActivityOverrides.push_back({"3_2/3_4",state->bodyStage==2});}
                if(state->bodyStage){pose.options.spriteEnabledOverrides.push_back({"Credits_speaker \u0436\u2039\u00b7\u0438\u0491\u045c_2",false});pose.options.nodeActivityOverrides.push_back({"Credits_speaker \u0436\u2039\u00b7\u0438\u0491\u045c_2/Credits_speaker",state->bodyStage==1});pose.options.nodeActivityOverrides.push_back({"Credits_speaker \u0436\u2039\u00b7\u0438\u0491\u045c_2/Credits_speaker_1",state->bodyStage==2});}
                if(state->bodyStage){pose.options.spriteEnabledOverrides.push_back({"Credits_speaker_1",false});pose.options.nodeActivityOverrides.push_back({"Credits_speaker_1/Credits_speaker_2",state->bodyStage==1});pose.options.nodeActivityOverrides.push_back({"Credits_speaker_1/Credits_speaker_3",state->bodyStage==2});}
                if(state->bodyStage){pose.options.spriteEnabledOverrides.push_back({"4_2",false});pose.options.nodeActivityOverrides.push_back({"4_2/4_3",state->bodyStage==1});pose.options.nodeActivityOverrides.push_back({"4_2/4_4",state->bodyStage==2});}
                if(state->bodyStage>=1)pose.options.nodeActivityOverrides.push_back({"Credits_light_red_1",false});
                if(state->bodyStage==2)pose.options.nodeActivityOverrides.push_back({"Credits_light_red_2",false});
                if(state->bodyStage==2)pose.options.nodeActivityOverrides.push_back({"Credits_light_yellow",false});
                if(state->bodyStage==2)pose.options.nodeActivityOverrides.push_back({"Credits_speaker_overlay_1",false});
                if(state->bodyStage==2)pose.options.nodeActivityOverrides.push_back({"Credits_speaker_overlay \u0436\u2039\u00b7\u0438\u0491\u045c_1",false});
            }
            if((kind==115||kind==116)&&state->squashLaunched363)pose.options.hiddenNodePaths.push_back("Zombie_head/SquashPrefab");
            if(kind==48)
            {
                if(!FieldChildren351(model,pose.options,"theFirstArmor",state->firstArmorStage))return false;
                if(z->mHelmType==HELMTYPE_NONE||z->mHelmHealth<=0)pose.options.hiddenNodePaths.push_back(model.conePath);
            }
            if(kind==114)
            {
                if(state->firstArmorStage&&!Extra316(model,pose.options,model.conePath,state->firstArmorStage==1?"Resources/Zombies/Image/reanim/Zombie_bucket2.asset":"Resources/Zombies/Image/reanim/Zombie_bucket3.asset"))return false;
                if(z->mHelmType==HELMTYPE_NONE||z->mHelmHealth<=0)pose.options.hiddenNodePaths.push_back(model.conePath);
                const auto& shield=model.fields351.at("theSecondArmor").front();
                if(state->secondArmorStage343&&!Extra316(model,pose.options,shield,state->secondArmorStage343==1?"Resources/Zombies/Door/Zombie_screendoor2.asset":"Resources/Zombies/Door/Zombie_screendoor3.asset"))return false;
                if(z->mShieldType==SHIELDTYPE_NONE||z->mShieldHealth<=0)
                {pose.options.hiddenNodePaths.push_back(shield);const auto& hands=model.fields351.at("doorHands");pose.options.hiddenNodePaths.insert(pose.options.hiddenNodePaths.end(),hands.begin(),hands.end());}
            }
            if(kind==118&&state->firstArmorStage&&!FieldChildren351(model,pose.options,"theFirstArmor",state->firstArmorStage))return false;
            if(kind==29&&state->firstArmorStage&&!Extra316(model,pose.options,model.conePath,state->firstArmorStage==1?
                "Resources/zombies/plantzombie/ironNut2.asset":"Resources/zombies/plantzombie/ironNut3.asset"))return false;
            if((kind==43||kind==212)&&state->firstArmorStage)
            {
                // Actual first-armor reference is loaded below from the
                // genuine prefab, independently of the custom body damage.
                if(!model.conePath.empty()&&!Extra316(model,pose.options,model.conePath,state->firstArmorStage==1?
                    "Resources/zombies/jackbox/superjackboxzombie/SuperJackbox_cone2.asset":
                    "Resources/zombies/jackbox/superjackboxzombie/SuperJackbox_cone3.asset"))return false;
            }
            // Base FirstArmorTakeDamage384BA0 destroys the actual helmet.
            // IronBalloon29 overrides it and retains the last sprite instead.
            if((kind==43||kind==212)&&(z->mHelmType==HELMTYPE_NONE||z->mHelmHealth<=0))
                pose.options.hiddenNodePaths.push_back(model.conePath);
        }
        else if(const auto* heavy=State316(z,context->heavyZombies313B))
        {
            // Same source-facing predicate as the heavy gameplay bridge.
            pose.options.rootFlipX=z->mMindControlled!=heavy->lookBack;
            if(kind==28&&!heavy->axePresent)pose.options.hiddenNodePaths.push_back("Zombie_digger_pickaxe");
            if(kind==32&&(heavy->lostJumper||heavy->status==0))for(const char* node:{"Zombie_pogo_stick2","Zombie_pogo_stick3"})pose.options.hiddenNodePaths.push_back(node);
            if(kind==33&&!heavy->ladderVisible)pose.options.hiddenNodePaths.push_back("Zombie_ladder_1");
            if(FusionCatapult343Kind(kind))
            {
                // Source3515B0 permanently switches its damaged renderers.
                // The canonical latch survives a later legitimate heal.
                if(kind!=216&&heavy->armorStage)for(size_t index:{size_t(1),size_t(2)})
                {
                    if(model.rootChildren.size()<=index)return false;
                    const auto& parent=model.rootChildren[index];const auto child=model.child316.find(parent);
                    if(child==model.child316.end())return false;
                    pose.options.spriteEnabledOverrides.push_back({parent,false});
                    pose.options.spriteEnabledOverrides.push_back({child->second,true});
                }
            }
            else
            {
                if(!z->mHasHead)pose.options.hiddenNodePaths.insert(pose.options.hiddenNodePaths.end(),model.headPaths.begin(),model.headPaths.end());
                if(!z->mHasArm)
                {
                    pose.options.hiddenNodePaths.insert(pose.options.hiddenNodePaths.end(),model.handPaths.begin(),model.handPaths.end());
                    if(kind==32||kind==34||kind==209)
                    {
                        for(const auto& parent:model.upperArmPaths)
                        {
                            const auto child=model.child316.find(parent);if(child==model.child316.end())return false;
                            const auto sprite=model.sprite316.find(child->second);if(sprite==model.sprite316.end())return false;
                            ReplaceSprite(pose.options,parent,sprite->second);
                        }
                        if(model.rootChildren.empty())return false;
                        const auto& parent=model.rootChildren.front();const auto child=model.child316.find(parent);
                        if(child==model.child316.end())return false;
                        pose.options.spriteEnabledOverrides.push_back({parent,false});
                        pose.options.spriteEnabledOverrides.push_back({child->second,true});
                    }
                    else if(kind==33||kind==223)
                        for(const auto& parent:model.upperArmPaths)
                        {
                            const auto child=model.child316.find(parent);if(child==model.child316.end())return false;
                            pose.options.spriteEnabledOverrides.push_back({parent,false});
                            pose.options.nodeActivityOverrides.push_back({child->second,true});
                        }
                    else if(kind==28||kind==203)
                        for(const auto& parent:model.upperArmPaths)
                            if(!Extra316(model,pose.options,parent,kind==28?
                                "Resources/zombies/zombie_digger/Zombie_digger_outerarm_upper2.asset":
                                "Resources/zombies/paperzombie/paper_bone1.asset"))return false;
                    // Bungi.ChangeArm30F780 is genuinely empty. Its upper-arm
                    // renderer must not receive an invented base replacement.
                }
            }
            if((kind==112||kind==216)&&!CatapultPose351(model,kind,*heavy,pose.options))return false;
            if((kind==34||kind==209)&&!BrokenChildren316(model,pose.options,"Zombie_pogo_head",heavy->armorStage))return false;
            if(kind==33)
            {
                if(heavy->ladderVisible&&!BrokenChildren316(model,pose.options,"Zombie_ladder_1",heavy->armorStage))return false;
            }
            if(kind==223)
            {
                if(z->mHelmType==HELMTYPE_NONE||z->mHelmHealth<=0)pose.options.hiddenNodePaths.push_back("Head/GatlingPea_helmet");
                else if(heavy->gold.firstStage)
                    // Genuine UltimatePaper first-armor uses child0 for BOTH lower-third
                    // branches. Preserve that source oddity, not filename order.
                    if(!BrokenChildren316(model,pose.options,"Head/GatlingPea_helmet",1))return false;
                if(z->mShieldType==SHIELDTYPE_NONE||z->mShieldHealth<=0)pose.options.hiddenNodePaths.push_back("book1");
                else if(!BrokenChildren316(model,pose.options,"book1",heavy->gold.secondStage))return false;
            }
            if(kind==203)
            {
                if(z->mShieldHealth<=0||z->mShieldType==SHIELDTYPE_NONE)pose.options.hiddenNodePaths.push_back("Zombie_paper_paper1");
                else if(heavy->gold.secondStage&&!Extra316(model,pose.options,"Zombie_paper_paper1",heavy->gold.secondStage==1?
                    "Resources/Zombies/PlantZombie/Paper/CherryPaper1.asset":"Resources/Zombies/PlantZombie/Paper/CherryPaper2.asset"))return false;
            }
            if(kind==45)
            {
                const auto cord=model.sprite316.find("BungeeCord");if(cord==model.sprite316.end())return false;
                ReplaceSprite(pose.options,"BungeeCord",cord->second);
                auto& replacement=pose.options.spriteReplacements.back();replacement.overrideScale=true;
                replacement.scaleX=heavy->bungee.cordScaleX;replacement.scaleY=heavy->bungee.cordScaleY;replacement.scaleZ=heavy->bungee.cordScaleZ;
                pose.options.spriteEnabledOverrides.push_back({"BungeeCord",true});
                pose.options.nodeActivityOverrides.push_back({"BungeeTarget",heavy->bungee.markerVisible});
                pose.collider=heavy->bungee.status!=24;
            }
            // Native ground clipping is the explicitly approved Digger
            // presentation adaptation for unavailable built-in sprite10905.
            if(kind==28)
            {
                bool mask=false;
                if(!model.animation.TryGetNodeActivity("Mask",mask,pose.state,pose.seconds,nullptr,-1,1,&pose.options))return false;
                if(mask)pose.options.nativeMaskClipBottom=sy-z->mY;
            }
        }
        else return false;
    }
    const bool baseParts=kind==115||kind==116||kind==117||kind==209||kind==208||kind==49||FusionSnowZombie363Kind(kind)||kind==114||kind==118||kind==119||kind==17||kind==19||kind==20||kind==27||FusionWaterShooter343Kind(kind)||kind==25||kind==29||kind==37||kind==213||
        kind==28||kind==32||kind==33||kind==34||kind==45||kind==203||kind==223||
        kind==202||kind==204||kind==218||kind==220;
    if(baseParts)
    {
        if(model.nodePaths316.contains("LoseHead"))pose.options.nodeActivityOverrides.push_back({"LoseHead",!z->mHasHead});
        if(model.nodePaths316.contains("LoseArm"))pose.options.nodeActivityOverrides.push_back({"LoseArm",!z->mHasArm});
    }
    if(kind==203&&model.nodePaths316.contains("LosePaper"))pose.options.nodeActivityOverrides.push_back({"LosePaper",z->mShieldHealth<=0});
    const int alpha=z->mZombieFade<0?255:int((std::clamp)(255LL*z->mZombieFade/10,0LL,255LL));
    if(z->mMindControlled)pose.options.colorMultiplierRGBA={128,64,192,alpha};
    else if(z->mChilledCounter>0||z->mIceTrapCounter>0)pose.options.colorMultiplierRGBA={75,75,255,alpha};
    else pose.options.colorMultiplierRGBA={255,255,255,alpha};
    pose.rootX=sx-(pose.options.rootFlipX?-model.shadowX:model.shadowX)*pose.pixels;
    pose.rootY=sy+model.shadowY*pose.pixels-z->mAltitude;
    if(kind==37)pose.options.nodeWorldPositions.push_back({"RealShadow",(sx-pose.rootX)/pose.pixels,(pose.rootY-sy)/pose.pixels});
    if(kind==45)
    {
        const auto* state=State316(z,context->heavyZombies313B);float px=0,py=0;
        if(!state||!NativeSourcePointToBoard313(z->mBoard,state->bungee.targetRow,sy,state->bungee.targetX,state->bungee.markerY,px,py))return false;
        pose.options.nodeWorldPositions.push_back({"BungeeTarget",(px-pose.rootX)/pose.pixels,(pose.rootY-py)/pose.pixels});
    }
    out=std::move(pose);return true;
}


bool SharedPose316(const Zombie* z,const ZombieModel& model,Pose316& out,bool allowFactoryRest=false)
{
    const auto* data=GetNativeSourceZombieData(z);if(!data)return false;
    if(allowFactoryRest&&!z->mNetGhost&&!z->mApp->NetIsClient()&&IsNativeSourceZombieInitializing313(z))
    {
        const auto* graph=GetNativeSourceGraph313(z);
        if(!graph||!graph->initialized)
        {
            float sx=0,sy=0;if(!NativeSourceGroundAnchor313(z,sx,sy))return false;
            Pose316 rest;rest.options=model.options;rest.options.restPoseOnly=true;rest.options.rootFlipX=z->mMindControlled;
            rest.pixels=kPixelsPerSourceUnit*z->mScaleZombie;
            if(!std::isfinite(rest.pixels)||rest.pixels<=0||!std::isfinite(z->mAltitude))return false;
            rest.rootX=sx-(rest.options.rootFlipX?-model.shadowX:model.shadowX)*rest.pixels;
            rest.rootY=sy+model.shadowY*rest.pixels-z->mAltitude;
            out=std::move(rest);return true;
        }
    }
    if(CohortPrefab316(data->kind))return NewPose316(z,model,out);
    if(!FusionJackbox300Kind(data->kind)&&data->kind!=101&&data->kind!=102&&data->kind!=103&&data->kind!=200&&data->kind!=211&&data->kind!=217)return false;
    const char* name=nullptr;float phase=0,sx=0,sy=0;
    if(!SourcePose273(z,name,phase)||!name||!const_cast<Zombie*>(z)->GetShadowCenter(sx,sy))return false;
    name=PresentationState(model,name);if(!name)return false;
    const auto duration=model.durations.find(name);if(duration==model.durations.end())return false;
    Pose316 pose;pose.state=name;pose.seconds=phase*duration->second;pose.options=model.options;
    SpecialPose(z,model,pose.options);if(!DayPose273(z,model,pose.options))return false;
    pose.options.rootFlipX=const_cast<Zombie*>(z)->IsWalkingBackwards();
    if(!JackboxPose300(z,model,pose.options))return false;
    const int alpha=z->mZombieFade<0?255:int((std::clamp)(255LL*z->mZombieFade/10,0LL,255LL));
    if(z->mMindControlled)pose.options.colorMultiplierRGBA={128,64,192,alpha};
    else if(z->mChilledCounter>0||z->mIceTrapCounter>0)pose.options.colorMultiplierRGBA={75,75,255,alpha};
    else pose.options.colorMultiplierRGBA={255,255,255,alpha};
    pose.pixels=kPixelsPerSourceUnit*z->mScaleZombie;
    pose.rootX=sx-(pose.options.rootFlipX?-model.shadowX:model.shadowX)*pose.pixels;
    pose.rootY=sy+model.shadowY*pose.pixels;
    if(!std::isfinite(pose.rootX)||!std::isfinite(pose.rootY)||!std::isfinite(pose.pixels)||pose.pixels<=0)return false;
    out=std::move(pose);return true;
}
bool SharedKind316(uint16_t kind)
{return CohortPrefab316(kind)||FusionJackbox300Kind(kind)||kind==101||kind==102||kind==103||kind==200||kind==211||kind==217;}

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
    const bool cohort=data&&SharedKind316(data->kind);
    const bool canonical=data&&(cohort||FusionJacksonFamily367Kind(data->kind)||FusionJackbox300Kind(data->kind));
    // The native mower locator overlays the real body; it has no own artwork.
    if(canonical&&zombie->mZombiePhase==PHASE_ZOMBIE_MOWERED)return false;
    if (!graphics || !data || zombie->mApp != mImpl->app ||
        int(zombie->mZombieType)!=GetNativeSourceZombieCarrier(data->kind)) return canonical;
    const auto found = mImpl->models.find(data->kind);
    if (found == mImpl->models.end()) return canonical;
    if(cohort)
    {
        Pose316 pose;if(!SharedPose316(zombie,*found->second,pose)||!pose.visible)return true;
        found->second->animation.DrawPose(graphics,pose.rootX-zombie->mX,pose.rootY-zombie->mY,pose.pixels/100,
            pose.state,pose.seconds,nullptr,-1,1,&pose.options);return true;
    }
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
        zombie->mScaleZombie <= 0 || zombie->mScaleZombie > 8) return canonical;
    ZombieDrawPosition nativePosition;
    zombie->GetDrawPos(nativePosition);
    // Source swim clips hide the legs and animate the actual duck/whitewater.
    // Applying the native body clip a second time would crop this source pose.
    if (!swimming&&nativePosition.mClipHeight > CLIP_HEIGHT_LIMIT) return canonical;
    const char* state = nullptr;
    float phase = 0, groundX = 0, groundY = 0;
    if (!SourcePose273(zombie,state,phase) || !state)
        return DayPrefab273(data->kind)!=nullptr||BatchPrefab282(data->kind)!=nullptr||FusionJackbox300Kind(data->kind);
    if(!zombie->GetShadowCenter(groundX,groundY))return canonical;
    if(data->kind==8&&zombie->mShieldHealth<=0&&!zombie->IsDeadOrDying())
        state=zombie->mIsEating?"attack_":data->originSpeed>kFusionZombieWalk2Threshold?"walk1_":"walk_";
    const auto& model = *found->second;
    state=PresentationState(model,state);if(!state)return true;
    const auto duration = model.durations.find(state);
    if (duration == model.durations.end()) return canonical;
    FusionAnimationDrawOptions options = model.options;
    SpecialPose(zombie,model,options);
    if(!DayPose273(zombie,model,options))return true; // Unadmitted Day state never renders a native carrier alias.
    if (!DayPrefab273(data->kind)&&!FusionJacksonFamily367Kind(data->kind)&&!FusionJackbox300Kind(data->kind)&&!zombie->mHasHead)
        options.hiddenNodePaths.insert(options.hiddenNodePaths.end(),model.headPaths.begin(),model.headPaths.end());
    if (!DayPrefab273(data->kind)&&!FusionJacksonFamily367Kind(data->kind)&&!FusionJackbox300Kind(data->kind)&&!zombie->mHasArm)
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
    if(data->kind==9)
    {
        if((zombie->mHelmType!=HELMTYPE_NONE&&zombie->mHelmType!=HELMTYPE_FOOTBALL)||data->firstArmorStage>2)return true;
        if(zombie->mHelmType==HELMTYPE_NONE||zombie->mHelmHealth<=0)options.hiddenNodePaths.push_back(model.conePath);
        else if(data->firstArmorStage)ReplaceSprite(options,model.conePath,model.damagedCone[data->firstArmorStage-1]);
    }
    const int alpha = zombie->mZombieFade < 0 ? 255 :
        static_cast<int>((std::clamp)(255LL*zombie->mZombieFade/10,0LL,255LL));
    if (zombie->mMindControlled) options.colorMultiplierRGBA = {128,64,192,alpha};
    else if (zombie->mChilledCounter > 0 || zombie->mIceTrapCounter > 0)
        options.colorMultiplierRGBA = {75,75,255,alpha};
    else options.colorMultiplierRGBA = {255,255,255,alpha};
    options.rootFlipX = zombie->IsWalkingBackwards();
    if(!JacksonPose292(zombie,model,options)||!JackboxPose300(zombie,model,options))return true;
    const float pixels = kPixelsPerSourceUnit*zombie->mScaleZombie;
    const float shadowX = options.rootFlipX ? -model.shadowX : model.shadowX;
    // BeginDraw already supplied entity/camera translation; source root scale is
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
    const auto& model=*found->second;
    if(SharedKind316(data->kind))
    {
        Pose316 pose;if(!SharedPose316(zombie,model,pose,true))
        {
            const char* state=nullptr;float phase=0,sx=0,sy=0;
            const bool source=SourcePose273(zombie,state,phase),shadow=const_cast<Zombie*>(zombie)->GetShadowCenter(sx,sy);
            LawnNet::NetTrace("Fusion source node pose refused kind=%u id=%u path=%s source=%d state=%s phase=%.9g shadow=%d pool=%d height=%d",data->kind,zombie->mNetId,path,source?1:0,state?state:"null",phase,shadow?1:0,zombie->mInPool?1:0,int(zombie->mZombieHeight));
            return false;
        }float nx=0,ny=0;bool enabled=false;
        if(!model.animation.TryGetNodeOffset(path,nx,ny,pose.state,pose.seconds,nullptr,-1,1,&pose.options)||
           (active&&!model.animation.TryGetNodeActivity(path,enabled,pose.state,pose.seconds,nullptr,-1,1,&pose.options)))return false;
        const float px=pose.rootX+nx*pose.pixels,py=pose.rootY-ny*pose.pixels;
        if(!std::isfinite(px)||!std::isfinite(py))return false;x=px;y=py;if(active)*active=enabled;return true;
    }
    const char* state=nullptr;float phase=0,sx=0,sy=0;
    if(!(actualBodyPose?SourceBodyPose276(zombie,state,phase):SourcePose273(zombie,state,phase))||!state||
       !const_cast<Zombie*>(zombie)->GetShadowCenter(sx,sy))return false;
    state=PresentationState(model,state);if(!state)return false;
    const auto duration=model.durations.find(state);if(duration==model.durations.end())return false;
    auto options=model.options;SpecialPose(zombie,model,options);if(!DayPose273(zombie,model,options))return false;
    options.rootFlipX=const_cast<Zombie*>(zombie)->IsWalkingBackwards();
    if(!JacksonPose292(zombie,model,options)||!JackboxPose300(zombie,model,options))return false;
    float nx=0,ny=0;
    if(!model.animation.TryGetNodeOffset(path,nx,ny,state,phase*duration->second,nullptr,-1,1,&options))return false;
    if(active&&!model.animation.TryGetNodeActivity(path,*active,state,phase*duration->second,nullptr,-1,1,&options))return false;
    const float pixels=kPixelsPerSourceUnit*zombie->mScaleZombie;
    x=sx-(options.rootFlipX?-model.shadowX:model.shadowX)*pixels+nx*pixels;
    y=sy+model.shadowY*pixels-ny*pixels;
    return std::isfinite(x)&&std::isfinite(y);
}

bool NativeZombieVisuals::Box313(const Zombie* zombie,FusionAnimationBoxCollider303& result) const
{
    const auto* data=GetNativeSourceZombieData(zombie);if(!data||!SharedKind316(data->kind)||zombie->mApp!=mImpl->app)return false;
    const auto found=mImpl->models.find(data->kind);if(found==mImpl->models.end()||!found->second->rootBox313)return false;
    const auto& model=*found->second;Pose316 pose;if(!SharedPose316(zombie,model,pose,true))return false;
    FusionAnimationBoxCollider303 pending;
    if(!model.animation.TryGetBoxCollider303(model.rootBox313,pending,pose.state,pose.seconds,nullptr,-1,1,&pose.options))return false;
    pending.enabled=pending.enabled&&pose.collider;result=pending;return true;
}
bool NativeZombieVisuals::Matrix313(const Zombie* zombie,const char* path,std::array<float,6>& result) const
{
    const auto* data=GetNativeSourceZombieData(zombie);if(!data||!path||!SharedKind316(data->kind)||zombie->mApp!=mImpl->app)return false;
    const auto found=mImpl->models.find(data->kind);if(found==mImpl->models.end())return false;
    const auto& model=*found->second;Pose316 pose;if(!SharedPose316(zombie,model,pose,true))return false;
    std::array<float,6> value;
    if(!model.animation.TryGetNodeTransform(path,value,pose.state,pose.seconds,nullptr,-1,1,&pose.options))return false;
    const std::array<float,6> pending{pose.pixels*value[0],-pose.pixels*value[1],pose.pixels*value[2],-pose.pixels*value[3],
        pose.rootX+pose.pixels*value[4],pose.rootY-pose.pixels*value[5]};
    if(!std::all_of(pending.begin(),pending.end(),[](float f){return std::isfinite(f);}))return false;
    result=pending;return true;
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
        if(!std::isfinite(duration)||duration<0||!model.animation.TryGetNodeOffset("",x,y,state.c_str(),0,nullptr,-1,1,&model.options)||
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
    const auto* data=GetNativeSourceZombieData(zombie);
    if(data&&(SharedKind316(data->kind)||FusionJacksonFamily367Kind(data->kind))&&(!context||!context->zombieVisuals))return true;
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
bool GetNativeSourceZombieBox313(const Zombie* zombie,FusionAnimationBoxCollider303& result)
{
    const auto* context=GetNativeContext(zombie?zombie->mBoard:nullptr);
    return context&&context->zombieVisuals&&context->zombieVisuals->Box313(zombie,result);
}
bool GetNativeSourceZombieMatrix313(const Zombie* zombie,const char* path,std::array<float,6>& result)
{
    const auto* context=GetNativeContext(zombie?zombie->mBoard:nullptr);
    return context&&context->zombieVisuals&&context->zombieVisuals->Matrix313(zombie,path,result);
}

}
