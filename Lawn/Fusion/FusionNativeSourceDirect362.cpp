#include "FusionNativeSourceDirect362.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNative.h"
#include "../Board.h"
#include "../../LawnApp.h"
namespace LawnFusion {
bool UseNativeSourceDirect362(const Board* b)
{
 const auto* c=GetNativeContext(b);
 return IsNativeSourceWorld313(b)&&c&&b->mApp->mGameMode==GAMEMODE_ADVENTURE&&
  (c->adventure.enabled||c->validation)&&!const_cast<Board*>(b)->IsPvPMode()&&
  !b->mApp->IsCoopEndlessMode(b->mApp->mGameMode);
}
}
