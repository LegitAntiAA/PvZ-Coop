#include "FusionNativeVisual.h"
#include "FusionNative.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionNativeCherryShooter.h"
#include "FusionNativeBasePlants.h"
#include "FusionNativeLobber.h"
#include "FusionNativeDefender.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeRecipientShooters.h"
#include "FusionNativeShooterBatch.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeAction266.h"
#include "FusionNativeEconomy.h"
#include "FusionNativeShooter267.h"
#include "FusionNativeAction267.h"
#include "FusionNativeUmbrellaBatch.h"
#include "FusionNativeSeaLantern267.h"
#include "FusionChomperRules.h"
#include "FusionNativeRollingProjectile.h"
#include "FusionRollingProjectileRules.h"
#include "FusionShooterTiming.h"
#include "FusionProducerRules.h"
#include "FusionNativeEquipment268.h"
#include "FusionNativeTools268.h"
#include "FusionNativeCannon268.h"
#include "FusionNativeWater268.h"
#include "FusionNativeChomper268.h"
#include "FusionNativeExtras268.h"
#include "FusionNativeStar269.h"
#include "FusionNativeDoom269.h"
#include "FusionNativeHypno269.h"
#include "FusionNativeSolar269.h"
#include "FusionNativeCompanions269.h"
#include "FusionNativeEmbeddedEffects269.h"
#include "FusionNativeWorldRemainder269.h"
#include "FusionNativeLastPlants269.h"
#include "FusionNativeSummonPlants269.h"
#include "FusionNativeMorePlants269.h"
#include "FusionNativeMorePlants269Backend.h"
#include "FusionNativeWorldActors269.h"
#include "FusionNativeLaserConnections269.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionChomper268Rules.h"
#include "FusionExtras268Rules.h"
#include "FusionAnimation.h"
#include "FusionRecipe.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Coin.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../Net/NetManager.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../GameConstants.h"
#include "../../SexyAppFramework/Graphics.h"
#include "../../SexyAppFramework/Point.h"
#include "../../SexyAppFramework/MemoryImage.h"
#include "../../SexyAppFramework/SharedImage.h"
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <limits>
#include <set>
#include <span>
#include <stdexcept>
#include <utility>
#include <Windows.h>

namespace LawnFusion
{
namespace
{
constexpr uint16_t kPeaSunFlower = 1000;
constexpr uint16_t kCherryShooter = 1001;
constexpr uint16_t kNutShooter = 1004;
constexpr uint16_t kCherryNut = 1003, kPotatoNut = 1010, kCherryMine = 1162;
constexpr uint16_t kSunNut = 1006, kTwinFlower = 1033;
constexpr uint16_t kDoubleCherry = 1008;
constexpr uint16_t kDoubleShooter = 1030;
constexpr uint16_t kGatlingPea = 1032;
constexpr uint16_t kCherryGatling = 1017;
constexpr uint16_t kPeaPuff = 1018, kDoublePuff = 1019, kGatlingPuff = 1065;
constexpr uint16_t kSplitPea = 1090, kCherrySplit = 1108, kSplitPuff = 1153, kPuffNut = 1021;
constexpr uint16_t kSnowPea = 1034, kDoubleSnow = 1105, kSnowGatling = 1106, kIcePuff = 1035, kSnowGatlingPuff = 908;
constexpr float kNativePixelsPerWorldUnit = 60.0f;
constexpr int kPortraitSize = 160;
constexpr float kFieldLeft = 40.0f, kFieldRight = 760.0f, kFieldBottom = 580.0f;
constexpr float kFieldPixelsX = 72.0f/1.35f, kFieldPixelsY = 100.0f/1.67f;
using Json = nlohmann::json;

float FieldX(float sourceX) { return 76.0f+(sourceX+4.8f)*kFieldPixelsX; }
float FieldY(float sourceY) { return 130.0f-(sourceY-2.3f)*kFieldPixelsY; }
float FieldY277(const Board* board,float sourceY)
{return 130.0f-(sourceY-2.3f)*float(GetNativeFusionPixelsY(board));}
struct FieldSpec277 {BackgroundType background;uint16_t scene;const char* package;};
const FieldSpec277* SourceFieldSpec277(const Board* board)
{
    static constexpr FieldSpec277 specs[]={{BACKGROUND_2_NIGHT,1,"background2"},{BACKGROUND_3_POOL,2,"background3"}};
    if(board)for(const auto& spec:specs)if(board->mBackground==spec.background)return &spec;
    return nullptr;
}

std::filesystem::path PackageRoot()
{
    std::wstring executable(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!length || length >= executable.size()) throw std::runtime_error("cannot locate native Fusion visual assets");
    executable.resize(length);
    return std::filesystem::path(executable).parent_path()/L"fusion"/L"pilot";
}

Json ReadPackage(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open native Fusion visual package: "+path.string());
    return Json::parse(input);
}

void LoadAnimation(FusionAnimation& animation, LawnApp* app, const std::filesystem::path& path,std::span<const int64_t> particleProof={})
{
    std::string error;
    if (!animation.Load(app, path.string(), error,particleProof)) throw std::runtime_error(path.string()+": "+error);
}
uint16_t EmbeddedProjectileModelKey(uint16_t kind)
{return kind==22||kind==30?uint16_t(3000+kind):0;}
int WorldAuxVisualKey(NativeWorldRemainder269Family family,uint8_t type)
{if(family==NativeWorldRemainder269Family::LourFly)return type==0?0:-1;
 if(family!=NativeWorldRemainder269Family::Meteorite)return -1;
 switch(type){case 0:return 10;case 1:return 11;case 2:return 12;case 5:return 47;default:return -1;}}
const char* WorldAuxPackage(int key)
{switch(key){case 0:return "world269-lourfly";case 10:return "world269-item-10";case 11:return "world269-item-11";
 case 12:return "world269-item-12";case 47:return "world269-item-47";default:return nullptr;}}
const char* WorldAuxSource(int key)
{switch(key){case 0:return "Resources/plants/cattailplant/cattaillour/CattailLour_fly.prefab";
 case 10:return "Resources/items/BigStar.prefab";case 11:return "Resources/items/BigSuperStar.prefab";
 case 12:return "Resources/items/BigUltimateStar.prefab";case 47:return "Resources/items/SolarStar.prefab";default:return nullptr;}}
int CompanionVisualKey(FusionCompanion269Family family,bool smallVariant)
{
    if(family==FusionCompanion269Family::CherryLittleFly)return smallVariant?3:2;
    if(smallVariant)return -1;
    if(family==FusionCompanion269Family::LittleFly)return 0;
    return family==FusionCompanion269Family::UltimateLittleFly?1:-1;
}
const char* CompanionPackage(int key)
{
    constexpr const char* names[]={"companion-littlefly","companion-ultimate-littlefly","companion-cherry-littlefly","companion-cherry-torch-fly"};
    return key>=0&&key<4?names[key]:nullptr;
}
FusionAnimationDrawOptions CompanionOptions(const Projectile* p)
{
    FusionAnimationDrawOptions options;const float px=kFieldPixelsX,py=float(GetNativeFusionPixelsY(p->mBoard));
    const float c=std::cos(p->mRotation),s=std::sin(p->mRotation);
    options.useBoardBasis=true;options.boardBasis={px*c,-py*s,-px*s,-py*c};
    if(const auto* state=GetNativeCompanion269State(p))options.extraStates.push_back({"blowloop",state->ageTicks*.01f,1});
    return options;
}
struct PlantPackage { uint16_t kind; const char* name; const char* portrait; };
const PlantPackage plantPackages[] = {
PlantPackage{34,"batch269-34","portrait269-34"},
PlantPackage{241,"batch269-241","portrait269-241"},
PlantPackage{243,"batch269-243","portrait269-243"},
PlantPackage{244,"batch269-244","portrait269-244"},
PlantPackage{245,"batch269-245","portrait269-245"},
PlantPackage{248,"batch269-248","portrait269-248"},
PlantPackage{249,"batch269-249","portrait269-249"},
PlantPackage{250,"batch269-250","portrait269-250"},
PlantPackage{251,"batch269-251","portrait269-251"},
PlantPackage{253,"batch269-253","portrait269-253"},
PlantPackage{254,"batch269-254","portrait269-254"},
PlantPackage{255,"batch269-255","portrait269-255"},
PlantPackage{256,"batch269-256","portrait269-256"},
PlantPackage{900,"batch269-900","portrait269-900"},
PlantPackage{905,"batch269-905","portrait269-905"},
PlantPackage{909,"batch269-909","portrait269-909"},
PlantPackage{911,"batch269-911","portrait269-911"},
PlantPackage{912,"batch269-912","portrait269-912"},
PlantPackage{917,"batch269-917","portrait269-917"},
PlantPackage{918,"batch269-918","portrait269-918"},
PlantPackage{922,"batch269-922","portrait269-922"},
PlantPackage{926,"batch269-926","portrait269-926"},
PlantPackage{928,"batch269-928","portrait269-928"},
PlantPackage{937,"batch269-937","portrait269-937"},
PlantPackage{938,"batch269-938","portrait269-938"},
PlantPackage{1041,"batch269-1041","portrait269-1041"},
PlantPackage{1110,"batch269-1110","portrait269-1110"},
PlantPackage{1147,"batch269-1147","portrait269-1147"},
PlantPackage{1150,"batch269-1150","portrait269-1150"},
PlantPackage{1151,"batch269-1151","portrait269-1151"},
PlantPackage{1157,"batch269-1157","portrait269-1157"},
PlantPackage{1166,"batch269-1166","portrait269-1166"},
PlantPackage{1169,"batch269-1169","portrait269-1169"},
PlantPackage{1178,"batch269-1178","portrait269-1178"},
PlantPackage{1104,"batch-1104","portrait-batch-1104"},
PlantPackage{916,"batch-916","portrait-batch-916"},
PlantPackage{923,"batch-923","portrait-batch-923"},
PlantPackage{1148,"batch-1148","portrait-batch-1148"},

PlantPackage{1026,"batch-1026","portrait-batch-1026"},
PlantPackage{924,"batch-924","portrait-batch-924"},
PlantPackage{934,"batch-934","portrait-batch-934"},

PlantPackage{1056,"batch-1056","portrait-batch-1056"},
PlantPackage{242,"batch-242","portrait-batch-242"},
PlantPackage{1174,"batch-1174","portrait-batch-1174"},
PlantPackage{1175,"batch269-1175","portrait269-1175"},
PlantPackage{1082,"batch-1082","portrait-batch-1082"},
PlantPackage{1084,"batch-1084","portrait-batch-1084"},
PlantPackage{1093,"batch-1093","portrait-batch-1093"},
PlantPackage{1094,"batch-1094","portrait-batch-1094"},
PlantPackage{1095,"batch-1095","portrait-batch-1095"},
PlantPackage{920,"batch-920","portrait-batch-920"},
PlantPackage{910,"batch-910","portrait-batch-910"},
PlantPackage{930,"batch-930","portrait-batch-930"},

PlantPackage{1085,"batch-1085","portrait-batch-1085"},
PlantPackage{25,"batch-25","portrait-batch-25"},
PlantPackage{1100,"batch-1100","portrait-batch-1100"},
PlantPackage{1101,"batch-1101","portrait-batch-1101"},
PlantPackage{1102,"batch-1102","portrait-batch-1102"},
PlantPackage{1092,"batch-1092","portrait-batch-1092"},
PlantPackage{1097,"batch-1097","portrait-batch-1097"},
PlantPackage{1098,"batch-1098","portrait-batch-1098"},
PlantPackage{1099,"batch-1099","portrait-batch-1099"},
PlantPackage{935,"batch-935","portrait-batch-935"},
PlantPackage{1154,"batch-1154","portrait-batch-1154"},
PlantPackage{15,"batch-15","portrait-batch-15"},
PlantPackage{1049,"batch-1049","portrait-batch-1049"},
PlantPackage{1050,"batch-1050","portrait-batch-1050"},
PlantPackage{1051,"batch-1051","portrait-batch-1051"},
PlantPackage{1066,"batch-1066","portrait-batch-1066"},
PlantPackage{1156,"batch-1156","portrait-batch-1156"},
PlantPackage{252,"batch-252","portrait-batch-252"},
PlantPackage{1067,"batch-1067","portrait-batch-1067"},
PlantPackage{1068,"batch-1068","portrait-batch-1068"},
PlantPackage{1069,"batch-1069","portrait-batch-1069"},
PlantPackage{1076,"batch-1076","portrait-batch-1076"},
PlantPackage{1083,"batch-1083","portrait-batch-1083"},
PlantPackage{1120,"batch-1120","portrait-batch-1120"},
PlantPackage{1122,"batch-1122","portrait-batch-1122"},
PlantPackage{1123,"batch-1123","portrait-batch-1123"},
PlantPackage{1132,"batch-1132","portrait-batch-1132"},
PlantPackage{1139,"batch-1139","portrait-batch-1139"},
PlantPackage{1142,"batch-1142","portrait-batch-1142"},
PlantPackage{914,"batch-914","portrait-batch-914"},
PlantPackage{915,"batch-915","portrait-batch-915"},
PlantPackage{1013,"batch-1013","portrait-batch-1013"},
PlantPackage{903,"batch-903","portrait-batch-903"},
PlantPackage{921,"batch-921","portrait-batch-921"},
PlantPackage{33,"batch-33","portrait-batch-33"},
PlantPackage{1177,"batch-1177","portrait-batch-1177"},
PlantPackage{1129,"batch-1129","portrait-batch-1129"},

PlantPackage{20, "batch-20", "portrait-batch-20"},
PlantPackage{22, "batch-22", "portrait-batch-22"},
PlantPackage{929, "batch-929", "portrait-batch-929"},
PlantPackage{1020, "batch-1020", "portrait-batch-1020"},
PlantPackage{1023, "batch-1023", "portrait-batch-1023"},
PlantPackage{1036, "batch-1036", "portrait-batch-1036"},
PlantPackage{1037, "batch-1037", "portrait-batch-1037"},
PlantPackage{1038, "batch-1038", "portrait-batch-1038"},
PlantPackage{1040, "batch-1040", "portrait-batch-1040"},
PlantPackage{1042, "batch-1042", "portrait-batch-1042"},
PlantPackage{1043, "batch-1043", "portrait-batch-1043"},
PlantPackage{1044, "batch-1044", "portrait-batch-1044"},
PlantPackage{1045, "batch-1045", "portrait-batch-1045"},
PlantPackage{1046, "batch-1046", "portrait-batch-1046"},
PlantPackage{1052, "batch-1052", "portrait-batch-1052"},
PlantPackage{1053, "batch-1053", "portrait-batch-1053"},
PlantPackage{1054, "batch-1054", "portrait-batch-1054"},
PlantPackage{1055, "batch-1055", "portrait-batch-1055"},
PlantPackage{1059, "batch-1059", "portrait-batch-1059"},
PlantPackage{1061, "batch-1061", "portrait-batch-1061"},
PlantPackage{1062, "batch-1062", "portrait-batch-1062"},
PlantPackage{1063, "batch-1063", "portrait-batch-1063"},
PlantPackage{1064, "batch-1064", "portrait-batch-1064"},
PlantPackage{1080, "batch-1080", "portrait-batch-1080"},
PlantPackage{1091, "batch-1091", "portrait-batch-1091"},
PlantPackage{1096, "batch-1096", "portrait-batch-1096"},
PlantPackage{1107, "batch-1107", "portrait-batch-1107"},
PlantPackage{1109, "batch-1109", "portrait-batch-1109"},
PlantPackage{1152, "batch-1152", "portrait-batch-1152"},
PlantPackage{1161, "batch-1161", "portrait-batch-1161"},
PlantPackage{1165, "batch-1165", "portrait-batch-1165"},
PlantPackage{1167, "batch-1167", "portrait-batch-1167"},
PlantPackage{1168, "batch-1168", "portrait-batch-1168"},
PlantPackage{1171, "batch-1171", "portrait-batch-1171"},
PlantPackage{1173, "batch-1173", "portrait-batch-1173"},
PlantPackage{1179, "batch-1179", "portrait-batch-1179"},
PlantPackage{1182, "batch-1182", "portrait-batch-1182"},
PlantPackage{1183, "batch-1183", "portrait-batch-1183"},
PlantPackage{1191, "batch-1191", "portrait-batch-1191"},
PlantPackage{1192, "batch-1192", "portrait-batch-1192"},
PlantPackage{kPeaSunFlower, "peasunflower", "portrait-peasunflower"},
            PlantPackage{kCherryShooter, "cherryshooter", "portrait-cherryshooter"},
            PlantPackage{kNutShooter, "nutshooter", "portrait-nutshooter"},
            PlantPackage{kSunNut, "sunnut", "portrait-sunnut"},
            PlantPackage{kTwinFlower, "twinflower", "portrait-twinflower"},
            PlantPackage{1187, "silversunflower", "portrait-silversunflower"},
            PlantPackage{1180, "icecaltrop", "portrait-icecaltrop"},
            PlantPackage{kDoubleCherry, "doublecherry", "portrait-doublecherry"},
            PlantPackage{kDoubleShooter, "doubleshooter", "portrait-doubleshooter"},
            PlantPackage{kGatlingPea, "gatlingpea", "portrait-gatlingpea"},
            PlantPackage{kCherryGatling, "cherrygatling", "portrait-cherrygatling"},
            PlantPackage{kPeaPuff, "peapuff", "portrait-peapuff"},
            PlantPackage{kDoublePuff, "doublepuff", "portrait-doublepuff"},
            PlantPackage{kGatlingPuff, "gatlingpuff", "portrait-gatlingpuff"},
            PlantPackage{kSplitPea, "splitpea", "portrait-splitpea"},
            PlantPackage{kCherrySplit, "cherrysplit", "portrait-cherrysplit"},
            PlantPackage{kSplitPuff, "splitpuff", "portrait-splitpuff"},
            PlantPackage{kPuffNut, "puffnut", "portrait-puffnut"},
            PlantPackage{kSnowPea, "snowpeashooter", "portrait-snowpeashooter"},
            PlantPackage{kDoubleSnow, "doublesnow", "portrait-doublesnow"},
            PlantPackage{kSnowGatling, "snowgatling", "portrait-snowgatling"},
            PlantPackage{kIcePuff, "icepuff", "portrait-icepuff"},
            PlantPackage{kSnowGatlingPuff, "snowgatlingpuff", "portrait-snowgatlingpuff"},
            PlantPackage{kCherryNut, "cherrynut", "portrait-cherrynut"},
            PlantPackage{kPotatoNut, "potatonut", "portrait-potatonut"},
            PlantPackage{kCherryMine, "batch-1162", "portrait-batch-1162"},
            PlantPackage{1012, "nutchomper", "portrait-nutchomper"},
            PlantPackage{1014, "sunchomper", "portrait-sunchomper"},
            PlantPackage{1016, "cherrychomper", "portrait-cherrychomper"},
            PlantPackage{26, "cabbage", "portrait-cabbage"},
            PlantPackage{32, "melonpult", "portrait-melonpult"},
            PlantPackage{1116, "wintermelon", "portrait-wintermelon"},
            PlantPackage{28, "cornpult", "portrait-cornpult"},
            PlantPackage{1111, "suncabbage", "portrait-suncabbage"},
            PlantPackage{1113, "corncabbage", "portrait-corncabbage"},
            PlantPackage{1121, "cornmelon", "portrait-cornmelon"},
            PlantPackage{1124, "cabbagemelon", "portrait-cabbagemelon"},
            PlantPackage{31,"batch-31","portrait-batch-31"},
            PlantPackage{1140,"batch-1140","portrait-batch-1140"},
            PlantPackage{1134,"batch-1134","portrait-batch-1134"},
            PlantPackage{1138,"batch-1138","portrait-batch-1138"},
            PlantPackage{1141,"batch-1141","portrait-batch-1141"},
            PlantPackage{1029,"batch-1029","portrait-batch-1029"},
            PlantPackage{1048,"batch-1048","portrait-batch-1048"},
            PlantPackage{1189,"batch-1189","portrait-batch-1189"},
            PlantPackage{1027,"batch-1027","portrait-batch-1027"},
            PlantPackage{1028,"batch-1028","portrait-batch-1028"},
            PlantPackage{1039,"batch-1039","portrait-batch-1039"},
            PlantPackage{1073,"batch-1073","portrait-batch-1073"},
            PlantPackage{27,"batch-27","portrait-batch-27"},
            PlantPackage{1112,"batch-1112","portrait-batch-1112"},
            PlantPackage{1114,"batch-1114","portrait-batch-1114"},
            PlantPackage{1125,"batch-1125","portrait-batch-1125"},
            PlantPackage{1130,"batch-1130","portrait-batch-1130"},
            PlantPackage{1133,"batch-1133","portrait-batch-1133"},
            PlantPackage{24,"batch-24","portrait-batch-24"},
            PlantPackage{1088,"batch-1088","portrait-batch-1088"},
            PlantPackage{1089,"batch-1089","portrait-batch-1089"},
            PlantPackage{1164,"batch-1164","portrait-batch-1164"},
            PlantPackage{8,"batch-8","portrait-batch-8"},
            PlantPackage{1022,"batch-1022","portrait-batch-1022"},
            PlantPackage{11,"batch-11","portrait-batch-11"},
            PlantPackage{16,"batch-16","portrait-batch-16"},
            PlantPackage{13,"batch-13","portrait-batch-13"},
            PlantPackage{1057,"batch-1057","portrait-batch-1057"},
            PlantPackage{17,"batch-17","portrait-batch-17"},
            PlantPackage{1155,"batch-1155","portrait-batch-1155"},
            PlantPackage{14,"batch-14","portrait-batch-14"},
            PlantPackage{1058,"batch-1058","portrait-batch-1058"},
            PlantPackage{9,"batch-9","portrait-batch-9"},
            PlantPackage{1024,"batch-1024","portrait-batch-1024"},
            PlantPackage{1025,"batch-1025","portrait-batch-1025"},
            PlantPackage{21,"cactus","portrait-cactus"},
            PlantPackage{23,"starfruit","portrait-starfruit"},
PlantPackage{1117,"batch-1117","portrait-batch-1117"},
PlantPackage{1118,"batch-1118","portrait-batch-1118"},
PlantPackage{1119,"batch-1119","portrait-batch-1119"},
PlantPackage{1149,"batch-1149","portrait-batch-1149"},
PlantPackage{1126,"batch-1126","portrait-batch-1126"},
PlantPackage{1079,"batch-1079","portrait-batch-1079"},
PlantPackage{1081,"batch-1081","portrait-batch-1081"},
PlantPackage{1159,"batch-1159","portrait-batch-1159"},
PlantPackage{1160,"batch-1160","portrait-batch-1160"},
PlantPackage{1011,"batch-1011","portrait-batch-1011"},
PlantPackage{1135,"batch-1135","portrait-batch-1135"},
PlantPackage{1005,"batch-1005","portrait-batch-1005"},
PlantPackage{901,"batch-901","portrait-batch-901"},
PlantPackage{907,"batch-907","portrait-batch-907"},
PlantPackage{933,"batch-933","portrait-batch-933"},
PlantPackage{1002,"batch-1002","portrait-batch-1002"},
PlantPackage{1009,"batch-1009","portrait-batch-1009"},
PlantPackage{1015,"batch-1015","portrait-batch-1015"},
PlantPackage{1060,"batch-1060","portrait-batch-1060"},
PlantPackage{1070,"batch-1070","portrait-batch-1070"},
PlantPackage{1071,"batch-1071","portrait-batch-1071"},
PlantPackage{1072,"batch-1072","portrait-batch-1072"},
PlantPackage{1074,"batch-1074","portrait-batch-1074"},
PlantPackage{1075,"batch-1075","portrait-batch-1075"},
PlantPackage{1087,"batch-1087","portrait-batch-1087"},
PlantPackage{1145,"batch-1145","portrait-batch-1145"},
PlantPackage{1146,"batch-1146","portrait-batch-1146"},
PlantPackage{1163,"batch-1163","portrait-batch-1163"},
PlantPackage{1170,"batch-1170","portrait-batch-1170"},
PlantPackage{1190,"batch-1190","portrait-batch-1190"},
PlantPackage{1031,"batch-1031","portrait-batch-1031"},
PlantPackage{1184,"batch-1184","portrait-batch-1184"},
PlantPackage{1136,"batch-1136","portrait-batch-1136"},
PlantPackage{1137,"batch-1137","portrait-batch-1137"},
PlantPackage{1188,"batch-1188","portrait-batch-1188"},
PlantPackage{1172,"batch-1172","portrait-batch-1172"},
PlantPackage{1158,"batch-1158","portrait-batch-1158"},
PlantPackage{1007,"batch-1007","portrait-batch-1007"},
PlantPackage{919,"batch-919","portrait-batch-919"},
PlantPackage{902,"batch-902","portrait-batch-902"},
PlantPackage{1103,"batch-1103","portrait-batch-1103"},
PlantPackage{927,"batch-927","portrait-batch-927"},
PlantPackage{904,"batch-904","portrait-batch-904"},
PlantPackage{1047,"batch-1047","portrait-batch-1047"},
PlantPackage{932,"batch-932","portrait-batch-932"},
PlantPackage{931,"batch-931","portrait-batch-931"},
PlantPackage{906,"batch-906","portrait-batch-906"},
PlantPackage{913,"batch-913","portrait-batch-913"},
PlantPackage{925,"batch-925","portrait-batch-925"},
PlantPackage{1181,"batch-1181","portrait-batch-1181"},
PlantPackage{1176,"batch-1176","portrait-batch-1176"},
PlantPackage{936,"batch-936","portrait-batch-936"},
PlantPackage{1077,"batch-1077","portrait-batch-1077"},
PlantPackage{30,"batch-30","portrait-batch-30"},
PlantPackage{1115,"batch-1115","portrait-batch-1115"},
PlantPackage{1127,"batch-1127","portrait-batch-1127"},
PlantPackage{1128,"batch-1128","portrait-batch-1128"},
PlantPackage{1131,"batch-1131","portrait-batch-1131"},
PlantPackage{1143,"batch-1143","portrait-batch-1143"},
PlantPackage{1144,"batch-1144","portrait-batch-1144"},
PlantPackage{12,"batch-12","portrait-batch-12"},
PlantPackage{1185,"batch-1185","portrait-batch-1185"},
PlantPackage{1186,"batch-1186","portrait-batch-1186"},
PlantPackage{19,"batch-19","portrait-batch-19"},
PlantPackage{1086,"batch-1086","portrait-batch-1086"},
PlantPackage{1078,"batch-1078","portrait-batch-1078"},
            PlantPackage{29,"garlic","portrait-garlic"}
};
struct ProjectilePackage { uint16_t kind; const char* name; const char* state; float radiansPerSecond; };
const ProjectilePackage projectilePackages[] = {
ProjectilePackage{8,"projectile-leaf269-8","Solar",0.0f},
ProjectilePackage{51,"projectile-leaf269-51","idle",0.0f},
ProjectilePackage{14,"projectile-batch-14","idle",0.0f},

ProjectilePackage{43,"projectile-batch-43","idle",0.0f},
ProjectilePackage{44,"projectile-batch-44","idle",0.0f},
ProjectilePackage{45,"projectile-batch-45","idle",0.0f},
ProjectilePackage{48,"projectile-batch-48","idle",0.0f},

ProjectilePackage{42,"projectile-batch-42","idle",0.0f},
ProjectilePackage{6,"projectile-batch-6","r",0.0f},
ProjectilePackage{20,"projectile-batch-20","idle",0.0f},
ProjectilePackage{29,"projectile-batch-29","idle",0.0f},
ProjectilePackage{32,"projectile-batch-32","idle",0.0f},
ProjectilePackage{34,"projectile-batch-34","idle",0.0f},
ProjectilePackage{35,"projectile-batch-35","idle",0.0f},
ProjectilePackage{41,"projectile-batch-41","idle",0.0f},
ProjectilePackage{65,"projectile-batch-65","idle",0.0f},
ProjectilePackage{68,"projectile-batch-68","idle",0.0f},
ProjectilePackage{69,"projectile-batch-69","idle",0.0f},
ProjectilePackage{72,"projectile-batch-72","idle",0.0f},
ProjectilePackage{73,"projectile-batch-73","idle",0.0f},
ProjectilePackage{77,"projectile-batch-77","idle",0.0f},
ProjectilePackage{79,"projectile-batch-79","idle",0.0f},
ProjectilePackage{80,"projectile-batch-80","idle",0.0f},
ProjectilePackage{82,"projectile-batch-82","idle",0.0f},
ProjectilePackage{83,"projectile-batch-83","idle",0.0f},
ProjectilePackage{84,"projectile-batch-84","idle",0.0f},
ProjectilePackage{85,"projectile-batch-85","idle",0.0f},
ProjectilePackage{89,"projectile-batch-89","idle",0.0f},
ProjectilePackage{94,"projectile-batch-94","idle",0.0f},
ProjectilePackage{91,"projectile-world269-91","idle",0.0f},
ProjectilePackage{49,"projectile-batch-49","idle",0.0f},
ProjectilePackage{30,"projectile-batch-30","FirePeaIdle",0.0f},

ProjectilePackage{3,"projectile-batch-3","idle",0.0f},
ProjectilePackage{4,"projectile-batch-4","r",0.0f},
ProjectilePackage{5,"projectile-batch-5","r",0.0f},
ProjectilePackage{23,"projectile-batch-23","idle",0.0f},
ProjectilePackage{38,"projectile-batch-38","idle",0.0f},
ProjectilePackage{40,"projectile-batch-40","idle",0.0f},
ProjectilePackage{61,"projectile-batch-61","idle",0.0f},
ProjectilePackage{62,"projectile-batch-62","idle",0.0f},
ProjectilePackage{63,"projectile-batch-63","idle",0.0f},
ProjectilePackage{64,"projectile-batch-64","idle",0.0f},
ProjectilePackage{71,"projectile-batch-71","idle",0.0f},
ProjectilePackage{75,"projectile-batch-75","idle",0.0f},
ProjectilePackage{86,"projectile-batch-86","idle",0.0f},
ProjectilePackage{100,"projectile-batch-100","idle",0.0f},
ProjectilePackage{7,"projectile-batch-7","idle",0.0f},
ProjectilePackage{36,"projectile-batch-36","idle",0.0f},
ProjectilePackage{47,"projectile-batch-47","idle",0.0f},
ProjectilePackage{58,"projectile-batch-58","idle",0.0f},
ProjectilePackage{59,"projectile-batch-59","idle",0.0f},
ProjectilePackage{87,"projectile-batch-87","FirePeaIdle",0.0f},
ProjectilePackage{88,"projectile-batch-88","FirePeaIdle",0.0f}, // Attached87 FX only; no native projectile authority.
ProjectilePackage{93,"projectile-batch-93","idle",0.0f},
ProjectilePackage{98,"projectile-batch-98","idle",0.0f},
ProjectilePackage{99,"projectile-batch-99","idle",0.0f},
ProjectilePackage{25,"hot-1","FirePeaIdle",0.0f},
ProjectilePackage{26,"hot-2","FirePeaIdle",0.0f},
ProjectilePackage{27,"hot-3","FirePeaIdle",0.0f},
ProjectilePackage{11, "projectile-batch-11", "idle", 0.0f},
ProjectilePackage{90, "projectile-batch-90", "idle", 0.0f},
ProjectilePackage{46, "projectile-batch-46", "idle", 0.0f},
ProjectilePackage{92, "projectile-batch-92", "idle", 0.0f},
ProjectilePackage{95, "projectile-batch-95", "idle", 0.0f},
ProjectilePackage{102, "projectile-batch-102", "idle", 0.0f},
ProjectilePackage{21, "projectile-batch-21", "idle", 0.0f},
ProjectilePackage{22, "projectile-batch-22", "idle", 0.0f},
ProjectilePackage{24, "projectile-batch-24", "idle", 0.0f},
ProjectilePackage{97, "projectile-batch-97", "idle", 0.0f},
ProjectilePackage{12, "projectile-batch-12", "rotate", 0.0f},
ProjectilePackage{28, "projectile-batch-28", "idle", 0.0f},
ProjectilePackage{1, "projectile-cherry", "idle", 0.0f},
            ProjectilePackage{2, "projectile-nut", "r", kFusionNutRollingMotion.rotationPerTick*100.0f},
            ProjectilePackage{10, "projectile-puffpea", "idle", 0.0f},
            ProjectilePackage{15, "projectile-snowpea", "idle", 0.0f},
            ProjectilePackage{18, "projectile-icicle-smallVariant", "idle", 0.0f},
            ProjectilePackage{53, "projectile-suncabbage", "idle", 0.0f},
            ProjectilePackage{54, "projectile-kernel", "idle", 0.0f},
            ProjectilePackage{55, "projectile-butter", "idle", 0.0f},
            ProjectilePackage{56, "projectile-bigkernel", "idle", 0.0f},
            ProjectilePackage{57, "projectile-bigbutter", "idle", 0.0f},
            ProjectilePackage{66, "projectile-cornmelon", "idle", 0.0f},
            ProjectilePackage{67, "projectile-buttermelon", "idle", 0.0f},
            ProjectilePackage{70, "projectile-cabbagemelon", "idle", 0.0f},
            ProjectilePackage{74,"projectile-money-74","idle",0.0f},
            ProjectilePackage{76,"projectile-money-76","idle",0.0f},
            ProjectilePackage{78,"projectile-money-78","idle",0.0f},
            ProjectilePackage{81,"projectile-money-81","idle",0.0f},
            ProjectilePackage{13,"projectile-colorfulpuff","idle",0.0f},
            ProjectilePackage{37,"projectile-cactus","idle",0.0f},
            ProjectilePackage{39,"projectile-star","idle",0.0f}
};
const PlantPackage* PlantPackageFor(uint16_t kind)
{ for (const auto& package : plantPackages) if (package.kind == kind) return &package; return nullptr; }
const ProjectilePackage* ProjectilePackageFor(uint16_t kind)
{ for (const auto& package : projectilePackages) if (package.kind == kind) return &package; return nullptr; }
bool NativePlantArtwork(uint16_t kind)
{
    switch (kind) { case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 10: case 18: return true; default: return false; }
}
bool NativeProjectileArtwork(uint16_t kind)
{
    switch (kind) { case 0: case 8: case 9: case 52: case 59: case 60: return true; default: return false; }
}
struct VisualNeeds { std::set<uint16_t> plants, projectiles; };
void AddProjectileNeed(VisualNeeds& needs, uint16_t kind)
{
    if (!ProjectilePackageFor(kind) && !NativeProjectileArtwork(kind))
        throw std::runtime_error("unregistered native Fusion projectile visual kind="+std::to_string(kind));
    needs.projectiles.insert(kind);
    if (kind == 87) needs.projectiles.insert(88);
    if (kind == 72) for(uint16_t child:{61,62,63,64}) needs.projectiles.insert(child);
    if (kind == 0) for (uint16_t hot : {25,26,27}) needs.projectiles.insert(hot);
}
VisualNeeds CollectVisualNeeds(std::span<const uint16_t> plantKinds, std::span<const uint16_t> projectileKinds)
{
    VisualNeeds needs;
    std::vector<uint16_t> pending(plantKinds.begin(), plantKinds.end());
    for (size_t index = 0; index < pending.size(); ++index)
    {
        const auto kind = pending[index];
        if (!needs.plants.insert(kind).second) continue;
        if ((GetNativeSeedForPlant(kind) == SEED_NONE&&kind!=251) || (!PlantPackageFor(kind) && !NativePlantArtwork(kind)))
            throw std::runtime_error("unregistered native Fusion plant visual kind="+std::to_string(kind));
        // Only actual natural callbacks, never all outgoing fusion recipes.
        if (kind == 1024) pending.push_back(9);
        if (kind == 1162) pending.push_back(4);
        if (kind == 1000) AddProjectileNeed(needs,8);
        if (kind == 929 || kind == 1192 || kind == 1171) AddProjectileNeed(needs,92);
        if (kind == 1183) { AddProjectileNeed(needs,15); AddProjectileNeed(needs,97); }
        if (kind == 1064 || kind == 1173 || kind == 1179) AddProjectileNeed(needs,28);
        if (kind == 20 || kind == 22 || kind == 1055 || kind == 1059 || kind == 1061 || kind == 1080)
        { AddProjectileNeed(needs,0); AddProjectileNeed(needs,15); AddProjectileNeed(needs,97); }
        if (kind == 6) AddProjectileNeed(needs,9);
        if (kind == 252 || kind == 1067 || kind == 1068 || kind == 1069) pending.push_back(12);
        if (kind == 1013) for(uint16_t shot:{4,5,6}) AddProjectileNeed(needs,shot);
        if (kind == 903 || kind == 921) AddProjectileNeed(needs,89);
        if (kind == 1177) AddProjectileNeed(needs,94);
        if (kind == 1051) AddProjectileNeed(needs,29);
        if (kind == 1066) AddProjectileNeed(needs,32);
        if (kind == 252) AddProjectileNeed(needs,20);
        if (kind == 1067 || kind == 1076) AddProjectileNeed(needs,37);
        if (kind == 1068) AddProjectileNeed(needs,34);
        if (kind == 1069) AddProjectileNeed(needs,35);
        if (kind == 1076) AddProjectileNeed(needs,9);
        if (kind == 1083) AddProjectileNeed(needs,41);
        if (kind == 1085) AddProjectileNeed(needs,42);
        if(const auto* star=FindFusionStar269Profile(kind))
            if(star->projectile!=65535)AddProjectileNeed(needs,star->projectile);
        if(kind==1104)AddProjectileNeed(needs,39);
        if(kind==1026||kind==924)AddProjectileNeed(needs,14);
        if(kind==934){AddProjectileNeed(needs,75);AddProjectileNeed(needs,53);}
        if (const auto* cannon=FindFusionCannon268Profile(kind)) AddProjectileNeed(needs,cannon->projectileKind);
        if (kind == 914) for(uint16_t shot:{61,62,63,64}) AddProjectileNeed(needs,shot);
        if (kind == 1139) {AddProjectileNeed(needs,79);AddProjectileNeed(needs,80);}
        if (kind == 1142) AddProjectileNeed(needs,83);
        if (kind == 915) AddProjectileNeed(needs,85);
        if (kind == 902)
        {
            for (uint16_t shot : {0,3,11,15,36,87,97}) AddProjectileNeed(needs,shot);
            pending.push_back(1054);
        }
        if (kind == 932) { AddProjectileNeed(needs,98); AddProjectileNeed(needs,99); }
        if (IsFusionUmbrellaBatchKind(kind)) { AddProjectileNeed(needs,58); AddProjectileNeed(needs,59); }
        if (const auto* profile = FindFusionShooterTimingProfile(kind))
            if (profile->projectileKind != 0xffff) AddProjectileNeed(needs,profile->projectileKind);
        for (bool alternate : {false,true})
        {
            if (const auto* profile = FindFusionLobberProfile(kind,alternate)) AddProjectileNeed(needs,profile->projectileKind);
            if (const auto* profile = FindFusionBatchTimingProfile(kind,alternate))
                if (profile->projectileKind != 0xffff) AddProjectileNeed(needs,profile->projectileKind);
        }
    }
    for (auto kind : projectileKinds) AddProjectileNeed(needs,kind);
    return needs;
}
void TraceVisualResidency(LawnApp* app, size_t plants, size_t projectiles, size_t portraits) noexcept
{
    try
    {
    const auto textures = FusionAnimation::GetTextureCacheStats(app);
    LawnNet::NetTrace("Fusion native source model cache plants=%zu projectiles=%zu portraits=%zu lazy=1",
        plants,projectiles,portraits);
    LawnNet::NetTrace("Fusion native source texture cache unique=%llu loaded=%llu reused=%llu liveRgbaBytes=%llu weakIndex=1",
        static_cast<unsigned long long>(textures.uniqueContents),static_cast<unsigned long long>(textures.loadedImages),
        static_cast<unsigned long long>(textures.reusedImages),static_cast<unsigned long long>(textures.retainedRgbaBytes));
    }
    catch (...) { /* Telemetry cannot veto an already validated resource commit. */ }
}

Plant* PumpkinBackOwner(Plant* pumpkin)
{
    if (!pumpkin || !pumpkin->mBoard) return nullptr;
    Plant* inside = pumpkin->IsOnBoard() ? pumpkin->mBoard->GetTopPlantAt(pumpkin->mPlantCol,pumpkin->mRow,
        PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION) : nullptr;
    if (inside && (inside->mRenderOrder > pumpkin->mRenderOrder || inside->mOnBungeeState == GETTING_GRABBED_BY_BUNGEE)) inside = nullptr;
    return inside ? inside : pumpkin;
}
}

struct NativeVisuals::Impl
{
    struct PlantVisual
    {
        FusionAnimation animation;
        float shadowX = 0, shadowY = 0;
        FusionAnimationDrawOptions options;
        FusionAnimationDrawOptions damageOptions[3];
        FusionAnimationDrawOptions pumpkinBackOptions;
        bool pumpkin = false;
        std::string baseState, actionState;
        std::vector<FusionAnimationDrawOptions::ExtraState> idleLayers, shotLayers, sourceDefaultLayers;
        std::map<uint16_t,std::string> bodySpriteVariants;
        std::vector<std::string> grabSpritePaths;
    };
    struct PlantPose
    {
        const char* baseState=nullptr;
        const char* actionState=nullptr;
        float baseSeconds=0,actionSeconds=-1;
        FusionAnimationDrawOptions options;
    };
    bool BuildPlantPose(Plant* plant,const PlantVisual& visual,PlantPose& out);
    struct ProjectileVisual
    {
        FusionAnimation animation;
        FusionAnimationDrawOptions options;
        const char* state = "idle";
        float radiansPerSecond = 0.0f;
        std::string variantNodePath;
        std::vector<std::string> variantSprites;
        std::string heatedSprite;
    };
    LawnApp* app = nullptr;
    std::filesystem::path root;
    static std::unique_ptr<PlantVisual> LoadPlant(LawnApp* app, const std::filesystem::path& root, const PlantPackage& package);
    static std::unique_ptr<ProjectileVisual> LoadProjectile(LawnApp* app, const std::filesystem::path& root, const ProjectilePackage& package);
    static std::unique_ptr<Sexy::MemoryImage> BakePortrait(LawnApp* app, const std::filesystem::path& root, const PlantPackage& package);
    std::map<uint16_t, std::unique_ptr<Sexy::MemoryImage>> portraits;
    std::map<uint16_t, std::unique_ptr<PlantVisual>> plants;
    std::map<uint8_t,std::unique_ptr<FusionAnimation>> equipment;
    std::unique_ptr<FusionAnimation> fertilize269;
    std::map<int,std::unique_ptr<FusionAnimation>> companions;
    std::unique_ptr<FusionAnimation> laserBall269;
    std::map<int,std::unique_ptr<FusionAnimation>> worldAux; // Asset cache only; no actor/clock domain.
    std::map<uint16_t, std::unique_ptr<ProjectileVisual>> projectiles;
    DWORD resourceThread = 0;
    bool prewarmPlanned = false, prewarmMemoryReported = false, prewarmThreadReported = false;
    std::set<uint16_t> prewarmBank, prewarmLive, prewarmPlants, prewarmProjectiles, prewarmReady, prewarmReported;
    std::vector<std::pair<uint16_t,VisualNeeds>> prewarmCandidates;
    // One complete object per non-playing frame: model, portrait, projectile, external Laser ball.
    std::vector<std::pair<uint8_t,uint16_t>> prewarmJobs;
    size_t prewarmNext = 0;
    int prewarmLastDraw = -1;
    bool PrewarmResourceReady(const std::pair<uint8_t,uint16_t>& job) const
    {
        switch (job.first)
        {
        case 0: return !PlantPackageFor(job.second) || plants.contains(job.second);
        case 1: return !PlantPackageFor(job.second) || portraits.contains(job.second);
        case 2: return !ProjectilePackageFor(job.second) || projectiles.contains(job.second);
        case 3: return bool(laserBall269);
        default: return false;
        }
    }
    struct AttachedClock { int firstAge=0,lastAge=0; };
    std::map<uint32_t,AttachedClock> attachedClocks; // Presentation only, keyed by full native generation.
    struct GrabAnchor { uint16_t target=0;uint32_t token=0;float x=0,y=0;int order=0;bool hasOrder=false; }; // Parent-local Board-pixel offset from the rendered source root.
    std::map<uint32_t,GrabAnchor> grabAnchors; // Presentation only, full native plant generation.
    struct FramePlant { PlantPose pose;float rootX=0,rootY=0;bool overlay=false,overlayQueued=false,laser=false;int order=0; };
    std::map<uint32_t,FramePlant> framePlants;
    std::vector<FusionAnimationMask> frameMasks;
    std::vector<FusionMask269View> frameMaskViews;
    Board* frameBoard=nullptr;
    Sexy::Image* frameDestination=nullptr;
    uint64_t frameSerial=0,frameScope=0; // Identity of one draw scope, never a simulation clock.
    bool Held(const Plant* plant) const
    {
        const auto* data=GetNativePlantData(plant);const auto* context=plant?GetNativeContext(plant->mBoard):nullptr;
        if(!data||!context)return false;
        for(const auto& hold:context->gloveHolds)if(hold.plantId==plant->mNetId&&hold.instanceToken==data->instanceToken)return true;
        return false;
    }
    Sexy::Image* nativeFieldSource = nullptr;
    std::unique_ptr<Sexy::MemoryImage> nativeField, fusionField;
    Sexy::SexyMatrix3 fusionFieldMatrix;
    std::string fieldError;
    bool fusionFieldFailed = false;
    struct FieldVisual277
    {
        std::unique_ptr<Sexy::MemoryImage> image;
        Sexy::SexyMatrix3 matrix;
        uint16_t scene=0;float waterX=0,waterY=0;
    };
    std::map<uint8_t,std::unique_ptr<FieldVisual277>> fieldBiomes;
    std::map<uint8_t,std::string> fieldFailures;
};

NativeVisuals::NativeVisuals() = default;
NativeVisuals::~NativeVisuals() = default;

std::unique_ptr<NativeVisuals::Impl::PlantVisual> NativeVisuals::Impl::LoadPlant(
    LawnApp* app, const std::filesystem::path& root, const PlantPackage& package)
{
    auto visual = std::make_unique<PlantVisual>();
    const auto plantPath = root/package.name/"animation.json";
    const Json plant = ReadPackage(plantPath);
    std::vector<int64_t> particleProof;
    for(const auto& component:plant.value("nonRenderingComponents",Json::array()))
        if(component.at("typeId")==198||component.at("typeId")==199)particleProof.push_back(component.at("fileId").get<int64_t>());
    if(!particleProof.empty())
    {
        std::string error;
        if(!app||!EnsureNativeEmbedded269Model(app->mBoard,package.kind,plantPath.string(),&error))
            throw std::runtime_error("source particle binding/resource preflight rejected: "+error);
    }
    LoadAnimation(visual->animation, app, plantPath,particleProof);
    if((package.kind==1104||package.kind==1089)&&particleProof.empty())
    {
        const auto& bounded=plant.at("boundedDormantParticles");
        const auto& proofs=bounded.at("proofs");size_t systems=0;
        for(const auto& component:plant.at("nonRenderingComponents"))if(component.at("typeId")==198)
        {
            ++systems;const auto found=std::find_if(proofs.begin(),proofs.end(),[&](const Json& proof){return proof.at("componentFileId")==component.at("fileId");});
            if(found==proofs.end()||found->at("serializedComponent")!=component.at("serializedComponent"))throw std::runtime_error("unproved dormant source particle");
            const auto raw=component.at("serializedComponent").get<std::string>();
            if(package.kind==1104)
            {
                if(bounded.at("maximumLight")!=0||found->at("reason")!="emission-disabled-no-bursts-light0"||
                   raw.find("  EmissionModule:\n    serializedVersion: 4\n    enabled: 0\n")==std::string::npos||
                   raw.find("    m_BurstCount: 0")==std::string::npos||raw.find("  prewarm: 0")==std::string::npos)
                    throw std::runtime_error("source1104 ordinary-light0 particle proof differs");
            }
            else
            {
                const auto ancestor=found->at("inactiveAncestor").get<std::string>();
                if(bounded.at("maximumDamageStage")!=0||found->at("reason")!="damage-ancestor-inactive-stage0"||
                   (ancestor!="pumpkin_damage1"&&ancestor!="pumpkin_damage2")||
                   !std::any_of(plant.at("nodes").begin(),plant.at("nodes").end(),[&](const Json& n){return n.at("path").get<std::string>()==ancestor&&!n.at("active").get<bool>();}))
                    throw std::runtime_error("source1089 stage0 particle proof differs");
            }
        }
        if(systems!=proofs.size()||systems!=(package.kind==1104?1u:2u)||!plant.at("deferredComponents").empty())
            throw std::runtime_error("incomplete dormant particle proof");
    }
    const auto* profile = FindFusionShooterTimingProfile(package.kind);
    visual->baseState = visual->animation.GetDefaultStateName();
    visual->actionState = profile ? profile->visualState :
        (visual->animation.GetStateSpeed("shoot") > 0 ? "shoot" : visual->baseState);
    if (!visual->animation.IsStatic() && (visual->animation.GetStateSpeed(visual->baseState.c_str()) <= 0 ||
        visual->animation.GetStateSpeed(visual->actionState.c_str()) <= 0))
        throw std::runtime_error("native Fusion plant is missing its bound source visual state");
    // Every bound source state must survive loading; null-motion controller
    // states are intentional and are never substituted for actual clips.
    const bool strict267=IsFusionShooter267Kind(package.kind)||IsFusionAction267Kind(package.kind)||
        package.kind==1022||package.kind==1024||package.kind==1025||package.kind==1058||package.kind==1073||package.kind==1112||
        package.kind==1114||package.kind==1125||package.kind==1133||package.kind==1078;
    const bool strict268=FindFusionEquipmentProfile(package.kind)||IsNativeWater268ImplementedKind(package.kind)||
        FindFusionCannon268Profile(package.kind)||IsNativeChomper268Kind(package.kind)||IsFusionExtras268Kind(package.kind);
    const bool strict269=FindFusionStar269Profile(package.kind)||FindFusionDoom269Timing(package.kind)||package.kind==1056||package.kind==242||package.kind==1174||package.kind==1175;
    if(strict267||strict268||strict269||package.kind==1026||package.kind==924||package.kind==934)for (const auto& state : plant.at("controller").at("states"))
        if (!state.at("clip").is_null() && visual->animation.GetStateSpeed(state.at("name").get<std::string>().c_str()) <= 0)
            throw std::runtime_error("native Fusion plant is missing a declared source clip state");
    std::vector<const char*> required267;
    switch(package.kind)
    {
    case 927:required267={"Kaiji","idle2","shoot","shoot_loop","fire"};break;
    case 904:required267={"idle","startshoot","shooting","endshoot"};break;
    case 1007:required267={"idle","rise","armed","mashed","shoot"};break;
    case 1077:required267={"idle","grow1","grow2","idle1","idle2"};break;
    case 1078:required267={"idle","grow","idle2"};break;
    case 1181:case 1176:case 1086:required267={"idle","blow","blowloop"};break;
    case 1128:case 1131:required267={"idle","block","block2"};break;
    case 1144:required267={"idle","block","super"};break;
    case 30:case 1115:case 1127:case 1143:case 1185:case 1186:required267={"idle","block"};break;
    case 906:required267={"idle","attack"};break;
    case 1103:required267={"idle","shoot1","rise","lower","shoot2","idle2"};break;
    default:break;
    }
    for(const char* state:required267)if(visual->animation.GetStateSpeed(state)<=0)
        throw std::runtime_error("native Fusion267 plant is missing a required source state");
    if(package.kind==927&&(visual->animation.GetStateSpeed("idle2",0)<=0||
        visual->animation.GetStateSpeed("shoot",1)<=0||visual->animation.GetStateSpeed("fire",2)<=0))
        throw std::runtime_error("native Fusion927 is missing its independent source layers");
    if (const auto* chomper=FindFusionChomperProfile(package.kind))
        for (const char* state : {"idle", "bite", chomper->digestVisualState,chomper->swallowVisualState,chomper->shootingVisualState})
            if (state && visual->animation.GetStateSpeed(state) <= 0)
                throw std::runtime_error("native Fusion Chomper is missing a required source state");
    const auto* batch = FindFusionBatchActionProfile(package.kind);
    const bool three267=package.kind==919||package.kind==1047;
    if (batch || three267)
    {
        std::vector<const char*> required{"idle"};
        if (batch && (batch->family == FusionBatchActionFamily::Doom || batch->family == FusionBatchActionFamily::Jalapeno)) required.push_back("explode");
        else if (batch && batch->family == FusionBatchActionFamily::Squash) required.insert(required.end(),{"lookleft","lookright","jump","jumppop"});
        else if (batch && batch->family == FusionBatchActionFamily::Caltrop) required.push_back("attack");
        else if (batch && batch->family == FusionBatchActionFamily::Scaredy) required.insert(required.end(),{"scaredidle","shoot"});
        else if (batch && batch->family == FusionBatchActionFamily::AdaptiveFume) required.insert(required.end(),{"shoot","shoot1"});
        else if (three267 || batch->family == FusionBatchActionFamily::Threepeater) required.push_back("shoot");
        for (const char* name : required)
            if (visual->animation.GetStateSpeed(name) <= 0) throw std::runtime_error("native Fusion batch plant is missing a required source state");
        if (three267 || batch->family == FusionBatchActionFamily::Threepeater)
        {
            const auto& layers = plant.at("controller").at("layers");
            const auto& states = plant.at("controller").at("states");
            if (layers.size() != 4) throw std::runtime_error("native Fusion Threepeater needs three actual head layers");
            for (int layer = 1; layer < 4; ++layer)
            {
                const auto defaultId = layers[layer].at("defaultState").get<int64_t>();
                std::string idleName; int idleCount = 0, shootCount = 0;
                for (const auto& state : states)
                {
                    const auto id = state.at("id").get<int64_t>();
                    if (id == defaultId)
                    {
                        idleName = state.at("name").get<std::string>(); ++idleCount;
                        if (state.at("clip").is_null() || !plant.at("clips").at(state.at("clip").get<std::string>()).at("loop").get<bool>())
                            throw std::runtime_error("native Fusion Threepeater head default must be a bound looping source state");
                    }
                    if (state.at("name").get<std::string>() != "shoot") continue;
                    for (const auto& member : layers[layer].at("states")) if (member.get<int64_t>() == id) ++shootCount;
                }
                if (idleCount != 1 || shootCount != 1 || visual->animation.GetStateSpeed(idleName.c_str(),layer) <= 0 ||
                    visual->animation.GetStateSpeed("shoot",layer) <= 0)
                    throw std::runtime_error("native Fusion Threepeater has an ambiguous/missing head state");
                visual->idleLayers.push_back({idleName,0.0f,layer});
                if (layer > 1) visual->shotLayers.push_back({"shoot",0.0f,layer});
            }
        }
    }
    for(const auto& sprite:plant.at("sprites"))
    {
        const auto path=plant.at("nodes").at(sprite.at("node").get<int>()).at("path").get<std::string>();
        if(path.starts_with("Grab/grabs/"))visual->grabSpritePaths.push_back(path);
    }
    if(FindFusionStar269Profile(package.kind))
        for(size_t layer=1;layer<plant.at("controller").at("layers").size();++layer)
        {
            const auto& sourceLayer=plant.at("controller").at("layers")[layer];
            const auto defaultId=sourceLayer.at("defaultState").get<int64_t>();
            for(const auto& state:plant.at("controller").at("states"))
                if(state.at("id").get<int64_t>()==defaultId&&!state.at("clip").is_null())
                    visual->sourceDefaultLayers.push_back({state.at("name").get<std::string>(),0,int(layer)});
        }
    if(package.kind==924)
        for(const char* required:{"head/Magnetshroom_idle_electric1","head/Magnetshroom_idle_electric1/Magnetshroom_shock1","body/body ec1"})
            if(!std::any_of(plant.at("nodes").begin(),plant.at("nodes").end(),[&](const Json& n){return n.at("path").get<std::string>()==required;}))
                throw std::runtime_error("native Hypno924 is missing actual source charge node");
    if(package.kind==934&&!std::any_of(plant.at("nodes").begin(),plant.at("nodes").end(),[](const Json& n){return n.at("path").get<std::string>()=="back";}))
        throw std::runtime_error("native Solar934 is missing actual source back group");
    const auto& shadow = plant.at("anchors").at("shadow");
    if (!shadow.at("relativeToRuntimeRoot").get<bool>()) throw std::runtime_error("invalid native Fusion visual ground anchor");
    visual->shadowX = shadow.at("x").get<float>(); visual->shadowY = shadow.at("y").get<float>();
    if (!std::isfinite(visual->shadowX) || !std::isfinite(visual->shadowY)) throw std::runtime_error("nonfinite native Fusion ground anchor");
    // The real Plant supplies the normal native shadow, once.
    visual->options.hiddenNodePaths.push_back(shadow.at("path").get<std::string>());
    if(package.kind==12)
        for(const auto& variant:{std::pair<uint16_t,const char*>{1,"Sun"},{13,"Squash"},{14,"Three"},{16,"Jalapeno"},{18,"TorchWood"}})
        {
            const auto path="Resources/plants/lilypad/Lily_"+std::string(variant.second)+".asset";
            const auto guid=plant.at("extraSpriteGuids").at(path).get<std::string>();
            if(!plant.at("spriteVariants").contains(guid))throw std::runtime_error("native Fusion Lily is missing a source body sprite");
            visual->bodySpriteVariants.emplace(variant.first,guid);
        }
    const auto* equipmentProfile=FindFusionEquipmentProfile(package.kind);
    const auto* defenderProfile=FindFusionDefenderProfile(package.kind);
    if ((defenderProfile && defenderProfile->family == FusionDefenderFamily::Pumpkin) ||
        (equipmentProfile && equipmentProfile->family == FusionEquipmentFamily::Pumpkin))
    {
        visual->pumpkin = true;
        visual->pumpkinBackOptions = visual->options;
        const int rootNode = plant.at("root").get<int>();
        bool backExists = false;
        for (const auto& sprite : plant.at("sprites"))
            if (sprite.at("node").get<int>() == rootNode) throw std::runtime_error("native Fusion pumpkin split requires child source renderers");
        for (const auto& node : plant.at("nodes"))
            if (node.at("parent").get<int>() == rootNode)
            {
                const auto path = node.at("path").get<std::string>();
                if (path == "pumpkin_back") backExists = true;
                else visual->pumpkinBackOptions.hiddenNodePaths.push_back(path);
            }
        if (!backExists) throw std::runtime_error("native Fusion pumpkin is missing its actual source back group");
        visual->options.hiddenNodePaths.push_back("pumpkin_back");
    }
    if (package.kind == kNutShooter || package.kind == kSunNut ||
        package.kind == kCherryNut || package.kind == kPotatoNut)
    {
        const char* groups[] = {"Wallnut_body", "Wallnut_cracked1", "Wallnut_cracked2"};
        if (package.kind == kPotatoNut)
        {
            groups[0] = "PotatoMine_body";
            groups[1] = "PotatoMine_body2";
            groups[2] = "PotatoMine_body3";
        }
        for (int stage = 0; stage < 3; ++stage)
        {
            visual->damageOptions[stage] = visual->options;
            for (int group = 0; group < 3; ++group)
                visual->damageOptions[stage].nodeActivityOverrides.push_back({groups[group], group == stage});
        }
    }
    if (package.kind == 1012)
    {
        for (int stage = 0; stage < 3; ++stage)
        {
            visual->damageOptions[stage] = visual->options;
            if (!stage) continue; // Source healthy sprites are the unchanged sampled pose.
            for (const auto& part : {std::pair<const char*,const char*>{"Wallnut_body","back"},
                std::pair<const char*,const char*>{"head","head"}})
            {
                const std::string path = "Resources/plants/_mixer/nutchomper/cracked" +
                    std::to_string(stage) + "_" + part.second + ".asset";
                const std::string guid = plant.at("extraSpriteGuids").at(path).get<std::string>();
                if (!plant.at("spriteVariants").contains(guid))
                    throw std::runtime_error("native Fusion Chomper is missing a damage sprite");
                visual->damageOptions[stage].spriteReplacements.push_back(
                    {part.first,guid,1.0f,1.0f,1.0f,false});
            }
        }
    }
    if (FindFusionStrongChomperProfile(package.kind))
        for(int stage=0;stage<3;++stage)
        {
            visual->damageOptions[stage]=visual->options;
            for(int sourceStage=0;sourceStage<3;++sourceStage)
                for(const char* part:{"Chomper_face_upper","Chomper_body"})
                {
                    const std::string path=std::string(part)+(sourceStage?std::to_string(sourceStage):"");
                    const bool exists=std::any_of(plant.at("nodes").begin(),plant.at("nodes").end(),[&](const Json& n){return n.at("path").get<std::string>()==path;});
                    if(!exists)throw std::runtime_error("native Fusion Strong Chomper is missing its source damage group");
                    visual->damageOptions[stage].nodeActivityOverrides.push_back({path,sourceStage==stage});
                }
        }
    if(package.kind==1129)
        for(uint8_t stage=0;stage<3;++stage)for(uint8_t form=0;form<3;++form)
        {
            const char* path=FusionMachine268FormNode(stage,form);
            if(!path||!std::any_of(plant.at("nodes").begin(),plant.at("nodes").end(),[&](const Json& n){return n.at("path").get<std::string>()==path;}))
                throw std::runtime_error("native Fusion Machine Nut is missing a source form node");
        }
    if (package.kind == 29)
        for (int stage = 0; stage < 3; ++stage)
        {
            visual->damageOptions[stage] = visual->options;
            for (int node = 0; node < 3; ++node)
            {
                const std::string group = "Garlic_body"+std::to_string(node+1);
                bool exists = false;
                for (const auto& sourceNode : plant.at("nodes")) if (sourceNode.at("path").get<std::string>() == group) exists = true;
                if (!exists) throw std::runtime_error("native Fusion Garlic is missing its source damage group");
                visual->damageOptions[stage].nodeActivityOverrides.push_back({group,node == stage});
            }
        }
    if (const auto* defender = FindFusionDefenderProfile(package.kind); defender && defender->damageNodes[0])
        for (int stage = 0; stage < 3; ++stage)
        {
            visual->damageOptions[stage] = visual->options;
            for (int node = 0; node < 3; ++node)
            {
                bool exists = false;
                for (const auto& sourceNode : plant.at("nodes")) if (sourceNode.at("path").get<std::string>() == defender->damageNodes[node]) exists = true;
                if (!exists) throw std::runtime_error("native Fusion defender is missing its source damage group");
                visual->damageOptions[stage].nodeActivityOverrides.push_back({defender->damageNodes[node],node == stage});
            }
        }

    return visual;
}
std::unique_ptr<NativeVisuals::Impl::ProjectileVisual> NativeVisuals::Impl::LoadProjectile(
    LawnApp* app, const std::filesystem::path& root, const ProjectilePackage& package)
{
    auto visual = std::make_unique<ProjectileVisual>();
    const auto path = root/package.name/"animation.json";
    const Json source = ReadPackage(path);
    std::vector<int64_t> particleProof;
    if(const auto modelKey=EmbeddedProjectileModelKey(package.kind))
    {std::string error;if(!EnsureNativeEmbedded269ProjectileModel(app->mBoard,modelKey,path.string(),&error))throw std::runtime_error(error);
     for(const auto& c:source.at("nonRenderingComponents"))if(c.at("typeId")==198||c.at("typeId")==199)particleProof.push_back(c.at("fileId").get<int64_t>());}
    LoadAnimation(visual->animation, app, path,particleProof);
    if (package.kind == 90)
        visual->options.spriteReplacements.push_back({"",source.at("extraSpriteGuids").at("Resources/bullet/bulletspirtes/IronPeaBullet.asset").get<std::string>(),1.0f,1.0f,1.0f,false});
    visual->state = package.state;
    if(!visual->animation.IsStatic()&&visual->animation.GetStateSpeed(package.state)<=0)
        throw std::runtime_error("native Fusion projectile is missing its required source state");
    if(package.kind==11)
    {
        visual->heatedSprite=source.at("extraSpriteGuids").at("Resources/bullet/bulletspirtes/fireironpea.asset").get<std::string>();
        if(!source.at("spriteVariants").contains(visual->heatedSprite))throw std::runtime_error("native Fusion heated11 is missing source Sprite39");
    }
    std::vector<const char*> requiredNodes;
    if(IsFusionCannon268Way14(package.kind)) requiredNodes={"Sprite","Shadow"};
    else if(package.kind==80)requiredNodes={"Sprite"};
    else if(package.kind==94)requiredNodes={"image"};
    else if(FusionStar269ProjectileClass(package.kind))requiredNodes={"ProjectileStar"};
    else if(package.kind==28||package.kind==32||package.kind==36)requiredNodes={"Image"};
    for(const char* required:requiredNodes)
        if(!std::any_of(source.at("nodes").begin(),source.at("nodes").end(),[&](const Json& n){return n.at("path").get<std::string>()==required;}))
            throw std::runtime_error("native Fusion projectile is missing a required source child");
    visual->radiansPerSecond = package.radiansPerSecond;
    if (package.radiansPerSecond != 0.0f && visual->animation.GetStateSpeed(package.state) <= 0.0f)
        throw std::runtime_error("native Fusion rolling projectile is missing its rotation state");
    if (source.at("anchors").contains("shadow"))
        visual->options.hiddenNodePaths.push_back(source.at("anchors").at("shadow").at("path").get<std::string>());
    if(package.kind==91)
    {
        const auto& root=source.at("nodes").at(source.at("root").get<int>());
        const auto& colliders=source.at("colliders");
        if(source.at("sourcePrefab")!="Resources/bullet/prefabs/CactusBullet_lour.prefab"||!visual->animation.IsStatic()||
           root.at("scale").at("x")!=.5||root.at("scale").at("y")!=.5||colliders.size()!=1)
            throw std::runtime_error("source91 expanded prefab/scale differs");
        const auto& box=colliders.at(0);
        if(box.at("type")!="box"||box.at("node")!=source.at("root")||!box.at("enabled").get<bool>()||!box.at("trigger").get<bool>()||
           std::abs(box.at("offset").at("x").get<double>()+.32343602)>1e-8||
           std::abs(box.at("offset").at("y").get<double>()+.0125605315)>1e-9||
           std::abs(box.at("size").at("x").get<double>()-.80531013)>1e-8||
           std::abs(box.at("size").at("y").get<double>()-.30916628)>1e-8)
            throw std::runtime_error("source91 expanded collider differs");
    }
    if (package.kind == 13)
    {
        const auto rootNode = source.at("root").get<int>();
        int renderers = 0;
        for (const auto& sprite : source.at("sprites")) if (sprite.at("node").get<int>() == rootNode) ++renderers;
        if (renderers != 1) throw std::runtime_error("native Fusion colorful puff needs its exact root SpriteRenderer");
        visual->variantNodePath = source.at("nodes").at(rootNode).at("path").get<std::string>();
        for (int variant = 1; variant <= 7; ++variant)
        {
            const std::string sourcePath = "Resources/bullet/bulletspirtes/ColorfulPuff"+std::to_string(variant)+".asset";
            const auto guid = source.at("extraSpriteGuids").at(sourcePath).get<std::string>();
            if (!source.at("spriteVariants").contains(guid)) throw std::runtime_error("native Fusion colorful puff is missing its audited palette sprite");
            visual->variantSprites.push_back(guid);
        }
    }
    return visual;
}
std::unique_ptr<Sexy::MemoryImage> NativeVisuals::Impl::BakePortrait(
    LawnApp* app, const std::filesystem::path& root, const PlantPackage& package)
{
    app->CleanSharedImages();
    const auto path = root/package.portrait/"animation.json";
    const Json portrait = ReadPackage(path);
    const auto& bounds = portrait.at("visualBounds");
    const float minX = bounds.at("minX").get<float>(), maxX = bounds.at("maxX").get<float>();
    const float minY = bounds.at("minY").get<float>(), maxY = bounds.at("maxY").get<float>();
    if (!std::isfinite(minX) || !std::isfinite(maxX) || !std::isfinite(minY) || !std::isfinite(maxY) || minX >= maxX || minY >= maxY)
        throw std::runtime_error("invalid native Fusion portrait bounds");
    auto image = std::make_unique<Sexy::MemoryImage>(app);
    image->Create(kPortraitSize,kPortraitSize);
    image->Clear();
    {
        FusionAnimation animation;
        LoadAnimation(animation,app,path);
        Sexy::Graphics preview(image.get());
        preview.SetLinearBlend(true);
        const float fit = (std::min)((kPortraitSize-4)/(maxX-minX),(kPortraitSize-4)/(maxY-minY));
        animation.Draw(&preview,kPortraitSize*.5f-fit*(minX+maxX)*.5f,
            kPortraitSize*.5f+fit*(minY+maxY)*.5f,fit/100.0f,0.0f);
    }
    // The independent raster owns its pixels. Drop temporary source atlases
    // before another portrait is decoded; live runtime references stay protected.
    app->CleanSharedImages();
    return image;
}

bool NativeVisuals::Load(LawnApp* app, std::string& error)
{
    std::string stage;
    try
    {
        if (!app) throw std::runtime_error("native Fusion visual loading needs an initialized application");
        auto result = std::make_unique<Impl>();
        result->app = app;
        result->resourceThread = GetCurrentThreadId();
        result->root = PackageRoot();
        std::set<uint16_t> kinds;
        for (const auto& package : plantPackages)
        {
            if (!kinds.insert(package.kind).second) throw std::runtime_error("duplicate native Fusion plant visual binding");
            if (IsNativeBasePlantKind(package.kind) && GetNativeSeedForPlant(package.kind) != SEED_NONE)
            {
                stage = "portrait kind="+std::to_string(package.kind)+": ";
                result->portraits.emplace(package.kind,Impl::BakePortrait(app,result->root,package));
            }
        }
        kinds.clear();
        for (const auto& package : projectilePackages)
            if (!kinds.insert(package.kind).second) throw std::runtime_error("duplicate native Fusion projectile visual binding");
        mImpl = std::move(result);
        TraceVisualResidency(app,mImpl->plants.size(),mImpl->projectiles.size(),mImpl->portraits.size());
        error.clear();
        return true;
    }
    catch (const std::exception& exception)
    { if (app) app->CleanSharedImages(); error = stage+exception.what(); return false; }
}

bool NativeVisuals::EnsureEquipment(uint8_t factory,std::string& error)
{
    if(!mImpl||factory>10||factory==3){error="unsupported source equipment visual";return false;}
    if(mImpl->equipment.contains(factory)){error.clear();return true;}
    try
    {
        auto visual=std::make_unique<FusionAnimation>();
        LoadAnimation(*visual,mImpl->app,mImpl->root/("equipment-bucket-"+std::to_string(factory))/"animation.json");
        mImpl->equipment.emplace(factory,std::move(visual));error.clear();return true;
    }
    catch(const std::exception& ex){error=ex.what();return false;}
}

bool CanCreateNativeEquipmentVisual(Board* board,FusionEquipmentType factory)
{
    auto* context=GetNativeContext(board);std::string error;
    return context&&context->visuals&&context->visuals->EnsureEquipment(static_cast<uint8_t>(factory),error);
}

bool NativeVisuals::DrawEquipment(Sexy::Graphics* graphics,Coin* coin) const
{
    const auto* data=GetNativeEquipmentData(coin);
    if(!data)return false;
    if(!mImpl||!graphics||coin->mDead)return true;
    const auto found=mImpl->equipment.find(static_cast<uint8_t>(data->factoryType));
    if(found==mImpl->equipment.end())return true;
    float x=coin->mPosX+20,y=coin->mPosY+20;
    if(data->phase==NativeEquipmentPhase::Cursor)
        GetNativeEquipmentCursorPosition(coin->mBoard,data->claimSlot,x,y);
    FusionAnimationDrawOptions options;
    const auto& b=data->attachmentBasis;
    const float k=kNativePixelsPerWorldUnit;
    options.useBoardBasis=true;
    options.boardBasis={k*b[0],-k*b[1],k*b[2],-k*b[3]};
    if(data->sourcePlantId&&!data->weapon)
    {
        Plant* source=nullptr;
        while(coin->mBoard->IteratePlants(source))
            if(!source->mDead&&source->mNetId==data->sourcePlantId)
                if(const auto* sd=GetNativePlantData(source);sd&&sd->instanceToken==data->sourcePlantToken)
                {
                    std::array<float,6> parent;
                    if(GetPlantNodeTransform(source,sd->kind==1102?"Blover_petal/Position":"",parent))
                        options.boardBasis={parent[0]*b[0]+parent[2]*b[1],parent[1]*b[0]+parent[3]*b[1],
                            parent[0]*b[2]+parent[2]*b[3],parent[1]*b[2]+parent[3]*b[3]};
                    break;
                }
    }
    if(data->weapon){options.boardBasis={k,0,0,-k};options.colorMultiplierRGBA={255,0,0,255};}
    const auto& animation=*found->second;
    animation.DrawPose(graphics,x,y,k/100,animation.GetDefaultStateName(),coin->mCoinAge*.01f,nullptr,-1,1,&options);
    return true;
}

bool DrawNativeFusionEquipment(Sexy::Graphics* graphics,Coin* coin)
{
    const auto* context=coin?GetNativeContext(coin->mBoard):nullptr;
    return context&&context->visuals&&context->visuals->DrawEquipment(graphics,coin);
}
bool NativeVisuals::EnsureFertilize269(std::string& error)
{
    if(!mImpl){error="native visual cache unavailable";return false;}if(mImpl->fertilize269){error.clear();return true;}
    try
    {
        const auto path=mImpl->root/"item-fertilize269"/"animation.json";
        if(ReadPackage(path).at("sourcePrefab")!="Resources/items/fertilize/Ferilize.prefab")throw std::runtime_error("foreign Fertilize source prefab");
        auto animation=std::make_unique<FusionAnimation>();LoadAnimation(*animation,mImpl->app,path);
        for(const char* state:{"idle","static","use"})if(animation->GetStateLayer(state)<0)throw std::runtime_error("missing source Fertilize state");
        const auto& events=animation->GetEvents("use");
        if(std::none_of(events.begin(),events.end(),[](const auto& e){return e.functionName=="Upgrade"&&std::abs(e.clipTimeSeconds-double(.9166667f))<1e-5;}))throw std::runtime_error("missing source Fertilize Upgrade event");
        mImpl->fertilize269=std::move(animation);error.clear();return true;
    }catch(const std::exception& ex){error=ex.what();return false;}
}
bool EnsureNativeFertilize269Visuals(Board* board)
{auto* c=GetNativeContext(board);std::string error;return c&&c->visuals&&c->visuals->EnsureFertilize269(error);}
bool NativeVisuals::DrawFertilize269(Sexy::Graphics* g,Coin* coin) const
{
    const auto* data=GetNativeFertilize269Data(coin);if(!data)return false;
    if(!mImpl||!mImpl->fertilize269||coin->mDead)return true;
    float x=coin->mPosX+20,y=coin->mPosY+20;if(data->phase==NativeFertilize269Phase::Cursor)GetNativeEquipmentCursorPosition(coin->mBoard,data->heldSlot,x,y);
    const char* state=nullptr;float seconds=0;if(!GetNativeFertilize269VisualPose(coin,state,seconds))return true;
    FusionAnimationDrawOptions options;options.useBoardBasis=true;options.boardBasis={float(kFusionBasePixelsX),0,0,-float(GetNativeFusionPixelsY(coin->mBoard))};
    mImpl->fertilize269->DrawPose(g,x,y,kNativePixelsPerWorldUnit/100,state,seconds,nullptr,-1,1,&options);return true;
}
bool DrawNativeFertilize269(Sexy::Graphics* g,Coin* coin)
{const auto* c=coin?GetNativeContext(coin->mBoard):nullptr;return c&&c->visuals&&c->visuals->DrawFertilize269(g,coin);}

bool NativeVisuals::EnsureCompanion(FusionCompanion269Family family,bool smallVariant,std::string& error)
{
    const int key=CompanionVisualKey(family,smallVariant);
    if(!mImpl||key<0){error="unsupported source companion model variant";return false;}
    if(mImpl->companions.contains(key)){error.clear();return true;}
    try
    {
        auto animation=std::make_unique<FusionAnimation>();
        const auto path=mImpl->root/CompanionPackage(key)/"animation.json";
        constexpr const char* sourcePrefabs[]={"Resources/plants/pumpkin/superpumpkin/LittleFly.prefab",
            "Resources/plants/pumpkin/ultimatepumpkin/UltimateLittleFly.prefab",
            "Resources/plants/pumpkin/cherryultimatepumpkin/CherryFly.prefab",
            "Resources/plants/torchwood/cherrytorch/CherryFly.prefab"};
        if(ReadPackage(path).at("sourcePrefab").get<std::string>()!=sourcePrefabs[key])
            throw std::runtime_error("source companion prefab binding differs");
        LoadAnimation(*animation,mImpl->app,path);
        for(const char* state:{"idle","shoot","blowloop"})
            if(animation->GetStateLayer(state)<0)throw std::runtime_error(std::string("missing source companion state ")+state);
        const auto* timing=FindFusionCompanion269Animation(family);
        if(!timing||std::abs(animation->GetStateSpeed("shoot")-timing->stateSpeed)>1e-5||
            std::abs(animation->GetStateDurationSeconds("shoot",1)-timing->clipDurationSeconds/timing->stateSpeed)>1e-5)
            throw std::runtime_error("source companion shoot timing differs from source profile");
        const auto& events=animation->GetEvents("shoot");
        if(events.size()!=timing->eventClipSeconds.size())throw std::runtime_error("source companion event count differs");
        for(std::size_t i=0;i<events.size();++i)
            if(events[i].functionName!="AnimShoot"||std::abs(events[i].clipTimeSeconds-timing->eventClipSeconds[i])>1e-5)
                throw std::runtime_error("source companion AnimShoot timeline differs");
        float x=0,y=0;
        if(family!=FusionCompanion269Family::CherryLittleFly&&!animation->TryGetNodeOffset("Shoot",x,y,"idle",0))
            throw std::runtime_error("missing source companion Shoot node");
        mImpl->companions.emplace(key,std::move(animation));error.clear();return true;
    }
    catch(const std::exception& ex){error=ex.what();return false;}
}

bool NativeVisuals::DrawCompanion(Sexy::Graphics* graphics,Projectile* projectile) const
{
    const auto* state=GetNativeCompanion269State(projectile);if(!state)return false;
    if(!mImpl||!graphics||projectile->mDead)return true;
    const auto found=mImpl->companions.find(CompanionVisualKey(state->family,state->smallVariant));
    if(found==mImpl->companions.end())return true;
    const char* pose=nullptr;float seconds=0;if(!GetNativeCompanion269VisualPose(projectile,pose,seconds))return true;
    const auto options=CompanionOptions(projectile);
    // Native GameObject::BeginDraw already translated by mX/mY. Preserve the
    // fractional authoritative root and source scale/rotation in its hierarchy.
    found->second->DrawPose(graphics,projectile->mPosX+20-projectile->mX,
        projectile->mPosY+projectile->mPosZ+20-projectile->mY,1,pose,seconds,nullptr,-1,1,&options);
    return true;
}

bool NativeVisuals::GetCompanionNodePoint(const Projectile* projectile,const char* node,float& x,float& y) const
{
    const auto* state=GetNativeCompanion269State(projectile);if(!mImpl||!state||!node||projectile->mDead)return false;
    const auto found=mImpl->companions.find(CompanionVisualKey(state->family,state->smallVariant));
    if(found==mImpl->companions.end())return false;
    const char* pose=nullptr;float seconds=0;if(!GetNativeCompanion269VisualPose(projectile,pose,seconds))return false;
    const auto options=CompanionOptions(projectile);float sx=0,sy=0;
    if(!found->second->TryGetNodeOffset(node,sx,sy,pose,seconds,nullptr,-1,1,&options))return false;
    const auto& b=options.boardBasis;
    const float px=projectile->mPosX+20+b[0]*sx+b[2]*sy,py=projectile->mPosY+projectile->mPosZ+20+b[1]*sx+b[3]*sy;
    if(!std::isfinite(px)||!std::isfinite(py))return false;x=px;y=py;return true;
}

bool EnsureNativeCompanion269Visual(Board* board,FusionCompanion269Family family,bool smallVariant,std::string* error)
{
    auto* context=IsNativeFusionBoard(board)?GetNativeContext(board):nullptr;std::string failure;
    if(!context||!context->visuals){if(error)*error="native Fusion visual index is unavailable";return false;}
    const bool result=context->visuals->EnsureCompanion(family,smallVariant,failure);if(error)*error=failure;
    if(!result)LawnNet::NetTrace("Fusion companion visual admission rejected family=%u smallVariant=%d error=%s",unsigned(family),smallVariant?1:0,failure.c_str());return result;
}
bool DrawNativeCompanion269(Sexy::Graphics* graphics,Projectile* projectile)
{
    const auto* context=projectile&&IsNativeFusionBoard(projectile->mBoard)?GetNativeContext(projectile->mBoard):nullptr;
    return context&&context->visuals&&context->visuals->DrawCompanion(graphics,projectile);
}
bool GetNativeCompanion269NodePoint(const Projectile* projectile,const char* node,float& x,float& y)
{
    const auto* context=projectile&&IsNativeFusionBoard(projectile->mBoard)?GetNativeContext(projectile->mBoard):nullptr;
    return context&&context->visuals&&context->visuals->GetCompanionNodePoint(projectile,node,x,y);
}

namespace
{
bool Laser269Main(const Plant* p)
{
    const auto* d=GetNativePlantData(p);const auto* state=GetNativeWorld269PlantState(p);
    if(!p||p->mDead||!p->mIsOnBoard||!d||d->kind!=937||!state||!state->connected||!state->main)return false;
    const auto* context=GetNativeContext(p->mBoard);if(!context)return false;
    for(const auto& held:context->gloveHolds)
        if((held.plantId==p->mNetId&&held.instanceToken==d->instanceToken)||
           (held.plantId==state->connectedPlant.id&&held.instanceToken==state->connectedPlant.token))return false;
    float x=0,y=0;int row=0;return GetNativeLaserConnections269BallPoint(p,x,y,row);
}
void DrawLaser269Ribbon(Sexy::Graphics* g,const Board* b,float x,float y,float tx,float ty,float localX,float localY)
{
    const double px=kFusionBasePixelsX,py=GetNativeFusionPixelsY(b),dx=tx-x,dy=ty-y,len=std::hypot(dx/px,dy/py);
    if(len<=1e-6)return;const double nx=-(dy/py)/len*.1*px,ny=(dx/px)/len*.1*py;
    g->PushState();
    // Actual theLight.prefab: widthMultiplier.2, constant widthCurve1,
    // two serialized gradient RGBA keys. Native AA ribbon; URP/line PS debt.
    for(int i=0;i<16;++i)
    {const double t0=i/16.,t1=(i+1)/16.,t=(t0+t1)*.5;
     Sexy::Point points[4]={{int(x-localX+dx*t0+nx),int(y-localY+dy*t0+ny)},{int(x-localX+dx*t1+nx),int(y-localY+dy*t1+ny)},
       {int(x-localX+dx*t1-nx),int(y-localY+dy*t1-ny)},{int(x-localX+dx*t0-nx),int(y-localY+dy*t0-ny)}};
     g->SetColor(Sexy::Color(255,int(255*(.9701796+(.96862745-.9701796)*t)),int(255*(.6650944+(.6666667-.6650944)*t)),128));g->PolyFillAA(points,4,true);}
    g->PopState();
}
}
bool NativeVisuals::EnsureLaser269(std::string& error)
{
    if(!mImpl){error="native Fusion visual cache unavailable";return false;}
    if(mImpl->laserBall269){error.clear();return true;}
    try
    {
        const auto path=mImpl->root/"world269-laser-ball"/"animation.json";const auto source=ReadPackage(path);
        if(source.at("sourcePrefab")!="Resources/plants/umbrellaleaf/laserumbrella/LightBall.prefab")throw std::runtime_error("foreign Laser937 ball prefab");
        const auto& root=source.at("nodes").at(source.at("root").get<int>());
        if(root.at("scale").at("x")!=.5||root.at("scale").at("y")!=.5)throw std::runtime_error("source Laser937 ball scale differs");
        std::vector<int64_t> proof;for(const auto& c:source.at("nonRenderingComponents"))if(c.at("typeId")==198||c.at("typeId")==199)proof.push_back(c.at("fileId").get<int64_t>());
        if(proof.size()!=2||std::find(proof.begin(),proof.end(),198884547391421755LL)==proof.end()||std::find(proof.begin(),proof.end(),199099118695303065LL)==proof.end())throw std::runtime_error("source Laser937 ball particle pair differs");
        if(!EnsureNativeEmbedded269ProjectileModel(mImpl->app->mBoard,4937,path.string(),&error))return false;
        auto model=std::make_unique<FusionAnimation>();LoadAnimation(*model,mImpl->app,path,proof);
        if(!model->IsStatic())throw std::runtime_error("source Laser937 ball unexpectedly has Animator");
        float x=0,y=0;if(!model->TryGetNodeOffset("fightball_1",x,y,nullptr,0))throw std::runtime_error("source Laser937 ball particle node missing");
        mImpl->laserBall269=std::move(model);error.clear();return true;
    }catch(const std::exception& ex){error=ex.what();return false;}
}
bool NativeVisuals::GetLaser269NodePoint(const Plant* p,const char* path,float& x,float& y) const
{
    if(!mImpl||!mImpl->laserBall269||!path||!Laser269Main(p))return false;
    int row=0;if(!GetNativeLaserConnections269BallPoint(p,x,y,row))return false;
    FusionAnimationDrawOptions options;options.useBoardBasis=true;options.boardBasis={float(kFusionBasePixelsX),0,0,-float(GetNativeFusionPixelsY(p->mBoard))};
    float sx=0,sy=0;if(!mImpl->laserBall269->TryGetNodeOffset(path,sx,sy,nullptr,0,nullptr,-1,1,&options))return false;
    x+=options.boardBasis[0]*sx+options.boardBasis[2]*sy;y+=options.boardBasis[1]*sx+options.boardBasis[3]*sy;return std::isfinite(x)&&std::isfinite(y);
}
bool NativeVisuals::GetLaser269NodeActivity(const Plant* p,const char* path,bool& active) const
{if(!mImpl||!mImpl->laserBall269||!path||!Laser269Main(p)){active=false;return false;}return mImpl->laserBall269->TryGetNodeActivity(path,active,nullptr,0);}
bool NativeVisuals::DrawLaser269(Sexy::Graphics* g,Plant* p,int sortingRow) const
{
    if(!mImpl||!g||!p)return false;const auto* d=GetNativePlantData(p);if(!d||d->kind!=937)return false;
    const auto* state=GetNativeWorld269PlantState(p);if(!state||mImpl->Held(p))return true;
    if(state->connected&&!Laser269Main(p))return true;
    if(state->connected)
    {
        float bx=0,by=0;int row=0;if(!mImpl->laserBall269||!GetNativeLaserConnections269BallPoint(p,bx,by,row))return true;
        if(sortingRow<0||sortingRow==row)
        {
            for(unsigned endpoint=0;endpoint<2;++endpoint)
            {Plant* member=p;if(endpoint){member=nullptr;while(p->mBoard->IteratePlants(member))if(!member->mDead&&member->mNetId==state->connectedPlant.id){const auto* md=GetNativePlantData(member);if(md&&md->instanceToken==state->connectedPlant.token)break;}}
             float x=0,y=0;if(member&&GetNativeSourcePlantNodePoint(member,"fightball",x,y))DrawLaser269Ribbon(g,p->mBoard,bx,by,x,y,float(p->mX),float(p->mY));}
            FusionAnimationDrawOptions options;options.useBoardBasis=true;options.boardBasis={float(kFusionBasePixelsX),0,0,-float(GetNativeFusionPixelsY(p->mBoard))};
            mImpl->laserBall269->DrawPose(g,bx-p->mX,by-p->mY,1,nullptr,0,nullptr,-1,1,&options);
        }
    }
    for(unsigned i=0;i<8;++i){float x=0,y=0,tx=0,ty=0;int row=0;if(GetNativeLaser269Segment(p,i,x,y,tx,ty,row)&&(sortingRow<0||sortingRow==row))DrawLaser269Ribbon(g,p->mBoard,x,y,tx,ty,float(p->mX),float(p->mY));}
    return true;
}
bool EnsureNativeLaser269Visuals(Board* b,std::string* error)
{auto* c=GetNativeContext(b);std::string failure;const bool ready=c&&c->visuals&&c->visuals->EnsureLaser269(failure);if(error)*error=failure;return ready;}
bool DrawNativeLaser269Visuals(Sexy::Graphics* g,Plant* p,int sortingRow)
{const auto* c=GetNativeContext(p?p->mBoard:nullptr);return c&&c->visuals&&c->visuals->DrawLaser269(g,p,sortingRow);}
bool GetNativeLaser269VisualRow(const Plant* p,unsigned component,int& row)
{
    const auto* d=GetNativePlantData(p);const auto* state=GetNativeWorld269PlantState(p);
    if(!p||p->mDead||!d||d->kind!=937||!state||component>8||(state->connected&&!Laser269Main(p)))return false;
    const auto* context=GetNativeContext(p->mBoard);if(!context)return false;
    for(const auto& hold:context->gloveHolds)if(hold.plantId==p->mNetId&&hold.instanceToken==d->instanceToken)return false;
    float x=0,y=0,tx=0,ty=0;
    return component==0?GetNativeLaserConnections269BallPoint(p,x,y,row):GetNativeLaser269Segment(p,component-1,x,y,tx,ty,row);
}
bool GetNativeLaser269NodePoint(const Plant* p,const char* path,float& x,float& y)
{const auto* c=GetNativeContext(p?p->mBoard:nullptr);return c&&c->visuals&&c->visuals->GetLaser269NodePoint(p,path,x,y);}
uint16_t GetNativeEmbedded269DerivedPlantModelKey(const Plant* p){return Laser269Main(p)?4937:0;}
bool GetNativeSourceDerivedPlant269NodePoint(const Plant* p,const char* path,float& x,float& y){return GetNativeLaser269NodePoint(p,path,x,y);}
bool GetNativeSourceDerivedPlant269NodeActivity(const Plant* p,const char* path,bool& active)
{const auto* c=GetNativeContext(p?p->mBoard:nullptr);return c&&c->visuals&&c->visuals->GetLaser269NodeActivity(p,path,active);}

// Same resource cache/native Tod leases as embedded plant particles. This
// adapter verifies every actual198/199 component before allowing LoadAnimation.
bool NativeVisuals::EnsureWorldRemainderAux(NativeWorldRemainder269Family family,uint8_t type,std::string& error)
{
 const int key=WorldAuxVisualKey(family,type);if(!mImpl||key<0){error="unsupported source world AUX model";return false;}
 if(mImpl->worldAux.contains(key)){error.clear();return true;}
 try
 {
  const auto path=mImpl->root/WorldAuxPackage(key)/"animation.json";const auto source=ReadPackage(path);
  if(source.at("sourcePrefab")!=WorldAuxSource(key))throw std::runtime_error("source world AUX prefab differs");
  std::vector<int64_t> proof;for(const auto& c:source.at("nonRenderingComponents"))
   if(c.at("typeId")==198||c.at("typeId")==199)proof.push_back(c.at("fileId").get<int64_t>());
  if(key==0&&!proof.empty())throw std::runtime_error("unexpected LourFly particle component");
  if(key!=0)
  {
   if(proof.size()!=(key==47?4u:2u))throw std::runtime_error("source Meteorite particle component count differs");
   if(!EnsureNativeEmbedded269ProjectileModel(mImpl->app->mBoard,uint16_t(4000+key),path.string(),&error))return false;
  }
  auto model=std::make_unique<FusionAnimation>();LoadAnimation(*model,mImpl->app,path,proof);
  if(!model->IsStatic())throw std::runtime_error("source world AUX unexpectedly has an Animator");
  float x=0,y=0;if(key==0&&!model->TryGetNodeOffset("Pos",x,y,nullptr,0))throw std::runtime_error("LourFly sourcePos node missing");
  mImpl->worldAux.emplace(key,std::move(model));error.clear();return true;
 }catch(const std::exception& ex){error=ex.what();return false;}
}
bool NativeVisuals::DrawWorldRemainderAux(Sexy::Graphics* g,Projectile* p) const
{
 const auto* state=GetNativeWorldRemainder269AuxState(p);if(!state)return false;
 if(!mImpl||!g||p->mDead)return true;const auto found=mImpl->worldAux.find(WorldAuxVisualKey(state->family,state->type));
 if(found==mImpl->worldAux.end())return true;auto options=CompanionOptions(p);options.extraStates.clear();
 found->second->DrawPose(g,p->mPosX+20-p->mX,p->mPosY+p->mPosZ+20-p->mY,1,nullptr,0,nullptr,-1,1,&options);return true;
}
bool NativeVisuals::GetWorldRemainderNodePoint(const Projectile* p,const char* node,float& x,float& y) const
{
 const auto* state=GetNativeWorldRemainder269AuxState(p);if(!mImpl||!state||!node||p->mDead)return false;
 const auto found=mImpl->worldAux.find(WorldAuxVisualKey(state->family,state->type));if(found==mImpl->worldAux.end())return false;
 auto options=CompanionOptions(p);options.extraStates.clear();float sx=0,sy=0;
 if(!found->second->TryGetNodeOffset(node,sx,sy,nullptr,0,nullptr,-1,1,&options))return false;const auto& b=options.boardBasis;
 x=p->mPosX+20+b[0]*sx+b[2]*sy;y=p->mPosY+p->mPosZ+20+b[1]*sx+b[3]*sy;return std::isfinite(x)&&std::isfinite(y);
}
bool NativeVisuals::GetWorldRemainderNodeActivity(const Projectile* p,const char* node,bool& active) const
{
 const auto* state=GetNativeWorldRemainder269AuxState(p);if(!mImpl||!state||!node||p->mDead)return false;
 const auto found=mImpl->worldAux.find(WorldAuxVisualKey(state->family,state->type));if(found==mImpl->worldAux.end())return false;
 return found->second->TryGetNodeActivity(node,active,nullptr,0);
}
bool EnsureNativeWorldRemainder269AuxVisual(Board* b,NativeWorldRemainder269Family family,uint8_t type,std::string* error)
{auto* c=GetNativeContext(b);std::string failure;const bool okay=c&&c->visuals&&c->visuals->EnsureWorldRemainderAux(family,type,failure);
 if(!okay)LawnNet::NetTrace("Fusion World269 AUX resource unavailable family=%d type=%u error=%s",int(family),type,failure.c_str());
 if(error)*error=failure;return okay;}
bool DrawNativeWorldRemainder269Aux(Sexy::Graphics* g,Projectile* p)
{const auto* c=GetNativeContext(p?p->mBoard:nullptr);return c&&c->visuals&&c->visuals->DrawWorldRemainderAux(g,p);}
bool GetNativeWorldRemainder269AuxNodePoint(const Projectile* p,const char* node,float& x,float& y)
{const auto* c=GetNativeContext(p?p->mBoard:nullptr);return c&&c->visuals&&c->visuals->GetWorldRemainderNodePoint(p,node,x,y);}
uint16_t GetNativeEmbedded269ProjectileModelKey(const Projectile* p)
{if(const auto* s=GetNativeWorldRemainder269AuxState(p)){const int key=WorldAuxVisualKey(s->family,s->type);return key>0?uint16_t(4000+key):0;}
 const auto* d=GetNativeProjectileData(p);return d?EmbeddedProjectileModelKey(d->kind):0;}
bool NativeVisuals::GetSourceProjectileNodePoint(const Projectile* p,const char* node,float& x,float& y) const
{
 if(GetNativeWorldRemainder269AuxState(p))return GetWorldRemainderNodePoint(p,node,x,y);
 const auto* d=GetNativeProjectileData(p);if(!mImpl||!d||!EmbeddedProjectileModelKey(d->kind)||!node||p->mDead)return false;
 const auto found=mImpl->projectiles.find(d->kind);if(found==mImpl->projectiles.end())return false;
 const auto& visual=*found->second;auto options=visual.options;
 options.useBoardBasis=true;options.boardBasis={kFieldPixelsX,0,0,-float(GetNativeFusionPixelsY(p->mBoard))};
 options.nodeRotations.push_back({"",p->mRotation,true});float sx=0,sy=0;
 if(!visual.animation.TryGetNodeOffset(node,sx,sy,visual.state,0,nullptr,-1,1,&options))return false;
 const auto& basis=options.boardBasis;x=p->mPosX+20+basis[0]*sx+basis[2]*sy;
 y=p->mPosY+p->mPosZ+20+basis[1]*sx+basis[3]*sy;return std::isfinite(x)&&std::isfinite(y);
}
bool NativeVisuals::GetSourceProjectileNodeActivity(const Projectile* p,const char* node,bool& active) const
{
 if(GetNativeWorldRemainder269AuxState(p))return GetWorldRemainderNodeActivity(p,node,active);
 const auto* d=GetNativeProjectileData(p);if(!mImpl||!d||!EmbeddedProjectileModelKey(d->kind)||!node||p->mDead)return false;
 const auto found=mImpl->projectiles.find(d->kind);if(found==mImpl->projectiles.end())return false;
 return found->second->animation.TryGetNodeActivity(node,active,found->second->state,0);
}
bool GetNativeSourceProjectile269NodePoint(const Projectile* p,const char* node,float& x,float& y)
{const auto* c=GetNativeContext(p?p->mBoard:nullptr);return c&&c->visuals&&c->visuals->GetSourceProjectileNodePoint(p,node,x,y);}
bool GetNativeSourceProjectile269NodeActivity(const Projectile* p,const char* node,bool& active)
{const auto* c=GetNativeContext(p?p->mBoard:nullptr);return c&&c->visuals&&c->visuals->GetSourceProjectileNodeActivity(p,node,active);}

namespace
{
bool Source91Box(const Projectile* p,float& x,float& y,float& ux,float& uy,float& vx,float& vy)
{
 const auto* d=GetNativeProjectileData(p);
 if(!p||!d||d->kind!=91||!IsNativeFusionBoard(p->mBoard)||p->mDead||!std::isfinite(p->mRotation))return false;
 // Expanded CactusBullet_lour root BoxCollider2D; Awake does not change scale .5.
 const float px=kFieldPixelsX,py=float(GetNativeFusionPixelsY(p->mBoard));
 const float c=std::cos(p->mRotation),s=std::sin(p->mRotation);
 constexpr float ox=-.32343602f*.5f,oy=-.0125605315f*.5f;
 constexpr float hw=.80531013f*.25f,hh=.30916628f*.25f;
 x=p->mPosX+20+(c*ox-s*oy)*px;y=p->mPosY+p->mPosZ+20-(s*ox+c*oy)*py;
 ux=c*hw*px;uy=-s*hw*py;vx=-s*hh*px;vy=-c*hh*py;
 return std::isfinite(x)&&std::isfinite(y);
}
}
bool GetNativeWorldRemainder269Source91Rect(const Projectile* p,Sexy::Rect& out)
{
 float x,y,ux,uy,vx,vy;if(!Source91Box(p,x,y,ux,uy,vx,vy))return false;
 const float hx=std::abs(ux)+std::abs(vx),hy=std::abs(uy)+std::abs(vy);
 const int left=int(std::floor(x-hx)),top=int(std::floor(y-hy));
 out=Sexy::Rect(left,top,int(std::ceil(x+hx))-left,int(std::ceil(y+hy))-top);return true;
}
bool NativeWorldRemainder269Source91Touches(const Projectile* p,const Sexy::Rect& rect)
{
 float x,y,ux,uy,vx,vy;if(rect.mWidth<=0||rect.mHeight<=0||!Source91Box(p,x,y,ux,uy,vx,vy))return false;
 const float tx=rect.mX+rect.mWidth*.5f,ty=rect.mY+rect.mHeight*.5f;
 // The anisotropic Board projection makes a parallelogram: test native axes
 // and normals of both projected source edges, rather than a stock SPIKE box.
 const std::array<std::array<float,2>,4> axes={{{1,0},{0,1},{-uy,ux},{-vy,vx}}};
 for(const auto& axis:axes)
 {
  const float ax=axis[0],ay=axis[1];
  const float sourceRadius=std::abs(ax*ux+ay*uy)+std::abs(ax*vx+ay*vy);
  const float targetRadius=std::abs(ax)*rect.mWidth*.5f+std::abs(ay)*rect.mHeight*.5f;
  if(std::abs(ax*(tx-x)+ay*(ty-y))>sourceRadius+targetRadius)return false;
 }
 return true;
}

bool NativeVisuals::Ensure(std::span<const uint16_t> plantKinds, std::span<const uint16_t> projectileKinds, std::string& error)
{
    const auto started = std::chrono::steady_clock::now();
    std::string stage;
    try
    {
        if (!mImpl || !mImpl->app) throw std::runtime_error("native Fusion visual index is unavailable");
        const auto needs = CollectVisualNeeds(plantKinds,projectileKinds);
        Impl pending;
        for (auto kind : needs.plants)
            if (const auto* package = PlantPackageFor(kind))
            {
                if (!mImpl->plants.contains(kind))
                {
                    stage = "plant kind="+std::to_string(kind)+": ";
                    pending.plants.emplace(kind,Impl::LoadPlant(mImpl->app,mImpl->root,*package));
                }
                if (!mImpl->portraits.contains(kind))
                {
                    stage = "portrait kind="+std::to_string(kind)+": ";
                    pending.portraits.emplace(kind,Impl::BakePortrait(mImpl->app,mImpl->root,*package));
                }
            }
        for (auto kind : needs.projectiles)
            if (const auto* package = ProjectilePackageFor(kind))
                if (!mImpl->projectiles.contains(kind))
                {
                    stage = "projectile kind="+std::to_string(kind)+": ";
                    pending.projectiles.emplace(kind,Impl::LoadProjectile(mImpl->app,mImpl->root,*package));
                }
        if(needs.plants.contains(937))
        {stage="Laser937 external ball: ";if(!EnsureLaser269(error))throw std::runtime_error(error);}
        const bool changed = !pending.plants.empty() || !pending.projectiles.empty() || !pending.portraits.empty();
        // All objects and map nodes are allocated and validated before these
        // integer-key merges. No partial dependency bundle is published on error.
        mImpl->plants.merge(pending.plants);
        mImpl->projectiles.merge(pending.projectiles);
        mImpl->portraits.merge(pending.portraits);
        if (changed) TraceVisualResidency(mImpl->app,mImpl->plants.size(),mImpl->projectiles.size(),mImpl->portraits.size());
        try
        {
            for (auto kind : plantKinds)
                if (changed || (mImpl->prewarmReady.contains(kind) && mImpl->prewarmReported.insert(kind).second))
                    LawnNet::NetTrace("Fusion native visual admission kind=%u prepared=%d cold=%d ms=%.3f",kind,
                        mImpl->prewarmReady.contains(kind)?1:0,changed?1:0,
                        std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
        }
        catch (...) { /* Diagnostics cannot veto the validated resource commit. */ }
        error.clear();
        return true;
    }
    catch (const std::exception& exception)
    { if (mImpl && mImpl->app) mImpl->app->CleanSharedImages(); error = stage+exception.what(); return false; }
}
bool NativeVisuals::EnsurePlant(uint16_t kind, std::string& error)
{ return Ensure(std::span<const uint16_t>(&kind,1),{},error); }
bool NativeVisuals::EnsureProjectile(uint16_t kind, std::string& error)
{ return Ensure({},std::span<const uint16_t>(&kind,1),error); }
bool NativeVisuals::EnsurePortrait(uint16_t kind, std::string& error)
{
    try
    {
        if (!mImpl || !mImpl->app) throw std::runtime_error("native Fusion visual index is unavailable");
        const auto* package = PlantPackageFor(kind);
        if (!package)
        {
            if (!NativePlantArtwork(kind) || GetNativeSeedForPlant(kind) == SEED_NONE)
                throw std::runtime_error("unregistered native Fusion portrait kind="+std::to_string(kind));
        }
        else if (!mImpl->portraits.contains(kind))
            mImpl->portraits.emplace(kind,Impl::BakePortrait(mImpl->app,mImpl->root,*package));
        error.clear();return true;
    }
    catch (const std::exception& exception)
    { if (mImpl && mImpl->app) mImpl->app->CleanSharedImages(); error = exception.what(); return false; }
}
void NativeVisuals::UpdatePrewarm(Board* board)
{
    if (!mImpl || !board || board->mApp != mImpl->app) return;
    const auto* context = GetNativeContext(board);
    if (!context) return;
    // The texture weak-index lock does not authorize worker-thread app/image operations.
    if (GetCurrentThreadId() != mImpl->resourceThread)
    {
        if (!mImpl->prewarmThreadReported)
            LawnNet::NetTrace("Fusion native visual prewarm skipped: resource thread differs");
        mImpl->prewarmThreadReported = true;
        return;
    }
    // Network/simulation catch-up may call this several times before the next draw.
    if (mImpl->prewarmLastDraw == board->mApp->mDrawCount) return;
    mImpl->prewarmLastDraw = board->mApp->mDrawCount;
    try
    {
        std::set<uint16_t> bank,live;
        const auto addBank = [&](uint16_t kind)
        { if (kind != kFusionNoPlant && GetNativeSeedForPlant(kind) != SEED_NONE) bank.insert(kind); };
        for (const auto& packet : context->selection.State().packets)
            if (!packet.mysteryPending) addBank(packet.kind);
        for (const auto& item : context->packets) addBank(item.second.kind);
        for (const auto& item : context->plants)
            if (auto* plant = board->mPlants.DataArrayTryToGet(item.first))
                if (!plant->mDead && plant->IsOnBoard()) live.insert(item.second.kind);
        if (!mImpl->prewarmPlanned || bank != mImpl->prewarmBank || live != mImpl->prewarmLive)
        {
            mImpl->prewarmPlanned = true;
            mImpl->prewarmBank = bank; mImpl->prewarmLive = live;
            mImpl->prewarmPlants.clear(); mImpl->prewarmProjectiles.clear();
            mImpl->prewarmCandidates.clear(); mImpl->prewarmJobs.clear();
            mImpl->prewarmReady.clear(); mImpl->prewarmNext = 0; mImpl->prewarmMemoryReported = false;
            std::vector<uint16_t> candidates(bank.begin(),bank.end());
            std::set<uint16_t> added = bank;
            size_t results = 0;
            // Direction matters. Only selected donors are available; do not expand a catalog-wide closure.
            for (const auto* existingKinds : {&live,&bank})
                for (size_t i = 0; i < GetFusionRecipeCount() && results < 24; ++i)
                {
                    const auto& recipe = GetFusionRecipes()[i];
                    if (!existingKinds->contains(recipe.existing) || !bank.contains(recipe.used) ||
                        GetNativeSeedForPlant(recipe.result) == SEED_NONE || !added.insert(recipe.result).second) continue;
                    candidates.push_back(recipe.result); ++results;
                }
            std::set<std::pair<uint8_t,uint16_t>> jobs;
            const auto addJob = [&](uint8_t type,uint16_t kind)
            { if (jobs.emplace(type,kind).second) mImpl->prewarmJobs.emplace_back(type,kind); };
            for (auto kind : candidates)
            {
                try
                {
                    auto needs = CollectVisualNeeds(std::span<const uint16_t>(&kind,1),{});
                    mImpl->prewarmPlants.insert(needs.plants.begin(),needs.plants.end());
                    mImpl->prewarmProjectiles.insert(needs.projectiles.begin(),needs.projectiles.end());
                    for (auto child : needs.plants) addJob(0,child);
                    for (auto shot : needs.projectiles) addJob(2,shot);
                    for (auto child : needs.plants) addJob(1,child);
                    if (needs.plants.contains(937)) addJob(3,937);
                    mImpl->prewarmCandidates.emplace_back(kind,std::move(needs));
                }
                catch (const std::exception& exception)
                { LawnNet::NetTrace("Fusion native visual prewarm candidate kind=%u skipped error=%s",kind,exception.what()); }
            }
            LawnNet::NetTrace("Fusion native visual prewarm plan selected=%zu live=%zu candidates=%zu jobs=%zu recipeLimit=24",
                bank.size(),live.size(),mImpl->prewarmCandidates.size(),mImpl->prewarmJobs.size());
        }
        // Never introduce a speculative disk/decode stall during active combat.
        const bool mayLoad = board->mApp->mGameScene == SCENE_LEVEL_INTRO ||
            context->selection.State().phase == FusionBankPhase::Intro || board->mPaused;
        while (mImpl->prewarmNext < mImpl->prewarmJobs.size())
        {
            const auto job = mImpl->prewarmJobs[mImpl->prewarmNext];
            if (mImpl->PrewarmResourceReady(job)) { ++mImpl->prewarmNext; continue; }
            if (!mayLoad) break;
            const auto resident = FusionAnimation::GetTextureCacheStats(mImpl->app).retainedRgbaBytes;
            if (resident >= 192ULL*1024*1024)
            {
                if (!mImpl->prewarmMemoryReported)
                    LawnNet::NetTrace("Fusion native visual prewarm deferred residentRgbaBytes=%llu limit=201326592",
                        static_cast<unsigned long long>(resident));
                mImpl->prewarmMemoryReported = true;
                break;
            }
            ++mImpl->prewarmNext;
            const auto started = std::chrono::steady_clock::now();
            std::string failure;
            try
            {
                switch (job.first)
                {
                case 0: mImpl->plants.emplace(job.second,Impl::LoadPlant(mImpl->app,mImpl->root,*PlantPackageFor(job.second))); break;
                case 1: mImpl->portraits.emplace(job.second,Impl::BakePortrait(mImpl->app,mImpl->root,*PlantPackageFor(job.second))); break;
                case 2: mImpl->projectiles.emplace(job.second,Impl::LoadProjectile(mImpl->app,mImpl->root,*ProjectilePackageFor(job.second))); break;
                case 3: if (!EnsureLaser269(failure)) throw std::runtime_error(failure); break;
                }
            }
            catch (const std::exception& exception)
            {
                failure = exception.what();
                mImpl->app->CleanSharedImages();
            }
            LawnNet::NetTrace("Fusion native visual prewarm resource type=%u kind=%u ok=%d ms=%.3f error=%s",job.first,job.second,
                failure.empty()?1:0,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count(),failure.c_str());
            break; // A whole validated resource is indivisible; no second object in this frame.
        }
        for (const auto& candidate : mImpl->prewarmCandidates)
        {
            if (mImpl->prewarmReady.contains(candidate.first)) continue;
            bool ready = true;
            for (auto kind : candidate.second.plants)
                ready = ready && mImpl->PrewarmResourceReady({0,kind}) && mImpl->PrewarmResourceReady({1,kind});
            for (auto kind : candidate.second.projectiles) ready = ready && mImpl->PrewarmResourceReady({2,kind});
            if (candidate.second.plants.contains(937)) ready = ready && bool(mImpl->laserBall269);
            if (!ready) continue;
            mImpl->prewarmReady.insert(candidate.first);
            LawnNet::NetTrace("Fusion native visual prewarm ready kind=%u plantModels=%zu projectileModels=%zu",
                candidate.first,candidate.second.plants.size(),candidate.second.projectiles.size());
        }
    }
    catch (const std::exception& exception)
    { LawnNet::NetTrace("Fusion native visual prewarm deferred error=%s",exception.what()); }
}

void NativeVisuals::ReleaseUnused(Board* board)
{
    if (!mImpl || !board) return;
    const auto* context = GetNativeContext(board);
    if (!context) return;
    try
    {
        std::vector<uint16_t> plants,projectiles;
        for (const auto& item : context->plants)
            if (board->mPlants.DataArrayTryToGet(item.first)) plants.push_back(item.second.kind);
        for (const auto& item : context->projectiles)
            if (board->mProjectiles.DataArrayTryToGet(item.first)) projectiles.push_back(item.second.kind);
        for(const auto& item:context->rollingPlants269)if(board->mPlants.DataArrayTryToGet(item.first))plants.push_back(item.second.factoryKind);
        for(const auto& item:context->droppedCards269)if(board->mCoins.DataArrayTryToGet(item.first))plants.push_back(item.second.kind);
        // Selected cards can become cursor previews without another world
        // allocation. Keep the same bank demand that admission preloads.
        for (const auto& item : context->packets)
            if (item.second.kind != kFusionNoPlant) plants.push_back(item.second.kind);
        auto needs = CollectVisualNeeds(plants,projectiles);
        needs.plants.insert(mImpl->prewarmPlants.begin(),mImpl->prewarmPlants.end());
        needs.projectiles.insert(mImpl->prewarmProjectiles.begin(),mImpl->prewarmProjectiles.end());
        std::set<uint16_t> portraits = needs.plants;
        for (const auto& package : plantPackages) if (IsNativeBasePlantKind(package.kind)) portraits.insert(package.kind);
        for (const auto& item : context->packets)
            if (item.second.kind != kFusionNoPlant) portraits.insert(item.second.kind);
        bool changed = false;
        for (auto it = mImpl->plants.begin(); it != mImpl->plants.end();)
            if (!needs.plants.contains(it->first)) { it = mImpl->plants.erase(it); changed = true; } else ++it;
        for (auto it = mImpl->projectiles.begin(); it != mImpl->projectiles.end();)
            if (!needs.projectiles.contains(it->first)) { it = mImpl->projectiles.erase(it); changed = true; } else ++it;
        for (auto it = mImpl->portraits.begin(); it != mImpl->portraits.end();)
            if (!portraits.contains(it->first)) { it = mImpl->portraits.erase(it); changed = true; } else ++it;
        for(auto it=mImpl->attachedClocks.begin();it!=mImpl->attachedClocks.end();)
            if(!board->mProjectiles.DataArrayTryToGet(it->first))it=mImpl->attachedClocks.erase(it);else ++it;
        for(auto it=mImpl->grabAnchors.begin();it!=mImpl->grabAnchors.end();)
            if(!board->mPlants.DataArrayTryToGet(it->first))it=mImpl->grabAnchors.erase(it);else ++it;
        if (changed)
        {
            mImpl->app->CleanSharedImages();
            TraceVisualResidency(mImpl->app,mImpl->plants.size(),mImpl->projectiles.size(),mImpl->portraits.size());
        }
    }
    catch (const std::exception& exception)
    { LawnNet::NetTrace("Fusion native visual release retained resources error=%s",exception.what()); }
}

bool NativeVisuals::Impl::BuildPlantPose(Plant* plant,const PlantVisual& visual,PlantPose& out)
{
    const auto* data=GetNativePlantData(plant);
    if(!data||!std::isfinite(data->idleSeconds)||!std::isfinite(data->shootSeconds))return false;
    const auto* options = &visual.options;
    if (data->kind == kNutShooter) options = &visual.damageOptions[FusionNutShooterDamageStage(plant->mPlantHealth)];
    else if (data->kind == kSunNut || data->kind == kCherryNut || data->kind == kPotatoNut)
        options = &visual.damageOptions[FusionSunNutStage(plant->mPlantHealth)];
    else if ((data->kind == 1012 || FindFusionStrongChomperProfile(data->kind)) && data->damageStage <= 2)
        options = &visual.damageOptions[data->damageStage];
    else if ((FusionDefenderHasDamageStages(data->kind) || data->kind == 29) && data->damageStage <= 2)
        options = &visual.damageOptions[data->damageStage];
    const char* baseState = visual.baseState.c_str();
    const char* actionState = visual.actionState.c_str();
    float baseSeconds = data->idleSeconds, actionSeconds = data->shootSeconds;
    if (const auto* profile = GetNativeLobberTimingProfile(plant)) actionState = profile->visualState;
    if (GetNativeChomperVisualPose(plant,baseState,baseSeconds))
    {
        actionState = nullptr;
        actionSeconds = -1.0f;
    }
    GetNativeShooterBatchBaseVisualPose(plant,baseState,baseSeconds);
    if(GetNativeEconomyVisualPose(plant,baseState,baseSeconds))
    {actionState=nullptr;actionSeconds=-1.0f;}
    const char* batchState = nullptr; float batchSeconds = 0.0f;
    if (GetNativeSeaLantern267VisualPose(plant,batchState,batchSeconds) ||
        GetNativeUmbrellaBatchVisualPose(plant,batchState,batchSeconds) ||
        GetNativeAction267VisualPose(plant,batchState,batchSeconds) ||
        GetNativeAction266VisualPose(plant,batchState,batchSeconds) ||
        GetNativeShooterBatchVisualPose(plant,batchState,batchSeconds) ||
        GetNativeFireActionVisualPose(plant,batchState,batchSeconds) ||
        GetNativeBatchVisualPose(plant,batchState,batchSeconds) || GetNativeRecipientVisualPose(plant,batchState,batchSeconds))
    {
        if (visual.baseState == batchState)
        {
            baseState = batchState; baseSeconds = batchSeconds;
            actionState = nullptr; actionSeconds = -1.0f;
        }
        else { actionState = batchState; actionSeconds = batchSeconds; }
    }
    if(GetNativeLastPlants269VisualPose(plant,batchState,batchSeconds)||GetNativeMorePlants269VisualPose(plant,batchState,batchSeconds)||GetNativeMore269SquashVisualPose(plant,batchState,batchSeconds)||GetNativeRemainingLeaves269VisualPose(plant,batchState,batchSeconds)||GetNativeRemaining269VisualPose(plant,batchState,batchSeconds)||GetNativeRollingActor269VisualPose(plant,batchState,batchSeconds)||
        GetNativeSummonPlants269VisualPose(plant,batchState,batchSeconds)||GetNativeWorld269VisualPose(plant,batchState,batchSeconds)||
        GetNativeEquipmentPlantVisualPose(plant,batchState,batchSeconds)||
        GetNativeWater268VisualPose(plant,batchState,batchSeconds)||
        GetNativeCannon268VisualPose(plant,batchState,batchSeconds)||
        GetNativeChomper268VisualPose(plant,batchState,batchSeconds)||
        GetNativeDoom269VisualPose(plant,batchState,batchSeconds))
    {baseState=batchState;baseSeconds=batchSeconds;actionState=nullptr;actionSeconds=-1;}
    // Ghosts adopt the source pose clock, not the unsent carrier countdown.
    if(data->kind==1077&&data->batchMode&&data->shootSeconds>=0)
    {baseState=data->batchMode==1?"grow1":"grow2";baseSeconds=data->shootSeconds;actionState=nullptr;actionSeconds=-1;}
    if(data->kind==1007&&actionState&&std::string(actionState)=="shoot")
        baseState=plant->mState==STATE_NOTREADY?"idle":plant->mState==STATE_POTATO_RISING?"rise":"armed";
    if(data->kind==927)
    {
        baseState=data->batchMode==0?"Kaiji":"idle2";
        baseSeconds=data->batchMode==0?data->shootSeconds:data->idleSeconds;
        if(data->batchMode>=2)
        {
            actionState=data->batchMode==3?"shoot_loop":"shoot";
            actionSeconds=data->shootSeconds; // attackSpeed below applies the source tier exactly once.
        }
        else {actionState=nullptr;actionSeconds=-1;}
    }
    FusionAnimationDrawOptions layeredOptions;
    FusionAnimationDrawOptions batchOptions;
    if(IsNativeRemainingLeaves269Plant(plant))
    {
        const int layer=visual.animation.GetStateLayer(baseState);
        if(layer>0)
        {batchOptions=*options;batchOptions.extraStates.push_back({baseState,baseSeconds,layer});options=&batchOptions;
         baseState=visual.baseState.c_str();baseSeconds=data->idleSeconds;actionState=nullptr;actionSeconds=-1;}
    }
    if(IsFusionAction266Rock(data->kind)||data->kind==906)
    {
        batchOptions=*options;
        batchOptions.nodeActivityOverrides.push_back({"SpikeRock_body/Spikerock_bigspike3",data->damageStage<1});
        batchOptions.nodeActivityOverrides.push_back({"SpikeRock_body/Spikerock_bigspike2",data->damageStage<2});
        options=&batchOptions;
    }
    if(data->kind==1170&&data->batchMode==1)
    {
        batchOptions=*options;
        batchOptions.nodeActivityOverrides.push_back({"PuffShroom_head/PuffShroom_head_2",false});
        options=&batchOptions;
    }
    if (data->kind == 1171)
    {
        batchOptions = *options;
        const bool prepared = data->batchMode == 1;
        batchOptions.nodeActivityOverrides.push_back({"ScaredyShroom_head/light1",prepared});
        batchOptions.nodeActivityOverrides.push_back({"ScaredyShroom_head/light2",!prepared});
        options = &batchOptions;
    }
    if (data->kind == 1161)
    {
        float velocity = 0, rate = 1;
        if (!TryGetFusionGatlingMilestone(data->batchShotCount,velocity,rate)) return false;
        batchOptions = *options;
        batchOptions.attackSpeed = rate;
        batchOptions.extraStates.push_back({"leaves",data->idleSeconds,2});
        options = &batchOptions;
    }
    if(data->kind==927)
    {
        batchOptions=*options;
        batchOptions.attackSpeed=data->batchBagCount==0?1.f:data->batchBagCount==1?1.5f:2.f;
        batchOptions.extraStates.push_back({"fire",data->idleSeconds,2});
        options=&batchOptions;
    }
    if(data->kind==12&&!visual.bodySpriteVariants.empty()&&plant->mBoard)
    {
        PlantsOnLawn layers{};plant->mBoard->GetPlantsOnLawn(plant->mPlantCol,plant->mRow,&layers);
        const auto* body=layers.mNormalPlant;const auto* bodyData=GetNativePlantData(body);
        if(body&&bodyData&&!body->mDead&&body->mIsOnBoard&&body->mPlantHealth>0)
            if(const auto variant=visual.bodySpriteVariants.find(bodyData->kind);variant!=visual.bodySpriteVariants.end())
            {batchOptions=*options;batchOptions.spriteReplacements.push_back({"LilyBody",variant->second,1,1,1,false});options=&batchOptions;}
    }
    if (!visual.idleLayers.empty())
    {
        layeredOptions = *options;
        const bool shooting = actionState && actionSeconds >= 0 && std::string(actionState) == "shoot";
        for (const auto& layer : shooting ? visual.shotLayers : visual.idleLayers)
        {
            auto extra = layer;
            if (shooting) extra.elapsedSeconds = actionSeconds;
            else
            {
                // These actual source head defaults loop. Keep the existing
                // additive action-tail semantics untouched for every old caller.
                const float duration = visual.animation.GetStateDurationSeconds(layer.stateName.c_str(),data->animationSpeed);
                extra.elapsedSeconds = duration > 0 ? std::fmod(baseSeconds,duration) : 0.0f;
            }
            layeredOptions.extraStates.push_back(std::move(extra));
        }
        options = &layeredOptions;
    }
    if(IsNativeWater268ImplementedKind(data->kind)||data->kind==915||data->kind==1177||data->kind==1122)
    {
        batchOptions=*options;
        const char* state=nullptr;float seconds=0;
        const auto loopSeconds=[&](const char* name)
        {
            const float duration=visual.animation.GetStateDurationSeconds(name,data->animationSpeed);
            return duration>0?std::fmod(data->idleSeconds,duration):0.f;
        };
        if(GetNativeWater268GrabVisualPose(plant,state,seconds))batchOptions.extraStates.push_back({state,seconds,1});
        if(GetNativeWater268ShootingVisualPose(plant,state,seconds))batchOptions.extraStates.push_back({state,seconds,2});
        if(data->kind==1066)batchOptions.extraStates.push_back({"water",loopSeconds("water"),3});
        if(data->kind==252)batchOptions.extraStates.push_back({"water",loopSeconds("water"),1});
        if(data->kind==1069||data->kind==1122)batchOptions.extraStates.push_back({"fire",loopSeconds("fire"),1});
        if(data->kind==1177)batchOptions.extraStates.push_back({"leaves",loopSeconds("leaves"),2});
        if(GetNativeCannon268SubVisualPose(plant,state,seconds))batchOptions.extraStates.push_back({state,seconds,1});
        if(GetNativeShulk268HeadVisualPose(plant,state,seconds))
            batchOptions.extraStates.push_back({state,std::string(state)=="head1"?loopSeconds(state):seconds,1});
        options=&batchOptions;
    }
    if(data->kind==243)
    {batchOptions=*options;for(const char* node:{"hamburger_0011_1/R1","hamburger_0011_1/R2","hamburger_0011_1/R3"}){bool active=false;if(GetNativeHamburger269NodeActive(plant,node,active))batchOptions.nodeActivityOverrides.push_back({node,active});}options=&batchOptions;}
    if(data->kind==1175)
    {batchOptions=*options;for(unsigned layer:{1u,2u}){const char* state=nullptr;float seconds=0;if(GetNativeRemaining269VisualLayer(plant,layer,state,seconds))batchOptions.extraStates.push_back({state,seconds,int(layer)});}options=&batchOptions;}
    if(IsNativeStar269Plant(plant)||IsNativeSolar269(plant)||IsNativeSummonPlants269(plant)||IsNativeWorld269Plant(plant))
    {
        batchOptions=*options;
        const char* state=nullptr;float seconds=0;
        if(GetNativeStar269VisualPose(plant,state,seconds)||GetNativeSolar269VisualPose(plant,state,seconds)||
            GetNativeSummonPlants269VisualPose(plant,state,seconds)||GetNativeWorld269VisualPose(plant,state,seconds))
        {
            const int layer=visual.animation.GetStateLayer(state);
            if(layer<0)return false;
            baseState=layer==0?state:visual.baseState.c_str();baseSeconds=layer==0?seconds:data->idleSeconds;
            actionState=nullptr;actionSeconds=-1;
            if(layer>0)batchOptions.extraStates.push_back({state,seconds,layer});
        }
        for(const auto& sourceDefault:visual.sourceDefaultLayers)
            if(!std::any_of(batchOptions.extraStates.begin(),batchOptions.extraStates.end(),[&](const auto& extra){return extra.layer==sourceDefault.layer;}))
                batchOptions.extraStates.push_back({sourceDefault.stateName,data->idleSeconds,sourceDefault.layer});
        if(IsNativeSolar269(plant))batchOptions.sortingGroupOverrides.push_back({"back",0,GetNativeSolar269BackOrder(plant)});
        options=&batchOptions;
    }
    const char* worldAdditive=nullptr;float worldAdditiveSeconds=0;
    if(GetNativeWorldRemainder269VisualPose(plant,batchState,batchSeconds,worldAdditive,worldAdditiveSeconds))
    {baseState=batchState;baseSeconds=batchSeconds;actionState=nullptr;actionSeconds=-1;}
    if(GetNativeWorldRemainder269SummonPose(plant,worldAdditive,worldAdditiveSeconds))
    {batchOptions=*options;batchOptions.extraStates.push_back({worldAdditive,worldAdditiveSeconds,1});options=&batchOptions;}
    if(data->kind==1175)
    {float x=0,y=0,rootX=0,rootY=0;if(GetNativeSeaShroom269GrabAnchor(plant,x,y)&&plant->GetShadowCenter(rootX,rootY))
     {batchOptions=*options;rootX+=plant->mShakeOffsetX-visual.shadowX*kNativePixelsPerWorldUnit;rootY+=plant->mShakeOffsetY+visual.shadowY*kNativePixelsPerWorldUnit;
      batchOptions.nodeWorldPositions.push_back({"Grab",(x-rootX)/kNativePixelsPerWorldUnit,-(y-rootY)/kNativePixelsPerWorldUnit});options=&batchOptions;}}
    std::array<NativeHypno269Layer,3> hypnoLayers{};int hypnoCount=0;
    if(GetNativeHypno269VisualLayers(plant,hypnoLayers,hypnoCount))
    {
        if(hypnoCount<0||hypnoCount>int(hypnoLayers.size()))return false;
        batchOptions=*options;
        baseState=visual.baseState.c_str();baseSeconds=data->idleSeconds;actionState=nullptr;actionSeconds=-1;
        for(int i=0;i<hypnoCount;++i)
            batchOptions.extraStates.push_back({hypnoLayers[i].state,hypnoLayers[i].seconds,hypnoLayers[i].layer});
        bool first=false,second=false;
        if(GetNativeHypno269ChargeNodes(plant,first,second))
        {
            batchOptions.nodeActivityOverrides.push_back({"head/Magnetshroom_idle_electric1",first});
            batchOptions.nodeActivityOverrides.push_back({"body/body ec1",second});
        }
        options=&batchOptions;
    }
    if(data->kind==1129)
    {
        batchOptions=*options;
        for(uint8_t stage=0;stage<3;++stage)for(uint8_t form=0;form<3;++form)
            batchOptions.nodeActivityOverrides.push_back({FusionMachine268FormNode(stage,form),stage==data->damageStage&&form==data->batchMode});
        options=&batchOptions;
    }
    const auto generation=plant->mBoard->mPlants.DataArrayGetID(plant);
    const char* grabState=nullptr;float grabSeconds=0;
    if(GetNativeWater268GrabVisualPose(plant,grabState,grabSeconds))
    {
        batchOptions=*options;
        float x=0,y=0;
        const auto previous=grabAnchors.find(generation);
        const bool sameTarget=previous!=grabAnchors.end() &&
            (!data->waterTargetId || (previous->second.target==data->waterTargetId&&previous->second.token==data->waterTargetToken));
        if(!sameTarget && GetNativeWater268GrabAnchor(plant,x,y))
        {
            // Grab's parent is placed once. Only child0 descends thereafter.
            if(data->batchMode==2)y-=float((50-data->pendingProductionTicks)*.01*GetNativeFusionPixelsY(plant->mBoard));
            float rootX=0,rootY=0;
            if(plant->GetShadowCenter(rootX,rootY))
            {
                rootX+=plant->mShakeOffsetX-visual.shadowX*kNativePixelsPerWorldUnit;
                rootY+=plant->mShakeOffsetY+visual.shadowY*kNativePixelsPerWorldUnit;
                GrabAnchor saved{data->waterTargetId,data->waterTargetToken,x-rootX,y-rootY};
                if(auto* target=plant->mBoard->ZombieTryToGet(plant->mTargetZombieID);target&&!target->mDead&&target->mNetId==data->waterTargetId)
                {
                    const auto* context=GetNativeContext(plant->mBoard);
                    const auto localGeneration=uint32_t(plant->mBoard->ZombieGetID(target));
                    const auto token=context->zombieInstanceTokens.find(localGeneration);
                    const uint32_t sourceToken=plant->mApp->NetIsClient()?(token==context->zombieInstanceTokens.end()?0:token->second):localGeneration;
                    if(sourceToken==data->waterTargetToken&&target->mRenderOrder>(std::numeric_limits<int>::min)()&&target->mRenderOrder<(std::numeric_limits<int>::max)()){saved.order=target->mRenderOrder;saved.hasOrder=true;}
                }
                grabAnchors[generation]=saved;
            }
        }
        const auto anchor=grabAnchors.find(generation);
        if(anchor!=grabAnchors.end())
        {
            if(anchor->second.hasOrder)
            {
                for(const auto& path:visual.grabSpritePaths)
                    batchOptions.spriteSortingOverrides.push_back({path,0,anchor->second.order+1});
                batchOptions.maskSortingOverrides.push_back({"Grab/Sprite Mask",0,anchor->second.order-1,0,anchor->second.order+1});
            }
            // Keep Grab under the real source parent when a glove moves it.
            // Only the independent hand child adds its sink displacement.
            batchOptions.nodeWorldPositions.push_back({"Grab",anchor->second.x/kNativePixelsPerWorldUnit,-anchor->second.y/kNativePixelsPerWorldUnit});
            if(data->batchMode==2)
            {
                float handX=0,handY=0;
                if(visual.animation.TryGetNodeOffset("Grab/grabs",handX,handY,baseState,baseSeconds,actionState,actionSeconds,data->animationSpeed,&batchOptions))
                    batchOptions.nodeWorldPositions.push_back({"Grab/grabs",handX,handY-float((50-data->pendingProductionTicks)*.01*GetNativeFusionPixelsY(plant->mBoard)/kNativePixelsPerWorldUnit)});
            }
        }
        else
            // A late join after target loss carries no original source anchor.
            // Keep this unsupported pose hidden rather than attach it to the plant.
            batchOptions.hiddenNodePaths.push_back("Grab");
        options=&batchOptions;
    }
    else grabAnchors.erase(generation);
    out={baseState,actionState,baseSeconds,actionSeconds,*options};
    return true;
}

bool NativeVisuals::GetPlantNodeTransform(const Plant* source,const char* path,std::array<float,6>& out) const
{
    if(!mImpl||!source||!path)return false;
    auto* plant=const_cast<Plant*>(source);
    const auto* data=GetNativePlantData(plant);
    if(!data)return false;
    const auto found=mImpl->plants.find(data->kind);
    if(found==mImpl->plants.end())return false;
    const auto& visual=*found->second;
    Impl::PlantPose pose;
    if(!mImpl->BuildPlantPose(plant,visual,pose))return false;
    std::array<float,6> sourceTransform;
    float gx=0,gy=0;
    if(!plant->GetShadowCenter(gx,gy)||!visual.animation.TryGetNodeTransform(path,sourceTransform,
        pose.baseState,pose.baseSeconds,pose.actionState,pose.actionSeconds,data->animationSpeed,&pose.options))return false;
    const float k=kNativePixelsPerWorldUnit;
    out={sourceTransform[0]*k,-sourceTransform[1]*k,sourceTransform[2]*k,-sourceTransform[3]*k,
        gx+plant->mShakeOffsetX+(sourceTransform[4]-visual.shadowX)*k,
        gy+plant->mShakeOffsetY-(sourceTransform[5]-visual.shadowY)*k};
    return std::all_of(out.begin(),out.end(),[](float v){return std::isfinite(v);});
}

bool NativeVisuals::GetPlantNodePoint(const Plant* plant,const char* path,float& x,float& y) const
{
    std::array<float,6> transform;
    if(!GetPlantNodeTransform(plant,path,transform))return false;
    x=transform[4];y=transform[5];return true;
}

bool NativeVisuals::GetPlantNodeActivity(const Plant* source,const char* path,bool& active) const
{
    if(!mImpl||!source||!path)return false;auto* plant=const_cast<Plant*>(source);const auto* data=GetNativePlantData(plant);if(!data)return false;
    const auto found=mImpl->plants.find(data->kind);if(found==mImpl->plants.end())return false;Impl::PlantPose pose;
    if(!mImpl->BuildPlantPose(plant,*found->second,pose))return false;
    return found->second->animation.TryGetNodeActivity(path,active,pose.baseState,pose.baseSeconds,pose.actionState,pose.actionSeconds,data->animationSpeed,&pose.options);
}

bool GetNativeSourcePlantNodeTransform(const Plant* plant,const char* path,std::array<float,6>& out)
{
    const auto* context=plant?GetNativeContext(plant->mBoard):nullptr;
    return context&&context->visuals&&context->visuals->GetPlantNodeTransform(plant,path,out);
}
float GetNativeSourcePixelsPerWorldUnit(){return kNativePixelsPerWorldUnit;}

bool GetNativeSourcePlantNodePoint(const Plant* plant,const char* path,float& x,float& y)
{
    const auto* context=plant?GetNativeContext(plant->mBoard):nullptr;
    return context&&context->visuals&&context->visuals->GetPlantNodePoint(plant,path,x,y);
}
bool GetNativeSourcePlantNodeActivity(const Plant* plant,const char* path,bool& active)
{
    const auto* context=plant?GetNativeContext(plant->mBoard):nullptr;
    return context&&context->visuals&&context->visuals->GetPlantNodeActivity(plant,path,active);
}

void NativeVisuals::EndWorldFrame(Board* board)
{
    if(!mImpl||mImpl->frameBoard!=board)return;
    mImpl->frameMaskViews.clear();mImpl->frameMasks.clear();mImpl->framePlants.clear();
    mImpl->frameBoard=nullptr;mImpl->frameDestination=nullptr;mImpl->frameScope=0;
}

bool NativeVisuals::BeginWorldFrame(Sexy::Graphics* graphics,Board* board)
{
    if(!mImpl)return false;
    // Always retire the preceding scope, including an interrupted Board draw.
    mImpl->frameMaskViews.clear();mImpl->frameMasks.clear();mImpl->framePlants.clear();
    mImpl->frameBoard=nullptr;mImpl->frameDestination=nullptr;mImpl->frameScope=0;
    if(!graphics||!board||!IsNativeFusionBoard(board))return false;
    if(++mImpl->frameSerial==0)++mImpl->frameSerial;
    const uint64_t scope=mImpl->frameSerial;
    Plant* plant=nullptr;
    while(board->IteratePlants(plant))
    {
        if(plant->mDead||!plant->mVisible||plant->mPlantHealth<=0||!plant->IsOnBoard()||mImpl->Held(plant)||
           plant->mOnBungeeState!=PlantOnBungeeState::NOT_ON_BUNGEE)continue;
        const auto* data=GetNativePlantData(plant);if(!data)continue;
        const auto found=mImpl->plants.find(data->kind);if(found==mImpl->plants.end())continue;
        const auto& visual=*found->second;Impl::FramePlant entry;
        if(!plant->GetShadowCenter(entry.rootX,entry.rootY)||!mImpl->BuildPlantPose(plant,visual,entry.pose))continue;
        entry.rootX+=plant->mShakeOffsetX-visual.shadowX*kNativePixelsPerWorldUnit;
        entry.rootY+=plant->mShakeOffsetY+visual.shadowY*kNativePixelsPerWorldUnit;
        entry.pose.options.maskScope=scope;
        if(visual.animation.HasMasks()&&!visual.animation.CollectMasks(graphics,entry.rootX,entry.rootY,kNativePixelsPerWorldUnit/100.f,
            entry.pose.baseState,entry.pose.baseSeconds,entry.pose.actionState,entry.pose.actionSeconds,data->animationSpeed,&entry.pose.options,mImpl->frameMasks))
        {
            mImpl->frameMasks.clear();mImpl->framePlants.clear();return false;
        }
        const auto generation=uint32_t(board->mPlants.DataArrayGetID(plant));
        const auto anchor=mImpl->grabAnchors.find(generation);const char* state=nullptr;float seconds=0;
        entry.overlay=!visual.grabSpritePaths.empty()&&anchor!=mImpl->grabAnchors.end()&&anchor->second.hasOrder&&
            GetNativeWater268GrabVisualPose(plant,state,seconds);
        if(entry.overlay)entry.order=anchor->second.order+1;
        if(data->kind==937)
        {
            const auto* laser=GetNativeWorld269PlantState(plant);int row=plant->mRow;float x=0,y=0;
            const bool connected=laser&&laser->connected;
            entry.laser=laser&&(!connected||Laser269Main(plant));
            if(entry.laser)
            {if(connected)GetNativeLaserConnections269BallPoint(plant,x,y,row);
             for(unsigned i=0;i<8;++i){float tx=0,ty=0;int targetRow=0;if(GetNativeLaser269Segment(plant,i,x,y,tx,ty,targetRow))row=(std::max)(row,targetRow);}
             entry.overlay=true;entry.order=Board::MakeRenderOrder(RENDER_LAYER_PARTICLE,row,0);}
        }
        mImpl->framePlants.emplace(generation,std::move(entry));
    }
    for(const auto& mask:mImpl->frameMasks)mImpl->frameMaskViews.push_back(mask.View());
    mImpl->frameBoard=board;mImpl->frameDestination=graphics->mDestImage;mImpl->frameScope=scope;
    return true;
}

bool NativeVisuals::PlantOverlayOrder(Plant* plant,int& order) const
{
    if(!mImpl||!plant||mImpl->frameBoard!=plant->mBoard||mImpl->Held(plant))return false;
    // Real magnets retain their existing native item pass unchanged.
    if(plant->mSeedType==SEED_MAGNETSHROOM||plant->mSeedType==SEED_GOLD_MAGNET)return false;
    const auto found=mImpl->framePlants.find(uint32_t(plant->mBoard->mPlants.DataArrayGetID(plant)));
    if(found==mImpl->framePlants.end()||!found->second.overlay)return false;
    order=found->second.order;return true;
}

void NativeVisuals::ConfirmPlantOverlayQueued(Plant* plant)
{
    int order=0;if(!PlantOverlayOrder(plant,order))return;
    mImpl->framePlants.at(uint32_t(plant->mBoard->mPlants.DataArrayGetID(plant))).overlayQueued=true;
}

bool NativeVisuals::DrawPlantOverlay(Sexy::Graphics* graphics,Plant* plant) const
{
    int order=0;if(!graphics||!PlantOverlayOrder(plant,order)||graphics->mDestImage!=mImpl->frameDestination)return false;
    const auto generation=uint32_t(plant->mBoard->mPlants.DataArrayGetID(plant));const auto& entry=mImpl->framePlants.at(generation);
    if(!entry.overlayQueued)return false;
    if(entry.laser)return DrawLaser269(graphics,plant);
    const auto* data=GetNativePlantData(plant);if(!data)return true;
    const auto visual=mImpl->plants.find(data->kind);if(visual==mImpl->plants.end())return true;
    auto options=entry.pose.options;options.useWorldMaskScope=true;options.maskScope=mImpl->frameScope;
    options.worldMasks=mImpl->frameMaskViews;options.spritePathPrefix="Grab";
    visual->second->animation.DrawPose(graphics,entry.rootX-plant->mX,entry.rootY-plant->mY,kNativePixelsPerWorldUnit/100.f,
        entry.pose.baseState,entry.pose.baseSeconds,entry.pose.actionState,entry.pose.actionSeconds,data->animationSpeed,&options);
    return true;
}

bool BeginNativeFusionWorldFrame(Sexy::Graphics* graphics,Board* board)
{
    auto* context=GetNativeContext(board);return context&&context->visuals&&context->visuals->BeginWorldFrame(graphics,board);
}
void EndNativeFusionWorldFrame(Board* board)
{
    auto* context=GetNativeContext(board);if(context&&context->visuals)context->visuals->EndWorldFrame(board);
}
bool GetNativeFusionPlantOverlayOrder(Plant* plant,int& order)
{
    auto* context=plant?GetNativeContext(plant->mBoard):nullptr;
    return context&&context->visuals&&context->visuals->PlantOverlayOrder(plant,order);
}
void ConfirmNativeFusionPlantOverlayQueued(Plant* plant)
{
    auto* context=plant?GetNativeContext(plant->mBoard):nullptr;
    if(context&&context->visuals)context->visuals->ConfirmPlantOverlayQueued(plant);
}
bool DrawNativeFusionPlantOverlay(Sexy::Graphics* graphics,Plant* plant)
{
    auto* context=plant?GetNativeContext(plant->mBoard):nullptr;
    return context&&context->visuals&&context->visuals->DrawPlantOverlay(graphics,plant);
}

bool NativePlantVisualStateSupported(const Plant* plant,std::string* error)
{
    const auto* data=GetNativePlantData(plant);bool okay=true;const char* why="";
    if(data&&data->kind==1104&&GetNativeFireLightLevel(plant)!=0)
    {okay=false;why="source1104 visual admission is ordinary-light0 only; light/Meteorite particle branches pending";}
    if(data&&data->kind==1089&&data->damageStage!=0)
    {okay=false;why="source1089 visual admission is damage-stage0 only; active damage-dust branches pending";}
    if(error)*error=why;return okay;
}

bool NativeVisuals::DrawPlant(Sexy::Graphics* graphics, Plant* plant) const
{
    if (!mImpl || !graphics || !plant) return false;
    const NativePlantData* data = GetNativePlantData(plant);
    if (!data) return false;
    std::string stateError;
    if(!NativePlantVisualStateSupported(plant,&stateError))
    {LawnNet::NetTrace("Fusion native visual state refused kind=%u id=%u error=%s",data->kind,plant->mNetId,stateError.c_str());return true;}
    const auto found = mImpl->plants.find(IsNativeRollingActor269(plant)?GetNativeRollingActor269ArtworkKind(plant):data->kind);
    if (found == mImpl->plants.end()) return false;
    const auto& visual = *found->second;
    if (!std::isfinite(data->idleSeconds) || !std::isfinite(data->shootSeconds)) return true;
    // Plant::Draw is called between the real entity's BeginDraw/EndDraw. Keep
    // these coordinates local; do not add Board/camera/entity translation again.
    float groundX = 0.0f, groundY = 0.0f;
    if (!plant->GetShadowCenter(groundX, groundY)) return false;
    plant->DrawShadow(graphics);
    if (plant->IsOnBoard())
        if (auto* pumpkin = plant->mBoard->GetPumpkinAt(plant->mPlantCol,plant->mRow); pumpkin && PumpkinBackOwner(pumpkin) == plant)
        {
            Sexy::Graphics back(*graphics);
            back.mTransX += pumpkin->mX-plant->mX;
            back.mTransY += pumpkin->mY-plant->mY;
            DrawPumpkinBack(&back,pumpkin);
        }
    if (!plant->IsOnBoard() && visual.pumpkin) DrawPumpkinBack(graphics,plant);
    // Native Plant::Draw adds shake to the body after drawing the shadow.
    groundX += plant->mShakeOffsetX-plant->mX;
    groundY += plant->mShakeOffsetY-plant->mY;
    Impl::PlantPose pose;
    const auto frame=mImpl->framePlants.find(uint32_t(plant->mBoard->mPlants.DataArrayGetID(plant)));
    const bool inWorld=mImpl->frameBoard==plant->mBoard&&mImpl->frameDestination==graphics->mDestImage&&
        plant->IsOnBoard()&&!mImpl->Held(plant)&&frame!=mImpl->framePlants.end();
    if(inWorld)
    {
        pose=frame->second.pose;pose.options.useWorldMaskScope=true;pose.options.maskScope=mImpl->frameScope;
        pose.options.worldMasks=mImpl->frameMaskViews;
        if(frame->second.overlayQueued&&!frame->second.laser)pose.options.hiddenNodePaths.push_back("Grab");
    }
    else if(!mImpl->BuildPlantPose(plant,visual,pose))return true;
    if(data->kind==937&&(!inWorld||!frame->second.overlayQueued))DrawLaser269(graphics,plant);
    visual.animation.DrawPose(graphics,
        groundX-visual.shadowX*kNativePixelsPerWorldUnit,
        groundY+visual.shadowY*kNativePixelsPerWorldUnit,
        kNativePixelsPerWorldUnit/100.0f, pose.baseState, pose.baseSeconds,
        pose.actionState, pose.actionSeconds, data->animationSpeed, &pose.options);
    return true;
}

bool NativeVisuals::DrawPumpkinBack(Sexy::Graphics* graphics, Plant* pumpkin) const
{
    if (!mImpl || !graphics || !pumpkin) return false;
    const auto* data = GetNativePlantData(pumpkin);
    const auto found = data ? mImpl->plants.find(data->kind) : mImpl->plants.end();
    if (found == mImpl->plants.end() || !found->second->pumpkin) return false;
    const auto& visual = *found->second;
    if (!std::isfinite(data->idleSeconds) || !std::isfinite(data->animationSpeed)) return true;
    float x = 0, y = 0;
    if (!pumpkin->GetShadowCenter(x,y)) return false;
    x += pumpkin->mShakeOffsetX-pumpkin->mX;
    y += pumpkin->mShakeOffsetY-pumpkin->mY;
    Impl::PlantPose pose;
    if(!mImpl->BuildPlantPose(pumpkin,visual,pose))return true;
    auto options=std::move(pose.options);
    options.hiddenNodePaths.erase(std::remove(options.hiddenNodePaths.begin(),options.hiddenNodePaths.end(),"pumpkin_back"),options.hiddenNodePaths.end());
    options.hiddenNodePaths.insert(options.hiddenNodePaths.end(),visual.pumpkinBackOptions.hiddenNodePaths.begin(),visual.pumpkinBackOptions.hiddenNodePaths.end());
    visual.animation.DrawPose(graphics,x-visual.shadowX*kNativePixelsPerWorldUnit,
        y+visual.shadowY*kNativePixelsPerWorldUnit,kNativePixelsPerWorldUnit/100.0f,
        pose.baseState,pose.baseSeconds,pose.actionState,pose.actionSeconds,data->animationSpeed,&options);
    return true;
}

bool NativeVisuals::DrawProjectile(Sexy::Graphics* graphics, Projectile* projectile) const
{
    if (!mImpl || !graphics || !projectile) return false;
    if(GetNativeWorldRemainder269AuxState(projectile))return DrawWorldRemainderAux(graphics,projectile);
    const auto* data = GetNativeProjectileData(projectile);
    if (!data) return false;
    const bool typedStar=GetNativeStar269ProjectileState(projectile)!=nullptr;
    const bool hotPea=data->kind==0 && data->fireState>0 && data->fireState<=3;
    const auto found = mImpl->projectiles.find(hotPea?uint16_t(24+data->fireState):data->kind);
    if (found == mImpl->projectiles.end()) return false;
    const auto& visual = *found->second;
    float centerX = 0.0f, centerY = 0.0f;
    const auto* lobber = FindFusionLobberProjectileTraits(data->kind);
    const bool planar=(data->kind==58||data->kind==59)&&data->specialType==13;
    const bool squashImage=data->kind==28||data->kind==32||data->kind==36;
    if(IsNativeStar269Projectile(projectile))GetNativeStar269ProjectileCenter(projectile,centerX,centerY);
    else if(data->kind==91||planar||squashImage||IsNativeWater268Projectile(projectile)||IsNativeChomper268Projectile(projectile)||IsNativeCannon268Kernel(projectile))
    {centerX=projectile->mPosX+20;centerY=projectile->mPosY+20;}
    else if(IsNativeCannon268Projectile(projectile)) GetNativeCannon268ProjectileCenter(projectile,centerX,centerY);
    else if (!(IsNativeShooterBatchProjectile(projectile) ? GetNativeShooterBatchProjectileCenter(projectile,centerX,centerY) :
        IsNativeFireActionProjectile(projectile) ? GetNativeFireActionProjectileCenter(projectile,centerX,centerY) :
        lobber ? GetNativeLobberProjectileCenter(projectile, centerX, centerY) :
        data->kind == 2 || data->kind == 4 || data->kind == 5 || data->kind == 6 ? GetNativeRollingProjectileCenter(projectile, centerX, centerY) :
        IsNativeBatchProjectile(projectile) ? GetNativeBatchProjectileCenter(projectile,centerX,centerY) :
        IsNativeRecipientProjectile(projectile) ? GetNativeRecipientProjectileCenter(projectile,centerX,centerY) :
        GetNativeTimedPeaProjectileCenter(projectile, centerX, centerY)))
    {
        // Loaded source bodies share the native center even when their gameplay
        // family has no specialized center query (ordinary Sun/Puff, for example).
        if(!NativeProjectileMetadataValid(*data,projectile->mProjectileType,projectile->mMotionType))return true;
        centerX=projectile->mPosX+20;
        centerY=projectile->mPosY+projectile->mPosZ+20;
        if(!std::isfinite(centerX)||!std::isfinite(centerY))return true;
    }
    if (hotPea)
    {
        auto options=visual.options; options.rootFlipX=data->specialType==1;
        visual.animation.DrawPose(graphics,centerX-projectile->mX,centerY-projectile->mY,
            kNativePixelsPerWorldUnit/100.0f*(data->effectVariant==1?.75f:1.0f),
            visual.state,projectile->mProjectileAge*.01f,nullptr,-1.0f,1.0f,&options);
        return true;
    }
    const float elapsed = data->kind==8?(data->specialType==16?0.f:projectile->mProjectileAge*.01f):data->kind == 12 || data->kind == 4 || data->kind == 5 || data->kind == 6 || data->kind == 20 || data->kind == 35 || data->kind == 87 ? projectile->mProjectileAge*.01f : visual.radiansPerSecond == 0.0f ? 0.0f : projectile->mRotation/visual.radiansPerSecond;
    auto options = visual.options;
    if(data->kind==91)
    {centerY+=projectile->mPosZ;options.useBoardBasis=true;options.boardBasis={kFieldPixelsX,0,0,-float(GetNativeFusionPixelsY(projectile->mBoard))};
     options.nodeRotations.push_back({"",projectile->mRotation,true});}
    if(data->kind==11&&data->effectVariant==1)
        options.spriteReplacements.push_back({"",visual.heatedSprite,1,1,1,false});
    if(data->kind==51)options.nodeRotations.push_back({"",projectile->mRotation,true});
    if(data->kind==8&&data->specialType==16){options.nodeRotations.push_back({"",projectile->mRotation,true});for(const char* node:{"Sun1","Sun2","Sun3"})options.nodeRotations.push_back({node,projectile->mRotationSpeed,false});}
    if(data->kind==38&&!data->effectVariant)options.colorMultiplierRGBA={69,26,0,255}; // Source(.27,.1,0,1).
    if (lobber && lobber->sourceRotation) options.nodeRotations.push_back({"Sprite", projectile->mRotation});
    if(IsNativeCannon268Projectile(projectile))
    {
        options.nodeRotations.push_back({"Sprite",projectile->mRotation,true});
        if(projectile->mRotation==kFusionCannon268DownRotation)
        {
            options.hiddenNodePaths.erase(std::remove(options.hiddenNodePaths.begin(),options.hiddenNodePaths.end(),"Shadow"),options.hiddenNodePaths.end());
            options.nodeActivityOverrides.push_back({"Shadow",true});
        }
    }
    if(data->kind==80) options.nodeRotations.push_back({"Sprite",projectile->mRotation,true});
    if(data->kind==94)
    {
        options.nodeRotations.push_back({"",projectile->mRotation,true});
        options.nodeRotations.push_back({"image",projectile->mRotationSpeed,true});
    }
    if(!typedStar&&(data->kind==41||data->kind==42))
        options.nodeRotations.push_back({"ProjectileStar",projectile->mRotationSpeed,true});
    if(!typedStar&&IsNativeWater268Projectile(projectile)&&data->kind!=29&&data->kind!=32)
        options.nodeRotations.push_back({"",projectile->mRotation,true});
    if(squashImage)
    {
        float x=0,y=0;
        if(visual.animation.TryGetNodeOffset("Image",x,y,visual.state,elapsed,nullptr,-1,1,&options))
            options.nodeWorldPositions.push_back({"Image",x,y-projectile->mPosZ/kNativePixelsPerWorldUnit});
    }
    if(typedStar)
    {
        options.nodeRotations.push_back({"",projectile->mRotation,true});
        options.nodeRotations.push_back({"ProjectileStar",projectile->mRotationSpeed,true});
    }
    else if (data->kind == 39 || data->kind == 40 || data->kind == 46)
    {
        const float rootAngle = std::atan2(-projectile->mVelY/kFieldPixelsY,projectile->mVelX/kFieldPixelsX);
        options.nodeRotations.push_back({"",rootAngle});
        options.nodeRotations.push_back({"ProjectileStar",projectile->mRotation});
    }
    if (data->kind == 102) options.nodeRotations.push_back({"",-projectile->mRotation});
    if (!visual.variantSprites.empty())
    {
        if (data->effectVariant >= visual.variantSprites.size()) return true;
        options.spriteReplacements.push_back({visual.variantNodePath,visual.variantSprites[data->effectVariant],1.0f,1.0f,1.0f,false});
    }
    visual.animation.DrawPose(graphics,
        centerX-projectile->mX,
        centerY-projectile->mY,
        kNativePixelsPerWorldUnit/100.0f*(data->kind == 97 ? 1.7f/1.5062028f : 1.0f), visual.state, elapsed, nullptr, -1.0f, 1.0f, &options);
    const auto generation=projectile->mBoard->mProjectiles.DataArrayGetID(projectile);
    if(data->kind==87&&data->fireState==1)
    {
        const auto effect=mImpl->projectiles.find(88);
        if(effect!=mImpl->projectiles.end())
        {
            const int age=projectile->mProjectileAge;
            auto clock=mImpl->attachedClocks.try_emplace(generation,Impl::AttachedClock{age,age}).first;
            if(age<clock->second.lastAge)clock->second.firstAge=age;
            clock->second.lastAge=age;
            // First observed conversion is a presentation epoch. The native
            // projectile clock freezes with pause; late-join source phase is not serialized.
            const float seconds=(age-clock->second.firstAge)*.01f;
            effect->second->animation.DrawPose(graphics,centerX-projectile->mX,centerY-projectile->mY,
                kNativePixelsPerWorldUnit/100.f,effect->second->state,seconds,nullptr,-1,1,&effect->second->options);
        }
    }
    else mImpl->attachedClocks.erase(generation);
    return true;
}

bool NativeVisuals::DrawPortrait(Sexy::Graphics* graphics, uint16_t kind, const Sexy::Rect& bounds, int grayness, bool preserveTint) const
{
    if (!mImpl || !graphics || bounds.mWidth <= 0 || bounds.mHeight <= 0) return false;
    const auto found = mImpl->portraits.find(kind);
    if (found == mImpl->portraits.end()) return false;
    const int gray = std::clamp(grayness, 0, 255);
    const auto tint = preserveTint && graphics->mColorizeImages ? graphics->mColor : Sexy::Color::White;
    graphics->PushState();
    graphics->ClipRect(bounds);
    graphics->SetColorizeImages(true);
    graphics->SetColor(Sexy::Color(tint.mRed*gray/255, tint.mGreen*gray/255, tint.mBlue*gray/255, tint.mAlpha));
    const int side = (std::min)(bounds.mWidth, bounds.mHeight);
    const Sexy::Rect fitted(bounds.mX+(bounds.mWidth-side)/2, bounds.mY+(bounds.mHeight-side)/2, side, side);
    graphics->DrawImage(found->second.get(), fitted, Sexy::Rect(0, 0, kPortraitSize, kPortraitSize));
    graphics->PopState();
    return true;
}

void NativeVisuals::ReconcileField(Board* board)
{
    if(!mImpl||!board||!board->mApp||board->mApp!=mImpl->app||board->mApp->mBoard!=board)return;
    const auto* spec=SourceFieldSpec277(board);
    if(!spec)
    {ReconcileNativeField277Particle(board,0,false,0,0,0);return;}
    const uint8_t key=uint8_t(board->mBackground);
    if(mImpl->fieldFailures.contains(key))
    {ReconcileNativeField277Particle(board,0,false,0,0,0);return;}
    auto found=mImpl->fieldBiomes.find(key);
    // Native resources finish loading after NativeContext construction. This
    // is a pending state, not a permanent source-resource failure.
    if(found==mImpl->fieldBiomes.end()&&!(spec->scene==1?Sexy::IMAGE_BACKGROUND2:Sexy::IMAGE_BACKGROUND3))
    {ReconcileNativeField277Particle(board,0,false,0,0,0);return;}
    if(found==mImpl->fieldBiomes.end())
    {
    try
    {
        const auto path=PackageRoot()/spec->package/"animation.json";
        const Json package=ReadPackage(path);std::vector<int64_t> proof;std::string error;
        if(!EnsureNativeField277Resources(board,spec->scene,path.string(),proof,error))throw std::runtime_error(error);
        FusionAnimation background;LoadAnimation(background,board->mApp,path,proof);
        const auto& nodes=package.at("nodes");const auto& root=nodes.at(package.at("root").get<int>()).at("position");
        const float rootX=root.at("x").get<float>(),rootY=root.at("y").get<float>();
        const auto& bounds=package.at("visualBounds");
        const float left=std::floor(bounds.at("minX").get<float>()*100.f),right=std::ceil(bounds.at("maxX").get<float>()*100.f);
        const float top=std::ceil(bounds.at("maxY").get<float>()*100.f),bottom=std::floor(bounds.at("minY").get<float>()*100.f);
        if(!std::isfinite(left+right+top+bottom+rootX+rootY)||left>=right||bottom>=top||right-left>4096||top-bottom>4096)
            throw std::runtime_error("invalid source biome field bounds");
        const Json* plane=nullptr;int planeNode=-1,planeCount=0;size_t sprites=0;
        for(const auto& sprite:package.at("sprites"))
        {
            const int index=sprite.at("node").get<int>();const auto nodePath=nodes.at(index).at("path").get<std::string>();
            if(nodePath.starts_with("bg/"))
            {
                ++sprites;
                if(sprite.value("maskInteraction",0)!=0)throw std::runtime_error("source backdrop requires a live gameplay mask");
            }
            if(nodePath=="bg/bg")
            {plane=&package.at("spriteVariants").at(sprite.at("guid").get<std::string>());planeNode=index;++planeCount;}
        }
        if(planeCount!=1||!plane)throw std::runtime_error("missing exact source biome plane");
        std::vector<int> ancestry;for(int n=planeNode;n>=0;n=nodes.at(n).at("parent").get<int>())ancestry.push_back(n);
        float planeX=0,planeY=0,scaleX=1,scaleY=1;
        for(auto n=ancestry.rbegin();n!=ancestry.rend();++n)
        {
            const auto& node=nodes.at(*n);const auto& rotation=node.at("rotation");
            if(std::abs(rotation.at("x").get<float>())+std::abs(rotation.at("y").get<float>())+std::abs(rotation.at("z").get<float>())>1e-5f)
                throw std::runtime_error("rotated source biome plane is not admitted");
            planeX+=scaleX*node.at("position").at("x").get<float>();planeY+=scaleY*node.at("position").at("y").get<float>();
            scaleX*=node.at("scale").at("x").get<float>();scaleY*=node.at("scale").at("y").get<float>();
        }
        const float ppu=plane->at("pixelsPerUnit").get<float>();
        const float width=plane->at("logicalSize").at("width").get<float>()*scaleX/ppu,height=plane->at("logicalSize").at("height").get<float>()*scaleY/ppu;
        const float planeLeft=planeX-plane->at("pivot").at("x").get<float>()*width;
        const float planeBottom=planeY-plane->at("pivot").at("y").get<float>()*height;
        const int rows=GetNativeFusionRowCount(board),pitch=rows==6?85:100;
        const float lawnBottom=float(board->GridToPixelY(0,rows-1)+pitch);
        const float gap=lawnBottom-FieldY277(board,planeBottom);
        if(ppu<=0||width<=0||height<=0||!std::isfinite(gap)||std::abs(gap)>100||
            FieldX(planeLeft)>kFieldLeft||FieldX(planeLeft+width)<kFieldRight)
            throw std::runtime_error("source biome field does not cover native10-column geometry");
        auto visual=std::make_unique<Impl::FieldVisual277>();visual->scene=spec->scene;
        visual->image=std::make_unique<Sexy::MemoryImage>(board->mApp);visual->image->Create(int(right-left),int(top-bottom));visual->image->Clear();
        Sexy::Graphics canvas(visual->image.get());canvas.SetLinearBlend(true);
        FusionAnimationDrawOptions options;options.spritePathPrefix="bg";
        background.DrawPose(&canvas,-left,top,1,"motion1",0,nullptr,-1,1,&options);
        visual->matrix.LoadIdentity();visual->matrix.m00=kFieldPixelsX/100;visual->matrix.m11=float(GetNativeFusionPixelsY(board))/100;
        visual->matrix.m02=FieldX(rootX+(left+right)*.005f);visual->matrix.m12=FieldY277(board,rootY+(top+bottom)*.005f)+gap;
        if(spec->scene==2)
        {
            float x=0,y=0;bool active=false;const char* node="bg/bg/water/Particle System";
            if(!background.TryGetNodeOffset(node,x,y,"motion1",0,nullptr,-1,1,&options)||
                !background.TryGetNodeActivity(node,active,"motion1",0,nullptr,-1,1,&options)||!active)
                throw std::runtime_error("source Pool water node is unavailable");
            visual->waterX=FieldX(rootX+x);visual->waterY=FieldY277(board,rootY+y)+gap;
        }
        found=mImpl->fieldBiomes.emplace(key,std::move(visual)).first;
        LawnNet::NetTrace("Fusion field277 art ready background=%d package=%s sprites=%zu cells=10x%d bottom=%.3f shiftY=%.3f phase=0 sourceOnly=1",int(board->mBackground),spec->package,sprites,rows,lawnBottom,gap);
    }
    catch(const std::exception& e)
    {
        mImpl->fieldFailures.emplace(key,e.what());ReconcileNativeField277Particle(board,0,false,0,0,0);
        LawnNet::NetTrace("Fusion field277 art refused background=%d error=%s nativeFallback=1",int(board->mBackground),e.what());return;
    }
    }
    if(found!=mImpl->fieldBiomes.end())
    {
        const auto& v=*found->second;
        ReconcileNativeField277Particle(board,v.scene,v.scene==2,v.waterX,v.waterY,Board::MakeRenderOrder(RENDER_LAYER_UI_BOTTOM,0,1));
    }
}
bool NativeVisuals::QueryField(const Board* board,NativeFieldView277& out) const
{
    out={};if(!mImpl||!board||!board->mApp||board->mApp!=mImpl->app)return false;
    out.background=uint8_t(board->mBackground);
    if(SourceFieldSpec277(board))
    {
        out.sourceSelected=true;const auto found=mImpl->fieldBiomes.find(out.background);
        out.artReady=found!=mImpl->fieldBiomes.end()&&found->second->image;
        QueryNativeField277Particle(board,out.waterResource,out.waterSystemId,out.waterX,out.waterY);return true;
    }
    if(board->mBackground!=BACKGROUND_1_DAY)return false;
    out.sourceSelected=!(board->mApp->mNetManager&&board->mApp->mNetManager->mConfig.mFusionClassicField);
    out.artReady=out.sourceSelected?bool(mImpl->fusionField):bool(mImpl->nativeField);return true;
}
bool NativeVisuals::DrawField(Sexy::Graphics* graphics, Board* board, bool useClassicArt)
{
    if(mImpl&&graphics&&board&&SourceFieldSpec277(board))
    {
        const auto found=mImpl->fieldBiomes.find(uint8_t(board->mBackground));
        if(found==mImpl->fieldBiomes.end()||!found->second->image)return false;
        // As with Day, the unchanged native image fills only the viewport
        // outside the genuine art extent; the admitted source plane covers
        // every one of the ten lawn columns. No native strips are stretched.
        auto* base=board->mBackground==BACKGROUND_2_NIGHT?Sexy::IMAGE_BACKGROUND2:Sexy::IMAGE_BACKGROUND3;
        if(base)graphics->DrawImageF(base,-BOARD_OFFSET+WIDESCREEN_OFFSETX,WIDESCREEN_OFFSETY);
        graphics->DrawImageMatrix(found->second->image.get(),found->second->matrix);return true;
    }
    if (!mImpl || !graphics || !board || !board->mApp || board->mBackground != BACKGROUND_1_DAY || !Sexy::IMAGE_BACKGROUND1) return false;
    if (!useClassicArt && mImpl->fusionFieldFailed) return false;
    try
    {
        // Background resources are loaded after NativeContext initialization.
        // Build only when the real, fully loaded native image is available.
        if (!mImpl->nativeField || mImpl->nativeFieldSource != Sexy::IMAGE_BACKGROUND1)
        {
            auto* original = Sexy::IMAGE_BACKGROUND1;
            if (original->GetWidth() != 1880 || original->GetHeight() != 720)
                throw std::runtime_error("native Fusion day field requires the loaded1880x720 native background");
            auto image = std::make_unique<Sexy::MemoryImage>(board->mApp);
            image->Create(original->GetWidth(), original->GetHeight());
            image->Clear();
            Sexy::Graphics canvas(image.get());
            canvas.DrawImage(original, 0, 0);
            const auto path = PackageRoot().parent_path()/"fields"/"classic-day-10";
            Sexy::SharedImageRef lawn = board->mApp->GetSharedImage(Sexy::StringToSexyString(path.string()));
            auto* grass = static_cast<Sexy::Image*>(lawn);
            if (!grass || grass->GetWidth() <= 0 || grass->GetHeight() <= 0 ||
                std::abs(static_cast<float>(grass->GetWidth())/grass->GetHeight()-1.6f) > 0.01f)
                throw std::runtime_error("native Fusion day field needs its generated10x5 lawn texture");
            // One continuous ten-column texture preserves its painted joins.
            // The existing house, border and street stay in the native image.
            canvas.SetLinearBlend(true);
            canvas.DrawImage(grass, Sexy::Rect(500, 140, 720, 500),
                Sexy::Rect(0, 0, grass->GetWidth(), grass->GetHeight()));
            mImpl->nativeField = std::move(image);
            mImpl->nativeFieldSource = original;
            LawnNet::NetTrace("Fusion native field art cached skin=native source=1880x720 lawn=500,140,720,500 cells=10x5 pitch=72,100 continuousTexture=1");
        }
        if (!useClassicArt && !mImpl->fusionField)
        {
            const auto path = PackageRoot()/"background1"/"animation.json";
            const Json package = ReadPackage(path);
            FusionAnimation background;
            LoadAnimation(background, board->mApp, path);
            const auto& nodes = package.at("nodes");
            const auto& root = nodes.at(package.at("root").get<int>()).at("position");
            const float rootX = root.at("x").get<float>(), rootY = root.at("y").get<float>();
            const auto& bounds = package.at("visualBounds");
            const float left = std::floor(bounds.at("minX").get<float>()*100.0f);
            const float right = std::ceil(bounds.at("maxX").get<float>()*100.0f);
            const float top = std::ceil(bounds.at("maxY").get<float>()*100.0f);
            const float bottom = std::floor(bounds.at("minY").get<float>()*100.0f);
            if (!std::isfinite(left+right+top+bottom+rootX+rootY) || left >= right || bottom >= top || right-left > 4096 || top-bottom > 4096)
                throw std::runtime_error("invalid Fusion background source bounds");
            auto image = std::make_unique<Sexy::MemoryImage>(board->mApp);
            image->Create(static_cast<int>(right-left), static_cast<int>(top-bottom));
            image->Clear();
            Sexy::Graphics canvas(image.get());
            canvas.SetLinearBlend(true);

            // Read the original plane's metadata only. Sprite/mesh composition
            // is still performed by the existing FusionAnimation below.
            const Json* plane = nullptr;
            int planeNode = -1;
            for (const auto& sprite : package.at("sprites"))
            {
                const auto& variant = package.at("spriteVariants").at(sprite.at("guid").get<std::string>());
                if (variant.at("sourceSprite").get<std::string>() == "Resources/background/sprites/bg.asset")
                { plane = &variant; planeNode = sprite.at("node").get<int>(); break; }
            }
            if (!plane) throw std::runtime_error("Fusion background has no verified grass plane");
            std::vector<int> ancestry;
            for (int node = planeNode; node >= 0; node = nodes.at(node).at("parent").get<int>()) ancestry.push_back(node);
            float planeX = 0, planeY = 0, scaleX = 1, scaleY = 1;
            for (auto node = ancestry.rbegin(); node != ancestry.rend(); ++node)
            {
                const auto& item = nodes.at(*node);
                const auto& rotation = item.at("rotation");
                if (std::abs(rotation.at("x").get<float>()) > 0.00001f || std::abs(rotation.at("y").get<float>()) > 0.00001f ||
                    std::abs(rotation.at("z").get<float>()) > 0.00001f)
                    throw std::runtime_error("rotated Fusion grass plane needs a new bounds mapping");
                planeX += scaleX*item.at("position").at("x").get<float>();
                planeY += scaleY*item.at("position").at("y").get<float>();
                scaleX *= item.at("scale").at("x").get<float>(); scaleY *= item.at("scale").at("y").get<float>();
            }
            const float ppu = plane->at("pixelsPerUnit").get<float>();
            const float planeWidth = plane->at("logicalSize").at("width").get<float>()*scaleX/ppu;
            const float planeHeight = plane->at("logicalSize").at("height").get<float>()*scaleY/ppu;
            const float planeLeft = planeX-plane->at("pivot").at("x").get<float>()*planeWidth;
            const float planeBottom = planeY-plane->at("pivot").at("y").get<float>()*planeHeight;
            const float gap = kFieldBottom-FieldY(planeBottom);
            if (ppu <= 0 || planeWidth <= 0 || planeHeight <= 0 || !std::isfinite(gap) || std::abs(gap) > 100 ||
                FieldX(planeLeft) > kFieldLeft || FieldX(planeLeft+planeWidth) < kFieldRight)
                throw std::runtime_error("unsupported Fusion grass plane extent");
            background.Draw(&canvas, -left, top, 1.0f, 0.0f);
            mImpl->fusionFieldMatrix.LoadIdentity();
            mImpl->fusionFieldMatrix.m00 = kFieldPixelsX/100.0f;
            mImpl->fusionFieldMatrix.m11 = kFieldPixelsY/100.0f;
            mImpl->fusionFieldMatrix.m02 = FieldX(rootX+(left+right)*0.005f);
            // Align the source grass bottom with the actual lawn. Shifting
            // the backdrop also aligns its rows, without moving game cells.
            mImpl->fusionFieldMatrix.m12 = FieldY(rootY+(top+bottom)*0.005f)+gap;
            mImpl->fusionField = std::move(image);
            LawnNet::NetTrace("Fusion native field art cached skin=fusion layers=%zu planeBottom=%.3f lawnBottom=580 backdropShiftY=%.3f cosmeticPhase=0",
                package.at("sprites").size(), FieldY(planeBottom)+gap, gap);
        }
        graphics->DrawImageF(mImpl->nativeField.get(), -BOARD_OFFSET+WIDESCREEN_OFFSETX, WIDESCREEN_OFFSETY);
        if (!useClassicArt) graphics->DrawImageMatrix(mImpl->fusionField.get(), mImpl->fusionFieldMatrix);
        mImpl->fieldError.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        if (!useClassicArt) mImpl->fusionFieldFailed = true;
        if (mImpl->fieldError != exception.what())
            LawnNet::NetTrace("Fusion native field art unavailable skin=%s error=%s", useClassicArt ? "native" : "fusion", exception.what());
        mImpl->fieldError = exception.what();
        return false;
    }
}

bool InitializeNativeVisuals(Board* board, std::string* error)
{
    NativeContext* context = GetNativeContext(board);
    if (!context || !board || !board->mApp)
    { if (error) *error = "native Fusion board context is unavailable"; return false; }
    auto visuals = std::make_unique<NativeVisuals>();
    std::string loadError;
    if (!visuals->Load(board->mApp, loadError))
    { if (error) *error = loadError; return false; }
    context->visuals = std::move(visuals);
    if (error) error->clear();
    return true;
}

bool EnsureNativeVisuals(Board* board, std::span<const uint16_t> plants, std::span<const uint16_t> projectiles, std::string* error)
{
    auto* context = GetNativeContext(board);
    if (!context || !context->visuals)
    { if (error) *error = "native Fusion visual index is unavailable"; return false; }
    std::string failure;
    const bool ok = context->visuals->Ensure(plants,projectiles,failure);
    if (error) *error = failure;
    if (!ok) LawnNet::NetTrace("Fusion native resource admission rejected error=%s",failure.c_str());
    return ok;
}
bool EnsureNativePlantVisuals(Board* board, uint16_t kind, std::string* error)
{ return EnsureNativeVisuals(board,std::span<const uint16_t>(&kind,1),{},error); }
bool EnsureNativeProjectileVisuals(Board* board, uint16_t kind, std::string* error)
{ return EnsureNativeVisuals(board,{},std::span<const uint16_t>(&kind,1),error); }
bool EnsureNativePortraitVisuals(Board* board, uint16_t kind, std::string* error)
{
    auto* context = GetNativeContext(board);
    if (!context || !context->visuals)
    { if (error) *error = "native Fusion visual index is unavailable"; return false; }
    std::string failure;
    const bool ok = context->visuals->EnsurePortrait(kind,failure);
    if (error) *error = failure;
    if (!ok) LawnNet::NetTrace("Fusion native resource admission rejected error=%s",failure.c_str());
    return ok;
}
void UpdateNativeVisualPrewarm(Board* board)
{ auto* context = GetNativeContext(board); if (context && context->visuals) context->visuals->UpdatePrewarm(board); }
void ReleaseUnusedNativeVisuals(Board* board)
{ auto* context = GetNativeContext(board); if (context && context->visuals) context->visuals->ReleaseUnused(board); }

bool DrawNativeFusionPlant(Sexy::Graphics* graphics, Plant* plant)
{
    NativeContext* context = plant ? GetNativeContext(plant->mBoard) : nullptr;
    return context && context->visuals && context->visuals->DrawPlant(graphics, plant);
}
bool DrawNativeFusionPumpkinBack(Sexy::Graphics* graphics, Plant* pumpkin)
{
    NativeContext* context = pumpkin ? GetNativeContext(pumpkin->mBoard) : nullptr;
    return context && context->visuals && context->visuals->DrawPumpkinBack(graphics,pumpkin);
}

bool DrawNativeFusionProjectile(Sexy::Graphics* graphics, Projectile* projectile)
{
    NativeContext* context = projectile ? GetNativeContext(projectile->mBoard) : nullptr;
    return context && context->visuals && context->visuals->DrawProjectile(graphics, projectile);
}

bool DrawNativeFusionPortrait(Sexy::Graphics* graphics, uint16_t kind, const Sexy::Rect& bounds, int grayness, bool preserveTint)
{
    NativeContext* context = gLawnApp ? GetNativeContext(gLawnApp->mBoard) : nullptr;
    return context && context->visuals && context->visuals->DrawPortrait(graphics, kind, bounds, grayness, preserveTint);
}

bool DrawNativeFusionField(Sexy::Graphics* graphics, Board* board, bool useClassicArt)
{
    // The native opening sod animation owns the one/three-row lawn.
    if(board&&board->mBackground==BACKGROUND_1_DAY&&board->mApp->IsFirstTimeAdventureMode()&&board->mLevel<=4)return false;
    NativeContext* context = GetNativeContext(board);
    return context && context->visuals && context->visuals->DrawField(graphics, board, useClassicArt);
}
void ReconcileNativeFusionField277(Board* board)
{auto* context=GetNativeContext(board);if(context&&context->visuals)context->visuals->ReconcileField(board);}
bool QueryNativeFusionField277(const Board* board,NativeFieldView277& out)
{out={};const auto* context=GetNativeContext(board);return context&&context->visuals&&context->visuals->QueryField(board,out);}
}
