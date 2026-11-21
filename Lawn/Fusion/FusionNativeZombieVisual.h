#ifndef LAWN_FUSION_NATIVE_ZOMBIE_VISUAL_H
#define LAWN_FUSION_NATIVE_ZOMBIE_VISUAL_H

#include <cstdint>
#include <memory>
#include <string>

class Board;
class LawnApp;
class Zombie;
namespace Sexy { class Graphics; }

namespace LawnFusion
{
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
    bool Node(const Zombie*,const char* path,float& x,float& y,bool* active=nullptr) const;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

// Factory admission and validated client preflight, before mutable world work.
bool EnsureNativeSourceZombieVisuals(Board* board, uint16_t kind, std::string* error = nullptr);
// Safe frame boundary after complete metadata restoration/pruning, not Draw.
void ReleaseUnusedNativeSourceZombieVisuals(Board* board);
// False retains native rendering for untyped or unsupported terminal poses.
bool DrawNativeSourceZombie(Sexy::Graphics* graphics, Zombie* zombie);
bool GetNativeSourceZombie269NodePoint(const Zombie*,const char*,float&,float&);
bool GetNativeSourceZombie269NodeActivity(const Zombie*,const char*,bool&);
}

#endif
