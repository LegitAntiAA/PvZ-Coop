#ifndef LAWN_FUSION_NATIVE_ZOMBIE_VISUAL_H
#define LAWN_FUSION_NATIVE_ZOMBIE_VISUAL_H

#include <cstdint>
#include <memory>
#include <string>
#include <array>

class Board;
class LawnApp;
class Zombie;
namespace Sexy { class Graphics; }

namespace LawnFusion
{
struct FusionAnimationBoxCollider303;
// Board-owned source body resources only. No actors, simulation or phase clock.
class NativeZombieVisuals
{
public:
    NativeZombieVisuals();
    ~NativeZombieVisuals();
    NativeZombieVisuals(const NativeZombieVisuals&) = delete;
    NativeZombieVisuals& operator=(const NativeZombieVisuals&) = delete;
    bool Ensure(LawnApp* app, uint16_t kind, std::string& error);
    void ReleaseUnused(Board* board);
    // Called for the body inside Zombie::Draw, in BeginDraw entity coordinates.
    // Native shadow/ice/butter/attachments/health UI remain in their own passes.
    bool Draw(Sexy::Graphics* graphics, Zombie* zombie) const;
    bool Node(const Zombie*,const char* path,float& x,float& y,bool* active=nullptr,bool actualBodyPose=false) const;
    bool CanQueryRoot(uint16_t kind) const;
    bool Box313(const Zombie*,FusionAnimationBoxCollider303&) const;
    bool Matrix313(const Zombie*,const char*,std::array<float,6>&) const;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

// Factory admission and validated client preflight, before mutable world work.
// Day273 body packages still require their exact damaged sprites and embedded
// particle resources; a staged body export alone does not satisfy this gate.
bool EnsureNativeSourceZombieVisuals(Board* board, uint16_t kind, std::string* error = nullptr);
// Safe frame boundary after complete metadata restoration/pruning, not Draw.
void ReleaseUnusedNativeSourceZombieVisuals(Board* board);
// False retains native rendering for untyped or unsupported terminal poses.
bool DrawNativeSourceZombie(Sexy::Graphics* graphics, Zombie* zombie);
bool GetNativeSourceZombie269NodePoint(const Zombie*,const char*,float&,float&);
bool GetNativeSourceZombie269NodeActivity(const Zombie*,const char*,bool&);
// Synchronous source loss callback: sample the actual native body track, including
// a lethal hit before PlayDeathAnim changes it. Never select a pose from HP.
bool GetNativeSourceZombieLimbBirthPoint276(const Zombie*,const char*,float&,float&);
bool CanQueryNativeSourceZombieRoot273(Board*,uint16_t kind);
// SAME sampled runtime options as Draw/Node. Box corners remain source units
// relative to the placed root. Kirov26 genuinely has no BoxCollider2D.
bool GetNativeSourceZombieBox313(const Zombie*,FusionAnimationBoxCollider303&);
// Complete source node transform composed once into Board pixels, [a,b,c,d,x,y].
// Local UID and opaque host token remain outside presentation calculations.
bool GetNativeSourceZombieMatrix313(const Zombie*,const char* nodePath,std::array<float,6>&);
}

#endif
