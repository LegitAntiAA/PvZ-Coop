#ifndef LAWN_FUSION_ALMANAC_TEXT_H
#define LAWN_FUSION_ALMANAC_TEXT_H

#include "FusionPortraitCache.h"
#include <memory>
#include <string>

namespace LawnFusion
{
struct FusionAlmanacTranslation
{
    std::string nameUtf8;
    std::string infoUtf8;
    std::string loreUtf8;
    std::string costUtf8;
};

// Only verified RU overrides are stored. Original source strings remain in
// FusionCatalog and are used by the dialog when FindRussian returns nullptr.
class FusionAlmanacTextProvider
{
public:
    FusionAlmanacTextProvider();
    ~FusionAlmanacTextProvider();
    FusionAlmanacTextProvider(const FusionAlmanacTextProvider&) = delete;
    FusionAlmanacTextProvider& operator=(const FusionAlmanacTextProvider&) = delete;

    bool Load(const std::string& catalogDirectory, std::string& error);
    const FusionAlmanacTranslation* FindRussian(FusionPortraitGroup group, uint16_t id) const;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
}

#endif
