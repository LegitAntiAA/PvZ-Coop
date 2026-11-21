#include "FusionDefenderRules.h"
#include "FusionExtras268Rules.h"
#include <cmath>

namespace LawnFusion
{
namespace
{
constexpr FusionDefenderProfile profiles[] = {
    {1092,FusionDefenderFamily::Pumpkin,FusionDefenderLayer::Pumpkin,4000,225,3000,FusionDefenderBite::None,{"pumpkin_damage0","pumpkin_damage1","pumpkin_damage2"},kDefenderPendingNone},
    {1097,FusionDefenderFamily::Pumpkin,FusionDefenderLayer::Pumpkin,8000,275,3000,FusionDefenderBite::None,{"pumpkin_damage0","pumpkin_damage1","pumpkin_damage2"},kDefenderPendingNone},
    {1098,FusionDefenderFamily::Pumpkin,FusionDefenderLayer::Pumpkin,4000,275,3000,FusionDefenderBite::None,{"pumpkin_damage0","pumpkin_damage1","pumpkin_damage2"},kDefenderPendingNone},
    {1099,FusionDefenderFamily::Pumpkin,FusionDefenderLayer::Pumpkin,4000,275,3000,FusionDefenderBite::None,{"pumpkin_damage0","pumpkin_damage1","pumpkin_damage2"},kDefenderPendingNone},
    {935,FusionDefenderFamily::Pumpkin,FusionDefenderLayer::Pumpkin,8000,325,5000,FusionDefenderBite::None,{"pumpkin_damage0","pumpkin_damage1","pumpkin_damage2"},kDefenderPendingNone},
    {931,FusionDefenderFamily::WallNut,FusionDefenderLayer::Body,16000,450,5000,FusionDefenderBite::None,{"Wallnut_body","Wallnut_cracked1","Wallnut_cracked2"},kDefenderPendingNone},
    {913,FusionDefenderFamily::TallNut,FusionDefenderLayer::Body,32000,350,9000,FusionDefenderBite::None,{"tnf1","tnf2","tnf3"},kDefenderPendingNone},
    {925,FusionDefenderFamily::TallNut,FusionDefenderLayer::Body,32000,350,9000,FusionDefenderBite::None,{"ObsidianPotatoNut_body","ObsidianPotatoNut_body2","ObsidianPotatoNut_body3"},kDefenderPendingNone},
    {936,FusionDefenderFamily::Pot,FusionDefenderLayer::Under,300,9999,5000,FusionDefenderBite::None,{nullptr,nullptr,nullptr},kDefenderPendingNone},
    {1029,FusionDefenderFamily::WallNut,FusionDefenderLayer::Body,8000,100,5000,FusionDefenderBite::None,{"Wallnut_body","Wallnut_cracked1","Wallnut_cracked2"},kDefenderPendingEquipment},
    {1048,FusionDefenderFamily::WallNut,FusionDefenderLayer::Body,4000,150,3000,FusionDefenderBite::ReflectHalf,{"Wallnut_body","Wallnut_cracked1","Wallnut_cracked2"},kDefenderPendingNone},
    {1189,FusionDefenderFamily::WallNut,FusionDefenderLayer::Body,4000,125,3000,FusionDefenderBite::Cold,{"Wallnut_body","Wallnut_cracked1","Wallnut_cracked2"},kDefenderPendingNone},
    {1027,FusionDefenderFamily::TallNut,FusionDefenderLayer::Body,8000,125,5000,FusionDefenderBite::None,{"tnf1","tnf2","tnf3"},kDefenderPendingEquipment},
    {1028,FusionDefenderFamily::TallNut,FusionDefenderLayer::Body,16000,175,5000,FusionDefenderBite::None,{"tnf1","tnf2","tnf3"},kDefenderPendingEquipment},
    {1039,FusionDefenderFamily::TallNut,FusionDefenderLayer::Body,8000,200,5000,FusionDefenderBite::Cold,{"tnf1","tnf2","tnf3"},kDefenderPendingNone},
    {1073,FusionDefenderFamily::TallNut,FusionDefenderLayer::Body,8000,250,5000,FusionDefenderBite::Fire,{"tnf1","tnf2","tnf3"},kDefenderPendingNone},
    {27,FusionDefenderFamily::Pot,FusionDefenderLayer::Under,300,25,750,FusionDefenderBite::None,{nullptr,nullptr,nullptr},kDefenderPendingNone},
    {1112,FusionDefenderFamily::Pot,FusionDefenderLayer::Under,300,100,750,FusionDefenderBite::None,{nullptr,nullptr,nullptr},kDefenderPendingNone},
    {1114,FusionDefenderFamily::Pot,FusionDefenderLayer::Under,300,100,750,FusionDefenderBite::None,{nullptr,nullptr,nullptr},kDefenderPendingNone},
    {1125,FusionDefenderFamily::Pot,FusionDefenderLayer::Under,300,100,750,FusionDefenderBite::None,{nullptr,nullptr,nullptr},kDefenderPendingNone},
    {1130,FusionDefenderFamily::Pot,FusionDefenderLayer::Under,300,50,750,FusionDefenderBite::None,{nullptr,nullptr,nullptr},kDefenderPendingNone},
    {1136,FusionDefenderFamily::Pot,FusionDefenderLayer::Under,300,0,750,FusionDefenderBite::None,{nullptr,nullptr,nullptr},kDefenderPendingNone},
    {1137,FusionDefenderFamily::Pot,FusionDefenderLayer::Under,300,9999,9000,FusionDefenderBite::None,{nullptr,nullptr,nullptr},kDefenderPendingNone},
    {1184,FusionDefenderFamily::Pot,FusionDefenderLayer::Under,300,25,750,FusionDefenderBite::None,{nullptr,nullptr,nullptr},kDefenderPendingNone},
    {1145,FusionDefenderFamily::WallNut,FusionDefenderLayer::Body,600,75,750,FusionDefenderBite::None,{"Garlic_body1","Garlic_body2","Garlic_body3"},kDefenderPendingNone},
    {1146,FusionDefenderFamily::WallNut,FusionDefenderLayer::Body,900,125,1500,FusionDefenderBite::None,{"Garlic_body1","Garlic_body2","Garlic_body3"},kDefenderPendingNone},
    {1133,FusionDefenderFamily::Pot,FusionDefenderLayer::Under,300,100,750,FusionDefenderBite::None,{nullptr,nullptr,nullptr},kDefenderPendingUmbrella},
    {24,FusionDefenderFamily::Pumpkin,FusionDefenderLayer::Pumpkin,4000,125,3000,FusionDefenderBite::None,{"pumpkin_damage0","pumpkin_damage1","pumpkin_damage2"},kDefenderPendingNone},
    {1091,FusionDefenderFamily::Pumpkin,FusionDefenderLayer::Pumpkin,4000,225,3000,FusionDefenderBite::None,{"pumpkin_damage0","pumpkin_damage1","pumpkin_damage2"},kDefenderPendingNone},
    {1087,FusionDefenderFamily::Pumpkin,FusionDefenderLayer::Pumpkin,4000,150,3000,FusionDefenderBite::None,{"pumpkin_damage0","pumpkin_damage1","pumpkin_damage2"},kDefenderPendingNone},
    {1190,FusionDefenderFamily::Pumpkin,FusionDefenderLayer::Pumpkin,4000,150,3000,FusionDefenderBite::None,{"pumpkin_damage0","pumpkin_damage1","pumpkin_damage2"},kDefenderPendingNone},
    {1088,FusionDefenderFamily::Pumpkin,FusionDefenderLayer::Pumpkin,4000,225,3000,FusionDefenderBite::ReflectFull,{"pumpkin_damage0","pumpkin_damage1","pumpkin_damage2"},kDefenderPendingNone},
    {1089,FusionDefenderFamily::Pumpkin,FusionDefenderLayer::Pumpkin,4000,250,3000,FusionDefenderBite::None,{"pumpkin_damage0","pumpkin_damage1","pumpkin_damage2"},kDefenderPendingStar},
    {1164,FusionDefenderFamily::Pumpkin,FusionDefenderLayer::Pumpkin,4000,275,3000,FusionDefenderBite::None,{"pumpkin_damage0","pumpkin_damage1","pumpkin_damage2"},kDefenderPendingZombieCherry}
};
}
const FusionDefenderProfile* FindFusionDefenderProfile(uint16_t kind)
{ for (const auto& row : profiles) if (row.kind == kind) return &row; return FindFusionExtras268DefenderProfile(kind); }
const FusionDefenderProfile* FusionDefenderProfileAt(std::size_t index)
{ const auto count=sizeof(profiles)/sizeof(profiles[0]);return index<count?&profiles[index]:index==count?FindFusionExtras268DefenderProfile(1129):nullptr; }
std::size_t FusionDefenderProfileCount() { return sizeof(profiles)/sizeof(profiles[0])+1; }
bool FusionDefenderHasDamageStages(uint16_t kind)
{ const auto* profile = FindFusionDefenderProfile(kind); return profile && profile->damageNodes[0]; }
bool TryGetFusionDefenderDamageStage(int health, int maximum, uint8_t previous, uint8_t& stage)
{
    if (maximum <= 0 || health > maximum || previous > 2) return false;
    const int low = maximum/3, high = static_cast<int>(static_cast<int64_t>(maximum)*2/3);
    uint8_t next = previous;
    if (health > high) next = 0;
    else if (health > low && health < high) next = 1;
    else if (health < low) next = 2;
    stage = next;
    return true;
}
bool TryGetFusionDefenderBiteEffect(uint16_t kind, int bite, FusionDefenderBiteEffect& effect)
{
    const auto* row = FindFusionDefenderProfile(kind);
    if (!row || bite <= 0) return false;
    FusionDefenderBiteEffect next;
    if (row->bite == FusionDefenderBite::ReflectHalf) next.shieldlessDamage = bite/2;
    else if (row->bite == FusionDefenderBite::ReflectFull) next.shieldlessDamage = bite;
    else if (row->bite == FusionDefenderBite::Cold) { next.chilledTicks = 1000; next.coldLevels = 10; }
    else if (row->bite == FusionDefenderBite::Fire) { next.shieldlessDamage = 20; next.warm = true; }
    effect = next;
    return true;
}
bool FusionDefenderBlocksPotDamage(uint16_t kind, bool carrying, int type)
{
    const auto* row = FindFusionDefenderProfile(kind);
    // Only source-known mapped damage callers opt in; unknown native events do not.
    return row && row->layer == FusionDefenderLayer::Under && carrying && type > 0 && type <= 2;
}
int FusionDefenderElementRecovery(uint16_t kind, FusionDefenderElement element)
{
    if (element == FusionDefenderElement::Freeze && kind == 1039) return 1000;
    if (element == FusionDefenderElement::Fire && kind == 1073) return 3000;
    return 0;
}
bool TryGetFusionDefenderBucketEffect(uint16_t kind, int bucket, FusionDefenderBucketEffect& effect)
{
    if (bucket < 0 || bucket > 10) return false;
    FusionDefenderBucketEffect next;
    if (kind == 1027 && bucket == 1) next.replacementKind = 1028;
    else if (kind == 1028 || kind == 1029) next.recoverHealth = 500;
    else return false;
    effect = next;
    return true;
}
float FusionDefenderLobberInterval(uint16_t kind, uint16_t pot, float original)
{
    if (pot != 1112 || !std::isfinite(original) || original <= 0) return original;
    switch (kind)
    {
    case 26: case 1113: case 1118: case 1124: case 1134: case 1135: return 2.0f;
    case 1111: case 1126: return 3.33f;
    case 914: case 934: return 1.5f;
    default: return original;
    }
}
int FusionDefenderCornDenominator(uint16_t kind, uint16_t pot, int original)
{
    if (pot != 1114 || original <= 0) return original;
    switch (kind)
    {
    case 28: case 914: case 1113: case 1117: case 1121: case 1126: case 1138: case 1139: return 2;
    default: return original;
    }
}
bool FusionDefenderMelonEnhanced(uint16_t kind, uint16_t pot)
{
    if (pot != 1125) return false;
    switch (kind)
    {
    case 32: case 914: case 1116: case 1119: case 1121: case 1124: case 1126: case 1149:
    case 1141: case 1142: return true; // MoneyMelon inherits the live MelonPotEffect call.
    default: return false;
    }
}
}
