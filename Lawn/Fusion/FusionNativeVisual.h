#ifndef LAWN_FUSION_NATIVE_VISUAL_H
#define LAWN_FUSION_NATIVE_VISUAL_H

#include "../../SexyAppFramework/Rect.h"
#include <cstdint>
#include <array>
#include <memory>
#include <span>
#include <string>

class Board;
class Coin;
class LawnApp;
class Plant;
class Projectile;
namespace Sexy { class Graphics; }

namespace LawnFusion
{
enum class FusionCompanion269Family:uint8_t;
enum class NativeWorldRemainder269Family:uint8_t;
// Board-owned asset cache. It owns no entities, clocks, gameplay or camera.
class NativeVisuals
{
public:
    NativeVisuals();
    ~NativeVisuals();
    NativeVisuals(const NativeVisuals&) = delete;
    NativeVisuals& operator=(const NativeVisuals&) = delete;
    bool Load(LawnApp* app, std::string& error);
    // Complete source objects are staged before publication. Native-art kinds
    // deliberately require no source model; an unknown binding fails closed.
    bool Ensure(std::span<const uint16_t> plantKinds, std::span<const uint16_t> projectileKinds, std::string& error);
    bool EnsurePlant(uint16_t kind, std::string& error);
    bool EnsureProjectile(uint16_t kind, std::string& error);
    bool EnsurePortrait(uint16_t kind, std::string& error);
    bool EnsureEquipment(uint8_t factoryType,std::string& error);
    bool DrawEquipment(Sexy::Graphics* graphics,Coin* coin) const;
    bool EnsureFertilize269(std::string& error);
    bool DrawFertilize269(Sexy::Graphics*,Coin*) const;
    bool EnsureCompanion(FusionCompanion269Family family,bool smallVariant,std::string& error);
    bool DrawCompanion(Sexy::Graphics*,Projectile*) const;
    bool GetCompanionNodePoint(const Projectile*,const char* node,float& x,float& y) const;
    bool EnsureWorldRemainderAux(NativeWorldRemainder269Family,uint8_t type,std::string& error);
    bool DrawWorldRemainderAux(Sexy::Graphics*,Projectile*) const;
    bool GetWorldRemainderNodePoint(const Projectile*,const char*,float&,float&) const;
    bool GetWorldRemainderNodeActivity(const Projectile*,const char*,bool&) const;
    bool EnsureLaser269(std::string& error);
    bool DrawLaser269(Sexy::Graphics*,Plant*,int sortingRow=-1) const;
    bool GetLaser269NodePoint(const Plant*,const char*,float&,float&) const;
    bool GetLaser269NodeActivity(const Plant*,const char*,bool&) const;
    bool GetSourceProjectileNodePoint(const Projectile*,const char*,float&,float&) const;
    bool GetSourceProjectileNodeActivity(const Projectile*,const char*,bool&) const;
    // Call at a safe frame boundary after metadata pruning, never inside Draw.
    // Full generations including dying/impact tails keep their dependencies.
    void ReleaseUnused(Board* board);
    void UpdatePrewarm(Board* board);
    bool BeginWorldFrame(Sexy::Graphics*,Board*);
    void EndWorldFrame(Board*);
    bool PlantOverlayOrder(Plant*,int&) const;
    void ConfirmPlantOverlayQueued(Plant*);
    bool DrawPlantOverlay(Sexy::Graphics*,Plant*) const;
    bool DrawPlant(Sexy::Graphics* graphics, Plant* plant) const;
    bool GetPlantNodePoint(const Plant* plant,const char* path,float& x,float& y) const;
    bool GetPlantNodeActivity(const Plant*,const char* path,bool& active) const;
    bool GetPlantNodeTransform(const Plant* plant,const char* path,std::array<float,6>& out) const;
    bool DrawPumpkinBack(Sexy::Graphics* graphics, Plant* pumpkin) const;
    bool DrawProjectile(Sexy::Graphics* graphics, Projectile* projectile) const;
    bool DrawPortrait(Sexy::Graphics* graphics, uint16_t kind, const Sexy::Rect& bounds, int grayness, bool preserveTint = false) const;
    bool DrawField(Sexy::Graphics* graphics, Board* board, bool useClassicArt);

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

bool InitializeNativeVisuals(Board* board, std::string* error = nullptr);
// Host calls before allocation/parent effects; client calls after full body/FNS
// validation and before any mutable snapshot decode. These consume no game RNG.
bool EnsureNativeVisuals(Board* board, std::span<const uint16_t> plantKinds,
    std::span<const uint16_t> projectileKinds, std::string* error = nullptr);
bool EnsureNativePlantVisuals(Board* board, uint16_t kind, std::string* error = nullptr);
// Bounded source resources reject states whose particle system is not yet native-supported.
bool NativePlantVisualStateSupported(const Plant*,std::string* error=nullptr);
bool EnsureNativeProjectileVisuals(Board* board, uint16_t kind, std::string* error = nullptr);
bool EnsureNativePortraitVisuals(Board* board, uint16_t kind, std::string* error = nullptr);
// Main resource thread, once per rendered frame, outside Draw/snapshot mutation.
// Intro/paused only: one complete resource per call, selected directed recipes only.
// Speculative failures never veto ordinary synchronous resource admission.
void UpdateNativeVisualPrewarm(Board* board);
void ReleaseUnusedNativeVisuals(Board* board);
// False delegates to the existing native Plant/SeedPacket renderer. Only the
// registered source plants are handled here; remaining native art stays native.
bool DrawNativeFusionPlant(Sexy::Graphics* graphics, Plant* plant);
bool DrawNativeFertilize269(Sexy::Graphics*,Coin*);
// Board DrawGameObjects owns this scope. Begin before constructing its native
// render list; End with a stack guard on every exit. Existing MAGNET_ITEMS can
// carry the filtered source overlay at the returned native render order.
bool BeginNativeFusionWorldFrame(Sexy::Graphics*,Board*);
void EndNativeFusionWorldFrame(Board*);
bool GetNativeFusionPlantOverlayOrder(Plant*,int& order);
// Call only after the native render item was actually appended.
void ConfirmNativeFusionPlantOverlayQueued(Plant*);
bool DrawNativeFusionPlantOverlay(Sexy::Graphics*,Plant*);
// Board coordinates from the actual current source pose, shared with Draw.
bool GetNativeSourcePlantNodePoint(const Plant* plant,const char* path,float& x,float& y);
bool GetNativeSourcePlantNodeActivity(const Plant*,const char* path,bool& active);
// Board=(a*x+c*y+tx,b*x+d*y+ty), input is parent-local source units.
bool GetNativeSourcePlantNodeTransform(const Plant* plant,const char* path,std::array<float,6>& out);
float GetNativeSourcePixelsPerWorldUnit();
bool DrawNativeFusionEquipment(Sexy::Graphics* graphics,Coin* coin);
bool EnsureNativeCompanion269Visual(Board*,FusionCompanion269Family,bool smallVariant,std::string* error=nullptr);
bool DrawNativeCompanion269(Sexy::Graphics*,Projectile*);
bool GetNativeCompanion269NodePoint(const Projectile*,const char* node,float& x,float& y);
// Derived external source LightBall; Draw uses native Plant-local Graphics.
bool EnsureNativeLaser269Visuals(Board*,std::string* error=nullptr);
bool DrawNativeLaser269Visuals(Sexy::Graphics*,Plant*,int sortingRow=-1);
// Source components0=ball+two connectors,1..8=target segments. Deduplicate
// returned rows in the native render list, then Draw(...,row) once per row.
bool GetNativeLaser269VisualRow(const Plant*,unsigned component,int& sortingRow);
bool GetNativeLaser269NodePoint(const Plant*,const char*,float&,float&);
uint16_t GetNativeEmbedded269DerivedPlantModelKey(const Plant*);
bool GetNativeSourceDerivedPlant269NodePoint(const Plant*,const char*,float&,float&);
bool GetNativeSourceDerivedPlant269NodeActivity(const Plant*,const char*,bool&);
bool EnsureNativeWorldRemainder269AuxVisual(Board*,NativeWorldRemainder269Family,uint8_t type,std::string* error=nullptr);
bool DrawNativeWorldRemainder269Aux(Sexy::Graphics*,Projectile*);
bool GetNativeWorldRemainder269Source91Rect(const Projectile*,Sexy::Rect&);
bool NativeWorldRemainder269Source91Touches(const Projectile*,const Sexy::Rect&);
uint16_t GetNativeEmbedded269ProjectileModelKey(const Projectile*);
bool GetNativeSourceProjectile269NodePoint(const Projectile*,const char*,float&,float&);
bool GetNativeSourceProjectile269NodeActivity(const Projectile*,const char*,bool&);
// Called in the native back-owner pass, with graphics local to this pumpkin.
bool DrawNativeFusionPumpkinBack(Sexy::Graphics* graphics, Plant* pumpkin);
bool DrawNativeFusionProjectile(Sexy::Graphics* graphics, Projectile* projectile);
bool DrawNativeFusionPortrait(Sexy::Graphics* graphics, uint16_t kind,
    const Sexy::Rect& bounds, int grayness = 255, bool preserveTint = false);
// DrawBackdrop calls this only for its native Fusion10x5 day field. The caller
// supplies the existing Board graphics transform; this adds no camera or world.
bool DrawNativeFusionField(Sexy::Graphics* graphics, Board* board, bool useClassicArt);
}

#endif
