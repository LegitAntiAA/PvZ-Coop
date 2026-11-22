#pragma once
#include <cstdint>
#include <string>
class Board; class Plant; class Projectile; class Zombie;
namespace LawnFusion {
struct FusionStrongChomperBite; struct NativeProjectileData;
bool CaptureNativeTravelSteel370(Plant*,NativeProjectileData&);
void NativeTravelSteel370AfterHit(NativeProjectileData&);
bool PrepareNativeTravelUltimate370(Board*,uint8_t,std::string* error=nullptr);
void PlanNativeTravelUltimate370Bite(Plant*,Zombie*,FusionStrongChomperBite&);
int NativeTravelUltimate370Recovery(const Plant*,int);
int NativeTravelUltimate370ChomperCap(const Plant*,int);
int NativeTravelUltimate370CherryDamage(const Board*,int);
int NativeTravelUltimate370FumeDamage(const Plant*,int);
int NativeTravelUltimate370FumeCold(const Plant*,int);
int NativeTravelUltimate370TorchLimit(const Plant*);
bool NativeTravelUltimate370TorchFailure(Plant*);
bool NativeTravelUltimate370ExtraMelon(const Plant*,int sourceDraw);
void NativeTravelUltimate370MelonAfterShot(Plant*);
bool NativeTravelUltimate370MelonEnhanced(const Plant*);
int NativeTravelUltimate370CannonDamage(const Board*,uint16_t,int);
int NativeTravelUltimate370CannonReload(const Plant*,int);
int NativeTravelUltimate370ElementRecovery(const Plant*,bool fire,int ordinary);
bool RedirectNativeTravelUltimate370Damage(Plant*,int,int sourceType);
float NativeTravelUltimate370SteelKnockback(const Board*);
int NativeTravelUltimate370TurretIncoming(const Plant*,int);
}
