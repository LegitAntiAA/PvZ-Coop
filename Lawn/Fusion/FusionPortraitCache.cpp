#include "FusionPortraitCache.h"
#include "FusionAnimation.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../SexyAppFramework/Graphics.h"
#include "../../SexyAppFramework/MemoryImage.h"
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <list>
#include <map>
#include <stdexcept>

namespace LawnFusion
{
namespace
{
using Json = nlohmann::json;
namespace fs = std::filesystem;
constexpr int kPortraitSize = 160;
constexpr size_t kCachedPortraits = 64;

bool ValidGroup(FusionPortraitGroup group)
{
    return group == FusionPortraitGroup::Plants || group == FusionPortraitGroup::Zombies;
}

uint32_t PortraitKey(FusionPortraitGroup group, uint16_t id)
{
    return static_cast<uint32_t>(id) | (group == FusionPortraitGroup::Zombies ? 0x10000U : 0U);
}

Json ReadJson(const fs::path& path)
{
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open portrait JSON: "+path.string());
    return Json::parse(input);
}

fs::path ResolveFile(const fs::path& root, const fs::path& base, const std::string& relative)
{
    const fs::path path = fs::u8path(relative);
    if (relative.empty() || path.has_root_path())
        throw std::runtime_error("portrait asset path must be relative");
    const fs::path resolved = fs::canonical(base/path);
    const fs::path within = resolved.lexically_relative(root);
    if (within.empty() || within.has_root_path() || *within.begin() == ".." || !fs::is_regular_file(resolved))
        throw std::runtime_error("portrait asset is outside its manifest directory: "+relative);
    return resolved;
}

struct Bounds
{
    float minX, minY, maxX, maxY;
};

Bounds ReadBounds(const Json& data)
{
    Bounds bounds{data.at("minX").get<float>(), data.at("minY").get<float>(),
        data.at("maxX").get<float>(), data.at("maxY").get<float>()};
    const float width = bounds.maxX-bounds.minX, height = bounds.maxY-bounds.minY;
    if (!std::isfinite(bounds.minX) || !std::isfinite(bounds.minY) ||
        !std::isfinite(bounds.maxX) || !std::isfinite(bounds.maxY) ||
        !std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0)
        throw std::runtime_error("invalid portrait visual bounds");
    return bounds;
}
}

struct FusionPortraitCache::Impl
{
    struct Entry
    {
        fs::path package;
        Bounds bounds;
        std::unique_ptr<Sexy::MemoryImage> image;
        std::list<uint32_t>::iterator recent;
        bool failed = false;
    };

    LawnApp* app = nullptr;
    fs::path root;
    std::map<uint32_t, Entry> entries;
    std::list<uint32_t> recent;

    std::unique_ptr<Sexy::MemoryImage> Bake(const Entry& entry) const
    {
        const Json package = ReadJson(entry.package);
        if (package.at("schema") != 1 || !package.value("static", false) ||
            !package.at("clips").empty() || !package.at("controller").at("states").empty())
            throw std::runtime_error("portrait package must contain a static visible pose");
        const Bounds bounds = ReadBounds(package.at("visualBounds"));
        if (bounds.minX != entry.bounds.minX || bounds.minY != entry.bounds.minY ||
            bounds.maxX != entry.bounds.maxX || bounds.maxY != entry.bounds.maxY)
            throw std::runtime_error("portrait bounds differ from its manifest");
        for (const auto& variant : package.at("spriteVariants"))
        {
            if (variant.value("hasBoneMetadata", true))
                throw std::runtime_error("skinned portrait sprites are unsupported");
            ResolveFile(root, entry.package.parent_path(), variant.at("texture").get<std::string>());
        }
        const int rootIndex = package.at("root").get<int>();
        const auto& nodes = package.at("nodes");
        if (rootIndex < 0 || rootIndex >= static_cast<int>(nodes.size()) || nodes[rootIndex].at("parent") != -1)
            throw std::runtime_error("invalid portrait root transform");
        const float fit = (std::min)((kPortraitSize-4)/(bounds.maxX-bounds.minX),
            (kPortraitSize-4)/(bounds.maxY-bounds.minY));
        // Schema1 visualBounds already excludes the prefab root translation,
        // matching FusionAnimation::DrawPose. Retained Almanac UI ancestor
        // translations remain in both. Do not add the editor root position.
        const float x = kPortraitSize*0.5f-fit*(bounds.minX+bounds.maxX)*0.5f;
        const float y = kPortraitSize*0.5f+fit*(bounds.minY+bounds.maxY)*0.5f;
        if (!std::isfinite(fit) || fit <= 0 || !std::isfinite(x) || !std::isfinite(y))
            throw std::runtime_error("invalid portrait fit transform");

        auto image = std::make_unique<Sexy::MemoryImage>(app);
        image->Create(kPortraitSize, kPortraitSize);
        image->Clear();
        {
            FusionAnimation animation;
            std::string error;
            if (!animation.Load(app, entry.package.string(), error)) throw std::runtime_error(error);
            Sexy::Graphics preview(image.get());
            preview.SetLinearBlend(true);
            animation.Draw(&preview, x, y, fit/100.0f, 0.0f);
        }
        // No source atlas reference survives the bake. SharedImageRef release
        // schedules zero-ref atlas deletion at the framework's normal cleanup.
        const auto* pixels = image->GetBits();
        bool visible = false;
        for (int index = 0; index < kPortraitSize*kPortraitSize; ++index)
            if ((pixels[index] >> 24) != 0) { visible = true; break; }
        if (!visible) throw std::runtime_error("portrait rendered no visible pixels");
        return image;
    }
};

FusionPortraitCache::FusionPortraitCache() = default;
FusionPortraitCache::~FusionPortraitCache() = default;

bool FusionPortraitCache::Load(LawnApp* app, const std::string& path, std::string& error)
{
    try
    {
        if (!app) throw std::runtime_error("portrait loading needs an initialized application");
        const fs::path manifestPath = fs::canonical(fs::u8path(path));
        const Json manifest = ReadJson(manifestPath);
        if (manifest.at("schema") != 1 || manifest.at("animationSchema") != 1 ||
            !manifest.at("staticOnly").get<bool>())
            throw std::runtime_error("unsupported portrait manifest schema");
        auto result = std::make_unique<Impl>();
        result->app = app;
        result->root = manifestPath.parent_path();
        for (const auto group : {FusionPortraitGroup::Plants, FusionPortraitGroup::Zombies})
        {
            const char* name = group == FusionPortraitGroup::Plants ? "plants" : "zombies";
            const auto& records = manifest.at("groups").at(name);
            if (!records.is_object()) throw std::runtime_error("invalid portrait manifest group");
            for (auto it = records.begin(); it != records.end(); ++it)
            {
                const Json& record = it.value();
                if (!record.at("id").is_number_integer()) throw std::runtime_error("invalid portrait source ID");
                const int64_t id = record.at("id").get<int64_t>();
                if (id < 0 || id >= 65535 || std::to_string(id) != it.key())
                    throw std::runtime_error("portrait source ID differs from its manifest key");
                if (record.at("status") != "ready" || !record.at("publicAccepted").get<bool>()) continue;
                Impl::Entry entry;
                entry.package = ResolveFile(result->root, result->root, record.at("package").get<std::string>());
                entry.bounds = ReadBounds(record.at("visualBounds"));
                result->entries.emplace(PortraitKey(group, static_cast<uint16_t>(id)), std::move(entry));
            }
        }
        mImpl = std::move(result);
        error.clear();
        return true;
    }
    catch (const std::exception& exception) { error = exception.what(); return false; }
}

bool FusionPortraitCache::Has(FusionPortraitGroup group, uint16_t id) const
{
    if (!mImpl || !ValidGroup(group)) return false;
    const auto found = mImpl->entries.find(PortraitKey(group, id));
    return found != mImpl->entries.end() && !found->second.failed;
}

bool FusionPortraitCache::Draw(Sexy::Graphics* graphics, FusionPortraitGroup group, uint16_t id,
    const Sexy::Rect& bounds, int grayness)
{
    if (!graphics || !Has(group, id) || bounds.mWidth <= 0 || bounds.mHeight <= 0) return false;
    const uint32_t key = PortraitKey(group, id);
    auto& entry = mImpl->entries.at(key);
    try
    {
        if (!entry.image)
        {
            auto image = mImpl->Bake(entry);
            if (mImpl->recent.size() == kCachedPortraits)
            {
                mImpl->entries.at(mImpl->recent.back()).image.reset();
                mImpl->recent.pop_back();
            }
            mImpl->recent.push_front(key);
            entry.recent = mImpl->recent.begin();
            entry.image = std::move(image);
        }
        else mImpl->recent.splice(mImpl->recent.begin(), mImpl->recent, entry.recent);
    }
    catch (const std::exception& exception)
    {
        entry.failed = true;
        LawnNet::NetTrace("Fusion portrait unavailable group=%s id=%u reason=%s",
            group == FusionPortraitGroup::Plants ? "plants" : "zombies", static_cast<unsigned>(id), exception.what());
        return false;
    }
    const int side = (std::min)(bounds.mWidth, bounds.mHeight);
    const Sexy::Rect fitted(bounds.mX+(bounds.mWidth-side)/2, bounds.mY+(bounds.mHeight-side)/2, side, side);
    const int gray = std::clamp(grayness, 0, 255);
    graphics->PushState();
    graphics->ClipRect(bounds);
    graphics->SetColorizeImages(true);
    graphics->SetColor(Sexy::Color(gray, gray, gray));
    graphics->DrawImage(entry.image.get(), fitted, Sexy::Rect(0, 0, kPortraitSize, kPortraitSize));
    graphics->PopState();
    return true;
}
}
