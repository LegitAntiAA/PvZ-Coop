#ifndef LAWN_FUSION_PRESENTATION_H
#define LAWN_FUSION_PRESENTATION_H

#include "FusionSimulation.h"
#include <memory>
#include <string>

class LawnApp;
namespace Sexy { class Graphics; template<class T> class TRect; }

namespace LawnFusion
{
// Source camera: orthographic size 5, gameplay position (0,0). Rounded pixel
// viewport preserves full 1066x600 and letterboxes 800x600 into 800x450.
struct FusionViewport
{
    int left = 0, top = 0, width = 0, height = 0;
    float pixelsPerUnit = 0;
    bool Contains(float x, float y) const
    { return width > 0 && height > 0 && x >= left && y >= top && x < left+width && y < top+height; }
    void ProjectWorld(float x, float y, float& screenX, float& screenY) const
    { screenX = left+width*0.5f+x*pixelsPerUnit; screenY = top+height*0.5f-y*pixelsPerUnit; }
    bool UnprojectScreen(float screenX, float screenY, float& x, float& y) const
    {
        if (!Contains(screenX, screenY) || pixelsPerUnit <= 0) return false;
        x = (screenX-left-width*0.5f)/pixelsPerUnit;
        y = (top+height*0.5f-screenY)/pixelsPerUnit;
        return true;
    }
};

inline FusionViewport GetFusionViewport(int width, int height)
{
    FusionViewport result;
    if (width <= 0 || height <= 0) return result;
    result.width = width; result.height = height;
    if (static_cast<int64_t>(width)*9 > static_cast<int64_t>(height)*16)
        result.width = static_cast<int>((static_cast<int64_t>(height)*16+4)/9);
    else result.height = static_cast<int>((static_cast<int64_t>(width)*9+8)/16);
    result.left = (width-result.width)/2; result.top = (height-result.height)/2;
    result.pixelsPerUnit = result.height/10.0f;
    return result;
}

class FusionPresentation
{
public:
    FusionPresentation();
    ~FusionPresentation();
    FusionPresentation(const FusionPresentation&) = delete;
    FusionPresentation& operator=(const FusionPresentation&) = delete;
    bool Load(LawnApp* app, const std::string& packageRoot, std::string& error);
    bool IsLoaded() const;
    void Draw(Sexy::Graphics* graphics, const FusionSimulationState& state,
        int width, int height, float cosmeticElapsed = 0.0f, float nativeBankScale = 1.0f) const;
    bool DrawPlantPortrait(Sexy::Graphics* graphics, FusionPlantId type,
        float x, float y, float width, float height) const;
    bool DrawPlantPortrait(Sexy::Graphics* graphics, FusionPlantId type, const Sexy::TRect<int>& bounds) const;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
}

#endif
