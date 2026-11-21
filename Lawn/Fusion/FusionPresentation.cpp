#include "FusionPresentation.h"
#include "FusionAnimation.h"
#include "../../LawnApp.h"
#include "../../SexyAppFramework/Graphics.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <utility>

namespace LawnFusion
{
namespace
{
using Json = nlohmann::json;
struct Profile
{
    FusionAnimation animation;
    float anchorX = 0, anchorY = 0, rootScale = 1;
    float minX = 0, maxX = 0, minY = 0, maxY = 0;
    std::string defaultState;
    FusionAnimationDrawOptions lostArm;
    FusionAnimationDrawOptions headless;
};

std::unique_ptr<Profile> LoadProfile(LawnApp* app, const std::filesystem::path& root,
    const char* name, bool requireGround, bool zombie)
{
    const auto path = root/name/"animation.json";
    auto result = std::make_unique<Profile>();
    std::string error;
    if (!result->animation.Load(app, path.string(), error)) throw std::runtime_error(std::string(name)+": "+error);
    std::ifstream file(path);
    const Json data = Json::parse(file);
    const auto rootIndex = data.at("root").get<size_t>();
    result->rootScale = data.at("nodes").at(rootIndex).at("scale").at("x").get<float>();
    if (result->rootScale <= 0) throw std::runtime_error(std::string(name)+": invalid root scale");
    const auto& bounds = data.at("visualBounds");
    result->minX = bounds.at("minX").get<float>(); result->maxX = bounds.at("maxX").get<float>();
    result->minY = bounds.at("minY").get<float>(); result->maxY = bounds.at("maxY").get<float>();
    if (result->minX >= result->maxX || result->minY >= result->maxY) throw std::runtime_error(std::string(name)+": invalid visual bounds");
    if (!data.value("static", false))
    {
        const auto id = data.at("controller").at("layers").at(0).at("defaultState").get<int64_t>();
        for (const auto& state : data.at("controller").at("states")) if (state.at("id").get<int64_t>() == id)
            result->defaultState = state.at("name").get<std::string>();
        if (result->defaultState.empty() || result->animation.GetStateSpeed(result->defaultState.c_str()) <= 0)
            throw std::runtime_error(std::string(name)+": invalid source default state");
    }
    if (requireGround)
    {
        const auto& anchor = data.at("anchors").at("shadow");
        if (!anchor.at("relativeToRuntimeRoot").get<bool>()) throw std::runtime_error(std::string(name)+": invalid ground anchor");
        result->anchorX = anchor.at("x").get<float>(); result->anchorY = anchor.at("y").get<float>();
    }
    if (zombie)
    {
        const char* replacementPath = "Resources/zombies/image/reanim/Zombie_outerarm_upper2.asset";
        const auto guid = data.at("extraSpriteGuids").at(replacementPath).get<std::string>();
        if (!data.at("spriteVariants").contains(guid)) throw std::runtime_error("zombie lost-arm sprite missing");
        for (const auto& node : data.at("nodes"))
        {
            const auto path = node.at("path").get<std::string>();
            const auto tag = node.at("tag").get<std::string>();
            if (tag == "ZombieHand") result->lostArm.hiddenNodePaths.push_back(path);
            if (tag == "ZombieHead" || tag == "ZombieJaw") result->headless.hiddenNodePaths.push_back(path);
            if (tag == "ZombieArmUpper") result->lostArm.spriteReplacements.push_back({path, guid, 4, 4, 4});
        }
        if (result->lostArm.hiddenNodePaths.empty() || result->headless.hiddenNodePaths.empty() || result->lostArm.spriteReplacements.empty())
            throw std::runtime_error("zombie source loss tags missing");
    }
    return result;
}

float ElapsedFromClipTime(const Profile& profile, const char* state, float clipTime)
{
    const float speed = profile.animation.GetStateSpeed(state);
    return speed > 0 ? clipTime/speed : -1.0f;
}

std::unique_ptr<Reanimation> LoadNativeSun()
{
    if (static_cast<unsigned>(ReanimationType::REANIM_SUN) >= gReanimatorDefCount)
        throw std::runtime_error("native sun animation definitions are unavailable");
    auto result = std::make_unique<Reanimation>();
    // The framework's default, uninitialized Reanimation is not destructible
    // until mDead is true. Keep partial/exceptional initialization safe too.
    result->mDead = true;
    try
    {
        result->ReanimationInitializeType(0.0f, 0.0f, ReanimationType::REANIM_SUN);
        if (result->mFrameCount <= 0) throw std::runtime_error("native sun animation has no frames");
        result->mLoopType = ReanimLoopType::REANIM_LOOP;
        result->mAnimRate = 6.0f;
    }
    catch (...) { result->mDead = true; throw; }
    return result;
}
}

struct FusionPresentation::Impl
{
    std::unique_ptr<Profile> background;
    std::map<int, std::unique_ptr<Profile>> plants;
    std::map<int, std::unique_ptr<Profile>> portraits;
    std::unique_ptr<Profile> zombie;
    std::map<int, std::unique_ptr<Profile>> projectiles;
    std::unique_ptr<Reanimation> sun;
    float backgroundX = 0, backgroundY = 0;
};

FusionPresentation::FusionPresentation() = default;
FusionPresentation::~FusionPresentation() = default;
bool FusionPresentation::IsLoaded() const { return mImpl != nullptr; }

bool FusionPresentation::Load(LawnApp* app, const std::string& packageRoot, std::string& error)
{
    try
    {
        const std::filesystem::path root(packageRoot);
        auto result = std::make_unique<Impl>();
        result->background = LoadProfile(app, root, "background1", false, false);
        std::ifstream backgroundFile(root/"background1/animation.json");
        const Json backgroundData = Json::parse(backgroundFile);
        const auto& position = backgroundData.at("nodes").at(backgroundData.at("root").get<size_t>()).at("position");
        result->backgroundX = position.at("x").get<float>(); result->backgroundY = position.at("y").get<float>();
        for (const auto& entry : {std::pair<int, const char*>(0, "peashooter"), {1, "sunflower"}, {1000, "peasunflower"}, {1030, "doubleshooter"}, {1032, "gatlingpea"}, {1090, "splitpea"}})
        {
            result->plants.emplace(entry.first, LoadProfile(app, root, entry.second, true, false));
            const auto portraitName = std::string("portrait-")+entry.second;
            result->portraits.emplace(entry.first, LoadProfile(app, root, portraitName.c_str(), false, false));
        }
        result->zombie = LoadProfile(app, root, "zombie", true, true);
        result->projectiles.emplace(0, LoadProfile(app, root, "projectile-pea", false, false));
        result->projectiles.emplace(8, LoadProfile(app, root, "projectile-sun", false, false));
        // A scratch native visual, never registered as a Classic Coin or effect.
        // Metadata-only loads have no initialized native resource environment.
        if (app) result->sun = LoadNativeSun();
        for (const auto& entry : result->plants)
        {
            if (entry.second->animation.GetStateSpeed("idle") <= 0 || (entry.first != 1 && entry.second->animation.GetStateSpeed("shoot") <= 0))
                throw std::runtime_error("missing verified plant state");
        }
        for (const char* state : {"walk", "walk2", "attack"}) if (result->zombie->animation.GetStateSpeed(state) <= 0)
            throw std::runtime_error(std::string("missing verified zombie state: ")+state);
        mImpl = std::move(result);
        error.clear();
        return true;
    }
    catch (const std::exception& exception) { error = exception.what(); return false; }
}

void FusionPresentation::Draw(Sexy::Graphics* graphics, const FusionSimulationState& state, int width, int height, float cosmeticElapsed, float nativeBankScale) const
{
    if (!mImpl || !graphics || !std::isfinite(cosmeticElapsed) || !std::isfinite(nativeBankScale) || nativeBankScale <= 0) return;
    const FusionViewport viewport = GetFusionViewport(width, height);
    if (viewport.pixelsPerUnit <= 0) return;
    const float scale = viewport.pixelsPerUnit/100.0f;
    graphics->PushState();
    graphics->SetColor(Sexy::Color(0, 0, 0));
    graphics->FillRect(0, 0, width, height);
    graphics->ClipRect(viewport.left, viewport.top, viewport.width, viewport.height);
    float x, y;
    viewport.ProjectWorld(mImpl->backgroundX, mImpl->backgroundY, x, y);
    mImpl->background->animation.Draw(graphics, x, y, scale, 0.0f);
    for (int row = 0; row < state.rows; ++row)
    {
        std::vector<const FusionPlantState*> plants;
        for (const auto& plant : state.plants) if (plant.row == row) plants.push_back(&plant);
        std::sort(plants.begin(), plants.end(), [](const auto* a, const auto* b) { return a->column > b->column; });
        for (const auto* plant : plants)
        {
            const auto found = mImpl->plants.find(plant->type);
            if (found == mImpl->plants.end()) continue;
            const Profile& profile = *found->second;
            viewport.ProjectWorld(FusionColumnX(plant->column)-profile.anchorX, FusionRowY(plant->row, state.rows)-profile.anchorY, x, y);
            const float idle = ElapsedFromClipTime(profile, "idle", plant->animationTime);
            const float shoot = plant->shootAnimationTime < 0 ? -1.0f : ElapsedFromClipTime(profile, "shoot", plant->shootAnimationTime);
            profile.animation.DrawPose(graphics, x, y, scale, "idle", idle, "shoot", shoot, 1.0f);
        }
        std::vector<const FusionZombieState*> zombies;
        for (const auto& zombie : state.zombies) if (zombie.row == row) zombies.push_back(&zombie);
        std::sort(zombies.begin(), zombies.end(), [](const auto* a, const auto* b) { return a->id < b->id; });
        for (const auto* zombie : zombies)
        {
            const Profile& profile = *mImpl->zombie;
            viewport.ProjectWorld(zombie->x-profile.anchorX, FusionRowY(zombie->row, state.rows)-profile.anchorY, x, y);
            const char* name = zombie->attackTarget ? "attack" : zombie->fastWalk ? "walk2" : "walk";
            FusionAnimationDrawOptions options;
            if (zombie->lostArm) options = profile.lostArm;
            if (zombie->headless) options.hiddenNodePaths.insert(options.hiddenNodePaths.end(), profile.headless.hiddenNodePaths.begin(), profile.headless.hiddenNodePaths.end());
            profile.animation.DrawPose(graphics, x, y, scale, name, ElapsedFromClipTime(profile, name, zombie->animationTime), nullptr, -1, 1, &options);
        }
        for (const auto& projectile : state.projectiles) if (projectile.row == row)
        {
            const auto found = mImpl->projectiles.find(projectile.type);
            if (found == mImpl->projectiles.end()) continue;
            viewport.ProjectWorld(projectile.x, projectile.y, x, y);
            found->second->animation.Draw(graphics, x, y, scale, cosmeticElapsed);
        }
    }
    graphics->PopState();
    // Collection flies into the native HUD, including the letterbox above an
    // 800x600 battlefield. Restore the caller's clip before drawing these suns.
    if (!mImpl->sun) return;
    graphics->PushState();
    Reanimation& visual = *mImpl->sun;
    const double seconds = static_cast<double>(state.tick)*0.01 + (std::max)(0.0f, cosmeticElapsed);
    visual.mAnimTime = static_cast<float>(std::fmod(seconds*6.0/visual.mFrameCount, 1.0));
    float targetX, targetY;
    viewport.ProjectWorld(kFusionSunCollectionTargetX, kFusionSunCollectionTargetY, targetX, targetY);
    for (const auto& sun : state.suns)
    {
        if (sun.amount != 5 && sun.amount != 25) continue;
        viewport.ProjectWorld(sun.x, sun.y, x, y);
        if (sun.collecting)
        {
            const float distance = std::hypot(sun.x-kFusionSunCollectionTargetX, sun.y-kFusionSunCollectionTargetY);
            const float progress = sun.collectionStartDistance > 0 ?
                std::clamp(1.0f-distance/sun.collectionStartDistance, 0.0f, 1.0f) : 1.0f;
            x += progress*(45.0f*nativeBankScale-targetX);
            y += progress*(30.0f*nativeBankScale-targetY);
        }
        visual.SetPosition(x, y);
        const float drawScale = sun.scale*viewport.pixelsPerUnit/kFusionNativePixelsPerWorldUnit;
        visual.OverrideScale(drawScale, drawScale);
        visual.mColorOverride = Sexy::Color::White;
        visual.Draw(graphics);
    }
    graphics->PopState();
}

bool FusionPresentation::DrawPlantPortrait(Sexy::Graphics* graphics, FusionPlantId type, float x, float y, float width, float height) const
{
    if (!mImpl || !graphics || width <= 0 || height <= 0) return false;
    const auto found = mImpl->portraits.find(type);
    if (found == mImpl->portraits.end()) return false;
    const Profile& profile = *found->second;
    const float fit = (std::min)(width/(profile.maxX-profile.minX), height/(profile.maxY-profile.minY));
    const float centerX = (profile.minX+profile.maxX)*0.5f, centerY = (profile.minY+profile.maxY)*0.5f;
    graphics->PushState();
    graphics->ClipRect(static_cast<int>(x), static_cast<int>(y), static_cast<int>(std::ceil(width)), static_cast<int>(std::ceil(height)));
    profile.animation.Draw(graphics, x+width*0.5f-fit*centerX, y+height*0.5f+fit*centerY, fit/100.0f, 0);
    graphics->PopState();
    return true;
}

bool FusionPresentation::DrawPlantPortrait(Sexy::Graphics* graphics, FusionPlantId type, const Sexy::Rect& bounds) const
{
    return DrawPlantPortrait(graphics, type, static_cast<float>(bounds.mX), static_cast<float>(bounds.mY), static_cast<float>(bounds.mWidth), static_cast<float>(bounds.mHeight));
}
}
