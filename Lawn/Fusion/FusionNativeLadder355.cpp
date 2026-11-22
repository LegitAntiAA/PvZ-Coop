#include "FusionNativeLadder355.h"
#include "FusionNativeDayZombie273.h"
#include "FusionNativeDryGround353.h"
#include "FusionNativeGarlic.h"
#include "FusionNative.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion
{
namespace
{
bool Authority(const Zombie* z)
{
    const auto* d=GetNativeSourceZombieData(z);
    return z&&d&&FusionDayZombie273Pole(d->kind)&&z->mApp&&z->mBoard&&z->mApp->mBoard==z->mBoard&&
        !z->mNetGhost&&!z->mApp->NetIsClient()&&!z->mDead&&!const_cast<Zombie*>(z)->IsDeadOrDying()&&
        NativeSourceDryRow353(z->mBoard,z->mRow);
}
bool Playing(const Zombie* z)
{return Authority(z)&&!z->mBoard->mPaused&&z->mApp->mGameScene==SCENE_PLAYING;}
}
bool ContactNativeDayLadder355(Zombie* z,Plant* p)
{
    auto* s=Playing(z)?EditNativeDayZombie273(z):nullptr;
    if(!s||!p||p->mBoard!=z->mBoard||p->mDead||!p->mIsOnBoard||p->mRow!=z->mRow||!GetNativePlantData(p))return false;
    return BeginFusionLadder355(z->mBoard->GetLadderAt(p->mPlantCol,p->mRow)!=nullptr,p->mPlantCol,p->mRow,s->ladder);
}
bool MoveNativeDayLadder355(Zombie* z)
{
    auto* s=Playing(z)?EditNativeDayZombie273(z):nullptr;if(!s)return false;
    auto next=s->ladder;bool translate=false;
    if(!StepFusionLadder355(next,z->mZombieAge,.01f,translate))return false;
    if(translate)
    {
        // Transform.Translate uses the real facing rotation, not Animator speed
        // or local scale. Board Y points down; source local Y points up.
        const float x=z->mPosX+float((z->mMindControlled?1:-1)*.01*kFusionBasePixelsX);
        const float y=z->mPosY-float(.01*GetNativeFusionPixelsY(z->mBoard));
        if(!std::isfinite(x)||!std::isfinite(y))return true;
        z->mPosX=x;z->mPosY=y;z->mX=int(x);z->mY=int(y);
    }
    s->ladder=next;return true;
}
void RecoverNativeDayLadder355(Zombie* z)
{
    auto* s=Playing(z)?EditNativeDayZombie273(z):nullptr;
    if(!s||!s->ladder.recovering||s->ladder.seconds>0||s->ladder.lastNativeAge==z->mZombieAge)return;
    const auto* garlic=GetNativeZombieGarlicData(z);
    if(garlic&&garlic->phase==GarlicActionPhase::Moving)return;
    const float target=z->GetPosYBasedOnRow(z->mRow),py=float(GetNativeFusionPixelsY(z->mBoard));
    if(!std::isfinite(target)||!std::isfinite(py)||py<=0)return;
    const float dy=target-z->mPosY;
    if(std::abs(dy)<=.03f*py){z->mPosY=target;s->ladder.recovering=false;}
    else z->mPosY+=std::copysign(.01f*py,dy);
    z->mY=int(z->mPosY);s->ladder.lastNativeAge=z->mZombieAge;
}
bool NativeDayLadderPositionOwned355(const Zombie* z)
{
    const auto* s=Authority(z)?GetNativeDayZombie273(z):nullptr;
    return s&&FusionLadder355Valid(s->ladder)&&(s->ladder.recovering||s->ladder.seconds>0||s->ladder.lastNativeAge==z->mZombieAge);
}
}
