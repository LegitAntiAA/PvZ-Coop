#include "FusionZombieBatch270Rules.h"

namespace LawnFusion
{
namespace
{
constexpr FusionZombieBatch270Profile profiles[]={
    {14,FusionZombieBatch270Family::Submarine,ZOMBIE_ZAMBONI,1350,0,0,500,"SubmarineZombie"},
    {15,FusionZombieBatch270Family::Paper,ZOMBIE_NEWSPAPER,720,0,1100,400,"ElitePaperZombie"},
    {18,FusionZombieBatch270Family::Driver,ZOMBIE_ZAMBONI,2700,0,0,500,"SuperDriverZombie"},
    {104,FusionZombieBatch270Family::Paper,ZOMBIE_NEWSPAPER,1500,0,200,50,"PaperCherryZ"},
    {3,FusionZombieBatch270Family::PoleVault,ZOMBIE_POLEVAULTER,720,0,0,50,"PolevaulterZombie"},
    {5,FusionZombieBatch270Family::Paper,ZOMBIE_NEWSPAPER,270,0,200,50,"PaperZombie"},
    {6,FusionZombieBatch270Family::PoleVault,ZOMBIE_POLEVAULTER,720,0,0,50,"DancePolevaulterZombie"},
    {7,FusionZombieBatch270Family::PoleVault,ZOMBIE_POLEVAULTER,270,0,0,50,"PolevaulterZombie"},
    {9,FusionZombieBatch270Family::Football,ZOMBIE_FOOTBALL,270,1400,0,50,"ZombieFootball"},
    {10,FusionZombieBatch270Family::Dance,ZOMBIE_DANCER,720,0,0,50,"ZombieJackson"},
    {16,FusionZombieBatch270Family::Driver,ZOMBIE_ZAMBONI,1350,0,0,500,"DriverZombie"},
    {17,FusionZombieBatch270Family::Snorkel,ZOMBIE_SNORKEL,270,0,0,50,"SnorkleZombie"},
    {19,FusionZombieBatch270Family::Dolphin,ZOMBIE_DOLPHIN_RIDER,720,0,0,50,"DolphinriderZ"},
    {20,FusionZombieBatch270Family::Drown,ZOMBIE_DUCKY_TUBE,720,0,0,50,"DrownZombie"},
    {21,FusionZombieBatch270Family::Doll,ZOMBIE_NORMAL,2000,4400,0,50,"DollZombie"},
    {22,FusionZombieBatch270Family::Doll,ZOMBIE_NORMAL,2000,2200,0,50,"DollZombie"},
    {23,FusionZombieBatch270Family::Doll,ZOMBIE_NORMAL,2000,1100,0,50,"DollZombie"},
    {24,FusionZombieBatch270Family::Jackbox,ZOMBIE_JACK_IN_THE_BOX,720,0,0,50,"JackboxZombie"},
    {25,FusionZombieBatch270Family::Balloon,ZOMBIE_BALLOON,270,20,0,50,"BalloonZombie"},
    {26,FusionZombieBatch270Family::Airship,ZOMBIE_BALLOON,1350,0,0,500,"KirovAirship"},
    {27,FusionZombieBatch270Family::Dolphin,ZOMBIE_DOLPHIN_RIDER,720,0,0,50,"SnowDolphinrider"},
    {28,FusionZombieBatch270Family::Digger,ZOMBIE_DIGGER,720,0,0,50,"MinerZombie"},
    {29,FusionZombieBatch270Family::Balloon,ZOMBIE_BALLOON,270,500,0,50,"IronBalloonZombie"},
    {30,FusionZombieBatch270Family::Jackbox,ZOMBIE_JACK_IN_THE_BOX,720,370,0,50,"SuperJackboxZombie"},
    {31,FusionZombieBatch270Family::Catapult,ZOMBIE_CATAPULT,850,0,0,500,"CatapultZombie"},
    {32,FusionZombieBatch270Family::Pogo,ZOMBIE_POGO,720,0,0,50,"PogoZombie"},
    {33,FusionZombieBatch270Family::Ladder,ZOMBIE_LADDER,720,0,720,50,"LadderZombie"},
    {34,FusionZombieBatch270Family::Pogo,ZOMBIE_POGO,270,2200,0,50,"SuperPogoZombie"},
    {35,FusionZombieBatch270Family::Gargantuar,ZOMBIE_GARGANTUAR,3000,0,0,50,"Gargantuar"},
    {36,FusionZombieBatch270Family::Gargantuar,ZOMBIE_GARGANTUAR,6000,0,0,50,"Gargantuar"},
    {37,FusionZombieBatch270Family::Imp,ZOMBIE_IMP,270,0,0,50,"ImpZombie"},
    {38,FusionZombieBatch270Family::IronGargantuar,ZOMBIE_GARGANTUAR,3000,0,0,50,"IronGargantuar"},
    {39,FusionZombieBatch270Family::IronGargantuar,ZOMBIE_GARGANTUAR,6000,0,0,50,"IronGargantuar"},
    {40,FusionZombieBatch270Family::MachineNut,ZOMBIE_NORMAL,1350,0,0,50,"MachineNutZombie"},
    {43,FusionZombieBatch270Family::Gargantuar,ZOMBIE_GARGANTUAR,8000,24000,0,50,"SuperGargantuar"},
    {45,FusionZombieBatch270Family::Bungee,ZOMBIE_BUNGEE,720,0,0,50,"BungiZombie"},
    {100,FusionZombieBatch270Family::PeaHead,ZOMBIE_PEA_HEAD,270,0,0,50,"PeaShooterZ"},
    {101,FusionZombieBatch270Family::CherryHead,ZOMBIE_PEA_HEAD,500,0,0,50,"CherryShooterZ"},
    {102,FusionZombieBatch270Family::CherryHead,ZOMBIE_PEA_HEAD,500,0,0,50,"SuperCherryZ"},
    {103,FusionZombieBatch270Family::WallNutArmor,ZOMBIE_WALLNUT_HEAD,270,1100,0,50,"WallNutZ"},
};
constexpr bool ProfilesConsistent()
{
    constexpr uint16_t admitted[]={0,2,4,8,11,12,13,41,42,109,111};
    if(sizeof(profiles)/sizeof(profiles[0])!=40)return false;
    for(std::size_t i=0;i<40;++i)
    {
        const auto& p=profiles[i];
        if(p.maxHealth<=0||p.firstArmor<0||p.secondArmor<0||p.biteDamage<0||
            p.carrier<ZOMBIE_NORMAL||p.carrier>=NUM_ZOMBIE_TYPES||!p.sourceClass)return false;
        for(auto old:admitted)if(p.kind==old)return false;
        for(std::size_t j=0;j<i;++j)if(profiles[j].kind==p.kind)return false;
    }
    return true;
}
static_assert(ProfilesConsistent());
constexpr uint64_t common=Zombie270Model|Zombie270LifecycleEffects|Zombie270BodyMotion|
    Zombie270BodyDamage|Zombie270SourceOverrides|Zombie270NativeFactory|
    Zombie270ReplicatedState|Zombie270PairedRuntimeProof;
}

const FusionZombieBatch270Profile* FindFusionZombieBatch270Profile(uint16_t kind)
{for(const auto& p:profiles)if(p.kind==kind)return &p;return nullptr;}
const FusionZombieBatch270Profile* FusionZombieBatch270ProfileAt(std::size_t index)
{return index<FusionZombieBatch270ProfileCount()?&profiles[index]:nullptr;}
std::size_t FusionZombieBatch270ProfileCount()
{return sizeof(profiles)/sizeof(profiles[0]);}

bool FusionZombieBatch270SourceStatsMatch(uint16_t kind,int maximum,int first,int second,int bite)
{
    const auto* p=FindFusionZombieBatch270Profile(kind);
    return p&&maximum==p->maxHealth&&first==p->firstArmor&&second==p->secondArmor&&bite==p->biteDamage;
}
bool TryGetFusionZombieBatch270ArmorLayers(uint16_t kind,uint8_t& layers)
{
    const auto* p=FindFusionZombieBatch270Profile(kind);if(!p)return false;
    layers=uint8_t((p->firstArmor>0?1:0)|(p->secondArmor>0?2:0));return true;
}
uint64_t FusionZombieBatch270RequiredCapabilities(uint16_t kind)
{
    const auto* p=FindFusionZombieBatch270Profile(kind);if(!p)return 0;
    uint64_t required=common;
    if(p->firstArmor>0)required|=Zombie270FirstArmor|Zombie270EquipmentCallbacks;
    if(p->secondArmor>0)required|=Zombie270SecondArmor|Zombie270EquipmentCallbacks;
    switch(p->family)
    {
    case FusionZombieBatch270Family::Paper:
        required|=Zombie270BiteEvents|Zombie270PaperTransition|Zombie270PaperMotion;
        if(kind==104)required|=Zombie270HeadLayer|Zombie270SourceProjectile|Zombie270BombCherryStart;
        break;
    case FusionZombieBatch270Family::PoleVault:
        required|=Zombie270BiteEvents|Zombie270Vault|Zombie270EquipmentCallbacks;
        if(kind==6)required|=Zombie270SummonRoster;
        break;
    case FusionZombieBatch270Family::Football:required|=Zombie270BiteEvents;break;
    case FusionZombieBatch270Family::Dance:required|=Zombie270BiteEvents|Zombie270SummonRoster;break;
    case FusionZombieBatch270Family::Submarine:required|=Zombie270Water|Zombie270CrashContact;break;
    case FusionZombieBatch270Family::Driver:
        required|=Zombie270CrashContact|Zombie270IceTerrain|Zombie270StatusOverrides;break;
    case FusionZombieBatch270Family::Snorkel:required|=Zombie270Water|Zombie270Dive|Zombie270BiteEvents;break;
    case FusionZombieBatch270Family::Dolphin:
        required|=Zombie270Water|Zombie270Vault|Zombie270BiteEvents;
        if(kind==27)required|=Zombie270StatusOverrides;
        break;
    case FusionZombieBatch270Family::Drown:required|=Zombie270Water|Zombie270CaptureThrow;break;
    case FusionZombieBatch270Family::Doll:required|=Zombie270BiteEvents|Zombie270SummonRoster;break;
    case FusionZombieBatch270Family::Jackbox:required|=Zombie270BiteEvents|Zombie270FuseAndExplosion;break;
    case FusionZombieBatch270Family::Balloon:required|=Zombie270BiteEvents|Zombie270FlightAndDrop;break;
    case FusionZombieBatch270Family::Airship:
        required|=Zombie270FlightAndDrop|Zombie270AirshipBomb|Zombie270CrashContact|Zombie270StatusOverrides;break;
    case FusionZombieBatch270Family::Digger:required|=Zombie270BiteEvents|Zombie270DigAndSurface|Zombie270EquipmentCallbacks;break;
    case FusionZombieBatch270Family::Catapult:required|=Zombie270CatapultAimAmmo|Zombie270SourceProjectile|Zombie270CrashContact;break;
    case FusionZombieBatch270Family::Pogo:required|=Zombie270BiteEvents|Zombie270Pogo|Zombie270EquipmentCallbacks;break;
    case FusionZombieBatch270Family::Ladder:required|=Zombie270BiteEvents|Zombie270Ladder;break;
    case FusionZombieBatch270Family::Gargantuar:
        required|=Zombie270GargSmash|Zombie270ImpThrow;
        if(kind==43)required|=Zombie270SuperBall|Zombie270SummonRoster|Zombie270StatusOverrides;
        break;
    case FusionZombieBatch270Family::Imp:required|=Zombie270BiteEvents|Zombie270ThrownLanding;break;
    case FusionZombieBatch270Family::IronGargantuar:
        required|=Zombie270IronBody|Zombie270CrashContact|Zombie270StatusOverrides|Zombie270EquipmentCallbacks;break;
    case FusionZombieBatch270Family::MachineNut:required|=Zombie270MachineType|Zombie270CrashContact|Zombie270StatusOverrides;break;
    case FusionZombieBatch270Family::Bungee:required|=Zombie270Bungee|Zombie270CaptureThrow;break;
    case FusionZombieBatch270Family::PeaHead:required|=Zombie270BiteEvents|Zombie270HeadLayer|Zombie270SourceProjectile;break;
    case FusionZombieBatch270Family::CherryHead:
        required|=Zombie270BiteEvents|Zombie270HeadLayer|Zombie270SourceProjectile|Zombie270BombCherryStart;break;
    case FusionZombieBatch270Family::WallNutArmor:required|=Zombie270BiteEvents|Zombie270ArmorNoFallStages;break;
    }
    return required;
}
bool TryGetFusionZombieBatch270MissingCapabilities(uint16_t kind,uint64_t actual,uint64_t& missing)
{
    const uint64_t required=FusionZombieBatch270RequiredCapabilities(kind);
    if(!required||(actual&~kFusionZombieBatch270KnownCapabilities))return false;
    missing=required&~actual;return true;
}
bool FusionZombieBatch270CapabilitiesReady(uint16_t kind,uint64_t actual)
{uint64_t missing=0;return TryGetFusionZombieBatch270MissingCapabilities(kind,actual,missing)&&!missing;}
}
