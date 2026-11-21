#include "../../LawnApp.h"
#include "NetCommon.h"
#include "../Fusion/FusionAnimation.h"
#include "../../SexyAppFramework/Graphics.h"
#include "../../SexyAppFramework/MemoryImage.h"
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>

namespace
{
using Json = nlohmann::json;
namespace fs = std::filesystem;
constexpr int kImageSize = 640;
struct Model
{
    int kind;
    const char* prefab;
    const char* attribute;
    const char* target;
    float times[2];
    int expected[2];
    const char* names[2];
};
constexpr Model models[] = {
    {9,"Resources/plants/scaredyshroom/ScaredyShroomPrefab.prefab","m_Color.a","ScaredyShroom_sweat1",
        {.31666666f,.6f},{114,255},{"09_alpha_low","09_alpha_full"}},
    {34,"Resources/plants/electriconion/ElectricOnionPrefab.prefab","m_Color.a","body/Ball1",
        {.20833333f,.41666666f},{128,255},{"34_alpha_half","34_alpha_full"}},
    {13,"Resources/plants/squash/SquashPrefab.prefab","m_Enabled","Shadow",
        {.549f,.55f},{1,0},{"13_enabled","13_disabled"}}};

double Number(const Json& value)
{
    if (value == "Infinity") return std::numeric_limits<double>::infinity();
    if (value == "-Infinity") return -std::numeric_limits<double>::infinity();
    return value.get<double>();
}
// Source scalar oracle, deliberately separate from native pixels/evaluator.
double Sample(const Json& curve, double time)
{
    const auto& keys = curve.at("keys");
    if (keys.empty()) throw std::runtime_error("probe scalar curve is empty");
    if (time <= keys.front().at("time").get<double>()) return Number(keys.front().at("value"));
    if (time >= keys.back().at("time").get<double>()) return Number(keys.back().at("value"));
    for (size_t i = 1; i < keys.size(); ++i)
    {
        const auto& a = keys[i-1]; const auto& b = keys[i];
        const double start = a.at("time"), end = b.at("time");
        if (time >= end) continue;
        if (a.at("weightedMode").get<int>() || b.at("weightedMode").get<int>() || end <= start)
            throw std::runtime_error("probe requires original ordered unweighted scalar curves");
        const double outgoing = Number(a.at("outSlope")), incoming = Number(b.at("inSlope"));
        if (!std::isfinite(outgoing) || !std::isfinite(incoming)) return Number(a.at("value"));
        const double t = (time-start)/(end-start), t2 = t*t, t3 = t2*t;
        return (2*t3-3*t2+1)*Number(a.at("value"))+(t3-2*t2+t)*(end-start)*outgoing+
            (-2*t3+3*t2)*Number(b.at("value"))+(t3-t2)*(end-start)*incoming;
    }
    throw std::runtime_error("probe failed to locate a source scalar segment");
}
void Publish(const fs::path& destination, const Json& report)
{
    const fs::path temporary = destination.string()+".tmp";
    { std::ofstream file(temporary,std::ios::binary); file << report.dump(2) << '\n'; file.close();
      if (!file) throw std::runtime_error("cannot write animation probe metadata"); }
    fs::rename(temporary,destination);
}

std::string ReadBytes(const fs::path& path)
{
    std::ifstream file(path,std::ios::binary);
    if (!file) throw std::runtime_error("cannot read animation probe package");
    return std::string(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>());
}
void SaveModel(LawnApp* app, Sexy::MemoryImage& rendered, const fs::path& destination)
{
    Sexy::MemoryImage image(app); image.Create(kImageSize,kImageSize); image.Clear();
    Sexy::Graphics composite(&image); composite.SetColor(Sexy::Color(238,242,238));
    composite.FillRect(0,0,kImageSize,kImageSize); composite.DrawImage(&rendered,0,0);
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> surface(
        SDL_CreateSurfaceFrom(kImageSize,kImageSize,SDL_PIXELFORMAT_ARGB8888,image.GetBits(),kImageSize*4),SDL_DestroySurface);
    if (!surface || !SDL_SaveBMP(surface.get(),destination.string().c_str()))
        throw std::runtime_error(std::string("animation probe BMP write failed: ")+SDL_GetError());
}
size_t DifferentPixels(Sexy::MemoryImage& a, Sexy::MemoryImage& b)
{
    const auto* left = a.GetBits(); const auto* right = b.GetBits();
    size_t count = 0;
    for (int i = 0; i < kImageSize*kImageSize; ++i) if (left[i] != right[i]) ++count;
    return count;
}

void ProbeModel(LawnApp* app, const fs::path& root, const fs::path& output, const Model& model,
    const std::string& runId, Json& frames)
{
    const fs::path path = root/std::to_string(model.kind)/"runtime"/"animation.json";
    const std::string originalBytes = ReadBytes(path);
    const Json package = Json::parse(originalBytes);
    if (package.at("schema") != 1 || package.at("sourcePrefab") != model.prefab ||
        !package.at("ignoredBindings").empty() || !package.at("omittedInactiveNodes").empty() ||
        package.at("unusedSpriteBonesExplicitlyAllowed").get<bool>())
        throw std::runtime_error("probe package is not the exact strict source model: "+path.string());
    const Json* curve = nullptr; std::string clipName, stateName; size_t curveIndex = 0;
    for (auto clip = package.at("clips").begin(); clip != package.at("clips").end(); ++clip)
        for (size_t index = 0; index < clip.value().at("curves").size(); ++index)
        {
            const auto& candidate = clip.value().at("curves")[index];
            if (candidate.at("kind") == "float" && candidate.at("classID") == 212 &&
                candidate.at("attribute") == model.attribute && candidate.at("path") == model.target)
            {
                if (curve) throw std::runtime_error("probe target matches more than one actual clip");
                curve = &candidate; clipName = clip.key(); curveIndex = index;
            }
        }
    if (!curve) throw std::runtime_error("probe source class212 curve is missing");
    for (const auto& state : package.at("controller").at("states"))
        if (state.at("clip") == clipName)
        {
            if (!stateName.empty()) throw std::runtime_error("probe clip has ambiguous controller states");
            stateName = state.at("name").get<std::string>();
        }
    if (stateName.empty()) throw std::runtime_error("probe source controller state is missing");
    LawnFusion::FusionAnimation animation; std::string error;
    if (!animation.Load(app,path.string(),error)) throw std::runtime_error(error);
    LawnFusion::FusionAnimation controls[2]; fs::path controlPaths[2];
    for (int value = 0; value <= 1; ++value)
    {
        Json controlled = package;
        Json key = curve->at("keys").front();
        key["time"] = 0; key["value"] = value; key["inSlope"] = 0; key["outSlope"] = 0;
        key["weightedMode"] = 0;
        auto& keys = controlled["clips"][clipName]["curves"][curveIndex]["keys"];
        keys = Json::array({key});
        Json restored = controlled;
        restored["clips"][clipName]["curves"][curveIndex]["keys"] = curve->at("keys");
        if (restored != package) throw std::runtime_error("counterfactual changed source data outside target style keys");
        controlPaths[value] = path.parent_path()/("animation-probe-cf-"+runId+"-"+std::to_string(value)+".json");
        if (fs::exists(controlPaths[value]) || fs::exists(controlPaths[value].string()+".tmp"))
            throw std::runtime_error("counterfactual package already exists for this unique run");
        Publish(controlPaths[value],controlled); // Same directory: original relative textures are reused.
        if (!controls[value].Load(app,controlPaths[value].string(),error)) throw std::runtime_error(error);
    }
    LawnFusion::FusionAnimationDrawOptions options;
    options.nodeActivityOverrides.push_back({model.target,true});
    const float speed = animation.GetStateSpeed(stateName.c_str());
    if (!std::isfinite(speed) || speed <= 0) throw std::runtime_error("probe source state speed is invalid");
    const auto& bounds = package.at("visualBounds");
    const float left = bounds.at("minX"), right = bounds.at("maxX"), bottom = bounds.at("minY"), top = bounds.at("maxY");
    const float span = (std::max)(right-left,top-bottom);
    if (!std::isfinite(span) || span <= 0) throw std::runtime_error("probe source bounds are invalid");
    const float fit = (std::min)(80.0f,360.0f/span);
    const float x = 320.0f-fit*(left+right)*.5f, y = 340.0f+fit*(bottom+top)*.5f;
    for (int index = 0; index < 2; ++index)
    {
        const float clipTime = model.times[index], elapsed = clipTime/speed;
        const double scalar = Sample(*curve,clipTime);
        const bool alpha = std::strcmp(model.attribute,"m_Color.a") == 0;
        const int expected = alpha ? static_cast<int>(std::clamp(scalar*255.0,0.0,255.0)+.5) : scalar >= .5 ? 1 : 0;
        if (expected != model.expected[index]) throw std::runtime_error("probe source scalar golden mismatch");
        Sexy::MemoryImage rendered(app); rendered.Create(kImageSize,kImageSize); rendered.Clear();
        Sexy::Graphics graphics(&rendered); graphics.SetLinearBlend(true);
        animation.DrawPose(&graphics,x,y,fit/100.0f,stateName.c_str(),elapsed,nullptr,-1,1,&options);
        const auto* pixels = rendered.GetBits();
        size_t visible = 0; int minX = kImageSize, minY = kImageSize, maxX = -1, maxY = -1;
        for (int row = 0; row < kImageSize; ++row) for (int column = 0; column < kImageSize; ++column)
            if (pixels[row*kImageSize+column] >> 24)
            { ++visible; minX = (std::min)(minX,column); maxX = (std::max)(maxX,column); minY = (std::min)(minY,row); maxY = (std::max)(maxY,row); }
        if (!visible) throw std::runtime_error("actual native animation draw produced no model pixels");
        const std::string name = std::string("animation_probe_")+model.names[index]+".bmp";
        SaveModel(app,rendered,output/name);
        frames.push_back({{"sourceId",model.kind},{"package",path.string()},{"sourcePrefab",model.prefab},
            {"state",stateName},{"clip",clipName},{"clipTime",clipTime},{"elapsed",elapsed},{"stateSpeed",speed},
            {"nativeEffectiveClipTime",elapsed*speed},{"originalCurve",*curve},{"sourceScalarOracle",scalar},
            {"expectedAlphaByteOrEnabled",expected},{"modelVisiblePixels",visible},
            {"nativeModelPixelHash",LawnNet::NetFnv1a32(pixels,kImageSize*kImageSize*4)},
            {"nativeModelBounds",{minX,minY,maxX,maxY}},
            {"touchesImageEdge",minX==0 || minY==0 || maxX==kImageSize-1 || maxY==kImageSize-1},
            {"bmp",name},{"width",kImageSize},{"height",kImageSize},{"sources",package.at("sources")},
            {"pixelParityAsserted",false},{"visualReview","Pending"},
            {"fixtureNodeActivityOverride",{{"nodePath",model.target},{"active",true}}}});
        Sexy::MemoryImage off(app), on(app);
        Sexy::MemoryImage* variants[] = {&off,&on};
        Json comparisons = Json::array();
        const bool saveControls = alpha ? index == 0 : index == 1; // Six additional BMPs in total.
        for (int value = 0; value <= 1; ++value)
        {
            auto& image = *variants[value]; image.Create(kImageSize,kImageSize); image.Clear();
            Sexy::Graphics variantGraphics(&image); variantGraphics.SetLinearBlend(true);
            controls[value].DrawPose(&variantGraphics,x,y,fit/100.0f,stateName.c_str(),elapsed,nullptr,-1,1,&options);
            const std::string controlName = std::string("animation_probe_")+model.names[index]+(value ? "_cf_on.bmp" : "_cf_off.bmp");
            if (saveControls) SaveModel(app,image,output/controlName);
            comparisons.push_back({{"syntheticControl",true},{"constant",value},{"package",controlPaths[value].string()},
                {"onlyTargetStyleKeysChanged",true},{"samePoseAndClipTime",true},{"clipTime",clipTime},
                {"nativePixelHash",LawnNet::NetFnv1a32(image.GetBits(),kImageSize*kImageSize*4)},
                {"actualDifferentPixels",DifferentPixels(rendered,image)},{"bmpSaved",saveControls},
                {"bmp",saveControls ? Json(controlName) : Json(nullptr)}});
        }
        const size_t offOn = DifferentPixels(off,on), actualOff = DifferentPixels(rendered,off), actualOn = DifferentPixels(rendered,on);
        const bool matches = alpha && expected > 0 && expected < 255 ? actualOff > 0 && actualOn > 0 :
            (expected == 0 ? actualOff == 0 : actualOn == 0);
        frames.back()["counterfactuals"] = comparisons;
        frames.back()["offOnDifferentPixels"] = offOn;
        frames.back()["nativeStyleComparisonPassed"] = offOn > 0 && matches;
        if (!offOn)
            throw std::runtime_error("probe target has no visible pixel contribution with only its GameObject forced active; stopping without hiding unrelated sprites");
        if (!matches) throw std::runtime_error("same-pose native style channel disagrees with its synthetic constant controls");
        LawnNet::NetTrace("SMOKE_ANIMATION_PROBE id=%d state=%s clipTime=%.9g scalar=%.9g oracle=%d modelPixels=%u file=%s nativeGraphics=1 pixelParityClaimed=0",
            model.kind,stateName.c_str(),clipTime,scalar,expected,static_cast<unsigned>(visible),name.c_str());
        LawnNet::NetTrace("SMOKE_ANIMATION_STYLE id=%d clipTime=%.9g offOn=%u actualOff=%u actualOn=%u controlledKeysOnly=1 syntheticControls=1 passed=1",
            model.kind,clipTime,static_cast<unsigned>(offOn),static_cast<unsigned>(actualOff),static_cast<unsigned>(actualOn));
    }
    if (ReadBytes(path) != originalBytes) throw std::runtime_error("original animation package changed during the probe");
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionAnimationProbe(LawnApp* app)
{
    const char* enabled = std::getenv("PVZ_COOP_SMOKE_FUSION_ANIM_PROBE");
    if (!app || !app->CoopSmokeActive() || !enabled || std::strcmp(enabled,"1")) return false;
    if (app->mCoopSmokeResultWritten) return true;
    if (!app->mLoadingThreadCompleted) return true;
    Json report = {{"schema",1},{"task","RENDER133B"},{"frames",Json::array()},
        {"renderer","FusionAnimation.DrawPose -> native Graphics/MemoryImage"},
        {"sourceScalarOracleIsPixelProof",false},{"visualReview","Pending"},{"gameplayWorldCreated",false}};
    fs::path output;
    try
    {
        if (!app->mCoopSmokeManifest.IsHost() || app->mCoopSmokeManifest.mScenario != NETSMOKE_SCENARIO_FUSION_VALIDATION ||
            app->mBoard || app->mFusionRuntime || app->mSeedChooserScreen || app->mCoopSmokeGameStarted)
            throw std::runtime_error("animation probe requires strict solo-host FusionValidation without a gameplay Board");
        const char* requestedRoot = std::getenv("PVZ_COOP_SMOKE_FUSION_ANIM_ROOT");
        if (!requestedRoot || !*requestedRoot) throw std::runtime_error("animation probe package root was not configured");
        const fs::path root = fs::canonical(fs::u8path(requestedRoot));
        const char* requestedRunId = std::getenv("PVZ_COOP_SMOKE_RUN_ID");
        const std::string runId = requestedRunId ? requestedRunId : "";
        if (runId.size() != 32 || runId.find_first_not_of("0123456789abcdef") != std::string::npos)
            throw std::runtime_error("counterfactual packages require the strict unique smoke run ID");
        output = fs::absolute(fs::u8path(app->mCoopSmokeManifest.mResultPath)).parent_path();
        fs::create_directories(output);
        for (const auto& model : models) ProbeModel(app,root,output,model,runId,report["frames"]);
        report["status"] = "Passed"; // Native same-pose style controls; not source-runtime visual parity.
        report["syntheticControlBmpCount"] = 6; report["originalPackagesUnmodified"] = true;
        report["frameCount"] = report["frames"].size();
        Publish(output/"animation-probe.json",report);
        NetTrace("SMOKE_ANIMATION_PROBE_COMPLETE models=3 sourceFrames=6 controlFrames=6 samePoseStylePassed=1 nativeGraphics=1 boardCreated=0 sourceParityClaimed=0");
        app->CoopSmokeWriteResult(true,nullptr);
        if (app->mCoopSmokeManifest.mCloseOnResult) app->CloseRequestAsync();
    }
    catch (const std::exception& exception)
    {
        report["status"] = "Failed"; report["error"] = exception.what();
        if (!output.empty()) try { Publish(output/"animation-probe.json",report); } catch (...) {}
        app->CoopSmokeFail(exception.what());
    }
    return true;
}
}
