#include "FusionNativeProjection313.h"
#include "FusionNative.h"
#include "FusionBasePlantRules.h"
#include "FusionNativeZombieVisual.h"
#include "../Zombie.h"
#include "../Board.h"
#include "../../LawnApp.h"
#include <cmath>
#include <cstring>
namespace LawnFusion
{
namespace
{
bool World(const Board* b)
{return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeFusionBoard(b)&&NativeFusionBackgroundValid(uint8_t(b->mBackground))&&b->GetColumnCount()==10;}
bool GroundCompensates(const Zombie* z)
{
 auto* actor=const_cast<Zombie*>(z);
 // Same original Zombie.GetShadowDrawParams compensation branch11919.
 return z->mZombieHeight==HEIGHT_UP_LADDER||z->mZombieHeight==HEIGHT_FALLING||z->mZombiePhase==PHASE_IMP_GETTING_THROWN||
     z->mZombieType==ZOMBIE_BUNGEE||actor->IsBouncingPogo()||actor->IsFlying();
}
}
bool NativeSourceRowLandY313(const Board* b,float x,int row,float& out)
{
 if(!World(b)||!std::isfinite(x)||row<0||row>=GetNativeFusionRowCount(b))return false;
 const bool roof=const_cast<Board*>(b)->StageHasRoof();
 float y=2.3f-row*((roof||GetNativeFusionRowCount(b)==6)?1.45f:1.67f);
 if(roof&&x<=1.5f)y+=x*.22f-.33f;
 if(!std::isfinite(y))return false;out=y;return true;
}
bool NativeSourceBounds313(const Board* b,float& low,float& high,float& top)
{
 if(!World(b))return false;
 // Board.Awake180312620: boardMinX=-10, zombieMaxX=last Mouse columnX+4,
 // boardMaxY(+0xB4)=0x40D00000 (6.5). Saved primary-bounds291.json.
 low=-10;high=-4.8f+(b->GetColumnCount()-1)*1.35f+4;
 top=6.5f;return true;
}
bool NativeSourceGroundAnchor313(const Zombie* z,float& x,float& y)
{
 if(!z||!World(z->mBoard)||!GetNativeSourceZombieData(z)||!std::isfinite(z->mAltitude))return false;
 float px=0,py=0;if(!const_cast<Zombie*>(z)->GetShadowCenter(px,py))return false;
 if(!GroundCompensates(z))py+=z->mAltitude;
 if(!std::isfinite(px)||!std::isfinite(py))return false;x=px;y=py;return true;
}
bool NativeBoardPointToSource313(const Board* b,int row,float ground,float x,float y,float& sx,float& sy)
{
 if(!World(b)||!std::isfinite(ground)||!std::isfinite(x)||!std::isfinite(y)||row<0||row>=GetNativeFusionRowCount(b))return false;
 const float sourceX=(x-const_cast<Board*>(b)->GetCellCenterX(0,row))/float(kFusionBasePixelsX)-4.8f;float land=0;
 if(!NativeSourceRowLandY313(b,sourceX,row,land))return false;
 const float sourceY=land+(ground-y)/float(GetNativeFusionPixelsY(b));
 if(!std::isfinite(sourceX)||!std::isfinite(sourceY))return false;sx=sourceX;sy=sourceY;return true;
}
bool NativeSourcePointToBoard313(const Board* b,int row,float ground,float sx,float sy,float& x,float& y)
{
 if(!World(b)||!std::isfinite(ground)||!std::isfinite(sx)||!std::isfinite(sy)||row<0||row>=GetNativeFusionRowCount(b))return false;
 float land=0;if(!NativeSourceRowLandY313(b,sx,row,land))return false;
 const float px=float(const_cast<Board*>(b)->GetCellCenterX(0,row)+(sx+4.8f)*kFusionBasePixelsX),py=ground+(land-sy)*float(GetNativeFusionPixelsY(b));
 if(!std::isfinite(px)||!std::isfinite(py))return false;x=px;y=py;return true;
}
bool NativeSourcePoint313(const Zombie* z,const char* path,float& x,float& y)
{
 if(!z||!path)return false;float px=0,py=0,gx=0,gy=0;
 const char* actual=!std::strcmp(path,"axis")?"Shadow":path;
 if(!GetNativeSourceZombie269NodePoint(z,actual,px,py)||!NativeSourceGroundAnchor313(z,gx,gy))return false;
 return NativeBoardPointToSource313(z->mBoard,z->mRow,gy,px,py,x,y);
}
bool SetNativeSourceAxis313(Zombie* z,float x,float y)
{
 if(!z||z->mZombieType!=ZOMBIE_NORMAL||!World(z->mBoard)||z->mNetGhost||z->mApp->NetIsClient()||z->mDead||
     !std::isfinite(x)||!std::isfinite(y))return false;
 float oldX=0,oldY=0,gx=0,gy=0,desiredX=0,desiredY=0;
 if(!GetNativeSourceZombie269NodePoint(z,"Shadow",oldX,oldY)||!NativeSourceGroundAnchor313(z,gx,gy)||
    !NativeSourcePointToBoard313(z->mBoard,z->mRow,gy,x,y,desiredX,desiredY))return false;
 const float nextX=z->mPosX+desiredX-oldX;
 const float nextY=z->mBoard->GetPosYBasedOnRow(nextX+40,z->mRow)-30;
 float land=0;if(!NativeSourceRowLandY313(z->mBoard,x,z->mRow,land))return false;
 const float nextAltitude=z->mAltitude+(oldY-gy)+(y-land)*float(GetNativeFusionPixelsY(z->mBoard));
 if(!std::isfinite(nextX)||!std::isfinite(nextY)||!std::isfinite(nextAltitude)||std::abs(nextX)>1000000||std::abs(nextY)>1000000||std::abs(nextAltitude)>1000000)return false;
 z->mPosX=nextX;z->mPosY=nextY;z->mX=int(nextX);z->mY=int(nextY);z->mAltitude=nextAltitude;return true;
}
}
