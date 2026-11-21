#ifndef LAWN_FUSION_PORTRAIT_CACHE_H
#define LAWN_FUSION_PORTRAIT_CACHE_H

#include "../../SexyAppFramework/Rect.h"
#include <cstdint>
#include <memory>
#include <string>

class LawnApp;
namespace Sexy { class Graphics; }

namespace LawnFusion
{
enum class FusionPortraitGroup { Plants, Zombies };

// Dialog-owned, main-thread cache of static source portraits. The application
// must outlive this cache. No Board, entity, animation clock or gameplay state
// is retained. At most 64 transparent 160x160 thumbnails are held at a time.
class FusionPortraitCache
{
public:
    FusionPortraitCache();
    ~FusionPortraitCache();
    FusionPortraitCache(const FusionPortraitCache&) = delete;
    FusionPortraitCache& operator=(const FusionPortraitCache&) = delete;

    bool Load(LawnApp* app, const std::string& manifestJsonPath, std::string& error);
    bool Has(FusionPortraitGroup group, uint16_t id) const;
    // False leaves the caller responsible for its explicit unavailable state.
    // Failed packages are logged once and are not retried during this load.
    bool Draw(Sexy::Graphics* graphics, FusionPortraitGroup group, uint16_t id,
        const Sexy::Rect& bounds, int grayness = 255);

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
}

#endif
