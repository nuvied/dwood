#include "ResourcesLoader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <utility>

namespace
{
struct FontEntry
{
    Font font{};
    std::string error;
};

std::map<std::string, FontEntry> fonts;
const std::filesystem::path fontDirectory = std::filesystem::path("resource") / "fonts";
}

bool ResourcesLoader::IsFontAssetName(const std::string& name)
{
    if (name.empty() || name.size() > 255 ||
        name.find_first_of("/\\:") != std::string::npos ||
        name.find('\0') != std::string::npos)
        return false;
    const std::filesystem::path path(name);
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return path.filename() == path && (extension == ".ttf" || extension == ".otf");
}

std::vector<std::string> ResourcesLoader::AvailableFonts()
{
    std::vector<std::string> result;
    if (!std::filesystem::exists(fontDirectory))
        return result;
    for (const auto& entry : std::filesystem::directory_iterator(fontDirectory)) {
        const auto name = entry.path().filename().string();
        if (entry.is_regular_file() && IsFontAssetName(name))
            result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    return result;
}

const Font* ResourcesLoader::GetCustomFont(const std::string& name, std::string* error)
{
    auto it = fonts.find(name);
    if (it == fonts.end()) {
        FontEntry entry;
        if (!IsFontAssetName(name)) {
            entry.error = "Font must be a .ttf or .otf filename in resource/fonts.";
        } else if (!IsWindowReady()) {
            entry.error = "Cannot load a font before the graphics window is initialized.";
        } else {
            const auto path = fontDirectory / name;
            std::error_code ec;
            const bool exists = std::filesystem::is_regular_file(path, ec);
            if (ec) {
                entry.error = "Cannot access font " + path.string() + ": " + ec.message();
            } else if (!exists) {
                entry.error = "Font file not found: " + path.string();
            } else {
                entry.font = LoadFontEx(path.string().c_str(), 128, nullptr, 0);
                if (!IsFontValid(entry.font) || entry.font.texture.id == 0 ||
                    entry.font.texture.id == GetFontDefault().texture.id) {
                    entry.font = {};
                    entry.error = "Unable to load font: " + path.string();
                } else {
                    SetTextureFilter(entry.font.texture, TEXTURE_FILTER_BILINEAR);
                }
            }
        }
        if (!entry.error.empty())
            TraceLog(LOG_WARNING, "%s", entry.error.c_str());
        it = fonts.emplace(name, std::move(entry)).first;
    }
    if (error)
        *error = it->second.error;
    return it->second.error.empty() ? &it->second.font : nullptr;
}

void ResourcesLoader::ClearFontErrors()
{
    for (auto it = fonts.begin(); it != fonts.end();) {
        if (!it->second.error.empty())
            it = fonts.erase(it);
        else
            ++it;
    }
}

void ResourcesLoader::UnloadFonts()
{
    for (auto& entry : fonts)
        if (entry.second.error.empty())
            UnloadFont(entry.second.font);
    fonts.clear();
}
