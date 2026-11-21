#include "FusionWorldActors269Rules.h"
#include "FusionNativeWorldActors269.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace
{
constexpr double prismEvent[]={.5};
constexpr FusionShooterTimingProfile prismTiming={34,1.5f,prismEvent,1,1,false,0xffff,"shoot"};
}
const FusionShooterTimingProfile* FindFusionWorldActors269Timing(uint16_t kind)
{return kind==34?&prismTiming:nullptr;}
bool FusionWorld269ReferenceValid(FusionWorld269Reference r)
{ return (r.id==0)==(r.token==0); }
bool TryGetFusionPrism269Damage(int base,int count,int& damage)
{
    if(base<0||count<0||count>1024)return false;
    const int64_t value=int64_t(base)*(2+int64_t(count)*3)/2;
    if(value>std::numeric_limits<int>::max())return false;
    damage=int(value);return true;
}
bool TryGetFusionLaser269Damage(int base,bool connected,bool advanced47,int status,int& damage)
{
    if(base<0)return false;
    const int64_t value=int64_t(base)*(connected?2:1)*(advanced47?3:1)*(status==10?3:1);
    if(value>std::numeric_limits<int>::max())return false;
    damage=int(value);return true;
}
bool FusionLaser269CanAcquire(int status,bool mc,bool alive,int rowDelta,int light,bool connected,bool air)
{
    if(mc||!alive||light<0||(!connected&&std::abs(int64_t(rowDelta))>light))return false;
    return air?status==10:status!=21&&status!=12;
}
int FusionLaser269Healing(int maximum)
{ return maximum>0?std::min(int(float(maximum)*.2f),2000):0; }
bool TryAdjustFusionLaser269Energy(int before,int maximum,int cost,int& after,bool& depleted)
{
    if(maximum<=0||before<0||before>maximum)return false;
    const int64_t value=int64_t(before)-cost;
    depleted=value<=0;after=int(std::clamp<int64_t>(value,0,maximum));return true;
}
float FusionPrism269Alpha(int age)
{ return age<0?0.f:age<=20?1.f:std::max(0.f,1.f-float(age-20)*.05f); }
bool FusionPrism269Expired(int age) { return age>=50; }
bool NativeWorld269PlantStateValid(uint16_t kind,const NativeWorld269PlantState& s)
{
    if(kind!=34&&kind!=937)return false;
    if(!FusionWorld269ReferenceValid(s.receiver)||!FusionWorld269ReferenceValid(s.connectedPlant)||s.explosionDeadline<0)return false;
    for(auto r:s.zombies)if(!FusionWorld269ReferenceValid(r))return false;
    for(auto r:s.plants)if(!FusionWorld269ReferenceValid(r))return false;
    for(std::size_t i=0;i<s.zombies.size();++i)for(std::size_t j=0;j<i;++j)if(s.zombies[i].id&&s.zombies[i].id==s.zombies[j].id)return false;
    for(std::size_t i=0;i<s.plants.size();++i)for(std::size_t j=0;j<i;++j)if(s.plants[i].id&&s.plants[i].id==s.plants[j].id)return false;
    if(kind==34)return s.energy==0&&s.maximumEnergy==0&&!s.connected&&!s.main&&!s.connectedPlant.id&&s.contributors>=0&&s.contributors<=1024&&s.explosionDeadline==0&&std::all_of(s.plants.begin(),s.plants.end(),[](auto r){return !r.id;})&&std::all_of(s.zombies.begin()+1,s.zombies.end(),[](auto r){return !r.id;});
    return !s.receiver.id&&s.contributors==0&&(s.maximumEnergy==2000||s.maximumEnergy==20000)&&s.energy>=0&&s.energy<=s.maximumEnergy&&s.connected==bool(s.connectedPlant.id)&&(!s.main||s.connected)&&(s.connected||(!s.zombies[4].id&&!s.zombies[5].id));
}
bool NativeWorld269BeamStateValid(const NativeWorld269BeamState& s)
{
    return s.instanceToken&&s.ownerId&&s.ownerToken&&s.sortingRow>=0&&s.sortingRow<6&&s.ageTicks>=0&&s.ageTicks<=50&&std::isfinite(s.startX)&&std::isfinite(s.startY)&&std::isfinite(s.endX)&&std::isfinite(s.endY)&&std::abs(s.startX)<=4096&&std::abs(s.startY)<=4096&&std::abs(s.endX)<=4096&&std::abs(s.endY)<=4096&&std::isfinite(s.width)&&s.width>=.2f&&s.width<=.450001f;
}

}
