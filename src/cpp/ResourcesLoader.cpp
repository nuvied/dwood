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
std::map<std::string, Texture2D> textures;
std::filesystem::path FontDirectory()
{
    return AssetPaths::Resource("fonts");
}
}

std::vector<std::string> ResourcesLoader::AvailableTextures()
{
    std::vector<std::string> result;
    const auto directory = AssetPaths::Resource("");
    if (!std::filesystem::exists(directory))
        return result;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file())
            continue;
        auto extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension == ".png")
            result.push_back(entry.path().filename().string());
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> ResourcesLoader::AvailableSounds()
{
    std::vector<std::string> result;
    const auto resourceDirectory = AssetPaths::Resource("");
    const auto soundDirectory = resourceDirectory / "sounds";
    if (!std::filesystem::exists(soundDirectory))
        return result;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(soundDirectory)) {
        if (!entry.is_regular_file())
            continue;
        auto extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension != ".wav" && extension != ".ogg" && extension != ".mp3" &&
            extension != ".flac" && extension != ".qoa" && extension != ".xm" &&
            extension != ".mod")
            continue;
        result.push_back(entry.path().lexically_relative(resourceDirectory).generic_string());
    }
    std::sort(result.begin(), result.end());
    return result;
}

const Texture2D* ResourcesLoader::GetTexture(const std::string& name, std::string* error)
{
    if (error)
        error->clear();
    const Texture2D* builtins[] = {
        name == "atlas_1.png" ? &boat_tex_page : nullptr,
        name == "ui_page.png" ? &ui_page : nullptr,
        name == "inv_items.png" ? &inv_items : nullptr,
        name == "light_page.png" ? &light_page : nullptr
    };
    for (const auto* texture : builtins)
        if (texture) {
            if (!IsWindowReady() || IsTextureValid(*texture))
                return texture;
            break;
        }

    const auto cached = textures.find(name);
    if (cached != textures.end())
        return &cached->second;

    const std::filesystem::path asset(name);
    std::string extension = asset.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (name.empty() || name.size() > 255 || name.find_first_of("/\\:") != std::string::npos ||
        name.find('\0') != std::string::npos || asset.filename() != asset || extension != ".png") {
        if (error)
            *error = "Texture must be a PNG filename in the resource directory.";
        return nullptr;
    }
    if (!IsWindowReady()) {
        if (error)
            *error = "Cannot load a texture before the graphics window is initialized.";
        return nullptr;
    }

    const auto path = AssetPaths::Resource(asset);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        if (error)
            *error = ec ? "Cannot access texture " + path.string() + ": " + ec.message()
                        : "Texture file not found: " + path.string();
        return nullptr;
    }
    auto texture = LoadTexture(path.string().c_str());
    if (!IsTextureValid(texture)) {
        if (error)
            *error = "Unable to load texture: " + path.string();
        return nullptr;
    }
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    return &textures.emplace(name, texture).first->second;
}

void ResourcesLoader::UnloadTextures()
{
    for (auto& entry : textures)
        if (IsTextureValid(entry.second))
            UnloadTexture(entry.second);
    textures.clear();

    Texture2D* builtins[] = {
        &boat_tex_page, &ui_page, &inv_items, &light_page, &light_blob0
    };
    for (auto* texture : builtins) {
        if (IsTextureValid(*texture))
            UnloadTexture(*texture);
        *texture = {};
    }
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
        const auto fontDirectory = FontDirectory();
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
                        const auto path = FontDirectory() / name;

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
