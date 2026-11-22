#include "FusionAnimation.h"
#include "FusionDeform269.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../SexyAppFramework/Graphics.h"
#include "../../SexyAppFramework/Image.h"
#include "../../SexyAppFramework/MemoryImage.h"
#include "../../SexyAppFramework/SharedImage.h"
#include "../../SexyAppFramework/SDL3Image.h"
#include "../../SexyAppFramework/TriVertex.h"
#include "../../PakLib/PakInterface.h"
#include "../../third_party/nlohmann/json.hpp"
#include <wincrypt.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <tuple>
#include <climits>

namespace LawnFusion
{
namespace
{
using Json = nlohmann::json;
using Vector = std::array<float, 3>;
using Quaternion = std::array<float, 4>;
constexpr float kPi = 3.14159265358979323846f;

// Fusion 2.3.1 ProjectSettings/TagManager.asset, m_SortingLayers in source
// order. SHA256: 4e045157856a0fe839806cd7a636a2b4def56aa900c38e4f292ef93238cf492f.
// Unity's layer IDs are identities, not ordering values. Each field row keeps
// back/low/plant/front/fly/zombie/bullet/particle order (row1 also has mower).
constexpr std::array<uint32_t, 105> kSourceSortingLayerIds = {
    0u, 2640843759u,
    1984699043u, 1925464861u, 821177707u, 2133931959u, 4042715363u, 2026729093u, 1700823033u, 2226946975u,
    3949585691u, 2352814571u, 110145437u, 373854663u, 3859020363u, 1425040035u, 2749242661u, 3240847671u, 1584691125u,
    1000701617u, 2026493315u, 1239939629u, 3818730509u, 3067023749u, 4141997293u, 389883871u, 3213154843u,
    133560575u, 563632411u, 2054189697u, 3519270683u, 4207595911u, 1839875491u, 1073562673u, 47817221u,
    3427884103u, 2199005579u, 1502006493u, 3746906789u, 668662651u, 2512983269u, 1069783123u, 2717222525u,
    3624676075u, 2474070201u, 3123972891u, 671657219u, 3149572503u, 3597740203u, 1322983597u, 2317548503u,
    641426885u, 410049621u, 932899667u, 1358711375u, 312655127u, 1579380307u, 1204590593u, 3928741075u,
    3237325651u, 3633290803u, 1716704297u, 4231898459u, 3485203371u, 2375840885u, 2914831315u, 709383273u,
    3335599961u, 1424200689u, 2751947867u, 3195378143u, 2414677375u, 2985220083u, 4101394141u, 721416257u,
    285975007u, 3509243003u, 1729512273u, 1897272441u, 364634009u, 3237926909u, 2848133491u, 1205531661u,
    3328141437u, 106410427u, 1785600793u, 4266515789u, 3370710035u, 1587292911u, 831177411u, 2522727951u,
    523874895u, 3035404103u, 1084719657u, 609544609u, 2435866387u, 1152095999u, 363228695u, 2634348021u,
    456764893u, 1923799275u, 1142935515u, 327025431u, 3429065625u, 2471945603u
};

int SourceSortingLayerRank(int64_t id)
{
    // AssetRipper may serialize the same 32-bit ID as signed or unsigned.
    if (id < (std::numeric_limits<int32_t>::min)() || id > (std::numeric_limits<uint32_t>::max)())
        throw std::runtime_error("invalid source sorting layer ID: " + std::to_string(id));
    const auto found = std::find(kSourceSortingLayerIds.begin(), kSourceSortingLayerIds.end(), static_cast<uint32_t>(id));
    if (found == kSourceSortingLayerIds.end())
        throw std::runtime_error("unknown source sorting layer ID: " + std::to_string(id));
    return static_cast<int>(found-kSourceSortingLayerIds.begin());
}

using NativeFile = std::unique_ptr<PFILE, decltype(&p_fclose)>;
NativeFile OpenNativeFile(const std::filesystem::path& path)
{
    const auto name = Sexy::StringToSexyString(path.string());
    return NativeFile(p_fopen(name.c_str(), _S("rb")), &p_fclose);
}

struct Sha256
{
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    Sha256()
    {
        if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
            throw std::runtime_error("cannot initialize Fusion texture SHA256 provider");
        if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash))
        {
            CryptReleaseContext(provider, 0); provider = 0;
            throw std::runtime_error("cannot initialize Fusion texture SHA256 hash");
        }
    }
    ~Sha256() { if (hash) CryptDestroyHash(hash); if (provider) CryptReleaseContext(provider, 0); }
    Sha256(const Sha256&) = delete;
    Sha256& operator=(const Sha256&) = delete;
    std::string Digest(const std::vector<unsigned char>& bytes)
    {
        for (size_t offset = 0; offset < bytes.size();)
        {
            const auto count = static_cast<DWORD>((std::min)(size_t(65536), bytes.size()-offset));
            if (!CryptHashData(hash, bytes.data()+offset, count, 0)) throw std::runtime_error("cannot hash Fusion texture");
            offset += count;
        }
        std::array<BYTE, 32> digest{}; DWORD length = static_cast<DWORD>(digest.size());
        if (!CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &length, 0) || length != digest.size())
            throw std::runtime_error("cannot finish Fusion texture SHA256");
        std::string result;
        for (BYTE value : digest) { result += "0123456789abcdef"[value >> 4]; result += "0123456789abcdef"[value & 15]; }
        return result;
    }
};

struct PngBytes
{
    std::vector<unsigned char> bytes;
    std::string sha256;
    uint32_t width = 0, height = 0;
};
PngBytes ReadNativePng(const std::filesystem::path& path)
{
    auto file = OpenNativeFile(path);
    if (!file || p_fseek(file.get(), 0, SEEK_END) != 0) throw std::runtime_error("cannot read Fusion texture: "+path.string());
    const int size = p_ftell(file.get());
    if (size < 24 || p_fseek(file.get(), 0, SEEK_SET) != 0) throw std::runtime_error("invalid Fusion PNG: "+path.string());
    PngBytes result; result.bytes.resize(static_cast<size_t>(size));
    unsigned char extra;
    if (p_fread(result.bytes.data(), 1, size, file.get()) != static_cast<size_t>(size) || p_fread(&extra, 1, 1, file.get()) != 0)
        throw std::runtime_error("incomplete or changing Fusion texture: "+path.string());
    const std::array<unsigned char, 16> header = {137,80,78,71,13,10,26,10,0,0,0,13,'I','H','D','R'};
    if (!std::equal(header.begin(), header.end(), result.bytes.begin())) throw std::runtime_error("invalid Fusion PNG header: "+path.string());
    const auto dimension = [&](size_t offset)
    {
        uint32_t value = 0;
        for (size_t i = offset; i < offset+4; ++i) value = (value << 8) | result.bytes[i];
        return value;
    };
    result.width = dimension(16); result.height = dimension(20);
    if (!result.width || !result.height || result.width > static_cast<uint32_t>((std::numeric_limits<int>::max)()) ||
        result.height > static_cast<uint32_t>((std::numeric_limits<int>::max)())) throw std::runtime_error("invalid Fusion PNG size");
    Sha256 hash; result.sha256 = hash.Digest(result.bytes);
    return result;
}

void RequireExactNativePng(const std::filesystem::path& path)
{
    if (Sexy::StringToUpper(Sexy::StringToSexyString(path.extension().string())) != _S(".PNG"))
        throw std::runtime_error("Fusion source texture must be PNG: "+path.string());
    auto stem = path; stem.replace_extension();
    // ImageLib GetImage appends extensions and can compose _name/name_ alpha
    // siblings. Source packages contain standalone PNGs; reject other inputs
    // so the native decoder consumes exactly the bytes whose hash is shared.
    const std::array<std::filesystem::path, 3> stems = {stem,
        stem.parent_path()/("_"+stem.filename().string()), std::filesystem::path(stem.string()+"_")};
    for (size_t i = 0; i < stems.size(); ++i)
        for (const char* extension : {".png", ".tga", ".jpg", ".gif", ".j2k", ".jp2"})
        {
            if (i == 0 && std::string(extension) == ".png") continue;
            const auto sibling = std::filesystem::path(stems[i].string()+extension);
            if (OpenNativeFile(sibling)) throw std::runtime_error("ambiguous Fusion texture/alpha sibling: "+sibling.string());
        }
}

struct SharedTexture
{
    Sexy::SharedImageRef image;
    Sexy::Image* pixels = nullptr;
    std::filesystem::path canonicalPath;
    std::string sha256;
    uint64_t encodedSize = 0, rgbaBytes = 0;
    mutable std::shared_ptr<const std::vector<uint32_t>> maskPixels;
};
struct AppTextureCache
{
    std::map<std::string, std::vector<std::weak_ptr<const SharedTexture>>> contents;
    uint64_t loaded = 0, reused = 0, nextNativeVariant = 0;
};
std::mutex textureCacheMutex;
std::map<LawnApp*, AppTextureCache> textureCaches; // No strong/static native image references.

std::shared_ptr<const SharedTexture> AcquireTexture(LawnApp* app, const std::filesystem::path& path)
{
    RequireExactNativePng(path);
    const auto png = ReadNativePng(path);
    const std::string key = png.sha256+":"+std::to_string(png.bytes.size());
    std::lock_guard<std::mutex> lock(textureCacheMutex);
    auto& cache = textureCaches[app]; auto& candidates = cache.contents[key];
    for (auto it = candidates.begin(); it != candidates.end();)
    {
        auto existing = it->lock();
        if (!existing) { it = candidates.erase(it); continue; }
        ++it;
        try
        {
            const auto original = ReadNativePng(existing->canonicalPath);
            if (original.sha256 == existing->sha256 && original.bytes == png.bytes)
            { ++cache.reused; return existing; }
        }
        catch (const std::runtime_error&) { /* Changed/missing canonical input cannot authorize sharing. */ }
    }
    auto result = std::make_shared<SharedTexture>();
    auto stem = path; stem.replace_extension();
    // A failed decode must not leave a native cache entry reusable by a later
    // attempt (for example if a file changed while ImageLib was reading it).
    const auto variant = Sexy::StringToSexyString("FUSION_PNG_SHA256:"+key+":"+std::to_string(++cache.nextNativeVariant));
    bool isNew = false;
    result->image = app->GetSharedImage(Sexy::StringToSexyString(stem.string()), variant, &isNew);
    result->pixels = static_cast<Sexy::Image*>(result->image);
    if (!result->pixels) throw std::runtime_error("cannot load Fusion texture: "+path.string());
    const auto after = ReadNativePng(path);
    if (after.sha256 != png.sha256 || after.bytes != png.bytes || result->pixels->mWidth != static_cast<int>(png.width) ||
        result->pixels->mHeight != static_cast<int>(png.height)) throw std::runtime_error("Fusion texture changed during native decode: "+path.string());
    result->canonicalPath = path; result->sha256 = png.sha256; result->encodedSize = png.bytes.size();
    result->rgbaBytes = uint64_t(png.width)*png.height*4;
    candidates.push_back(result);
    if (isNew) ++cache.loaded; else ++cache.reused;
    return result;
}

float Number(const Json& value)
{
    if (value.is_string())
    {
        const std::string text = value.get<std::string>();
        if (text == "Infinity") return std::numeric_limits<float>::infinity();
        if (text == "-Infinity") return -std::numeric_limits<float>::infinity();
        throw std::runtime_error("invalid curve tangent");
    }
    return value.get<float>();
}

Vector ReadVector(const Json& value)
{
    if (value.is_object()) return {Number(value.at("x")), Number(value.at("y")), Number(value.at("z"))};
    return {Number(value), 0.0f, 0.0f};
}

struct Matrix
{
    float m[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    Vector position{};
};

Matrix Multiply(const Matrix& p, const Matrix& q)
{
    Matrix result;
    for (int row = 0; row < 3; ++row)
    {
        result.position[row] = p.position[row];
        for (int column = 0; column < 3; ++column)
        {
            result.m[row][column] = 0;
            result.position[row] += p.m[row][column]*q.position[column];
            for (int axis = 0; axis < 3; ++axis) result.m[row][column] += p.m[row][axis]*q.m[axis][column];
        }
    }
    return result;
}

Quaternion Multiply(const Quaternion& p, const Quaternion& q)
{
    return {p[3]*q[0]+p[0]*q[3]+p[1]*q[2]-p[2]*q[1],
        p[3]*q[1]-p[0]*q[2]+p[1]*q[3]+p[2]*q[0],
        p[3]*q[2]+p[0]*q[1]-p[1]*q[0]+p[2]*q[3],
        p[3]*q[3]-p[0]*q[0]-p[1]*q[1]-p[2]*q[2]};
}

Quaternion EulerRotation(const Vector& euler)
{
    const float x = euler[0]*kPi/360.0f, y = euler[1]*kPi/360.0f, z = euler[2]*kPi/360.0f;
    // Unity's Euler curves use Z, then X, then Y rotations (ZXY).
    return Multiply(Multiply(Quaternion{0, std::sin(y), 0, std::cos(y)},
        Quaternion{std::sin(x), 0, 0, std::cos(x)}), Quaternion{0, 0, std::sin(z), std::cos(z)});
}

Matrix Transform(const Vector& position, const Quaternion& q, const Vector& scale)
{
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    Matrix result;
    result.m[0][0] = (1-2*(y*y+z*z))*scale[0]; result.m[0][1] = 2*(x*y-z*w)*scale[1]; result.m[0][2] = 2*(x*z+y*w)*scale[2];
    result.m[1][0] = 2*(x*y+z*w)*scale[0]; result.m[1][1] = (1-2*(x*x+z*z))*scale[1]; result.m[1][2] = 2*(y*z-x*w)*scale[2];
    result.m[2][0] = 2*(x*z-y*w)*scale[0]; result.m[2][1] = 2*(y*z+x*w)*scale[1]; result.m[2][2] = (1-2*(x*x+y*y))*scale[2];
    result.position = position;
    return result;
}

struct Variant
{
    struct Vertex { float x, y, u, v, z=0; };
    std::shared_ptr<const SharedTexture> image;
    Sexy::Image* pixels = nullptr;
    Sexy::Rect rect;
    float logicalWidth = 0, logicalHeight = 0, pivotX = 0, pivotY = 0;
    float offsetX = 0, offsetY = 0, pixelsPerUnit = 100;
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    std::vector<FusionDeform269Weight> skinWeights;
    std::vector<FusionDeform269Matrix> bindPoses;
};

struct Pose
{
    Vector position{}, scale{1, 1, 1};
    Quaternion rotation{0, 0, 0, 1};
    bool active = true;
    bool spriteChanged = false;
    const Variant* sprite = nullptr;
    float alpha = 1.0f;
    bool alphaChanged = false, enabled = true, enabledChanged = false;
    bool flipX = false, flipXChanged = false, flipY = false, flipYChanged = false; // Renderer-only; never a Transform/child reflection.
    float boxOffsetY=0,boxSizeY=1;
    bool boxOffsetYChanged=false,boxSizeYChanged=false;
    Vector colorRGB{1, 1, 1};
    std::array<bool,3> colorRGBChanged{};
    float sortingOrder = 0;
    bool sortingOrderChanged = false;
};

struct Node { std::string path; int parent = -1; Pose rest; };
struct BoxCollider303
{
    int64_t fileId=0;int node=-1;
    std::array<float,2> offset{},size{};
    bool enabled=false,trigger=false;
};
std::vector<BoxCollider303> ReadBoxes303(const Json& data,size_t nodeCount)
{
    bool enabled=false;
    if(data.contains("physicsFeatures")&&!data.at("physicsFeatures").is_array())throw std::runtime_error("invalid source physics feature list");
    for(const auto& feature:data.value("physicsFeatures",Json::array()))
    {
        if(feature!="boxY303"||enabled)throw std::runtime_error("unsupported/duplicate source physics feature");
        enabled=true;
    }
    if(!enabled)return {};
    if(!data.contains("colliders")||!data.at("colliders").is_array())throw std::runtime_error("invalid source collider list");
    std::vector<BoxCollider303> boxes;
        // Component IDs are integer Unity IDs, never binary64 identities.
        std::set<int64_t> colliderIds303;
        for(const auto& item:data.value("colliders",Json::array()))
        {
            if(item.at("type")!="box")throw std::runtime_error("unsupported collider geometry");
            const auto& identity=item.at("fileId");
            if(!identity.is_number_integer()||(identity.is_number_unsigned()&&identity.get<uint64_t>()>uint64_t(INT64_MAX)))
                throw std::runtime_error("invalid BoxCollider2D component identity");
            BoxCollider303 box;box.fileId=identity.get<int64_t>();box.node=item.at("node").get<int>();
            if(!box.fileId||!colliderIds303.insert(box.fileId).second||box.node<0||box.node>=int(nodeCount))
                throw std::runtime_error("invalid/duplicate BoxCollider2D identity/node");
            box.enabled=item.at("enabled").get<bool>();box.trigger=item.at("trigger").get<bool>();
            const auto readXY=[](const Json& value)
            {
                if(!value.is_object()||value.size()!=2||!value.contains("x")||!value.contains("y"))throw std::runtime_error("invalid box vector");
                std::array<float,2> xy{value.at("x").get<float>(),value.at("y").get<float>()};
                if(!std::isfinite(xy[0])||!std::isfinite(xy[1]))throw std::runtime_error("nonfinite box vector");
                return xy;
            };
            box.offset=readXY(item.at("offset"));box.size=readXY(item.at("size"));
            if(box.size[0]<=0||box.size[1]<=0)throw std::runtime_error("nonpositive source box size");
            boxes.push_back(box);
        }
    return boxes;
}
void BindBoxDefaults303(std::span<Node> nodes,const std::vector<BoxCollider303>& boxes)
{
        for(const auto& box:boxes)
            if(std::count_if(boxes.begin(),boxes.end(),[&](const auto& other){return other.node==box.node;})==1)
            {nodes[box.node].rest.boxOffsetY=box.offset[1];nodes[box.node].rest.boxSizeY=box.size[1];}
}
struct Sprite { int node; const Variant* variant; Sexy::Color color; bool flipX, flipY, enabled; int sortingLayerRank, sortingOrder;uint8_t maskInteraction=0; };
enum class CurveKind { Position, Scale, Euler, Active, Sprite, Alpha, Red, Green, Blue, Enabled, FlipX, FlipY, BoxOffsetY, BoxSizeY, SortingOrder };
bool IsColorCurve(CurveKind kind)
{ return kind == CurveKind::Alpha || kind == CurveKind::Red || kind == CurveKind::Green || kind == CurveKind::Blue; }
struct Key { float time = 0; Vector value{}, inSlope{}, outSlope{}; const Variant* sprite = nullptr; };
struct Curve { int node; CurveKind kind; std::vector<Key> keys; };
struct Clip { float duration = 0; bool loop = false; std::vector<Curve> curves; std::vector<FusionAnimationEvent> events; };
struct State { std::string clip, speedParameterName; float speed = 1; bool speedParameter = false; int layer = -1; bool additive = false, supported = true; bool writeDefaultsKnown = false, writeDefaults = false; bool initialRest347 = false; };

// Preserve legacy packages/calls without inventing default-write history.
// A new stateless blend requires both exact source states to prove WD1.
std::map<int64_t,bool> SourceWriteDefaults(const std::string& serialized)
{
    std::map<int64_t,bool> result;
    std::set<int64_t> seen;
    std::istringstream stream(serialized);
    std::string line;
    std::optional<int64_t> state;
    while (std::getline(stream,line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.starts_with("--- !u!"))
        {
            state.reset();
            const std::string prefix = "--- !u!1102 &";
            if (line.starts_with(prefix))
            {
                try
                {
                    size_t length = 0;
                    const auto id = std::stoll(line.substr(prefix.size()),&length);
                    if (id && length == line.size()-prefix.size())
                    {
                        if (seen.insert(id).second) state = id;
                        else result.erase(id); // Duplicate source identity cannot prove WD1.
                    }
                }
                catch (const std::exception&) {} // Unknown metadata refuses only new blends.
            }
        }
        else if (state && line.starts_with("  m_WriteDefaultValues:"))
        {
            if ((line != "  m_WriteDefaultValues: 0" && line != "  m_WriteDefaultValues: 1") ||
                !result.emplace(*state,line.back() == '1').second)
            { result.erase(*state); state.reset(); }
        }
    }
    return result;
}

bool BlendRotation(const Quaternion& from,const Quaternion& to,float weight,Quaternion& output)
{
    auto a = from, b = to;
    const auto normalize = [](Quaternion& q)
    {
        double length = 0;
        for (float value : q) { if (!std::isfinite(value)) return false; length += double(value)*value; }
        if (length <= 1e-20) return false;
        const float inverse = float(1/std::sqrt(length));
        for (auto& value : q) value *= inverse;
        return true;
    };
    if (!normalize(a) || !normalize(b)) return false;
    float dot = 0;
    for (size_t i = 0; i < 4; ++i) dot += a[i]*b[i];
    if (dot < 0) { for (auto& value : b) value = -value; dot = -dot; }
    dot = std::clamp(dot,0.0f,1.0f);
    float left = 1-weight, right = weight;
    if (dot < .9995f)
    {
        const float angle = std::acos(dot), sine = std::sin(angle);
        left = std::sin((1-weight)*angle)/sine;
        right = std::sin(weight*angle)/sine;
    }
    Quaternion value;
    for (size_t i = 0; i < 4; ++i) value[i] = left*a[i]+right*b[i];
    if (!normalize(value)) return false;
    output = value;
    return true;
}

bool BlendBasePoses(const std::vector<Pose>& outgoing,std::vector<Pose>& incoming,
    const std::vector<Sprite>& sprites,float weight)
{
    if (!std::isfinite(weight) || weight < 0 || weight > 1 || outgoing.size() != incoming.size()) return false;
    if (weight == 1) return true;
    if (weight == 0) { incoming = outgoing; return true; }
    std::vector<Pose> blended;
    blended.reserve(incoming.size());
    for (size_t i = 0; i < incoming.size(); ++i)
    {
        const auto& a = outgoing[i]; const auto& b = incoming[i];
        // Active/enabled/sorting/Sprite are discrete. Copy all change flags too,
        // so an explicit null Sprite stays different from an unkeyed default.
        Pose value = weight < .5f ? a : b;
        for (size_t axis = 0; axis < 3; ++axis)
        {
            value.position[axis] = a.position[axis]+weight*(b.position[axis]-a.position[axis]);
            value.scale[axis] = a.scale[axis]+weight*(b.scale[axis]-a.scale[axis]);
        }
        if (!BlendRotation(a.rotation,b.rotation,weight,value.rotation)) return false;
        // One actual Box61 per animated node; its original Y defaults are
        // already in Node.rest, so an unkeyed transition side is not zero.
        value.boxOffsetY=a.boxOffsetY+weight*(b.boxOffsetY-a.boxOffsetY);
        value.boxSizeY=a.boxSizeY+weight*(b.boxSizeY-a.boxSizeY);
        value.boxOffsetYChanged=a.boxOffsetYChanged||b.boxOffsetYChanged;
        value.boxSizeYChanged=a.boxSizeYChanged||b.boxSizeYChanged;
        if (a.alphaChanged || b.alphaChanged ||
            std::any_of(a.colorRGBChanged.begin(),a.colorRGBChanged.end(),[](bool changed){return changed;}) ||
            std::any_of(b.colorRGBChanged.begin(),b.colorRGBChanged.end(),[](bool changed){return changed;}))
        {
            const auto sprite = std::find_if(sprites.begin(),sprites.end(),[&](const Sprite& source){return source.node == int(i);});
            if (sprite == sprites.end() || std::find_if(std::next(sprite),sprites.end(),
                [&](const Sprite& source){return source.node == int(i);}) != sprites.end()) return false;
            if (a.alphaChanged || b.alphaChanged)
            {
                const float rest = sprite->color.mAlpha/255.0f;
                const float left = a.alphaChanged ? a.alpha : rest, right = b.alphaChanged ? b.alpha : rest;
                value.alpha = left+weight*(right-left); value.alphaChanged = true;
            }
            const std::array<int,3> rest{sprite->color.mRed,sprite->color.mGreen,sprite->color.mBlue};
            for (size_t axis = 0; axis < 3; ++axis)
                if (a.colorRGBChanged[axis] || b.colorRGBChanged[axis])
                {
                    const float left = a.colorRGBChanged[axis] ? a.colorRGB[axis] : rest[axis]/255.0f;
                    const float right = b.colorRGBChanged[axis] ? b.colorRGB[axis] : rest[axis]/255.0f;
                    value.colorRGB[axis] = left+weight*(right-left); value.colorRGBChanged[axis] = true;
                }
        }
        blended.push_back(value);
    }
    incoming = std::move(blended);
    return true;
}

Vector Sample(const Curve& curve, float time)
{
    if (time <= curve.keys.front().time) return curve.keys.front().value;
    if (time >= curve.keys.back().time) return curve.keys.back().value;
    size_t next = 1;
    while (curve.keys[next].time <= time) ++next;
    const Key& a = curve.keys[next-1];
    const Key& b = curve.keys[next];
    const float duration = b.time-a.time;
    const float t = (time-a.time)/duration;
    const float t2 = t*t, t3 = t2*t;
    Vector result;
    for (int axis = 0; axis < 3; ++axis)
    {
        result[axis] = !std::isfinite(a.outSlope[axis]) || !std::isfinite(b.inSlope[axis]) ? a.value[axis] :
            (2*t3-3*t2+1)*a.value[axis] + (t3-2*t2+t)*duration*a.outSlope[axis] +
            (-2*t3+3*t2)*b.value[axis] + (t3-t2)*duration*b.inSlope[axis];
    }
    return result;
}
}

struct FusionAnimation::Impl
{
    struct Skin { int node=-1,root=-1;bool enabled=false;std::vector<int> bones; };
    struct Mask { int node=-1;const Variant* variant=nullptr;bool enabled=false;double cutoff=.2;FusionMask269Sort back,front;FusionMask269Filter filter=FusionMask269Filter::Bilinear;FusionMask269Wrap wrap=FusionMask269Wrap::Clamp; };
    std::vector<Mask> masks;
    std::map<int,Skin> skins;
    mutable std::unique_ptr<Sexy::MemoryImage> maskScratch;
    bool SamplePose(const char* baseName,float baseElapsed,const char* actionName,float actionElapsed,float speed,const FusionAnimationDrawOptions* options,std::vector<Pose>& pose) const;
    void World(const std::vector<Pose>& pose,std::vector<Matrix>& world,std::vector<bool>& active) const
    {
        world.resize(pose.size());active.assign(pose.size(),true);std::vector<bool> done(pose.size(),false);
        std::function<void(int)> calculate=[&](int index)
        {
            if(done[index])return;
            const auto& local=pose[index];const auto transform=Transform(local.position,local.rotation,local.scale);
            const int parent=nodes[index].parent;
            if(parent>=0){calculate(parent);world[index]=Multiply(world[parent],transform);active[index]=active[parent]&&local.active;}
            else{world[index]=transform;active[index]=local.active;}done[index]=true;
        };
        for(int i=0;i<int(pose.size());++i)calculate(i);
    }
    std::vector<Node> nodes;
    // Unity sibling names need not be unique. Hierarchy/render identity is the
    // serialized node index; a textual path can be missing(-1) or ambiguous(-2).
    std::map<std::string,int,std::less<>> nodeIndex;
    int FindNode(const char* path) const
    {
        if(!path)return -1;
        const auto found=nodeIndex.find(path);
        if(found!=nodeIndex.end())return found->second;
        // Unity can address same-named siblings by Transform child index.
        // Imported node indices preserve that identity without deleting nodes
        // or resolving an ambiguous source path to an arbitrary sibling.
        if(!std::strncmp(path,"@node:",6))
        {
            int index=-1;const char* end=path+std::strlen(path);
            const auto parsed=std::from_chars(path+6,end,index);
            if(parsed.ec==std::errc{}&&parsed.ptr==end&&index>=0&&size_t(index)<nodes.size())return index;
        }
        return -1;
    }
    std::vector<Sprite> sprites;
    std::vector<BoxCollider303> boxes;
    std::map<std::string, Variant> variants;
    std::map<std::string, Clip> clips;
    std::map<std::string, State> states;
    std::map<std::pair<std::string, int>, State> layeredStates;
    std::string defaultState;
    int root = 0;
    bool applyRootMotion = false;
    bool isStatic = false;

    const State* FindState(const char* name) const
    {
        const auto found = name ? states.find(name) : states.end();
        return found == states.end() || !found->second.supported ? nullptr : &found->second;
    }
    const State* FindState(const char* name, int layer) const
    {
        if (layer < 0) return FindState(name);
        if (!name) return nullptr;
        const auto found = layeredStates.find({name,layer});
        return found == layeredStates.end() || !found->second.supported ? nullptr : &found->second;
    }

    void Apply(std::vector<Pose>& pose, const State& state, float elapsed, float speed, bool additive) const
    {
        const auto found = clips.find(state.clip);
        if (found == clips.end()) return;
        const Clip& clip = found->second;
        float time = (std::max)(0.0f, elapsed)*state.speed*(state.speedParameter ? speed : 1.0f);
        if (additive && !clip.loop && time > clip.duration) return;
        time = clip.loop ? std::fmod(time, clip.duration) : (std::min)(time, clip.duration);
        for (const Curve& curve : clip.curves)
        {
            if (curve.node < 0) continue; // Unity ignores bindings absent from this prefab.
            Pose& target = pose[curve.node];
            if (curve.kind == CurveKind::Sprite)
            {
                const Key* key = &curve.keys.front();
                for (const Key& candidate : curve.keys) { if (candidate.time > time) break; key = &candidate; }
                target.spriteChanged = true;
                target.sprite = key->sprite;
                continue;
            }
            if (curve.kind == CurveKind::FlipX || curve.kind == CurveKind::FlipY)
            {
                const Key* key = &curve.keys.front();
                for (const Key& candidate : curve.keys) { if (candidate.time > time) break; key = &candidate; }
                if(curve.kind == CurveKind::FlipX){target.flipX = key->value[0] != 0; target.flipXChanged = true;}
                else{target.flipY = key->value[0] != 0; target.flipYChanged = true;}
                continue;
            }
            const Vector value = Sample(curve, time);
            const Vector reference = curve.keys.front().value;
            if (curve.kind == CurveKind::Active) target.active = value[0] >= 0.5f;
            else if (curve.kind == CurveKind::Alpha) { target.alpha = additive ? target.alpha+value[0]-reference[0] : value[0]; target.alphaChanged = true; }
            else if (curve.kind == CurveKind::Red) { target.colorRGB[0] = value[0]; target.colorRGBChanged[0] = true; }
            else if (curve.kind == CurveKind::Green) { target.colorRGB[1] = value[0]; target.colorRGBChanged[1] = true; }
            else if (curve.kind == CurveKind::Blue) { target.colorRGB[2] = value[0]; target.colorRGBChanged[2] = true; }
            else if (curve.kind == CurveKind::Enabled) { target.enabled = (additive ? float(target.enabled)+value[0]-reference[0] : value[0]) >= 0.5f; target.enabledChanged = true; }
            else if (curve.kind == CurveKind::SortingOrder)
            {
                target.sortingOrder = additive ? target.sortingOrder+value[0]-reference[0] : value[0];
                target.sortingOrderChanged = true;
            }
            else if(curve.kind == CurveKind::BoxOffsetY)
            {target.boxOffsetY=additive?target.boxOffsetY+value[0]-reference[0]:value[0];target.boxOffsetYChanged=true;}
            else if(curve.kind == CurveKind::BoxSizeY)
            {target.boxSizeY=additive?target.boxSizeY+value[0]-reference[0]:value[0];target.boxSizeYChanged=true;}
            else if (curve.kind == CurveKind::Euler)
            {
                if (!additive) target.rotation = EulerRotation(value);
                else
                {
                    const Quaternion initial = EulerRotation(reference);
                    target.rotation = Multiply(target.rotation, Multiply(Quaternion{-initial[0], -initial[1], -initial[2], initial[3]}, EulerRotation(value)));
                }
            }
            else if (curve.kind == CurveKind::Position)
            {
                // Authoritative gameplay applies root displacement; rendering must not apply it again.
                if (applyRootMotion && curve.node == root) continue;
                for (int axis = 0; axis < 3; ++axis) target.position[axis] = additive ? target.position[axis]+value[axis]-reference[axis] : value[axis];
            }
            else for (int axis = 0; axis < 3; ++axis) target.scale[axis] = additive ? target.scale[axis]*value[axis]/reference[axis] : value[axis];
        }
    }
};

FusionAnimation::FusionAnimation() = default;
FusionAnimation::~FusionAnimation() = default;
FusionAnimation::TextureCacheStats FusionAnimation::GetTextureCacheStats(LawnApp* app)
{
    TextureCacheStats result;
    std::lock_guard<std::mutex> lock(textureCacheMutex);
    const auto found = textureCaches.find(app);
    if (found == textureCaches.end()) return result;
    result.loadedImages = found->second.loaded; result.reusedImages = found->second.reused;
    for (auto content = found->second.contents.begin(); content != found->second.contents.end();)
    {
        auto& entries = content->second;
        for (auto entry = entries.begin(); entry != entries.end();)
            if (auto image = entry->lock()) { ++result.uniqueContents; result.retainedRgbaBytes += image->rgbaBytes; ++entry; }
            else entry = entries.erase(entry);
        if (entries.empty()) content = found->second.contents.erase(content); else ++content;
    }
    return result;
}
bool FusionAnimation::IsLoaded() const { return mImpl != nullptr; }
bool FusionAnimation::IsStatic() const { return mImpl && mImpl->isStatic; }
bool FusionAnimation::HasMasks() const { return mImpl && !mImpl->masks.empty(); }
const char* FusionAnimation::GetDefaultStateName() const { return mImpl ? mImpl->defaultState.c_str() : nullptr; }

bool FusionAnimation::Load(LawnApp* app, const std::string& path, std::string& error,
    std::span<const int64_t> validatedParticleComponents)
{
    try
    {
        std::ifstream file(path);
        if (!file) throw std::runtime_error("cannot open animation package: "+path);
        const Json data = Json::parse(file);
        if (data.at("schema") != 1) throw std::runtime_error("unsupported animation schema");
        std::set<int64_t> sourceParticles,validatedParticles;
        for(const auto& component:data.value("nonRenderingComponents",Json::array()))
            if(component.at("typeId")==198||component.at("typeId")==199)
                if(!sourceParticles.insert(component.at("fileId").get<int64_t>()).second)throw std::runtime_error("duplicate source particle component");
        for(auto id:validatedParticleComponents)if(!validatedParticles.insert(id).second)throw std::runtime_error("duplicate native particle binding proof");
        // Preserve only previously accepted sprite+Shadow models with their
        // explicitly deferred trails (cold109.md:44, renderer188/README.md:13,
        // and batch40-264 projectile-art-ready.json / grouped acceptance).
        // Exact prefab+component IDs restore that bounded baseline, not new VFX parity.
        const auto legacyPrefab=data.value("sourcePrefab",std::string());
        const bool legacySnowPeaTrail=(legacyPrefab=="Resources/bullet/prefabs/ProjectileSnowPea.prefab"||
            legacyPrefab=="Resources/bullet/prefabs/ExtremeSnowPea.prefab")&&
            validatedParticles.empty()&&sourceParticles==std::set<int64_t>{198243736805655561LL,199061268324235159LL};
        const bool legacyPuffTrail=(legacyPrefab=="Resources/bullet/prefabs/PuffRandomColor.prefab"||legacyPrefab=="Resources/bullet/prefabs/PuffSnow.prefab")&&
            validatedParticles.empty()&&sourceParticles==std::set<int64_t>{198099118695303065LL,199243736805655561LL};
        const bool legacyTrackTrail=legacyPrefab=="Resources/bullet/prefabs/TrackBullet.prefab"&&validatedParticles.empty()&&
            sourceParticles==std::set<int64_t>{198705390221481576LL,199977153420339851LL,198256069497284392LL,199161607090078389LL};
        const bool legacyIceDoomTrail=legacyPrefab=="Resources/bullet/prefabs/IceDoomBullet.prefab"&&validatedParticles.empty()&&
            sourceParticles==std::set<int64_t>{198243736805655561LL,199061268324235159LL};
        // SourceZombie247 / the prior nine-source-NPC bounded baseline retained
        // original decorative particles without drawing them. Keep only those
        // exact prefab/component sets; this does not add particle VFX parity.
        const bool legacyZombieParticles=validatedParticles.empty()&&(
            (legacyPrefab=="Resources/zombies/_prefab/Zombie.prefab"&&sourceParticles==std::set<int64_t>{
                198477046003934300LL,199045458335737618LL,198924050743934937LL,199215879266689626LL})||
            ((legacyPrefab=="Resources/zombies/_prefab/ConeZombie.prefab"||legacyPrefab=="Resources/zombies/_prefab/BucketZombie.prefab")&&
                sourceParticles==std::set<int64_t>{198215879266689626LL,199689364072382846LL,198743393880019028LL,
                    199094320201520353LL,198572839542275309LL,199279359913592680LL})||
            (legacyPrefab=="Resources/zombies/_prefab/DoorZombie.prefab"&&sourceParticles==std::set<int64_t>{
                198149520701623706LL,199292246286975720LL,198630643969539439LL,199370222726513266LL,
                198009931292380359LL,199393105877652414LL})||
            (legacyPrefab=="Resources/zombies/_prefab/DuckZombie.prefab"&&sourceParticles==std::set<int64_t>{
                199026196548002408LL,198572839542275309LL,198384403120170545LL,199149520701623706LL})||
            ((legacyPrefab=="Resources/zombies/_prefab/DuckConeZombie.prefab"||legacyPrefab=="Resources/zombies/_prefab/DuckBucketZombie.prefab")&&
                sourceParticles==std::set<int64_t>{199149520701623706LL,198292246286975720LL,198630643969539439LL,
                    199370222726513266LL,198009931292380359LL,199393105877652414LL}));
        // Native22 packages prove LoseArm inactive in the born/intact scope.
        // NativeZombieVisual also validates the complete missing-target proof
        // and refuses the active limb-loss path; retain those bounds here.
        const auto dormantZombieParticles=[&](const char* prefab,const char* proofKey,const char* bound,int64_t system,int64_t renderer)
        {
            if(legacyPrefab!=prefab||!validatedParticles.empty()||sourceParticles!=std::set<int64_t>{system,renderer}||
                !data.contains(proofKey)||!data.value("deferredComponents",Json::array()).empty())return false;
            const auto& dormant=data.at(proofKey);
            if(dormant.value("runtimeBound",std::string())!=bound||!dormant.contains("proofs")||dormant.at("proofs").size()!=1)return false;
            const auto& proof=dormant.at("proofs").at(0);
            if(proof.value("componentFileId",int64_t(0))!=system||proof.value("nodePath",std::string())!="LoseArm"||
                proof.value("reason",std::string())!="inactive-until-source-arm-loss")return false;
            bool inactive=false,serialized=false;
            for(const auto& node:data.at("nodes"))if(node.at("path")=="LoseArm"&&!node.at("active").get<bool>())inactive=true;
            for(const auto& component:data.at("nonRenderingComponents"))if(component.at("fileId")==system&&component.contains("serializedComponent")&&
                proof.contains("serializedComponent")&&component.at("serializedComponent")==proof.at("serializedComponent"))serialized=true;
            for(const auto& clip:data.at("clips"))for(const auto& curve:clip.at("curves"))
                if(curve.at("path")=="LoseArm"&&curve.value("attribute",std::string())=="m_IsActive")return false;
            return inactive&&serialized;
        };
        const bool dormantNutZombieParticles=dormantZombieParticles("Resources/zombies/_prefab/TallIceNutZombie.prefab",
            "source111DormantParticleProof","born-withEffect-false-and-intact-head-arm",198632746491512912LL,199719342369648492LL)||
            dormantZombieParticles("Resources/zombies/_prefab/TallNutFootballZombie.prefab",
                "source109DormantParticleProof","born-and-intact-head-arm",198722951815217645LL,199924050743934937LL);
        if(sourceParticles!=validatedParticles&&!legacySnowPeaTrail&&!legacyPuffTrail&&!legacyTrackTrail&&!legacyIceDoomTrail&&
            !legacyZombieParticles&&!dormantNutZombieParticles)throw std::runtime_error("source particle components require exact native binding/resource preflight");
        // The legacy native background retains seven bg SpriteRenderers only.
        // Original background1.prefab gives all seven m_MaskInteraction=0;
        // its five IceRoad masks affect omitted scene actors, not these sprites.
        bool legacyUnmaskedBackground=data.value("sourcePrefab",std::string())=="Resources/background/background1.prefab";
        if(legacyUnmaskedBackground)
        {
            for(const auto& sprite:data.at("sprites"))if(sprite.value("maskInteraction",0)!=0)legacyUnmaskedBackground=false;
            for(const auto& clip:data.at("clips"))for(const auto& curve:clip.at("curves"))
            {const auto kind=curve.at("kind").get<std::string>();if(kind!="position"&&kind!="scale"&&kind!="euler")legacyUnmaskedBackground=false;}
        }
        const auto declaredFeatures=data.value("rendererFeatures",Json::array());
        // Genuine Miner28 references Unity's unavailable builtin sprite10905.
        // Only this audited complete package may use the explicit native
        // Digger ground clip. No source alpha pixels are invented or omitted.
        bool nativeTunnel313=false;
        if(std::any_of(declaredFeatures.begin(),declaredFeatures.end(),[](const Json& f){return f=="nativeTunnelClip313";}))
        {
            std::ifstream original(path,std::ios::binary);
            const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(original)),std::istreambuf_iterator<char>());
            Sha256 hash;
            if(data.value("sourcePrefab",std::string())!="Resources/zombies/_prefab/MinerZombie.prefab"||
               !original||hash.Digest(bytes)!="763ba0f3a71857e4e5efc3c2612c6535858a270e16d289a24f5f8415cfb059f1"||
               data.at("externalSpriteMasks313").size()!=1||!data.at("sourceMasks").empty())
                throw std::runtime_error("native Miner tunnel projection proof differs");
            nativeTunnel313=true;
        }
        const bool nativeMasks=data.contains("sourceMasks")&&!data.at("sourceMasks").empty()&&
            std::any_of(declaredFeatures.begin(),declaredFeatures.end(),[](const Json& feature){return feature=="spriteMaskV1";});
        for(const auto& deferred:data.value("deferredComponents",Json::array()))
        {
            if(deferred==331&&(legacyUnmaskedBackground||nativeMasks||nativeTunnel313))continue;
            if((deferred!=198&&deferred!=199)||sourceParticles.empty())throw std::runtime_error("unsupported deferred source renderer component");
        }
        bool skinFeature=false,maskFeature=false;
        for(const auto& feature:data.value("rendererFeatures",Json::array()))
        {
            const auto name=feature.get<std::string>();
            if(name=="spriteSkinV1")skinFeature=true;
            else if(name=="spriteMaskV1")maskFeature=true;
            else if(name=="nativeTunnelClip313"&&nativeTunnel313)continue;
            else throw std::runtime_error("unsupported source renderer feature: "+name);
        }
        auto result = std::make_unique<Impl>();
        result->root = data.at("root").get<int>();
        result->applyRootMotion = data.value("applyRootMotion", false);
        result->isStatic = data.value("static", false);
        const auto folder = std::filesystem::path(path).parent_path();
        std::map<std::filesystem::path, std::shared_ptr<const SharedTexture>> loadTextures;
        for (auto it = data.at("spriteVariants").begin(); it != data.at("spriteVariants").end(); ++it)
        {
            const Json& item = it.value();
            Variant variant;
            const auto texture = folder/item.at("texture").get<std::string>();
            if (!std::filesystem::is_regular_file(texture)) throw std::runtime_error("missing texture: "+texture.string());
            if (app)
            {
                auto& shared = loadTextures[texture.lexically_normal()];
                if (!shared) shared = AcquireTexture(app, texture);
                variant.image = shared;
                variant.pixels = shared->pixels;
            }
            const auto& rect = item.at("textureRect");
            variant.rect = Sexy::Rect(rect.at("x").get<int>(), rect.at("yTop").get<int>(), rect.at("width").get<int>(), rect.at("height").get<int>());
            variant.logicalWidth = item.at("logicalSize").at("width").get<float>();
            variant.logicalHeight = item.at("logicalSize").at("height").get<float>();
            variant.pivotX = item.at("pivot").at("x").get<float>(); variant.pivotY = item.at("pivot").at("y").get<float>();
            variant.offsetX = item.at("textureRectOffset").at("x").get<float>(); variant.offsetY = item.at("textureRectOffset").at("y").get<float>();
            variant.pixelsPerUnit = item.at("pixelsPerUnit").get<float>();
            if (variant.pixelsPerUnit <= 0 || variant.rect.mWidth <= 0 || variant.rect.mHeight <= 0) throw std::runtime_error("invalid sprite dimensions");
            if (item.contains("mesh"))
            {
                for (const auto& vertex : item.at("mesh").at("vertices"))
                    variant.vertices.push_back({vertex.at("x").get<float>(), vertex.at("y").get<float>(), vertex.at("u").get<float>(), vertex.at("v").get<float>(),vertex.at("z").get<float>()});
                for (const auto& index : item.at("mesh").at("indices"))
                {
                    const int value = index.get<int>();
                    if (value < 0 || value >= static_cast<int>(variant.vertices.size()) || value > 65535) throw std::runtime_error("invalid sprite mesh index");
                    variant.indices.push_back(static_cast<uint16_t>(value));
                }
                if (variant.indices.empty() || variant.indices.size()%3) throw std::runtime_error("invalid sprite mesh triangles");
            }
            if(item.contains("skin"))
            {
                if(!skinFeature)throw std::runtime_error("undeclared source skin feature");
                const auto& skin=item.at("skin");
                for(const auto& matrix:skin.at("bindPoses"))
                {
                    if(matrix.size()!=4)throw std::runtime_error("invalid source bind matrix");
                    FusionDeform269Matrix value;
                    for(int row=0;row<4;++row)
                    {
                        if(matrix[row].size()!=4)throw std::runtime_error("invalid source bind matrix row");
                        for(int col=0;col<4;++col)value.value[row*4+col]=matrix[row][col].get<double>();
                    }
                    variant.bindPoses.push_back(value);
                }
                const auto& weights=skin.at("weights");const auto& indices=skin.at("boneIndices");
                if(weights.size()!=variant.vertices.size()||indices.size()!=weights.size())throw std::runtime_error("skin vertex stream length differs");
                for(size_t i=0;i<weights.size();++i)
                {
                    if(weights[i].size()!=4||indices[i].size()!=4)throw std::runtime_error("skin needs exactly four influences");
                    FusionDeform269Weight value;
                    for(int j=0;j<4;++j){value.weights[j]=weights[i][j].get<double>();value.indices[j]=indices[i][j].get<uint32_t>();}
                    for(double weight:value.weights)if(!std::isfinite(weight)||weight<0)throw std::runtime_error("invalid source skin weight");
                    variant.skinWeights.push_back(value);
                }
                std::vector<FusionDeform269Matrix> identity(variant.bindPoses.size()),checked;
                if(!FusionDeform269Palette(FusionDeform269Matrix{},identity,variant.bindPoses,checked))throw std::runtime_error("invalid source bind-pose palette");
            }
            result->variants.emplace(it.key(), variant);
        }
        for (const auto& item : data.at("nodes"))
        {
            Node node;
            node.path = item.at("path").get<std::string>(); node.parent = item.at("parent").get<int>();
            node.rest.position = ReadVector(item.at("position")); node.rest.scale = ReadVector(item.at("scale"));
            const auto& rotation = item.at("rotation");
            node.rest.rotation = {Number(rotation.at("x")), Number(rotation.at("y")), Number(rotation.at("z")), Number(rotation.at("w"))};
            float norm = 0;
            for (float value : node.rest.rotation) norm += value*value;
            if (!std::isfinite(norm) || norm < 0.000001f) throw std::runtime_error("invalid rotation at node: "+node.path);
            for (float& value : node.rest.rotation) value /= std::sqrt(norm);
            node.rest.active = item.at("active").get<bool>();
            const auto [path,unique]=result->nodeIndex.emplace(node.path,static_cast<int>(result->nodes.size()));
            if(!unique)path->second=-2;
            result->nodes.push_back(node);
        }
        if (result->root < 0 || result->root >= static_cast<int>(result->nodes.size())) throw std::runtime_error("invalid root transform");
        for (size_t i = 0; i < result->nodes.size(); ++i)
        {
            int parent = static_cast<int>(i);
            for (size_t depth = 0; parent >= 0; ++depth)
            {
                if (parent >= static_cast<int>(result->nodes.size()) || depth > result->nodes.size()) throw std::runtime_error("invalid transform hierarchy");
                parent = result->nodes[parent].parent;
            }
        }
        if(skinFeature)for(const auto& item:data.at("sourceSkins"))
        {
            if(item.at("scriptFileId")!=-972417856||item.at("scriptGuid")!="f68e4d9210748575b7313c5ba1460876")throw std::runtime_error("unproved source SpriteSkin identity");
            Impl::Skin skin;skin.node=item.at("node").get<int>();skin.root=item.at("rootBoneNode").get<int>();
            skin.enabled=item.at("enabled").get<bool>();skin.bones=item.at("boneNodes").get<std::vector<int>>();
            if(skin.node<0||skin.node>=int(result->nodes.size())||skin.root< -1||skin.root>=int(result->nodes.size())||item.at("autoRebind").get<bool>())throw std::runtime_error("unsupported source skin rig");
            for(int node:skin.bones)if(node< -1||node>=int(result->nodes.size()))throw std::runtime_error("unresolved source bone");
            const auto sourceValidation=[&](const std::string& guid)
            {
                if(guid.empty())return FusionSkin269Validation::NoSprite;
                const auto& variant=result->variants.at(guid);
                return FusionDeform269ValidateSkin(true,variant.bindPoses.size(),skin.root>=0,skin.bones,variant.skinWeights);
            };
            const std::string initial=item.at("sourceSpriteGuid").is_null()?"":item.at("sourceSpriteGuid").get<std::string>();
            if(int(sourceValidation(initial))!=item.at("sourceValidationResult").get<int>())throw std::runtime_error("source skin fallback differs");
            for(auto it=item.at("sourceValidationBySprite").begin();it!=item.at("sourceValidationBySprite").end();++it)
                if(int(sourceValidation(it.key()))!=it.value().get<int>())throw std::runtime_error("source sprite-swap palette fallback differs");
            if(!result->skins.emplace(skin.node,std::move(skin)).second)throw std::runtime_error("duplicate SpriteSkin on renderer");
        }
        for (const auto& item : data.at("sprites"))
        {
            const int node = item.at("node").get<int>();
            if (node < 0 || node >= static_cast<int>(result->nodes.size())) throw std::runtime_error("invalid sprite transform index");
            const int sortingLayerRank = SourceSortingLayerRank(item.at("sortingLayerId").get<int64_t>());
            const auto& color = item.at("color");
            result->nodes[node].rest.alpha=color.at("a").get<float>();
            result->nodes[node].rest.enabled=item.at("enabled").get<bool>();
            const auto channel = [&](const char* key) { return static_cast<int>(std::clamp(color.at(key).get<float>()*255.0f, 0.0f, 255.0f)+0.5f); };
            const Variant* variant = item.at("guid").is_null() ? nullptr : &result->variants.at(item.at("guid").get<std::string>());
            result->sprites.push_back({node, variant, Sexy::Color(channel("r"), channel("g"), channel("b"), channel("a")),
                item.at("flipX").get<bool>(), item.at("flipY").get<bool>(), item.at("enabled").get<bool>(),
                sortingLayerRank, item.at("sortingOrder").get<int>(),static_cast<uint8_t>(item.value("maskInteraction",0))});
            if(result->sprites.back().maskInteraction>2||(!maskFeature&&result->sprites.back().maskInteraction))throw std::runtime_error("undeclared/invalid source mask interaction");
        }
        if(maskFeature)for(const auto& item:data.at("sourceMasks"))
        {
            Impl::Mask mask;mask.node=item.at("node").get<int>();
            if(mask.node<0||mask.node>=int(result->nodes.size()))throw std::runtime_error("invalid source mask node");
            mask.variant=&result->variants.at(item.at("spriteGuid").get<std::string>());
            if(mask.variant->indices.empty())throw std::runtime_error("source mask requires original triangles");
            mask.enabled=item.at("enabled").get<bool>();mask.cutoff=item.at("alphaCutoff").get<double>();
            if(!std::isfinite(mask.cutoff)||mask.cutoff<0||mask.cutoff>1)throw std::runtime_error("invalid source mask cutoff");
            if(item.at("customRange").get<bool>())
            {
                mask.back={SourceSortingLayerRank(item.at("backSortingLayerId").get<int64_t>()),item.at("backSortingOrder").get<int>(),0};
                mask.front={SourceSortingLayerRank(item.at("frontSortingLayerId").get<int64_t>()),item.at("frontSortingOrder").get<int>(),0};
            }
            else { mask.back={INT_MIN,INT_MIN,0};mask.front={INT_MAX,INT_MAX,0}; }
            if(std::tie(mask.front.layer,mask.front.order)<std::tie(mask.back.layer,mask.back.order))throw std::runtime_error("reversed source mask range");
            const auto& sampler=item.at("sampler");const int filter=sampler.at("filter").get<int>(),wrap=sampler.at("wrap").get<int>();
            if(filter<0||filter>1||wrap<0||wrap>1)throw std::runtime_error("unsupported source mask sampler");
            mask.filter=filter?FusionMask269Filter::Bilinear:FusionMask269Filter::Point;
            mask.wrap=wrap?FusionMask269Wrap::Clamp:FusionMask269Wrap::Repeat;
            if(mask.variant->image)
            {
                auto& texture=*mask.variant->image;
                if(!texture.maskPixels)
                {
                    auto* native=dynamic_cast<Sexy::MemoryImage*>(texture.pixels);
                    if(!native||!native->GetBits())throw std::runtime_error("cannot retain source mask alpha");
                    static_assert(sizeof(*native->GetBits())==sizeof(uint32_t));
                    auto pixels=std::make_shared<std::vector<uint32_t>>(size_t(native->mWidth)*native->mHeight);
                    std::memcpy(pixels->data(),native->GetBits(),pixels->size()*sizeof(uint32_t));texture.maskPixels=std::move(pixels);
                }
            }
            result->masks.push_back(mask);
        }
        for (const auto& sprite : result->sprites) result->nodes[sprite.node].rest.sortingOrder=static_cast<float>(sprite.sortingOrder);
        std::stable_sort(result->sprites.begin(), result->sprites.end(), [](const Sprite& a, const Sprite& b)
        {
            return a.sortingLayerRank != b.sortingLayerRank ? a.sortingLayerRank < b.sortingLayerRank : a.sortingOrder < b.sortingOrder;
        });
        result->boxes=ReadBoxes303(data,result->nodes.size());
        BindBoxDefaults303(result->nodes,result->boxes);
        for (auto it = data.at("clips").begin(); it != data.at("clips").end(); ++it)
        {
            Clip clip;
            clip.duration = it.value().at("duration").get<float>(); clip.loop = it.value().at("loop").get<bool>();
            if (clip.duration <= 0) throw std::runtime_error("invalid clip duration");
            for (const auto& item : it.value().at("curves"))
            {
                Curve curve;
                const auto path=item.at("path").get<std::string>();curve.node=result->FindNode(path.c_str());
                if(curve.node==-2)throw std::runtime_error("ambiguous transform path: "+path);
                const std::string kind = item.at("kind").get<std::string>();
                if (kind == "position") curve.kind = CurveKind::Position;
                else if (kind == "scale") curve.kind = CurveKind::Scale;
                else if (kind == "euler") curve.kind = CurveKind::Euler;
                else if (kind == "sprite") curve.kind = CurveKind::Sprite;
                else if(kind=="skinAutoRebind")
                {
                    if(!skinFeature||item.at("classID")!=114||item.at("attribute")!="m_AutoRebind"||item.at("scriptFileId")!=-972417856||item.at("scriptGuid")!="f68e4d9210748575b7313c5ba1460876")throw std::runtime_error("unproved skin property");
                    for(const auto& key:item.at("keys"))if(Number(key.at("value"))!=0)throw std::runtime_error("dynamic skin rebind is unsupported");
                    continue; // Explicit source property is constant false, matching the serialized rig.
                }
                else if (kind == "float")
                {
                    const int type = item.at("classID").get<int>();
                    const std::string attribute = item.at("attribute").get<std::string>();
                    if (type == 1 && attribute == "m_IsActive") curve.kind = CurveKind::Active;
                    else if (type == 212 && item.value("scriptFileId", int64_t(0)) == 0 && attribute == "m_Color.a") curve.kind = CurveKind::Alpha;
                    else if (type == 212 && item.value("scriptFileId", int64_t(0)) == 0 && attribute == "m_Color.r") curve.kind = CurveKind::Red;
                    else if (type == 212 && item.value("scriptFileId", int64_t(0)) == 0 && attribute == "m_Color.g") curve.kind = CurveKind::Green;
                    else if (type == 212 && item.value("scriptFileId", int64_t(0)) == 0 && attribute == "m_Color.b") curve.kind = CurveKind::Blue;
                    else if (type == 212 && item.value("scriptFileId", int64_t(0)) == 0 && attribute == "m_Enabled") curve.kind = CurveKind::Enabled;
                    else if(type==61&&item.value("scriptFileId",int64_t(-1))==0&&attribute=="m_Offset.y")curve.kind=CurveKind::BoxOffsetY;
                    else if(type==61&&item.value("scriptFileId",int64_t(-1))==0&&attribute=="m_Size.y")curve.kind=CurveKind::BoxSizeY;
                    else if (type == 212 && item.value("scriptFileId", int64_t(0)) == 0 && attribute == "m_FlipY") curve.kind = CurveKind::FlipY;
                    else if (type == 212 && item.value("scriptFileId", int64_t(0)) == 0 && attribute == "m_FlipX") curve.kind = CurveKind::FlipX;
                    else if (type == 212 && item.value("scriptFileId", int64_t(0)) == 0 && attribute == "m_SortingOrder") curve.kind = CurveKind::SortingOrder;
                    else throw std::runtime_error("unsupported animation float target at node: "+item.at("path").get<std::string>());
                }
                else throw std::runtime_error("unsupported animation curve at node: "+item.at("path").get<std::string>());
                for (const auto& value : item.at("keys"))
                {
                    Key key; key.time = value.at("time").get<float>();
                    if (curve.kind == CurveKind::Sprite)
                    { if (!value.at("spriteGuid").is_null()) key.sprite = &result->variants.at(value.at("spriteGuid").get<std::string>()); }
                    else
                    {
                        if (value.at("weightedMode").get<int>() != 0) throw std::runtime_error("weighted keyframes are unsupported at node: "+item.at("path").get<std::string>());
                        key.value = ReadVector(value.at("value")); key.inSlope = ReadVector(value.at("inSlope")); key.outSlope = ReadVector(value.at("outSlope"));
                        if (curve.kind == CurveKind::Scale && (key.value[0] == 0 || key.value[1] == 0 || key.value[2] == 0) && curve.keys.empty()) throw std::runtime_error("zero additive scale reference is unsupported");
                    }
                    if (!curve.keys.empty() && key.time <= curve.keys.back().time) throw std::runtime_error("unordered animation keyframes");
                    curve.keys.push_back(key);
                }
                if (curve.keys.empty()) throw std::runtime_error("empty animation curve");
                if(curve.kind==CurveKind::BoxOffsetY||curve.kind==CurveKind::BoxSizeY)
                {
                    if(curve.node<0||std::count_if(result->boxes.begin(),result->boxes.end(),[&](const auto& box){return box.node==curve.node;})!=1)
                        throw std::runtime_error("BoxCollider2D animation requires one exact component: "+path);
                    for(const auto& key:curve.keys)
                        if(!std::isfinite(key.time)||key.time<0||!std::isfinite(key.value[0])||std::isnan(key.inSlope[0])||std::isnan(key.outSlope[0])||
                            (curve.kind==CurveKind::BoxSizeY&&key.value[0]<=0))throw std::runtime_error("invalid source BoxCollider2D Y curve");
                }
                if (IsColorCurve(curve.kind) || curve.kind == CurveKind::Enabled || curve.kind == CurveKind::FlipX || curve.kind == CurveKind::FlipY || curve.kind == CurveKind::SortingOrder)
                {
                    if (curve.node >= 0 && std::count_if(result->sprites.begin(), result->sprites.end(),
                        [&](const Sprite& sprite) { return sprite.node == curve.node; }) != 1)
                        throw std::runtime_error("ambiguous or missing SpriteRenderer at node: "+item.at("path").get<std::string>());
                    for (size_t i = 0; i < curve.keys.size(); ++i)
                    {
                        const auto& key = curve.keys[i];
                        if (!std::isfinite(key.value[0]) || ((curve.kind == CurveKind::Enabled || curve.kind == CurveKind::FlipX || curve.kind == CurveKind::FlipY) && key.value[0] != 0 && key.value[0] != 1))
                            throw std::runtime_error("invalid SpriteRenderer float value");
                        if (curve.kind == CurveKind::SortingOrder && (key.value[0] != std::trunc(key.value[0]) || key.value[0] < -32768 || key.value[0] > 32767))
                            throw std::runtime_error("invalid SpriteRenderer sorting order");
                        if (curve.kind == CurveKind::Enabled && i && key.value[0] != curve.keys[i-1].value[0] &&
                            std::isfinite(curve.keys[i-1].outSlope[0]) && std::isfinite(key.inSlope[0]))
                            throw std::runtime_error("interpolated SpriteRenderer enabled curve is unsupported");
                    }
                }
                if (curve.kind == CurveKind::FlipX || curve.kind == CurveKind::FlipY)
                {
                    if (const auto skin=result->skins.find(curve.node);skin!=result->skins.end()&&skin->second.enabled)
                        throw std::runtime_error("animated SpriteRenderer flip on active SpriteSkin requires proven deformation frame");
                    for (size_t i=1;i<curve.keys.size();++i)
                    {
                        const auto& a=curve.keys[i-1];const auto& b=curve.keys[i];
                        if (std::isfinite(a.outSlope[0])&&std::isfinite(b.inSlope[0])&&
                            (a.value[0]!=b.value[0]||a.outSlope[0]!=0||b.inSlope[0]!=0))
                            throw std::runtime_error("interpolated SpriteRenderer flip curve is unsupported");
                    }
                }
                clip.curves.push_back(std::move(curve));
            }
            for (const auto& event : it.value().at("events")) clip.events.push_back({event.at("time").get<float>(), event.at("functionName").get<std::string>()});
            result->clips.emplace(it.key(), std::move(clip));
        }
        // The exact source218 package has one WD0 walk entry. Fresh source
        // SetZombie451130 directly plays it from idle; no edge enters it.
        // Its five unkeyed properties are untouched by idle. Keep WD0 genuine,
        // and permit only its outgoing rest-based fade, never an incoming one.
        bool initialRest347=false;
        if(data.value("sourcePrefab",std::string())=="Resources/zombies/_prefab/UltimateFootballDrown.prefab")
        {
            std::ifstream original(path,std::ios::binary);
            const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(original)),std::istreambuf_iterator<char>());
            Sha256 hash;
            initialRest347=original&&hash.Digest(bytes)=="300175ec9d1d311aacaee4caa4c9911696b24990147f3c816b0de86256cf5d04";
        }
        const auto writeDefaults = SourceWriteDefaults(data.at("controller").value("serializedController",std::string()));
        const auto& layers = data.at("controller").at("layers");
        // An actual source Animator may contain an empty state machine (LilyPad).
        // The exporter's static flag denotes absence of an Animator, not this
        // equally static rest-pose case. Any clip/state/reference retains the
        // existing animated default-state validation.
        bool emptyController=data.at("clips").empty()&&data.at("controller").at("states").empty();
        for(const auto& layer:layers)
            emptyController=emptyController&&layer.at("defaultState").get<int64_t>()==0&&layer.at("states").empty();
        if(emptyController)result->isStatic=true;
        if (layers.empty() && !result->isStatic) throw std::runtime_error("missing controller layer");
        const auto defaultId = layers.empty() ? int64_t(0) : layers[0].at("defaultState").get<int64_t>();
        for (const auto& item : data.at("controller").at("states"))
        {
            const bool nullMotion=item.at("clip").is_null();
            State state; state.clip = nullMotion?std::string():item.at("clip").get<std::string>();
            if(!nullMotion&&!result->clips.contains(state.clip))throw std::runtime_error("controller state references an unavailable animation clip");
            state.speed = item.at("speed").get<float>(); state.speedParameter = item.at("speedParameterActive").get<bool>();
            if (!std::isfinite(state.speed) || state.speed <= 0) throw std::runtime_error("invalid controller state speed");
            state.speedParameterName = item.at("speedParameter").get<std::string>();
            // Keep unrequested source states without pretending that a custom
            // gameplay parameter is the shared animator Speed value.
            state.supported = !state.speedParameter || state.speedParameterName == "Speed" || state.speedParameterName == "attackSpeed" || state.speedParameterName == "jumpSpeed";
            const auto id = item.at("id").get<int64_t>();
            if (const auto found = writeDefaults.find(id); found != writeDefaults.end())
            { state.writeDefaultsKnown = true; state.writeDefaults = found->second; }
            state.initialRest347=initialRest347&&id==1102061268324235159LL&&state.writeDefaultsKnown&&!state.writeDefaults;
            // A no-motion WD1 state samples the genuine rest/prior-layer pose.
            // WD0 would retain history that this stateless sampler does not own.
            if(nullMotion)state.supported=state.supported&&state.writeDefaultsKnown&&state.writeDefaults;
            for (size_t layer = 0; layer < layers.size(); ++layer) for (const auto& stateId : layers[layer].at("states")) if (stateId.get<int64_t>() == id)
            { state.layer = static_cast<int>(layer); state.additive = layers[layer].at("blendingMode").get<int>() == 1; }
            if (state.additive&&!nullMotion)
                for (const auto& curve : result->clips.at(state.clip).curves)
                    if (curve.kind == CurveKind::FlipX || curve.kind == CurveKind::FlipY)
                        throw std::runtime_error("additive SpriteRenderer flip requires proved reference semantics");
                    else if (curve.kind == CurveKind::Alpha || curve.kind == CurveKind::Enabled)
                    {
                        // The existing additive transform sampler uses the clip's
                        // time-zero reference. Apply the same delta to alpha and
                        // discrete enabled values, retaining the actual base pose.
                        // An identity curve must not overwrite another layer.
                        const auto& sourceClip=data.at("clips").at(state.clip);
                        const bool proven=sourceClip.contains("additiveReferencePoseTime")&&
                            sourceClip.at("additiveReferencePoseTime")==0&&
                            sourceClip.contains("hasAdditiveReferencePose")&&!sourceClip.at("hasAdditiveReferencePose").get<bool>()&&
                            state.layer>=0&&layers[state.layer].contains("defaultWeight")&&layers[state.layer].at("defaultWeight")==1;
                        if(!proven)throw std::runtime_error("additive SpriteRenderer alpha/enabled reference is unsupported");
                    }
                    else if (IsColorCurve(curve.kind))
                        throw std::runtime_error("additive SpriteRenderer RGB curves require verified reference semantics");
            const auto name = item.at("name").get<std::string>();
            if (id == defaultId) result->defaultState = name;
            result->layeredStates.emplace(std::make_pair(name,state.layer),state);
            result->states.emplace(name, std::move(state));
        }
        if (result->defaultState.empty() && !result->isStatic) throw std::runtime_error("default state has no animation clip");
        if (!result->defaultState.empty() && !result->states.at(result->defaultState).supported)
            throw std::runtime_error("default controller state requires an unsupported speed parameter: " +
                result->states.at(result->defaultState).speedParameterName);
        mImpl = std::move(result);
        error.clear();
        return true;
    }
    catch (const std::exception& exception) { error = exception.what(); return false; }
}

int FusionAnimation::GetStateLayer(const char* state) const
{
    const State* found=mImpl?mImpl->FindState(state):nullptr;return found?found->layer:-1;
}

float FusionAnimation::GetStateSpeed(const char* state) const
{
    const State* found = mImpl ? mImpl->FindState(state) : nullptr;
    return found ? found->speed : 0.0f;
}
float FusionAnimation::GetStateSpeed(const char* state, int layer) const
{
    const State* found = mImpl ? mImpl->FindState(state,layer) : nullptr;
    return found ? found->speed : 0.0f;
}

float FusionAnimation::GetStateDurationSeconds(const char* state, float animatorSpeed) const
{
    const State* found = mImpl ? mImpl->FindState(state) : nullptr;
    if (!found || animatorSpeed <= 0) return 0;
    const auto clip=mImpl->clips.find(found->clip);if(clip==mImpl->clips.end())return 0;
    const bool externalParameter=found->speedParameter&&found->speedParameterName!="jumpSpeed";
    return clip->second.duration/(found->speed*(externalParameter ? animatorSpeed : 1.0f));
}

const std::vector<FusionAnimationEvent>& FusionAnimation::GetEvents(const char* state) const
{
    static const std::vector<FusionAnimationEvent> empty;
    const State* found = mImpl ? mImpl->FindState(state) : nullptr;
    if(!found)return empty;const auto clip=mImpl->clips.find(found->clip);
    return clip==mImpl->clips.end()?empty:clip->second.events;
}

void FusionAnimation::Draw(Sexy::Graphics* graphics, float x, float y, float scale, float idleElapsed, float shootElapsed, float speed) const
{
    if (mImpl) DrawPose(graphics, x, y, scale, mImpl->defaultState.c_str(), idleElapsed, "shoot", shootElapsed, speed);
}

bool FusionAnimation::Impl::SamplePose(const char* baseName,float baseElapsed,const char* actionName,float actionElapsed,float speed,const FusionAnimationDrawOptions* options,std::vector<Pose>& pose) const
{
    if(!std::isfinite(baseElapsed)||!std::isfinite(actionElapsed)||!std::isfinite(speed)||speed<=0)return false;
    const bool restOnly=options&&options->restPoseOnly;
    const State* base = restOnly?nullptr:FindState(baseName);
    if (!base && !isStatic&&!restOnly) return false;
    if (options && (!std::isfinite(options->attackSpeed) || options->attackSpeed <= 0)) return false;
    if(options && (!std::isfinite(options->brightnessGain) || options->brightnessGain<1 || options->brightnessGain>4 ||
       (options->nativeMaskClipBottom && !std::isfinite(*options->nativeMaskClipBottom))))return false;
    if(options)
    {
        const auto ambiguous=[&](const auto& values)
        {return std::any_of(values.begin(),values.end(),[&](const auto& value){return FindNode(value.nodePath.c_str())==-2;});};
        for(const auto& path:options->hiddenNodePaths)if(FindNode(path.c_str())==-2)return false;
        if(FindNode(options->spritePathPrefix.c_str())==-2||
            ambiguous(options->nodeActivityOverrides)||ambiguous(options->nodeRotations)||
            ambiguous(options->nodeWorldPositions)||ambiguous(options->spriteReplacements)||
            ambiguous(options->spriteSortingOverrides)||ambiguous(options->sortingGroupOverrides)||
            ambiguous(options->maskSortingOverrides)||ambiguous(options->spriteEnabledOverrides))return false;
    }
    const auto parameterSpeed = [&](const State& state)
    {
        // The source graph supplies jumpSpeed through its real native clock;
        // elapsed already contains it, so sampling must not apply it twice.
        if(state.speedParameterName=="jumpSpeed")return 1.0f;
        return state.speedParameterName == "attackSpeed" ? (options ? options->attackSpeed : 1.0f) : speed;
    };
    pose.clear(); pose.reserve(nodes.size());
    for (const Node& node : nodes) pose.push_back(node.rest);
    if (base) Apply(pose, *base, baseElapsed, parameterSpeed(*base), false);
    if (options && !restOnly && options->baseBlend)
    {
        const auto& blend = *options->baseBlend;
        const State* outgoing = FindState(blend.outgoingState.c_str());
        if (!base || !outgoing || base->layer < 0 || base->layer != outgoing->layer || base->additive || outgoing->additive ||
            !base->writeDefaultsKnown || !outgoing->writeDefaultsKnown || !base->writeDefaults || (!outgoing->writeDefaults&&!outgoing->initialRest347) ||
            !std::isfinite(blend.outgoingElapsedSeconds) || blend.outgoingElapsedSeconds < 0 ||
            !std::isfinite(blend.incomingWeight) || blend.incomingWeight < 0 || blend.incomingWeight > 1) return false;
        std::vector<Pose> previous;
        previous.reserve(nodes.size());
        for (const Node& node : nodes) previous.push_back(node.rest);
        Apply(previous,*outgoing,blend.outgoingElapsedSeconds,parameterSpeed(*outgoing),false);
        if (!BlendBasePoses(previous,pose,sprites,blend.incomingWeight)) return false;
    }
    const State* action = restOnly?nullptr:FindState(actionName);
    if (action && actionElapsed >= 0)
    {
        if (!action->additive && base && action->layer == base->layer)
            for (size_t i = 0; i < pose.size(); ++i) pose[i] = nodes[i].rest;
        Apply(pose, *action, actionElapsed, parameterSpeed(*action), action->additive);
    }
    if (options&&!restOnly)
        for (const auto& extra : options->extraStates)
        {
            if (!std::isfinite(extra.weight) || extra.weight < 0 || extra.weight > 1) return false;
            if (extra.baseBlend)
            {
                const auto& blend = *extra.baseBlend;
                const State* incoming = FindState(extra.stateName.c_str(),extra.layer);
                const State* outgoing = FindState(blend.outgoingState.c_str(),extra.layer);
                if (!incoming || !outgoing || !incoming->additive || !outgoing->additive ||
                    incoming->layer < 0 || incoming->layer != outgoing->layer ||
                    !incoming->writeDefaultsKnown || !outgoing->writeDefaultsKnown ||
                    !incoming->writeDefaults || !outgoing->writeDefaults ||
                    !std::isfinite(extra.elapsedSeconds) || extra.elapsedSeconds < 0 ||
                    !std::isfinite(blend.outgoingElapsedSeconds) || blend.outgoingElapsedSeconds < 0 ||
                    !std::isfinite(blend.incomingWeight) || blend.incomingWeight < 0 || blend.incomingWeight > 1) return false;
                if (extra.weight == 0) continue;
                const auto baseline = pose;
                auto previous = baseline;
                Apply(previous,*outgoing,blend.outgoingElapsedSeconds,parameterSpeed(*outgoing),true);
                Apply(pose,*incoming,extra.elapsedSeconds,parameterSpeed(*incoming),true);
                if (!BlendBasePoses(previous,pose,sprites,blend.incomingWeight)) return false;
                if (extra.weight < 1 && !BlendBasePoses(baseline,pose,sprites,extra.weight)) return false;
                continue;
            }
            if (std::isfinite(extra.elapsedSeconds) && extra.elapsedSeconds >= 0)
                if (const State* state = FindState(extra.stateName.c_str(),extra.layer))
                {
                    if (extra.weight == 0) continue;
                    if (extra.weight == 1)
                        Apply(pose,*state,extra.elapsedSeconds,parameterSpeed(*state),state->additive);
                    else
                    {
                        // The admitted partial layer is additive. Blend its
                        // local delta through the same TRS/color/discrete rules
                        // as base fades, retaining the already sampled base.
                        if (!state->additive) return false;
                        const auto previous = pose;
                        Apply(pose,*state,extra.elapsedSeconds,parameterSpeed(*state),true);
                        if (!BlendBasePoses(previous,pose,sprites,extra.weight)) return false;
                    }
                }
        }
    // CreatePlant/board placement overwrites the prefab's editor world position.
    // Root motion belongs to the authoritative simulation rather than this draw.
    pose[root].position = {0.0f, 0.0f, 0.0f};
    if (options)
    {
        for (size_t i = 0; i < pose.size(); ++i)
        {
            if (std::any_of(options->hiddenNodePaths.begin(),options->hiddenNodePaths.end(),[&](const auto& path){return FindNode(path.c_str())==int(i);}))
                pose[i].active = false;
            for (const auto& activity : options->nodeActivityOverrides)
                if (FindNode(activity.nodePath.c_str()) == int(i)) pose[i].active = activity.active;
            for (const auto& renderer : options->spriteEnabledOverrides)
                if (FindNode(renderer.nodePath.c_str()) == int(i))
                { pose[i].enabled=renderer.enabled;pose[i].enabledChanged=true; }
            for (const auto& rotation : options->nodeRotations)
                if (FindNode(rotation.nodePath.c_str()) == int(i) && std::isfinite(rotation.radiansZ))
                {
                    const float half = rotation.radiansZ * .5f;
                    const Quaternion value{0, 0, std::sin(half), std::cos(half)};
                    pose[i].rotation = rotation.replace ? value : Multiply(pose[i].rotation, value);
                }
            for (const auto& replacement : options->spriteReplacements)
                if (FindNode(replacement.nodePath.c_str()) == int(i))
                {
                    const auto variant = variants.find(replacement.spriteGuid);
                    if (variant != variants.end())
                    {
                        pose[i].spriteChanged = true;
                        pose[i].sprite = &variant->second;
                        if (replacement.overrideScale)
                            pose[i].scale = {replacement.scaleX, replacement.scaleY, replacement.scaleZ};
                    }
                }
        }
    }
    if (options && options->rootFlipX) pose[root].scale[0] = -pose[root].scale[0];
    if (options)
        for (const auto& position : options->nodeWorldPositions)
        {
            if (!std::isfinite(position.x) || !std::isfinite(position.y)) return false;
            const int index=FindNode(position.nodePath.c_str());
            if(index<0)return false;
            Matrix world=Transform(pose[index].position,pose[index].rotation,pose[index].scale);
            Matrix parent=Transform(Vector{0,0,0},Quaternion{0,0,0,1},Vector{1,1,1});
            int ancestor=nodes[index].parent;
            while(ancestor>=0)
            {
                parent=Multiply(Transform(pose[ancestor].position,pose[ancestor].rotation,pose[ancestor].scale),parent);
                ancestor=nodes[ancestor].parent;
            }
            world=Multiply(parent,world);
            const double determinant=double(parent.m[0][0])*parent.m[1][1]-double(parent.m[0][1])*parent.m[1][0];
            if(!std::isfinite(determinant)||std::abs(determinant)<=1e-12) return false;
            const double dx=position.x-world.position[0],dy=position.y-world.position[1];
            pose[index].position[0]+=float((parent.m[1][1]*dx-parent.m[0][1]*dy)/determinant);
            pose[index].position[1]+=float((-parent.m[1][0]*dx+parent.m[0][0]*dy)/determinant);
        }
    return true;
}

bool FusionAnimation::TryGetBoxCollider303(int64_t fileId,FusionAnimationBoxCollider303& result,
    const char* baseName,float baseElapsed,const char* actionName,float actionElapsed,
    float speed,const FusionAnimationDrawOptions* options) const
{
    if(!mImpl||!fileId)return false;
    const auto found=std::find_if(mImpl->boxes.begin(),mImpl->boxes.end(),[&](const auto& box){return box.fileId==fileId;});
    if(found==mImpl->boxes.end()||std::find_if(std::next(found),mImpl->boxes.end(),[&](const auto& box){return box.fileId==fileId;})!=mImpl->boxes.end())return false;
    std::vector<Pose> pose;if(!mImpl->SamplePose(baseName,baseElapsed,actionName,actionElapsed,speed,options,pose))return false;
    if(found->node<0||found->node>=int(pose.size()))return false;
    FusionAnimationBoxCollider303 pending;pending.localOffset=found->offset;pending.localSize=found->size;
    const auto& local=pose[found->node];
    if(local.boxOffsetYChanged)pending.localOffset[1]=local.boxOffsetY;
    if(local.boxSizeYChanged)pending.localSize[1]=local.boxSizeY;
    if(!std::isfinite(pending.localOffset[0])||!std::isfinite(pending.localOffset[1])||
        !std::isfinite(pending.localSize[0])||!std::isfinite(pending.localSize[1])||pending.localSize[0]<=0||pending.localSize[1]<=0)return false;
    std::vector<Matrix> world;std::vector<bool> active;mImpl->World(pose,world,active);
    const auto& transform=world[found->node];
    const std::array<std::array<float,2>,4> signs={{{-1,-1},{1,-1},{1,1},{-1,1}}};
    for(size_t i=0;i<4;++i)
    {
        const float x=pending.localOffset[0]+signs[i][0]*pending.localSize[0]*.5f;
        const float y=pending.localOffset[1]+signs[i][1]*pending.localSize[1]*.5f;
        // Compose actual 3D ancestor matrices FIRST. Local collider Z is0;
        // transformed Z has already influenced parent XY through World().
        const float px=transform.m[0][0]*x+transform.m[0][1]*y+transform.position[0];
        const float py=transform.m[1][0]*x+transform.m[1][1]*y+transform.position[1];
        if(!std::isfinite(px)||!std::isfinite(py))return false;
        pending.corners[i]={px,py};
        if(i==0)pending.aabb={px,py,px,py};
        else{pending.aabb[0]=(std::min)(pending.aabb[0],px);pending.aabb[1]=(std::min)(pending.aabb[1],py);
             pending.aabb[2]=(std::max)(pending.aabb[2],px);pending.aabb[3]=(std::max)(pending.aabb[3],py);}
    }
    pending.enabled=found->enabled;pending.trigger=found->trigger;pending.nodeActive=active[found->node];
    result=pending;return true;
}

bool FusionAnimation::TryGetNodeTransform(const char* path,std::array<float,6>& result,
    const char* baseName,float baseElapsed,const char* actionName,float actionElapsed,
    float speed,const FusionAnimationDrawOptions* options) const
{
    if(!mImpl||!path)return false;
    int index=mImpl->FindNode(path);
    if(index<0)return false;
    std::vector<Pose> pose;
    if(!mImpl->SamplePose(baseName,baseElapsed,actionName,actionElapsed,speed,options,pose))return false;
    Matrix world=Transform(pose[index].position,pose[index].rotation,pose[index].scale);
    while((index=mImpl->nodes[index].parent)>=0)
        world=Multiply(Transform(pose[index].position,pose[index].rotation,pose[index].scale),world);
    result={world.m[0][0],world.m[1][0],world.m[0][1],world.m[1][1],world.position[0],world.position[1]};
    return std::all_of(result.begin(),result.end(),[](float v){return std::isfinite(v);});
}

bool FusionAnimation::TryGetNodeOffset(const char* path,float& x,float& y,
    const char* baseName,float baseElapsed,const char* actionName,float actionElapsed,
    float speed,const FusionAnimationDrawOptions* options) const
{
    std::array<float,6> transform;
    if(!TryGetNodeTransform(path,transform,baseName,baseElapsed,actionName,actionElapsed,speed,options))return false;
    x=transform[4];y=transform[5];return true;
}

bool FusionAnimation::TryGetNodeActivity(const char* path,bool& active,
    const char* baseName,float baseElapsed,const char* actionName,float actionElapsed,
    float speed,const FusionAnimationDrawOptions* options) const
{
    if(!mImpl||!path)return false;
    int index=mImpl->FindNode(path);
    if(index<0)return false;std::vector<Pose> pose;
    if(!mImpl->SamplePose(baseName,baseElapsed,actionName,actionElapsed,speed,options,pose))return false;
    bool result=true;
    while(index>=0){result=result&&pose[index].active;index=mImpl->nodes[index].parent;}
    active=result;return true;
}

FusionMask269View FusionAnimationMask::View() const
{
    FusionMask269View result;result.vertices=vertices;result.indices=indices;
    result.texture={texturePixels?std::span<const uint32_t>(*texturePixels):std::span<const uint32_t>{},width,height,filter,wrap};
    result.back=back;result.front=front;result.alphaCutoff=alphaCutoff;return result;
}

bool FusionAnimation::CollectMasks(Sexy::Graphics* graphics,float x,float y,float scale,
    const char* baseName,float baseElapsed,const char* actionName,float actionElapsed,float speed,
    const FusionAnimationDrawOptions* options,std::vector<FusionAnimationMask>& output) const
{
    if(!mImpl||!graphics||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(scale))return false;
    if(options&&options->useBoardBasis)for(float v:options->boardBasis)if(!std::isfinite(v))return false;
    if(mImpl->masks.empty())return true;
    std::vector<Pose> pose;if(!mImpl->SamplePose(baseName,baseElapsed,actionName,actionElapsed,speed,options,pose))return false;
    std::vector<Matrix> world;std::vector<bool> active;mImpl->World(pose,world,active);
    std::vector<FusionAnimationMask> pending;
    for(const auto& mask:mImpl->masks)
    {
        if(!mask.enabled||!active[mask.node])continue;
        const auto& variant=*mask.variant;
        if(!variant.image||!variant.image->maskPixels||!variant.pixels)return false;
        FusionAnimationMask record;record.indices=variant.indices;record.texturePixels=variant.image->maskPixels;
        record.width=variant.pixels->mWidth;record.height=variant.pixels->mHeight;
        record.filter=mask.filter;record.wrap=mask.wrap;record.alphaCutoff=mask.cutoff;record.back=mask.back;record.front=mask.front;
        if(options)
        {
            record.back.scope=record.front.scope=options->maskScope;
            for(const auto& override:options->maskSortingOverrides)if(override.nodePath==mImpl->nodes[mask.node].path)
            {
                record.back.layer=override.backLayer;record.back.order=override.backOrder;
                record.front.layer=override.frontLayer;record.front.order=override.frontOrder;
            }
        }
        const auto& transform=world[mask.node];
        for(const auto& v:variant.vertices)
        {
            const float wx=transform.m[0][0]*v.x+transform.m[0][1]*v.y+transform.m[0][2]*v.z+transform.position[0];
            const float wy=transform.m[1][0]*v.x+transform.m[1][1]*v.y+transform.m[1][2]*v.z+transform.position[1];
            const bool affine=options&&options->useBoardBasis;
            record.vertices.push_back({x+(affine?options->boardBasis[0]*wx+options->boardBasis[2]*wy:scale*100.f*wx)+graphics->mTransX,
                y+(affine?options->boardBasis[1]*wx+options->boardBasis[3]*wy:-scale*100.f*wy)+graphics->mTransY,0,v.u,v.v});
        }
        bool covered=false;if(!FusionMask269Coverage(record.View(),0,0,covered))return false;
        pending.push_back(std::move(record));
    }
    for(auto& mask:pending)output.push_back(std::move(mask));return true;
}

void FusionAnimation::DrawPose(Sexy::Graphics* graphics, float x, float y, float scale, const char* baseName, float baseElapsed, const char* actionName, float actionElapsed, float speed, const FusionAnimationDrawOptions* options) const
{
    if(options&&options->restPoseOnly)return; // Never a rendering or client-prediction fallback.
    if (!mImpl || !graphics || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(scale) || !std::isfinite(baseElapsed) || !std::isfinite(actionElapsed) || !std::isfinite(speed) || speed <= 0) return;
    if(options&&options->brightnessGain>1&&graphics->mDrawMode!=Sexy::Graphics::DRAWMODE_NORMAL)return;
    if(options&&options->useBoardBasis)
        for(float value:options->boardBasis)if(!std::isfinite(value))return;
    std::vector<Pose> pose;
    if (!mImpl->SamplePose(baseName,baseElapsed,actionName,actionElapsed,speed,options,pose)) return;
    std::vector<Matrix> world;std::vector<bool> active;mImpl->World(pose,world,active);
    std::vector<FusionAnimationMask> localMasks;std::vector<FusionMask269View> localViews;
    std::span<const FusionMask269View> masks;
    if(options&&options->useWorldMaskScope)
    {
        if(!options->maskScope)return; // A world domain must have an explicit frame-owned identity.
        masks=options->worldMasks;
    }
    else if(std::any_of(mImpl->sprites.begin(),mImpl->sprites.end(),[](const Sprite& v){return v.maskInteraction!=0;}))
    {
        if(!CollectMasks(graphics,x,y,scale,baseName,baseElapsed,actionName,actionElapsed,speed,options,localMasks))return;
        for(const auto& mask:localMasks)localViews.push_back(mask.View());masks=localViews;
    }
    const auto spriteSort=[&](const Sprite& sprite)
    {
        FusionMask269Sort sort{sprite.sortingLayerRank,sprite.sortingOrder,options?options->maskScope:0};
        const auto& local=pose[sprite.node];
        if(local.sortingOrderChanged)sort.order=int(std::clamp(local.sortingOrder,-32768.f,32767.f));
        if(options)for(const auto& override:options->spriteSortingOverrides)if(override.nodePath==mImpl->nodes[sprite.node].path){sort.layer=override.layer;sort.order=override.order;}
        return sort;
    };
    std::vector<const Sprite*> sortedSprites;
    const bool animatedOrder = (options&&(!options->spriteSortingOverrides.empty()||!options->sortingGroupOverrides.empty()))||std::any_of(pose.begin(),pose.end(),[](const Pose& local) { return local.sortingOrderChanged; });
    if (animatedOrder)
    {
        sortedSprites.reserve(mImpl->sprites.size());
        for (const Sprite& sprite : mImpl->sprites) sortedSprites.push_back(&sprite);
        std::stable_sort(sortedSprites.begin(),sortedSprites.end(),[&](const Sprite* a,const Sprite* b)
        {
            const auto x=spriteSort(*a),y=spriteSort(*b);
            if(options&&!options->sortingGroupOverrides.empty())
            {
                const auto key=[&](const Sprite* sprite,const FusionMask269Sort& leaf)
                {
                    std::vector<const FusionAnimationDrawOptions::SortingGroup*> groups;
                    const auto& path=mImpl->nodes[sprite->node].path;
                    for(const auto& group:options->sortingGroupOverrides)
                        if(path==group.nodePath||path.starts_with(group.nodePath+"/"))groups.push_back(&group);
                    std::stable_sort(groups.begin(),groups.end(),[](const auto* a,const auto* b){return a->nodePath.size()<b->nodePath.size();});
                    std::vector<std::pair<int,int>> result;
                    for(const auto* group:groups)result.emplace_back(group->layer,group->order);
                    result.emplace_back(leaf.layer,leaf.order);return result;
                };
                return key(a,x)<key(b,y);
            }
            return x.layer!=y.layer?x.layer<y.layer:x.order<y.order;
        });
    }
    for (size_t spriteIndex = 0; spriteIndex < mImpl->sprites.size(); ++spriteIndex)
    {
        const Sprite& sprite = animatedOrder ? *sortedSprites[spriteIndex] : mImpl->sprites[spriteIndex];
        if(options&&!options->spritePathPrefix.empty())
        {
            const auto& path=mImpl->nodes[sprite.node].path;const auto& prefix=options->spritePathPrefix;
            if(path!=prefix&&!path.starts_with(prefix+"/"))continue;
        }
        const Variant* variant = pose[sprite.node].spriteChanged ? pose[sprite.node].sprite : sprite.variant;
        const Pose& spritePose = pose[sprite.node];
        if (!(spritePose.enabledChanged ? spritePose.enabled : sprite.enabled) || !active[sprite.node] || !variant) continue;
        Sexy::Color color = sprite.color;
        if (spritePose.alphaChanged)
            color.mAlpha = static_cast<int>(std::clamp(spritePose.alpha*255.0f,0.0f,255.0f)+0.5f);
        if (spritePose.colorRGBChanged[0])
            color.mRed = static_cast<int>(std::clamp(spritePose.colorRGB[0]*255.0f,0.0f,255.0f)+0.5f);
        if (spritePose.colorRGBChanged[1])
            color.mGreen = static_cast<int>(std::clamp(spritePose.colorRGB[1]*255.0f,0.0f,255.0f)+0.5f);
        if (spritePose.colorRGBChanged[2])
            color.mBlue = static_cast<int>(std::clamp(spritePose.colorRGB[2]*255.0f,0.0f,255.0f)+0.5f);
        if (options)
        {
            const auto& tint = options->colorMultiplierRGBA;
            color = Sexy::Color(color.mRed*std::clamp(tint[0],0,255)/255,
                color.mGreen*std::clamp(tint[1],0,255)/255,
                color.mBlue*std::clamp(tint[2],0,255)/255,
                color.mAlpha*std::clamp(tint[3],0,255)/255);
        }
        // One source material pass belongs to one sprite. Keep every additive
        // contribution before the next sprite so overlapping children retain
        // SRCOVER order. Fractional gain scales alpha, not clamped RGB.
        Sexy::Graphics spriteGraphics(*graphics);
        if(options&&options->nativeMaskClipBottom&&sprite.maskInteraction==2)
        {
            const int bottom=int(std::floor(*options->nativeMaskClipBottom+graphics->mTransY));
            spriteGraphics.mClipRect.mHeight=(std::max)(0,(std::min)(spriteGraphics.mClipRect.mY+spriteGraphics.mClipRect.mHeight,bottom)-spriteGraphics.mClipRect.mY);
        }
        const auto drawPasses=[&](auto&& draw)
        {
            draw(spriteGraphics,color);
            float remaining=options?options->brightnessGain-1:0;
            spriteGraphics.mDrawMode=Sexy::Graphics::DRAWMODE_ADDITIVE;
            while(remaining>0)
            {
                auto extra=color;const float fraction=(std::min)(remaining,1.f);
                extra.mAlpha=int(std::floor(extra.mAlpha*fraction+.5f));
                if(extra.mAlpha)draw(spriteGraphics,extra);
                remaining-=fraction;
            }
        };
        Sexy::Image* image = variant->pixels;
        if (!image) continue;
        const Matrix& transform = world[sprite.node];
        const float flipX = (spritePose.flipXChanged ? spritePose.flipX : sprite.flipX) ? -1.0f : 1.0f;
        const float flipY = (spritePose.flipYChanged ? spritePose.flipY : sprite.flipY) ? -1.0f : 1.0f;
        if (!variant->indices.empty())
        {
            std::vector<FusionDeform269Vertex> deformed;
            const auto skin=mImpl->skins.find(sprite.node);
            if(skin!=mImpl->skins.end()&&skin->second.enabled)
            {
                const auto validation=FusionDeform269ValidateSkin(true,variant->bindPoses.size(),skin->second.root>=0,skin->second.bones,variant->skinWeights);
                if(validation==FusionSkin269Validation::Ready)
                {
                    const auto matrix=[](const Matrix& value)
                    {
                        FusionDeform269Matrix result;
                        for(int row=0;row<3;++row){for(int col=0;col<3;++col)result.value[row*4+col]=value.m[row][col];result.value[row*4+3]=value.position[row];}
                        return result;
                    };
                    FusionDeform269Matrix inverse;std::vector<FusionDeform269Matrix> bones,palette;
                    for(int node:skin->second.bones)bones.push_back(matrix(world[node]));
                    std::vector<FusionDeform269Vertex> vertices;vertices.reserve(variant->vertices.size());
                    for(const auto& v:variant->vertices)vertices.push_back({v.x,v.y,v.z,v.u,v.v});
                    if(!FusionDeform269Inverse(matrix(transform),inverse)||!FusionDeform269Palette(inverse,bones,variant->bindPoses,palette)||!FusionDeform269Vertices(vertices,variant->skinWeights,palette,deformed))continue;
                }
                // Exact Unity Validate fallback retains original sprite geometry;
                // unsupported/malformed streams were rejected at resource load.
            }
            std::vector<std::array<Sexy::TriVertex, 3>> triangles(variant->indices.size()/3);
            std::vector<FusionDeform269Vertex> projected(variant->vertices.size());
            for (size_t i = 0; i < variant->vertices.size(); ++i)
            {
                const auto& vertex = variant->vertices[i];
                const float vx=float(deformed.empty()?vertex.x:deformed[i].x)*flipX,vy=float(deformed.empty()?vertex.y:deformed[i].y)*flipY;
                const float vz=float(deformed.empty()?vertex.z:deformed[i].z);
                const float wx=transform.m[0][0]*vx+transform.m[0][1]*vy+transform.m[0][2]*vz+transform.position[0];
                const float wy=transform.m[1][0]*vx+transform.m[1][1]*vy+transform.m[1][2]*vz+transform.position[1];
                const bool affine=options&&options->useBoardBasis;
                const float px=x+(affine?options->boardBasis[0]*wx+options->boardBasis[2]*wy:scale*100.0f*wx)+graphics->mTransX;
                const float py=y+(affine?options->boardBasis[1]*wx+options->boardBasis[3]*wy:-scale*100.0f*wy)+graphics->mTransY;
                projected[i]={px,py,0,vertex.u,vertex.v};
            }
            if(sprite.maskInteraction)
            {
                if(!mImpl->maskScratch)mImpl->maskScratch=std::make_unique<Sexy::MemoryImage>();
                drawPasses([&](Sexy::Graphics& g,const Sexy::Color& c){FusionMask269DrawTriangles(g,*image,projected,variant->indices,c,sprite.maskInteraction,spriteSort(sprite),masks,*mImpl->maskScratch);});
                continue;
            }
            for(size_t j=0;j<variant->indices.size();++j)
            {const auto& v=projected[variant->indices[j]];triangles[j/3][j%3]=Sexy::TriVertex(float(v.x),float(v.y),float(v.u),float(v.v));}
            const auto raw = reinterpret_cast<const Sexy::TriVertex (*)[3]>(triangles.data());
            drawPasses([&](Sexy::Graphics& g,const Sexy::Color& c){
                if(auto target=dynamic_cast<Sexy::SDL3Image*>(g.mDestImage))
                    target->BltTrianglesTex(image,raw,static_cast<int>(triangles.size()),g.mClipRect,c,g.mDrawMode,0,0,g.mLinearBlend,g.mPixelArtBlend);
                else g.mDestImage->BltTrianglesTex(image,raw,static_cast<int>(triangles.size()),g.mClipRect,c,g.mDrawMode,0,0,g.mLinearBlend);
            });
            continue;
        }
        const float centerX = flipX*(variant->rect.mWidth*0.5f-variant->logicalWidth*variant->pivotX+variant->offsetX)/variant->pixelsPerUnit;
        const float centerY = flipY*(variant->rect.mHeight*0.5f-variant->logicalHeight*variant->pivotY+variant->offsetY)/variant->pixelsPerUnit;
        const float factor = scale*100.0f/variant->pixelsPerUnit;
        Sexy::SexyMatrix3 matrix; matrix.LoadIdentity();
        matrix.m00 = factor*transform.m[0][0]*flipX; matrix.m10 = -factor*transform.m[1][0]*flipX;
        matrix.m01 = -factor*transform.m[0][1]*flipY; matrix.m11 = factor*transform.m[1][1]*flipY;
        matrix.m02 = x+scale*100.0f*(transform.m[0][0]*centerX+transform.m[0][1]*centerY+transform.position[0])+graphics->mTransX;
        matrix.m12 = y-scale*100.0f*(transform.m[1][0]*centerX+transform.m[1][1]*centerY+transform.position[1])+graphics->mTransY;
        if(options&&options->useBoardBasis)
        {
            const auto& a=options->boardBasis;
            const float u=1.f/variant->pixelsPerUnit;
            matrix.m00=u*(a[0]*transform.m[0][0]+a[2]*transform.m[1][0])*flipX;
            matrix.m10=u*(a[1]*transform.m[0][0]+a[3]*transform.m[1][0])*flipX;
            matrix.m01=-u*(a[0]*transform.m[0][1]+a[2]*transform.m[1][1])*flipY;
            matrix.m11=-u*(a[1]*transform.m[0][1]+a[3]*transform.m[1][1])*flipY;
            const float wx=transform.m[0][0]*centerX+transform.m[0][1]*centerY+transform.position[0];
            const float wy=transform.m[1][0]*centerX+transform.m[1][1]*centerY+transform.position[1];
            matrix.m02=x+a[0]*wx+a[2]*wy+graphics->mTransX;
            matrix.m12=y+a[1]*wx+a[3]*wy+graphics->mTransY;
        }
        drawPasses([&](Sexy::Graphics& g,const Sexy::Color& c){TodBltMatrix(&g,image,matrix,g.mClipRect,c,g.mDrawMode,variant->rect);});
    }
}
}
