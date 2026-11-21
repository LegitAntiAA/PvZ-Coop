#include "FusionChomper268Rules.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion
{
namespace
{
constexpr FusionStrongChomperProfile profiles[] = {
    {1013,4000,200}, {903,8000,1000}, {921,8000,1000}};
}
const FusionStrongChomperProfile* FindFusionStrongChomperProfile(uint16_t kind)
{
    for (const auto& p : profiles) if (p.kind == kind) return &p;
    return nullptr;
}
bool TryPlanFusionStrongChomperBite(uint16_t kind, float bodyHealth,
    bool firstArmorObject, int firstArmorHealth, bool charged, int randomShot,
    FusionStrongChomperBite& result)
{
    const auto* p = FindFusionStrongChomperProfile(kind);
    if (!p || !std::isfinite(bodyHealth) || bodyHealth <= 0 || firstArmorHealth < 0) return false;
    FusionStrongChomperBite next;
    if (kind == 1013)
    {
        if (firstArmorObject || bodyHealth > float(p->attackDamage)*2.5f)
        {
            if (randomShot != 4 && randomShot != 5) return false;
            next.sourceDamage = next.recovery = charged ? 1000000 : p->attackDamage;
            next.rechargeTicks = charged ? 4000 : 0;
            next.nextShotKind = static_cast<uint16_t>(randomShot);
        }
        else
        {
            next.sourceDamage = static_cast<int>(float(p->attackDamage)*2.5f);
            next.recovery = p->attackDamage*2;
            next.nextShotKind = 6;
        }
    }
    else
    {
        const bool swallow = float(firstArmorHealth)+bodyHealth <= float(p->attackDamage*2);
        next.sourceDamage = swallow ? 1000000 : p->attackDamage;
        next.recovery = swallow ? p->baseHealth : p->attackDamage;
        next.nextShotKind = kind == 921 ? 89 : 3;
    }
    result = next;
    return true;
}
bool TryGetFusionStrongChomperShot(uint16_t kind, uint16_t storedShot,
    uint16_t& projectile, uint8_t& movingWay, int& damage)
{
    const auto* p = FindFusionStrongChomperProfile(kind);
    if (!p || (kind == 1013 && storedShot != 4 && storedShot != 5 && storedShot != 6)) return false;
    projectile = kind == 1013 ? storedShot : kind == 921 ? 89 : 3;
    movingWay = kind == 1013 ? 1 : 0;
    damage = kind == 1013 ? static_cast<int>(float(p->attackDamage)*.4f) : 300;
    return true;
}
bool TryCapFusionStrongChomperDamage(uint16_t kind, int incoming, int& damage)
{
    const auto* p = FindFusionStrongChomperProfile(kind);
    if (!p || incoming < 0) return false;
    damage = (std::min)(incoming,p->baseHealth/3);
    return true;
}
bool TryDecayFusionStrongChomperHealth(uint16_t kind, int health, int& result)
{
    const auto* p = FindFusionStrongChomperProfile(kind);
    if (!p || health <= 0 || health > p->baseHealth*4) return false;
    // Called by the sole native timer after the source's strict timer < 0 gate.
    result = health <= p->baseHealth ? health :
        (std::max)(p->baseHealth,health-static_cast<int>(float(p->baseHealth)*(kind == 1013 ? .05f : .1f)));
    return true;
}
bool TryCapFusionStrongChomperHealth(uint16_t kind, int health, int& result)
{
    const auto* p = FindFusionStrongChomperProfile(kind);
    if (!p || health <= 0) return false;
    result = (std::min)(health,p->baseHealth*4);
    return true;
}
bool TryAdvanceFusionStrongChomperStage(uint16_t kind, int health,
    uint8_t previous, uint8_t& stage)
{
    const auto* p = FindFusionStrongChomperProfile(kind);
    if (!p || health <= 0 || health > p->baseHealth*4 || previous > 2) return false;
    const int low = p->baseHealth/3, high = p->baseHealth*2/3;
    stage = health > high ? 0 : health > low && health < high ? 1 : health < low ? 2 : previous;
    return true;
}
bool FusionStrongChomperTarget(int s,int row,bool mc,bool dying,bool collider)
{return collider&&!mc&&!dying&&!row&&s>=0&&s<=24&&s!=1&&s!=7&&s!=10&&s!=12&&s!=16&&s<20;}
bool TryGetFusionStrongChomperBiteTicks(float speed,int& ticks)
{if(!std::isfinite(speed)||speed<.9f||speed>1.1f)return false;ticks=int(std::ceil((2.*.75/speed+.25)*100));return true;}
bool FusionStrongChomperEventDue(float speed,int remaining,bool shoot)
{int total=0;return TryGetFusionStrongChomperBiteTicks(speed,total)&&remaining==total-int(std::ceil(double(shoot?1.65f:1.05f)/speed*100));}
bool FusionChomper268StateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t bag,uint8_t mode,uint8_t stage)
{if(!FindFusionStrongChomperProfile(kind)||rage||mode>1||stage>2)return false;return kind==1013?(bag==4||bag==5||bag==6):bag==0;}
bool FusionChomper268ClockStateValid(uint16_t k,int charge,int decay,float speed,float seconds,uint8_t mode)
{
    int total=0;if(!FindFusionStrongChomperProfile(k)||!TryGetFusionStrongChomperBiteTicks(speed,total)||!std::isfinite(seconds)||charge<0||charge>(k==1013?4000:0)||decay<0||decay>101||mode>1)return false;
    return mode==0?seconds==-1:seconds>=0&&seconds<=float(total)*.01f;
}
}
