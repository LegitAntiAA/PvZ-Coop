#include "FusionNativeTravelBuff369.h"
#include "FusionNative.h"
#include "../Board.h"
#include "../../LawnApp.h"
namespace LawnFusion {
const FusionTravelBuff360Ledger* GetNativeTravelBuff369(const Board* b)
{return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeFusionBoard(b)?&b->mApp->mFusionTravelBuff369:nullptr;}
FusionTravelBuff360Ledger* EditNativeTravelBuff369(Board* b)
{return GetNativeTravelBuff369(b)&&!b->mApp->NetIsClient()?&b->mApp->mFusionTravelBuff369:nullptr;}
void UpdateNativeTravelBuffCooldown369(Board* b)
{
 auto* c=GetNativeContext(b);if(!c||!b->mApp||b->mApp->NetIsClient()||b->mPaused||b->mApp->mGameScene!=SCENE_PLAYING)return;
 if(c->travelBuffTick369==b->mMainCounter)return;c->travelBuffTick369=b->mMainCounter;
 float next=0;if(StepFusionTravelBuff360Cooldown(c->travelBuffCooldown369,.01f,next))c->travelBuffCooldown369=next;
}
}
