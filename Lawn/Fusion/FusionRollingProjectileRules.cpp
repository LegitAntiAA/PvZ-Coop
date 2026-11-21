#include "FusionRollingProjectileRules.h"
#include "FusionCherryShooterRules.h"
#include <cmath>

namespace LawnFusion
{
bool AdvanceFusionRollingMotion(const FusionRollingMotionProfile& profile, float& x, float& z, float& velocity, float& rotation)
{
    if (!std::isfinite(x) || !std::isfinite(z) || !std::isfinite(velocity) || !std::isfinite(rotation) || z > 0 || velocity < 0 ||
        !std::isfinite(profile.velocityX) || !std::isfinite(profile.accelerationZ) || profile.accelerationZ < 0 ||
        !std::isfinite(profile.rotationPerTick)) return false;
    x += profile.velocityX;
    if (z < 0 || velocity > 0)
    {
        velocity += profile.accelerationZ;
        z += velocity;
        if (z > 0) { z = 0; velocity = 0; }
    }
    else velocity = 0;
    rotation += profile.rotationPerTick;
    return true;
}

int FusionNutShooterDamageStage(int health)
{
    // Source max4000, strict integer-division comparisons. Equality at1333
    // deliberately returns intact, just as equality at2666 does.
    if (health < 4000 / 3) return 2;
    return health > 4000 / 3 && health < (2 * 4000) / 3 ? 1 : 0;
}

bool TryMakeFusionRollingRect(double x, double y, FusionRollingRect& result)
{return TryMakeFusionRollingRect(2,x,y,result);}
bool TryMakeFusionRollingRect(uint16_t kind,double x,double y,FusionRollingRect& result)
{
    if (!std::isfinite(x) || !std::isfinite(y)||(kind!=2&&kind!=4&&kind!=5&&kind!=6)) return false;
    const double width = (kind==2?.358:.537) * kFusionRollingPixelsX, height = (kind==2?.417:.6255) * kFusionRollingPixelsY;
    result = {x - width / 2, y - height / 2, width, height};
    return true;
}

bool FusionRollingProjectileExpired(double x, double y, int ticks, double minX, double maxX, double minY, double maxY)
{
    return FusionCherryProjectileExpired(x, y, ticks, minX, maxX, minY, maxY);
}

bool FusionRollingProjectileCanHit(int status, bool sameRow, bool mindControlled, bool flying, float health)
{
    if (status < 0 || !sameRow || mindControlled || flying || !std::isfinite(health) || health <= 0) return false;
    switch (status)
    {
    case 3: case 9: case 10: case 12: case 13: case 16: case 20: case 21: return false;
    default: return true; // Source movingWay1 permits7; family mapping is separate.
    }
}

bool RememberFusionRollingHit(FusionRollingHitHistory& history, uint32_t projectile, uint32_t zombie)
{
    return projectile && zombie && history[projectile].insert(zombie).second;
}
std::size_t FusionProjectileHitCount(const FusionRollingHitHistory& history, uint32_t projectile)
{
    const auto found = history.find(projectile);
    return found == history.end() ? 0 : found->second.size();
}
}
