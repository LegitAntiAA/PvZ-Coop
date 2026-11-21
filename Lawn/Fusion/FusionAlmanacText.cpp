#include "FusionAlmanacText.h"
#include "FusionCatalog.h"
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <vector>

namespace LawnFusion
{
namespace
{
uint32_t TextKey(FusionPortraitGroup group, uint16_t id)
{
    return static_cast<uint32_t>(id) | (group == FusionPortraitGroup::Zombies ? 0x10000U : 0U);
}

bool Sha256Text(const std::string& value)
{
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char ch)
        { return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'); });
}

bool SameText(const FusionAlmanacTranslation& a, const FusionAlmanacTranslation& b)
{
    return a.nameUtf8 == b.nameUtf8 && a.infoUtf8 == b.infoUtf8 &&
        a.loreUtf8 == b.loreUtf8 && a.costUtf8 == b.costUtf8;
}
}

struct FusionAlmanacTextProvider::Impl
{
    std::map<uint32_t, FusionAlmanacTranslation> entries;
};

FusionAlmanacTextProvider::FusionAlmanacTextProvider() = default;
FusionAlmanacTextProvider::~FusionAlmanacTextProvider() = default;

bool FusionAlmanacTextProvider::Load(const std::string& directory, std::string& error)
{
    try
    {
        const auto root = std::filesystem::u8path(directory);
        std::vector<std::filesystem::path> files;
        for (const auto& item : std::filesystem::directory_iterator(root))
        {
            const std::string name = item.path().filename().string();
            if (item.is_regular_file() && name.rfind("almanac.ru.", 0) == 0 && item.path().extension() == ".json")
                files.push_back(item.path());
        }
        std::sort(files.begin(), files.end());
        auto result = std::make_unique<Impl>();
        for (const auto& path : files)
        {
            std::ifstream input(path);
            if (!input) throw std::runtime_error("cannot open almanac translation batch: "+path.string());
            const auto batch = nlohmann::json::parse(input);
            if (batch.at("schema") != 1 || batch.at("language") != "ru" ||
                !Sha256Text(batch.at("sourceCatalogSha256").get<std::string>()))
                throw std::runtime_error("invalid almanac translation batch schema: "+path.filename().string());
            const std::string groupName = batch.at("group").get<std::string>();
            if (groupName != "plants" && groupName != "zombies")
                throw std::runtime_error("almanac translation group must be explicit");
            const auto group = groupName == "plants" ? FusionPortraitGroup::Plants : FusionPortraitGroup::Zombies;
            const auto& rows = batch.at("entries");
            if (!rows.is_array()) throw std::runtime_error("invalid almanac translation entries");
            for (const auto& row : rows)
            {
                if (!row.at("id").is_number_integer()) throw std::runtime_error("invalid almanac translation ID");
                const int64_t id = row.at("id").get<int64_t>();
                if (id < 0 || id >= 65535) throw std::runtime_error("almanac translation ID is out of range");
                const auto* source = group == FusionPortraitGroup::Plants ?
                    FusionFindPlantAlmanac(static_cast<int>(id)) : FusionFindZombieAlmanac(static_cast<int>(id));
                const auto hash = row.at("sourceFieldsSha256").get<std::string>();
                if (!source || !Sha256Text(hash) || !source->sourceFieldsSha256 || hash != source->sourceFieldsSha256)
                    throw std::runtime_error("almanac translation source mismatch: "+groupName+"/"+std::to_string(id));
                FusionAlmanacTranslation text{row.at("name").get<std::string>(), row.at("info").get<std::string>(),
                    row.at("introduce").get<std::string>(), row.at("cost").get<std::string>()};
                if (text.nameUtf8.empty() || text.infoUtf8.empty()) throw std::runtime_error("empty almanac translation name/info");
                const uint32_t key = TextKey(group, static_cast<uint16_t>(id));
                const auto found = result->entries.find(key);
                if (found != result->entries.end())
                {
                    if (!SameText(found->second, text)) throw std::runtime_error("conflicting almanac translation: "+groupName+"/"+std::to_string(id));
                }
                else result->entries.emplace(key, std::move(text));
            }
        }
        mImpl = std::move(result);
        error.clear();
        return true;
    }
    catch (const std::exception& exception) { error = exception.what(); return false; }
}

const FusionAlmanacTranslation* FusionAlmanacTextProvider::FindRussian(FusionPortraitGroup group, uint16_t id) const
{
    if (!mImpl || (group != FusionPortraitGroup::Plants && group != FusionPortraitGroup::Zombies)) return nullptr;
    const auto found = mImpl->entries.find(TextKey(group, id));
    return found == mImpl->entries.end() ? nullptr : &found->second;
}
}
