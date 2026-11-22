#include "FusionSnowGun363Rules.h"
#include <cmath>
namespace LawnFusion {
bool FusionSnowBall363Metadata(uint16_t k,int damage,uint8_t way,ProjectileType t,ProjectileMotion m)
{return k==101&&damage==75&&way==17&&t==PROJECTILE_PEA&&m==MOTION_LOBBED;}
bool TryPlanFusionSnowGun363Shot(double x,double y,double tx,double ty,double py,FusionHostilePlanarShot& out)
{
 if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(tx)||!std::isfinite(ty)||!std::isfinite(py)||py<=0)return false;
 const double gravity=double(kFusionSnowGun363Gravity)*py;
 const double duration=std::sqrt(std::abs(2*(y-ty)/gravity));
 if(!std::isfinite(duration)||duration<=0)return false;
 // Existing actual native planar carrier stores one100Hz step, not px/sec.
 FusionHostilePlanarShot p{x,y,(tx-x)/duration*.01,0,gravity*.0001};
 if(!FusionHostilePlanarShotValid(p))return false;out=p;return true;
}
bool FusionSnowBall363AreaCell(int x,int y){return x>=-1&&x<=1&&y>=-1&&y<=1;}
bool FusionSnowBall363PlantAffected(bool flying,bool pumpkin,bool covered)
{return !flying&&(!covered||pumpkin);}
}
