#ifndef LAWN_FUSION_ANIMATION_H
#define LAWN_FUSION_ANIMATION_H

#include "FusionDeform269.h"
#include <memory>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <optional>

class LawnApp;
namespace Sexy { class Graphics; }

namespace LawnFusion
{
struct FusionAnimationEvent
{
    float clipTimeSeconds;
    std::string functionName;
};

// Owns only one sampled frame's projected mask geometry and immutable texture pixels.
// Build views after collection; the owning records must outlive those views.
struct FusionAnimationMask
{
    std::vector<FusionDeform269Vertex> vertices;
    std::vector<uint16_t> indices;
    std::shared_ptr<const std::vector<uint32_t>> texturePixels;
    int width=0,height=0;
    FusionMask269Filter filter=FusionMask269Filter::Bilinear;
    FusionMask269Wrap wrap=FusionMask269Wrap::Clamp;
    FusionMask269Sort back,front;
    double alphaCutoff=.2;
    FusionMask269View View() const;
};
// Read-only BoxCollider2D geometry, in the SAME source-unit placed-root XY
// contract as TryGetNodeOffset. Full 3D TRS is composed before XY projection.
// Sprite flips/skin do not affect a physics box. Native callers apply their
// boardBasis/placement exactly once to corners before constructing native AABB.
// Original enabled and nodeActive are separate; runtime script enable is NOT
// inferred here. Geometry remains queryable when either flag is false.
struct FusionAnimationBoxCollider303
{
    std::array<float,2> localOffset{},localSize{};
    std::array<std::array<float,2>,4> corners{};
    std::array<float,4> aabb{}; // minX,minY,maxX,maxY.
    bool enabled=false,trigger=false,nodeActive=false;
};
struct FusionAnimationDrawOptions
{
    struct BaseBlend
    {
        std::string outgoingState;
        float outgoingElapsedSeconds = 0;
        float incomingWeight = 1;
    };
    bool rootFlipX = false; // Reflect the complete source hierarchy around its placed root.
    std::array<int,4> colorMultiplierRGBA{255,255,255,255};
    float attackSpeed = 1.0f; // Verified source Animator parameter, distinct from Speed.
    // Optional source-parent attachment projection, in Board pixels per
    // source unit. Translation stays in the ordinary DrawPose x/y arguments.
    bool useBoardBasis = false;
    std::array<float,4> boardBasis{1,0,0,-1};
    struct NodeActivity
    {
        std::string nodePath;
        bool active;
    };
    struct NodeRotation
    {
        std::string nodePath;
        float radiansZ;
        bool replace = false; // Source runtime localRotation assignment.
    };
    struct NodeWorldPosition
    {
        std::string nodePath;
        float x, y; // Sampled source units relative to the placed root.
    };
    struct ExtraState
    {
        std::string stateName;
        float elapsedSeconds;
        int layer = -1; // Exact source layer; -1 keeps the existing first-name lookup.
    };
    struct SpriteReplacement
    {
        std::string nodePath;
        std::string spriteGuid;
        float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;
        bool overrideScale = true;
    };
    struct SpriteSorting
    {
        std::string nodePath;
        int layer=0,order=0; // One consistent presentation sorting space per scope.
    };
    struct SortingGroup
    {
        std::string nodePath;
        int layer=0,order=0; // Source group sorts atomically, then retains child sorting.
    };
    struct MaskSorting
    {
        std::string nodePath;
        int backLayer=0,backOrder=0,frontLayer=0,frontOrder=0;
    };
    // Isolated by default: portraits/glove/UI can never consume world masks.
    // Explicit world views are valid only for the caller's completed frame scope.
    bool useWorldMaskScope=false;
    uint64_t maskScope=0;
    std::span<const FusionMask269View> worldMasks;
    std::string spritePathPrefix; // Optional filtered source-node pass; empty draws all sprites.
    std::vector<SpriteSorting> spriteSortingOverrides;
    std::vector<SortingGroup> sortingGroupOverrides;
    std::vector<MaskSorting> maskSortingOverrides;
    // Source Transform paths; hiding a node also hides its descendants.
    std::vector<std::string> hiddenNodePaths;
    // Runtime SetActive changes apply after the source animation curves.
    std::vector<NodeActivity> nodeActivityOverrides;
    // Runtime child rotation composes with its original source transform.
    std::vector<NodeRotation> nodeRotations;
    std::vector<NodeWorldPosition> nodeWorldPositions;
    // Explicit extra source layers; default empty preserves all existing poses.
    std::vector<ExtraState> extraStates;
    std::vector<SpriteReplacement> spriteReplacements;
    // Optional same-layer base transition. All consumers share SamplePose;
    // rendering/querying never owns a transition clock or dispatches events.
    std::optional<BaseBlend> baseBlend;
};

// Source curves remain in Unity units. Draw uses 100 pixels per Unity unit,
// followed by the caller's scale. x/y replace the source prefab root position;
// elapsed phases are supplied by the simulation.
// Rendering never dispatches animation events or changes gameplay state.
class FusionAnimation
{
public:
    struct TextureCacheStats
    {
        uint64_t uniqueContents = 0, loadedImages = 0, reusedImages = 0, retainedRgbaBytes = 0;
    };
    // Fusion-only diagnostics. The index holds weak references, not images.
    static TextureCacheStats GetTextureCacheStats(LawnApp* app);

    FusionAnimation();
    ~FusionAnimation();
    FusionAnimation(const FusionAnimation&) = delete;
    FusionAnimation& operator=(const FusionAnimation&) = delete;

    // Component IDs are accepted only after the caller's native particle
    // resource/binding preflight. Empty preserves strict sprite-only admission.
    bool Load(LawnApp* app, const std::string& packageJsonPath, std::string& error,
        std::span<const int64_t> validatedParticleComponents = {});
    bool IsLoaded() const;
    bool IsStatic() const;
    bool HasMasks() const;
    const char* GetDefaultStateName() const;
    void Draw(Sexy::Graphics* graphics, float x, float y, float scale,
        float idleElapsedSeconds, float shootElapsedSeconds = -1.0f, float animatorSpeed = 1.0f) const;
    void DrawPose(Sexy::Graphics* graphics, float x, float y, float scale,
        const char* baseState, float baseElapsedSeconds, const char* actionState = nullptr,
        float actionElapsedSeconds = -1.0f, float animatorSpeed = 1.0f,
        const FusionAnimationDrawOptions* options = nullptr) const;
    // Appends the exact current sampled mask mesh in destination pixel coordinates.
    // No actor state or clock is owned here. Failure leaves output unchanged.
    bool CollectMasks(Sexy::Graphics* graphics,float x,float y,float scale,
        const char* baseState,float baseElapsedSeconds,const char* actionState,
        float actionElapsedSeconds,float animatorSpeed,const FusionAnimationDrawOptions* options,
        std::vector<FusionAnimationMask>& output) const;
    // Same sampled hierarchy/layers as DrawPose, in source units relative to
    // the placed root. This never dispatches events or advances a clock.
    bool TryGetNodeOffset(const char* path,float& x,float& y,
        const char* baseState,float baseElapsedSeconds,const char* actionState=nullptr,
        float actionElapsedSeconds=-1,float animatorSpeed=1,
        const FusionAnimationDrawOptions* options=nullptr) const;
    bool TryGetNodeActivity(const char* path,bool& active,
        const char* baseState,float baseElapsedSeconds,const char* actionState=nullptr,
        float actionElapsedSeconds=-1,float animatorSpeed=1,
        const FusionAnimationDrawOptions* options=nullptr) const;
    bool TryGetNodeTransform(const char* path,std::array<float,6>& result,
        const char* baseState,float baseElapsedSeconds,const char* actionState=nullptr,
        float actionElapsedSeconds=-1,float animatorSpeed=1,
        const FusionAnimationDrawOptions* options=nullptr) const;
    // Exact serialized component identity, including static sibling boxes.
    // Unknown/ambiguous/invalid samples preserve the ENTIRE output unchanged.
    bool TryGetBoxCollider303(int64_t sourceFileId,FusionAnimationBoxCollider303& result,
        const char* baseState,float baseElapsedSeconds,const char* actionState=nullptr,
        float actionElapsedSeconds=-1,float animatorSpeed=1,
        const FusionAnimationDrawOptions* options=nullptr) const;
    int GetStateLayer(const char* state) const;
    float GetStateSpeed(const char* state) const;
    float GetStateSpeed(const char* state, int layer) const;
    float GetStateDurationSeconds(const char* state, float animatorSpeed = 1.0f) const;
    const std::vector<FusionAnimationEvent>& GetEvents(const char* state) const;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
}

#endif
