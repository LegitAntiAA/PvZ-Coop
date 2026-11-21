#include "FusionSummon269Rules.h"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
namespace LawnFusion
{
namespace
{
// Source ZombieData stores are copied below; registry/controller admission is
// independent. See work/fusion/batch40-269/hypno-npc-contract.json.
constexpr FusionSummon269NpcProfile profiles[]={
    {9,"ZombieFootball",270,1400,0,50,false},
    {0,"BasicZombie",270,0,0,50,true},{2,"ConeZombie",270,370,0,50,true},
    {4,"BucketZombie",270,1100,0,50,true},
    {8,"DoorZombie",270,0,1100,50,false},{11,"ZombieDuck",270,0,0,50,false},
    {12,"ConeZombieDuck",270,370,0,50,false},{13,"BucketZombieDuck",270,1100,0,50,false},
    {14,"SubmarineZombie",1350,0,0,500,false},{15,"ElitePaperZombie",720,0,1100,400,false},
    {18,"SuperDriverZombie",2700,0,0,500,false},{24,"JackboxZombie",720,0,0,50,false},
    {28,"MinerZombie",720,0,0,50,false},{30,"SuperJackboxZombie",720,370,0,50,false},
    {32,"PogoZombie",720,0,0,50,false},{33,"LadderZombie",720,0,720,50,false},
    {34,"SuperPogoZombie",270,2200,0,50,false},{38,"IronGargantuar",3000,0,0,50,false},
    {39,"IronGargantuar",6000,0,0,50,false},{40,"MachineNutZombie",1350,0,0,50,false},
    {41,"MoneyZombie",200,0,0,50,false},{42,"MoneyZombie",300,0,0,50,false},
    {43,"SuperGargantuar",8000,24000,0,50,false},
    {104,"PaperCherryZ",1500,0,200,50,false},{105,"RandomZombie",270,370,0,50,false},
    {106,"BucketNutZ",270,2200,0,50,false},{109,"TallNutFootballZ",270,4400,0,50,false},
    {110,"RandomPlusZombie",270,720,0,50,false},{111,"TallIceNutZ",270,2200,0,50,false},
    {114,"SuperDoorZombie",1350,1100,1100,50,false},{118,"GatlingFootballZombie",270,1400,0,50,false},
    {200,"SubmarineZombie",3000,0,0,500,false},{201,"JacksonDriver",12000,0,0,500,false},
    {202,"FootballDrown",270,7500,0,50,false},{203,"PaperCherryZ95",1500,0,1500,400,false},
    {204,"BlackFootballZombie",3000,12000,0,100,false},{205,"SuperKirov",3000,0,0,50,false},
    {209,"JackboxJumpZombie",720,4400,0,50,false},{210,"SuperMachineNutZombie",1350,0,0,50,false},
    {211,"LandSubmarineZombie",3000,0,0,50,false},{212,"UltimateGargantuar",8000,24000,0,50,false},
    {214,"DolphinGatlingZombie",1500,0,0,50,false},{218,"UltimateFootballDrownZombie",8000,24000,0,50,false},
    {219,"UltimateMachineNutZombie",12000,0,0,50,false},{220,"UltimateFootballZombie",8000,24000,0,200,false},
    {221,"UltimateKirovZombie",12000,0,0,50,false},{222,"UltimateJacksonDriver",32000,0,0,50,false},
    {223,"UltimatePaperZombie",1500,12000,1500,50,false},{224,"UltimateJackboxZombie",32000,0,0,50,false}};
bool Draw(int draw,int maximum){return draw>=0&&draw<maximum;}
}
const FusionSummon269NpcProfile* FindFusionSummon269NpcProfile(uint16_t kind)
{for(const auto& p:profiles)if(p.kind==kind)return &p;return nullptr;}
bool IsFusionSummon269Caller(uint16_t kind)
{for(uint16_t k:{900,917,924,928,938,1026,1041,1147,1151,1157,1164,916,923,1148})if(k==kind)return true;return false;}
bool TryChooseFusionHypno269(uint16_t kind,bool water,bool shooting,bool advanced,bool event,int draw,FusionSummon269Choice& output)
{
    if((kind!=900&&kind!=917)||!Draw(draw,4))return false;
    FusionSummon269Choice next;
    if(event)next.kind=105;
    else if(kind==917)next.kind=water?(advanced||shooting?214:14):(advanced||shooting?203:104);
    else if(water)next.kind=advanced?200:14;
    else{constexpr uint16_t ordinary[]={15,109,18,104},strong[]={205,204,201,203};next.kind=advanced?strong[draw]:ordinary[draw];}
    if(next.kind==104||next.kind==203)next.post=FusionSummon269Post::BreakSecondArmor;
    output=next;return true;
}
bool TryChooseFusionNut269(bool rainbow,bool water,FusionSummon269Choice& output)
{output={uint16_t(water?(rainbow?13:12):(rainbow?110:105))};return true;}
bool TryChooseFusionIceHypno269(bool water,FusionSummon269Choice& output)
{output={uint16_t(water?13:111),FusionSummon269Post::None,true,false};return true;}
bool TryChooseFusionMachine269(bool water,int health,FusionSummon269Choice& output)
{if(health<1)return false;output={uint16_t(water?200:211),FusionSummon269Post::PlantThirdHealth,true,true,health/3};return true;}
bool TryChooseFusionMagnet269(uint8_t item,int draw,bool water,FusionSummon269Choice& output)
{
    if(item>10||!Draw(draw,3))return false;
    constexpr uint16_t land[11][3]={{4,106,114},{118,9,109},{24,24,30},{28,28,28},{40,40,40},{210,210,210},
        {32,32,34},{33,33,33},{38,38,38},{39,39,39},{8,8,114}};
    constexpr uint16_t wet[]={13,13,13,12,14,14,12,12,14,14,11};
    const auto selected=land[item][draw];
    // Source dispatches the land callback even after replacing the kind with a
    // duck. Those component-incompatible water branches are not implemented.
    if(water&&(selected==28||selected==32||selected==34))return false;
    auto post=selected==28?FusionSummon269Post::MinerRiseHealth60:
        selected==32||selected==34?FusionSummon269Post::PogoStartHealth60:FusionSummon269Post::Health60;
    output={water?wet[item]:selected,post,true,true};return true;
}
bool TryGetFusionSummon269Interval(uint16_t kind,bool shooting,bool advanced,float& seconds)
{
    if(kind==1147){seconds=45;return true;}
    if(kind!=900&&kind!=917)return false;
    seconds=kind==917&&shooting?2.5f:shooting||advanced?10.f:30.f;return true;
}
bool TryGetFusionHypnoDoom269Rows(int row,int rows,bool advanced,std::array<int,3>& output,int& count)
{
    if(rows<1||rows>6||row<0||row>=rows)return false;
    output={row,row?row-1:0,row==rows-1?row:row+1};count=advanced?3:0;return true;
}
bool TryChooseFusionHypnoDoom269(bool water,FusionSummon269Choice& output)
{output={uint16_t(water?205:43),FusionSummon269Post::None,true,false};return true;}
bool TryPlanFusionGoldHypno269(int background,bool summoned,int draw,FusionGoldHypno269Plan& output)
{
    if(background<0||summoned||!Draw(draw,7))return false;
    uint16_t boss=0;bool extra=true;
    switch(background){case 0:case 6:boss=223;break;case 1:case 7:boss=220;break;case 2:boss=222;break;
    case 3:boss=221;break;case 4:case 10:boss=218;break;case 5:case 13:boss=219;break;case 12:boss=224;break;
    default:{constexpr uint16_t choices[]={218,220,212,222,221,219,223};boss=choices[draw];extra=false;}}
    FusionGoldHypno269Plan next;if(extra)next.bosses[next.count++]=212;next.bosses[next.count++]=boss;output=next;return true;
}
bool TryGetFusionGoldHypno269Minion(uint16_t boss,uint16_t& output)
{
    uint16_t kind;switch(boss){case 218:kind=202;break;case 219:kind=210;break;case 220:kind=204;break;
    case 221:kind=205;break;case 222:kind=201;break;case 223:kind=203;break;case 224:kind=209;break;default:return false;}
    output=kind;return true;
}
bool TryPlanFusionUmbrella269(uint16_t kind,float body,int armor,int maximum,int armorMaximum,float gold,float silver,
    bool big,bool water,bool sameRow,bool beforeDying,FusionUmbrella269Plan& output)
{
    if((kind!=916&&kind!=923&&kind!=1148)||!std::isfinite(body)||body<0||armor<0||maximum<1||armorMaximum<0||
        !std::isfinite(gold)||!std::isfinite(silver)||gold<0||silver<gold||int64_t(maximum)+armorMaximum>(std::numeric_limits<int>::max)())return false;
    const float factor=kind==1148?1.f:2.f,ratio=(float(armor)+body)/float(maximum+armorMaximum);
    FusionUmbrella269Plan next;
    if(ratio>=silver*factor&&!beforeDying){if(sameRow)next.action=FusionUmbrella269Action::Garlic;}
    else
    {
        const bool golden=ratio<gold*factor;next.coins=golden?3:1;
        if(big){next.action=FusionUmbrella269Action::DamageAndCoins;next.damage=1000000;}
        else if(water){next.action=FusionUmbrella269Action::Coins;next.retireOriginal=true;}
        else{next.action=FusionUmbrella269Action::Replace;next.kind=golden?42:41;next.coins=0;next.retireOriginal=true;}
    }
    output=next;return true;
}
bool IsFusionSummon269Money(uint16_t kind){return kind==41||kind==42;}
bool IsFusionSummon269BigKind(uint16_t kind)
{
    constexpr uint16_t kinds[]={14,16,18,31,35,36,38,39,40,43,44,46,112,200,201,210,212,216,218,219,220,221,222,223,224};
    for(auto k:kinds)if(kind==k)return true;return false;
}
bool TryStepFusionMoney269Health(uint16_t kind,float health,int maximum,float age,float delta,float& output)
{
    if(!IsFusionSummon269Money(kind)||!std::isfinite(health)||maximum<1||!std::isfinite(age)||age<0||!std::isfinite(delta)||delta<0)return false;
    float next=health;if(age>15.f){const volatile float part=delta*.1f;const volatile float loss=part*float(maximum);next=health-loss;}
    if(!std::isfinite(next))return false;output=next;return true;
}
bool TryGetFusionMoney269Reward(uint16_t kind,int& coin,int& count)
{if(!IsFusionSummon269Money(kind))return false;coin=36;count=kind==42?3:1;return true;}
bool TryGetFusionHypno269Projectile(uint16_t kind,int attack,int level,uint16_t& payload,int& damage)
{
    if((kind!=924&&kind!=1026)||attack<0||level<0)return false;
    const float value=kind==924?(float(level)*.05f+1.f)*float(attack):float(attack);
    if(!std::isfinite(value)||double(value)>(std::numeric_limits<int>::max)())return false;
    payload=14;damage=int(value);return true;
}
bool TryGetFusionPuffLove269Threshold(float body,int armor,int maximum,int armorMaximum,bool ultimate,float& output)
{
    if(!std::isfinite(body)||armor<0||maximum<1||armorMaximum<0||int64_t(maximum)+armorMaximum>(std::numeric_limits<int>::max)())return false;
    float value=(float(armor)+body)/float(maximum+armorMaximum),floor=.75f;
    if(value>.6f){if(!ultimate)return false;value=std::sqrt(value);value=std::sqrt(value);floor=.9f;}
    else value=value/.6f;
    value=std::sqrt(value);output=value<=floor?floor:value;return true;
}
bool TryUseFusionUltimateHypno269Item(int status,float countdown,int& nextStatus,float& nextCountdown)
{
    if((status!=0&&status!=4&&status!=5)||!std::isfinite(countdown)||!std::isfinite(countdown+30.f))return false;
    nextStatus=status?5:4;nextCountdown=countdown+30.f;return true;
}
namespace
{
constexpr double hypno1[]={double(.75f)},hypno2[]={double(.75f),double(.9166667f)},
    hypno4[]={double(.75f),double(.9166667f),double(1.0833334f),double(1.25f)};
const FusionShooterTimingProfile hypnoTiming[]={
    {1026,3,hypno1,2,1,true,14,"shoot"},
    {924,2,hypno1,2,1,true,14,"shoot"},
    {924,2,hypno2,2,double(1.1666666f),true,14,"shoot2"},
    {924,2,hypno4,2,1.5,true,14,"shoot3"}};
}
const FusionShooterTimingProfile* FindFusionHypno269TimingProfile(uint16_t kind,int status)
{
    if(kind==1026)return status==0?&hypnoTiming[0]:nullptr;
    if(kind!=924)return nullptr;
    return status==0?&hypnoTiming[1]:status==4?&hypnoTiming[2]:status==5?&hypnoTiming[3]:nullptr;
}
const FusionShooterTimingProfile* FindFusionHypno269TimingProfile(uint16_t kind,const char* state)
{
    if(!state)return nullptr;
    for(const auto& profile:hypnoTiming)if(profile.kind==kind&&std::strcmp(profile.visualState,state)==0)return &profile;
    return nullptr;
}
float FusionHypno269MaximumDuration(uint16_t kind,float speed)
{
    const auto* p=FindFusionHypno269TimingProfile(kind,kind==924?5:0);
    return p?FusionShooterDuration(*p,speed):0;
}
bool FusionHypno269PlantStateValid(uint16_t kind,int mode,int shot,int attribute,float idle,float shooting,float speed,uint32_t shots)
{
    if(!FindFusionHypno269TimingProfile(kind,mode)||!FindFusionHypno269TimingProfile(kind,shot)||
        !std::isfinite(idle)||idle<0||!std::isfinite(shooting)||shooting< -1||
        !std::isfinite(speed)||speed<.9f||speed>1.1f||shots>uint32_t((std::numeric_limits<int>::max)()))return false;
    if((kind==1026&&attribute!=0)||(kind==924&&attribute<0&&attribute!=-100))return false;
    const auto* p=FindFusionHypno269TimingProfile(kind,shot);
    return shooting<0||shooting<=FusionShooterDuration(*p,speed)+.011f;
}
bool FusionPuffLove269CanHit(int status,bool same,bool mc,bool captured)
{
    if(status<0||!same||mc||captured)return false;
    switch(status){case 3:case 7:case 9:case 10:case 12:case 13:case 16:case 20:case 21:return false;default:return true;}
}
bool FusionPuffLove269CanSplash(int status,int rowDelta,bool mc,bool captured)
{
    if(status<0||rowDelta<=-2||rowDelta>=2||mc||captured)return false;
    // AoeDamage.InLandAoeRange intentionally differs from Bullet.HitZombieCondition.
    switch(status){case 1:case 7:case 10:case 12:case 16:case 20:case 21:case 22:case 23:case 24:return false;default:return true;}
}
bool FusionPuffLove269CircleTouchesRect(double x,double y,double left,double bottom,double width,double height)
{
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(left)||!std::isfinite(bottom)||
        !std::isfinite(width)||!std::isfinite(height)||width<0||height<0)return false;
    const double dx=(std::max)(left-x,(std::max)(0.,x-left-width));
    const double dy=(std::max)(bottom-y,(std::max)(0.,y-bottom-height));
    return dx*dx+dy*dy<=2.25;
}
bool FusionHypno269ProjectileMetadataValid(uint16_t kind,int damage,uint8_t way)
{return kind==14&&damage>=80&&way==0;}
}
