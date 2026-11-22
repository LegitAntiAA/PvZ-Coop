#pragma once
#include <cstdint>
class Board;
namespace LawnFusion
{
constexpr uint8_t Drown343Horizontal=1,Drown343Big=2;
bool TryMakeFusionDrown343HorizontalVelocity(float draw,bool big,float& vx,float& vy);
float FusionDrown343FlashRemaining(int age,uint32_t lastFlashAge,bool longFlash);
float FusionDrown343Brightness(int age,uint32_t lastFlashAge,bool longFlash);
bool FusionDrown343BigBulletContact(uint8_t way,bool hostile,bool sameRow);
bool FusionDrown343OwnerVariant(uint16_t owner,uint8_t variant);
int NativeUltimateFootballDrownCount343(const Board*); // Source218 Start/DieEvent owner:347.
}
