#include "FusionDayZombie273Rules.h"
#include "FusionZombieMotion.h"
#include <cmath>
#include <limits>

namespace LawnFusion
{
namespace
{
constexpr FusionZombieProfile profiles[]={
    {3,720,0,0,50,false},{6,720,0,0,50,false},{7,270,0,0,50,false},{21,2000,4400,0,50,false},
    {22,2000,2200,0,50,false},{23,2000,1100,0,50,false},
    {100,270,0,0,50,false},{106,270,2200,0,50,false},
    {107,270,1100,0,50,false},{108,1350,0,0,50,false}};
constexpr uint16_t danceDeps[]={7},jacksonDeps[]={6,7};
constexpr uint16_t diamondDeps[]={22,23,2},goldDeps[]={23,2},silverDeps[]={2};
struct RootKey {float time,value,inSlope,outSlope;};
// Actual Pole controller10f6cfec... clips. Values and Hermite slopes are from
// source-clips.json, NOT the similarly named unused/dance animation clips.
constexpr RootKey run[]={ {0,1.9616f,0,0},{3,1.5680001f,-1.1175871e-8f,0} };
constexpr RootKey jump[]={ {0,4.1048f,0,0},{3.5f,2.2784f,2.6077032e-7f,0} };
constexpr RootKey walk[]={
    {0,5.131f,0,0},{.35f,5.0848f,-.23107746f,-.23107749f},
    {.65f,4.9807997f,-.3419996f,-.34199956f},{1.0166667f,4.8568f,3.7498587e-8f,0},
    {1.3f,4.9272f,-1.02942316e-7f,0},{1.6833333f,4.8287997f,-.2923634f,-.29236338f},
    {2.0333333f,4.7128f,-.33726257f,-.33726266f},{2.3166666f,4.6152f,-.28676912f,-.2867691f},
    {2.6833334f,4.5264f,4.0772832e-8f,0},{2.8333333f,4.5344f,.10666531f,.106665306f},
    {2.9833333f,4.5592f,.0064011076f,.0064010634f},{3.3333333f,4.56f,4.191303e-10f,0}};

// Actual DancePole6 walk uses its own source root curve; kind7 shares Pole3.
constexpr RootKey danceWalk[]={
    {0,5.131f,0,0},
    {0.35f,5.0512f,-0.26923028f,-0.26923034f},
    {0.65f,4.9560003f,-0.3155994f,-0.31559947f},
    {1.0166667f,4.8408003f,4.5058783e-08f,0},
    {1.3f,4.8888f,2.6227122e-08f,0},
    {1.6833333f,4.7784f,-0.303273f,-0.303273f},
    {2.0333333f,4.6664f,-0.35494748f,-0.35494754f},
    {2.3166666f,4.5536f,-0.36685672f,-0.3668567f},
    {2.5f,4.4952f,-0.24436378f,-0.24436378f},
    {2.6833334f,4.4639997f,-5.983824e-10f,0},
    {2.9833333f,4.4751997f,0.046769667f,0.046769667f},
    {3.3333333f,4.4944f,-1.0605386e-08f,0}};
bool SpanValid(FusionZombieClipPosition a,FusionZombieClipPosition b,float duration,bool loop=true)
{
    return std::isfinite(a.seconds)&&std::isfinite(b.seconds)&&a.seconds>=0&&b.seconds>=0&&
        a.seconds<=duration&&b.seconds<=duration&&b.loop>=a.loop&&
        (b.loop!=a.loop||b.seconds>=a.seconds)&&(loop||(!a.loop&&!b.loop));
}
float Sample(std::span<const RootKey> keys,float seconds)
{
    if(seconds<=keys.front().time)return keys.front().value;
    if(seconds>=keys.back().time)return keys.back().value;
    size_t i=1;while(keys[i].time<=seconds)++i;
    const auto& a=keys[i-1];const auto& b=keys[i];
    const float d=b.time-a.time,t=(seconds-a.time)/d,t2=t*t,t3=t2*t;
    return (2*t3-3*t2+1)*a.value+(t3-2*t2+t)*d*a.outSlope+
        (-2*t3+3*t2)*b.value+(t3-t2)*d*b.inSlope;
}
bool Count(FusionZombieClipPosition a,FusionZombieClipPosition b,float duration,std::span<const float> events,uint32_t& out)
{
    if(!SpanValid(a,b,duration))return false;
    uint64_t count=uint64_t(b.loop-a.loop)*events.size();
    for(float e:events){if(a.seconds>=e)--count;if(b.seconds>=e)++count;}
    if(count>(std::numeric_limits<uint32_t>::max)())return false;
    out=uint32_t(count);return true;
}
int WrappedTwoThirds(int maximum)
{
    const uint32_t twice=uint32_t(maximum)*2U;
    const int64_t signedTwice=twice&0x80000000U?int64_t(twice)-0x100000000LL:twice;
    return int(signedTwice/3);
}
}
const FusionZombieProfile* FindFusionDayZombie273Profile(uint16_t kind)
{for(const auto& p:profiles)if(p.kind==kind)return &p;return nullptr;}
bool FusionDayZombie273Pole(uint16_t kind){return kind==3||kind==6||kind==7;}
bool FusionDayZombie273Doll(uint16_t kind){return kind==21||kind==22||kind==23;}
bool FusionDayZombie273Shooter(uint16_t kind){return kind==100||kind==108;}
std::span<const uint16_t> FusionDayZombie273NpcDependencies(uint16_t kind)
{
    if(kind==10)return jacksonDeps;if(kind==6)return danceDeps;if(kind==21)return diamondDeps;if(kind==22)return goldDeps;if(kind==23)return silverDeps;
    return {};
}
uint32_t FusionDayZombie273RequiredCapabilities(uint16_t kind)
{
    if(!FindFusionDayZombie273Profile(kind))return 0;
    uint32_t result=Day273Model|Day273Wire|Day273SharedBody|Day273BodyParts;
    if(FusionDayZombie273Pole(kind))return result|Day273Pole|Day273PlantContact|Day273Fx;
    if(FusionDayZombie273Doll(kind))return result|Day273Armor|Day273DollFall|Day273Fx|Day273HeadClock;
    if(kind==106||kind==107)return result|Day273Armor|(kind==106?Day273BucketItem:0);
    return result|Day273HeadClock|Day273PlantContact|Day273Fx|(kind==100?Day273Bullet0:Day273Bullet11|Day273BucketItem);
}
bool FusionDayZombie273CapabilitiesReady(uint16_t kind,uint32_t actual)
{const auto needed=FusionDayZombie273RequiredCapabilities(kind);return needed&&(actual&needed)==needed;}
bool FusionDayZombie273Ready(uint16_t kind,uint32_t actual,std::span<const uint16_t> ready)
{
    if(!FusionDayZombie273CapabilitiesReady(kind,actual))return false;
    for(uint16_t dependency:FusionDayZombie273NpcDependencies(kind))
    {
        bool found=false;for(uint16_t npc:ready)if(npc==dependency){found=true;break;}
        if(!found)return false;
    }
    return true;
}
bool TryGetFusionDayZombie273Clip(uint16_t kind,int status,bool eating,FusionDayZombie273Clip& out)
{
    FusionDayZombie273Clip c;
    if(FusionDayZombie273Pole(kind))
    {
        if(status==2)c={"walk","anim_run",3,2.5f,true};
        else if(status==3)c={"jump","anim_jump",3.5f,2,false};
        else if(status==0)c=eating?FusionDayZombie273Clip{"attack","anim_eat",2.25f,1,true}:
            FusionDayZombie273Clip{"walk2","anim_walk",3.3333333f,.5f,true};
        else if(status==1)c={"die","anim_death",2.1666667f,kind==6?2.f:1.8f,false};
        else return false;
        // The staged Pole6/7 Animator serializes original clip names.
        if(kind!=3)c.sourceName=status==2?"Zombie_polevaulter_run":status==3?
            (kind==6?"Zombie_polevaulter_jump 1":"Zombie_polevaulter_jump"):status==1?
            "Zombie_polevaulter_die":eating?"Zombie_polevaulter_eat":"Zombie_polevaulter_walk";
    }
    else if(FusionDayZombie273Shooter(kind)||kind==106||kind==107)
    {
        if(status==1)c={"die","anim_death",1.3333334f,1,false};
        else if(status==0)c=eating?FusionDayZombie273Clip{"attack","anim_eat",1.35f,1,true}:
            FusionDayZombie273Clip{"walk","anim_walk2",1.9166666f,.5f,true};
        else return false;
    }
    else return false;
    out=c;return true;
}
bool TryGetFusionDayZombie273RootDelta(uint16_t kind,int status,FusionZombieClipPosition before,
    FusionZombieClipPosition after,bool right,double projection,double& out)
{
    if(!std::isfinite(projection)||projection<0)return false;
    if(FusionDayZombie273Shooter(kind)||kind==106||kind==107)
        return status==0&&TryGetFusionZombieRootDelta(0,true,before,after,right,out,projection);
    if(!FusionDayZombie273Pole(kind))return false;
    std::span<const RootKey> keys;float duration=0;
    if(status==2){keys=run;duration=3;}else if(status==3){keys=jump;duration=3.5f;}
    else if(status==0){keys=kind==6?std::span<const RootKey>(danceWalk):std::span<const RootKey>(walk);duration=3.3333333f;}else return false;
    // A native PLAY_ONCE carrier may report loopCount1 at its final held frame.
    if(status==3&&after.loop==1&&after.seconds==duration)after.loop=0;
    if(!SpanValid(before,after,duration,status!=3))return false;
    const double raw=double(after.loop-before.loop)*(double(keys.back().value)-keys.front().value)+
        double(Sample(keys,after.seconds))-Sample(keys,before.seconds);
    const double delta=raw*projection*(right?-1:1);
    if(!std::isfinite(delta))return false;out=delta;return true;
}
bool TryCountFusionDayZombie273Bites(uint16_t kind,FusionZombieClipPosition a,FusionZombieClipPosition b,uint32_t& count)
{
    if(!FusionDayZombie273Pole(kind))return false;
    constexpr float events[]={.98333335f,2.f};return Count(a,b,2.25f,events,count);
}
bool TryGetFusionDayZombie273PoleEvents(FusionZombieClipPosition a,FusionZombieClipPosition b,uint8_t& out)
{
    if(b.loop==1&&b.seconds==3.5f)b.loop=0;
    if(!SpanValid(a,b,3.5f,false))return false;
    constexpr float times[]={.75f,1.5f,2.f,3.5f};uint8_t events=0;
    for(int i=0;i<4;++i)if(a.seconds<times[i]&&b.seconds>=times[i])events|=uint8_t(1U<<i);
    out=events;return true;
}
bool FusionDayZombie273PoleCanStart(int status,bool controlled,bool frozen,bool sameRow,bool ahead,
    const FusionDayZombie273PlantTraits& p)
{return status==2&&!controlled&&!frozen&&sameRow&&ahead&&!p.caltrop&&!p.flying&&!p.ladder;}
bool FusionDayZombie273PoleBlocksLanding(bool sameRow,const FusionDayZombie273PlantTraits& p)
{return sameRow&&p.tallNut&&!p.ladder;}
bool TryStepFusionDayZombie273Shoot(uint16_t kind,float before,float interval,float delta,int status,
    bool stopped,bool pending,float jitter,float& after,bool& play)
{
    if(!FusionDayZombie273Shooter(kind)||!std::isfinite(before)||!std::isfinite(interval)||interval<=0||
        !std::isfinite(delta)||delta<0||delta>1||!std::isfinite(jitter)||jitter<-.1f||jitter>.1f)return false;
    float next=before;bool due=false;
    if(status!=1&&!stopped&&!pending){next-=delta;if(next<0){next=interval+jitter;due=true;}}
    after=next;play=due;return true;
}
bool TryMakeFusionDayZombie273Shot(uint16_t kind,int status,bool pending,bool changing,int move,bool controlled,FusionDayZombie273Shot& out)
{
    if(!FusionDayZombie273Shooter(kind)||status==1||pending||changing||(move!=0&&move!=1))return false;
    FusionDayZombie273Shot shot;shot.kind=kind==100?0:11;shot.damage=kind==100?20:80;
    shot.way=move==0?9:0;shot.hostile=!controlled;shot.offsetX=move==0?-.4f:.4f;out=shot;return true;
}
bool FusionDayZombie273ProjectileTuple(uint16_t kind,int damage,uint8_t way,bool hostile)
{
    if((kind!=0&&kind!=11)||(way!=0&&way!=9))return false;
    const int original=kind==0?20:80;
    return damage==original||(!hostile&&damage==original*2);
}
bool FusionDayZombie273ProjectileCanHit(int status)
{
    // Bullet.HitZombieCondition422BB0, ordinary way0/backwards way9.
    return status>=0&&status!=1&&status!=3&&status!=7&&status!=9&&status!=10&&
        status!=12&&status!=13&&status!=16&&status!=20&&status!=21;
}
bool TryPlanFusionDayZombie273ZombieHit(uint16_t kind,uint8_t way,int damage,int secondArmor,bool controlled,FusionDayZombie273ZombieHit& out)
{
    if((kind!=0&&kind!=11)||(way!=0&&way!=9)||damage<0||secondArmor<0)return false;
    FusionDayZombie273ZombieHit hit;hit.damage=damage;
    if(kind==0)hit.sourceDamageType=way==9?4:0;
    else
    {
        const uint32_t sum=uint32_t(damage)+uint32_t(secondArmor);
        hit.damage=int(sum&0x80000000U?int64_t(sum)-0x100000000LL:int64_t(sum));
        hit.knockback=controlled?-.2f:.2f;
    }
    out=hit;return true;
}
bool TryGetFusionDayZombie273ArmorStage(uint16_t kind,int health,int maximum,uint8_t previous,uint8_t& stage,int& sprite)
{
    if((!FusionDayZombie273Doll(kind)&&kind!=106&&kind!=107)||maximum<=0||health<0||health>maximum||previous>2)return false;
    uint8_t next=previous;
    if(kind==107)
    {
        const uint32_t twice=uint32_t(maximum)*2U;
        const int64_t wrapped=twice&0x80000000U?int64_t(twice)-0x100000000LL:twice;
        if(float(health)<float(wrapped)/3.f&&next<1)next=1;
        if(float(health)<float(maximum)/3.f&&next<2)next=2;
    }
    else
    {
        if(health<WrappedTwoThirds(maximum)&&next<1)next=1;
        if(health<maximum/3&&next<2)next=2;
    }
    stage=next;
    // GameAPP.zombieSprite indices:21=44/45,22=46/47,23=48/49,
    // BucketNut=14/15, CherryNut=16/17. stage0 retains original prefab sprite.
    const int base=FusionDayZombie273Doll(kind)?44+(kind-21)*2:kind==106?14:16;
    sprite=next?base+int(next)-1:-1;return true;
}
bool TryStepFusionDayZombie273Armor(uint16_t kind,int before,int damage,int& after,int& overkill)
{
    if((!FusionDayZombie273Doll(kind)&&kind!=106&&kind!=107)||before<0||damage<0)return false;
    after=before>damage?before-damage:0;overkill=damage>before?damage-before:0;return true;
}
bool TryGetFusionDayZombie273DollChild(uint16_t kind,uint16_t& child)
{if(!FusionDayZombie273Doll(kind))return false;child=kind==21?22:kind==22?23:2;return true;}
bool FusionDayZombie273DollCanFall(int live,int capacity)
{return live>=0&&capacity>1&&live<capacity-1;}
}
