#include "SoundManager.h"

#include "ProjectConfig.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>

namespace
{
bool IsSupportedAsset(const std::string& asset)
{
    if (asset.empty() || asset.find('\0') != std::string::npos)
        return false;
    const std::filesystem::path path(asset);
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
        return false;
    for (const auto& part : path)
        if (part == "." || part == "..")
            return false;
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".wav" || extension == ".ogg" || extension == ".mp3" ||
           extension == ".flac" || extension == ".qoa" || extension == ".xm" ||
           extension == ".mod";
}

ManagedSoundType ParseType(const nlohmann::json& value)
{
    if (!value.is_string())
        throw std::runtime_error("Sound type must be bgm or sfx.");
    const auto type = value.get<std::string>();
    if (type == "bgm")
        return ManagedSoundType::BGM;
    if (type == "sfx")
        return ManagedSoundType::SFX;
    throw std::runtime_error("Sound type must be bgm or sfx.");
}

const char* TypeName(ManagedSoundType type)
{
    return type == ManagedSoundType::BGM ? "bgm" : "sfx";
}
}

SoundManager::~SoundManager()
{
    Unload();
}

void SoundManager::Load(const std::filesystem::path& projectDirectory)
{
    Unload();
    sounds.clear();
    filePath = projectDirectory / ".devilwood" / "sounds.json";
    if (!std::filesystem::exists(filePath))
        return;

    std::ifstream input(filePath, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open sound manager file: " + filePath.string());
    nlohmann::json data;
    try {
        input >> data;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("Invalid sound manager JSON: " + std::string(e.what()));
    }
    if (!data.is_object() || data.value("version", 0) != 1 ||
        !data.contains("sounds") || !data["sounds"].is_array())
        throw std::runtime_error("Sound manager must contain version 1 and a sounds array.");

    std::vector<ManagedSound> loaded;
    std::set<int> ids;
    for (const auto& entry : data["sounds"]) {
        if (!entry.is_object() || !entry.contains("id") ||
            !entry["id"].is_number_integer() || !entry.contains("name") ||
            !entry["name"].is_string() || !entry.contains("asset") ||
            !entry["asset"].is_string() || !entry.contains("type") ||
            !entry.contains("volume") || !entry["volume"].is_number())
            throw std::runtime_error("Sound manager contains an incomplete sound.");
        const auto idValue = entry["id"].get<std::int64_t>();
        if (idValue <= 0 || idValue > std::numeric_limits<int>::max())
            throw std::runtime_error("Sound IDs must be positive integers.");
        const int id = static_cast<int>(idValue);
        if (!ids.insert(id).second)
            throw std::runtime_error("Sound manager contains a duplicate ID.");
        const auto type = ParseType(entry["type"]);
        const double volumeValue = entry["volume"].get<double>();
        if (!std::isfinite(volumeValue) || volumeValue < 0 || volumeValue > 1)
            throw std::runtime_error("Sound volume must be between 0 and 1.");
        if (entry.contains("loop") && !entry["loop"].is_boolean())
            throw std::runtime_error("Sound loop setting must be a boolean.");
        loaded.push_back({
            id,
            entry["name"].get<std::string>(),
            entry["asset"].get<std::string>(),
            type,
            static_cast<float>(volumeValue),
            entry.value("loop", type == ManagedSoundType::BGM)
        });
    }
    sounds = std::move(loaded);
}

void SoundManager::Save() const
{
    if (filePath.empty())
        throw std::runtime_error("Sound manager has no project directory.");
    std::set<int> ids;
    nlohmann::json entries = nlohmann::json::array();
    for (const auto& sound : sounds) {
        if (sound.id <= 0 || !ids.insert(sound.id).second)
            throw std::runtime_error("Sound IDs must be unique positive integers.");
        if (sound.name.empty() || sound.name.size() > 127 ||
            sound.name.find_first_not_of(" \t\r\n") == std::string::npos)
            throw std::runtime_error("Sound name must contain 1-127 non-blank characters.");
        if (!IsSupportedAsset(sound.asset))
            throw std::runtime_error("Sound asset must be a supported file inside the resource directory.");
        if (!std::isfinite(sound.volume) || sound.volume < 0 || sound.volume > 1)
            throw std::runtime_error("Sound volume must be between 0 and 1.");
        entries.push_back({
            {"id", sound.id}, {"name", sound.name}, {"asset", sound.asset},
            {"type", TypeName(sound.type)}, {"volume", sound.volume}, {"loop", sound.loop}
        });
    }

    std::filesystem::create_directories(filePath.parent_path());
    std::ofstream output(filePath, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("Cannot write sound manager file: " + filePath.string());
    output << nlohmann::json{{"version", 1}, {"sounds", std::move(entries)}}.dump(2) << '\n';
    output.close();
    if (!output)
        throw std::runtime_error("Cannot finish writing sound manager file: " + filePath.string());
}

ManagedSound* SoundManager::Find(int id)
{
    const auto it = std::find_if(sounds.begin(), sounds.end(),
                                 [id](const ManagedSound& sound) { return sound.id == id; });
    return it == sounds.end() ? nullptr : &*it;
}

const ManagedSound* SoundManager::Find(int id) const
{
    const auto it = std::find_if(sounds.begin(), sounds.end(),
                                 [id](const ManagedSound& sound) { return sound.id == id; });
    return it == sounds.end() ? nullptr : &*it;
}

bool SoundManager::HasSound(int id) const
{
    return Find(id) != nullptr;
}

int SoundManager::AddSound(ManagedSoundType type)
{
    int id = 1;
    for (const auto& sound : sounds) {
        if (sound.id == std::numeric_limits<int>::max())
            throw std::runtime_error("No more sound IDs are available.");
        id = std::max(id, sound.id + 1);
    }
    sounds.push_back({id, type == ManagedSoundType::BGM ? "New BGM" : "New SFX",
                      "", type, 1.0f, type == ManagedSoundType::BGM});
    return id;
}

bool SoundManager::RemoveSound(int id)
{
    auto sound = std::find_if(sounds.begin(), sounds.end(),
                              [id](const ManagedSound& item) { return item.id == id; });
    if (sound == sounds.end())
        return false;
    Off(id);
    auto state = playback.find(id);
    if (state != playback.end()) {
        auto& current = state->second;
        if (current.musicLoaded && IsAudioDeviceReady())
            UnloadMusicStream(current.music);
        for (auto& voice : current.voices)
            if (voice.loaded && IsAudioDeviceReady())
                UnloadSoundAlias(voice.sound);
        if (current.soundLoaded && IsAudioDeviceReady())
            UnloadSound(current.sound);
        playback.erase(state);
    }
    sounds.erase(sound);
    return true;
}

ManagedSound& SoundManager::RequireSound(int id)
{
    auto* sound = Find(id);
    if (!sound)
        throw std::runtime_error("Sound manager entry does not exist: " + std::to_string(id));
    return *sound;
}

const ManagedSound& SoundManager::RequireSound(int id) const
{
    const auto* sound = Find(id);
    if (!sound)
        throw std::runtime_error("Sound manager entry does not exist: " + std::to_string(id));
    return *sound;
}

SoundManager::Playback& SoundManager::State(int id)
{
    return playback[id];
}

void SoundManager::EnsureMusic(const ManagedSound& sound, Playback& current)
{
    if (current.musicLoaded)
        return;
    if (!IsSupportedAsset(sound.asset))
        throw std::runtime_error("Sound asset path is invalid: " + sound.asset);
    if (!IsAudioDeviceReady())
        throw std::runtime_error("Cannot play a managed sound before the audio device is initialized.");
    const auto path = AssetPaths::Resource(sound.asset);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec))
        throw std::runtime_error(ec ? "Cannot access sound asset " + path.string() + ": " + ec.message()
                                    : "Sound asset does not exist: " + path.string());
    current.music = LoadMusicStream(path.string().c_str());
    if (!IsMusicValid(current.music))
        throw std::runtime_error("Cannot load sound asset: " + path.string());
    current.music.looping = sound.type == ManagedSoundType::BGM && sound.loop;
    current.musicVolume = 0;
    current.musicLoaded = true;
}

void SoundManager::EnsureSound(const ManagedSound& sound, Playback& current)
{
    if (current.soundLoaded)
        return;
    if (!IsSupportedAsset(sound.asset))
        throw std::runtime_error("Sound asset path is invalid: " + sound.asset);
    if (!IsAudioDeviceReady())
        throw std::runtime_error("Cannot play a managed sound before the audio device is initialized.");
    const auto path = AssetPaths::Resource(sound.asset);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec))
        throw std::runtime_error(ec ? "Cannot access sound asset " + path.string() + ": " + ec.message()
                                    : "Sound asset does not exist: " + path.string());
    current.sound = LoadSound(path.string().c_str());
    if (!IsSoundValid(current.sound))
        throw std::runtime_error("Cannot load sound asset: " + path.string());
    current.soundLoaded = true;
}

void SoundManager::StopBGMExcept(int id)
{
    for (const auto& sound : sounds) {
        if (sound.type != ManagedSoundType::BGM || sound.id == id)
            continue;
        auto it = playback.find(sound.id);
        if (it != playback.end() && it->second.musicLoaded) {
            StopMusicStream(it->second.music);
            it->second.musicVolume = 0;
            it->second.musicFade = {};
        }
    }
}

void SoundManager::BeginMusicFade(Playback& current, float target, float duration,
                                  bool stopAtEnd)
{
    if (duration <= 0) {
        current.musicVolume = target;
        SetMusicVolume(current.music, target);
        current.musicFade = {};
        if (stopAtEnd)
            StopMusicStream(current.music);
        return;
    }
    current.musicFade = {current.musicVolume, target, 0, duration, stopAtEnd};
}

void SoundManager::StartVoice(const ManagedSound& sound, Playback& current,
                              bool fadeIn, float duration)
{
    EnsureSound(sound, current);
    auto voice = std::find_if(current.voices.begin(), current.voices.end(),
                              [](const Voice& item) { return !item.active; });
    if (voice == current.voices.end()) {
        current.voices.push_back({});
        voice = std::prev(current.voices.end());
    }
    if (!voice->loaded) {
        voice->sound = LoadSoundAlias(current.sound);
        if (!IsSoundValid(voice->sound))
            throw std::runtime_error("Cannot create a playback voice for sound: " + sound.name);
        voice->loaded = true;
    }
    voice->active = true;
    voice->volume = fadeIn && duration > 0 ? 0 : sound.volume;
    SetSoundVolume(voice->sound, voice->volume);
    PlaySound(voice->sound);
    voice->fade = {};
    if (fadeIn && duration > 0)
        voice->fade = {0, sound.volume, 0, duration, false};
}

void SoundManager::On(int id)
{
    const auto& sound = RequireSound(id);
    auto& current = State(id);
    if (sound.type == ManagedSoundType::BGM) {
        if (!IsAudioDeviceReady())
            throw std::runtime_error("Cannot play a managed sound before the audio device is initialized.");
        StopBGMExcept(id);
        EnsureMusic(sound, current);
        current.music.looping = sound.loop;
        current.musicFade = {};
        current.musicVolume = sound.volume;
        SetMusicVolume(current.music, sound.volume);
        if (!IsMusicStreamPlaying(current.music))
            PlayMusicStream(current.music);
    } else {
        StartVoice(sound, current, false, 0);
    }
}

void SoundManager::Off(int id)
{
    const auto& sound = RequireSound(id);
    auto it = playback.find(id);
    if (it == playback.end())
        return;
    auto& current = it->second;
    if (sound.type == ManagedSoundType::BGM) {
        if (current.musicLoaded)
            StopMusicStream(current.music);
        current.musicVolume = 0;
        current.musicFade = {};
    } else {
        for (auto& voice : current.voices) {
            if (voice.active && voice.loaded)
                StopSound(voice.sound);
            voice.active = false;
            voice.fade = {};
        }
    }
}

void SoundManager::FadeIn(int id, float duration)
{
    const auto& sound = RequireSound(id);
    if (!std::isfinite(duration) || duration < 0)
        throw std::runtime_error("Sound fade duration must be non-negative.");
    if (sound.type == ManagedSoundType::SFX) {
        StartVoice(sound, State(id), true, duration);
        return;
    }

    if (!IsAudioDeviceReady())
        throw std::runtime_error("Cannot play a managed sound before the audio device is initialized.");
    StopBGMExcept(id);
    auto& current = State(id);
    EnsureMusic(sound, current);
    current.music.looping = sound.loop;
    if (!IsMusicStreamPlaying(current.music)) {
        current.musicVolume = 0;
        SetMusicVolume(current.music, 0);
        PlayMusicStream(current.music);
    }
    BeginMusicFade(current, sound.volume, duration, false);
}

void SoundManager::FadeOut(int id, float duration)
{
    const auto& sound = RequireSound(id);
    if (!std::isfinite(duration) || duration < 0)
        throw std::runtime_error("Sound fade duration must be non-negative.");
    auto it = playback.find(id);
    if (it == playback.end())
        return;
    auto& current = it->second;
    if (sound.type == ManagedSoundType::BGM) {
        if (current.musicLoaded && IsMusicStreamPlaying(current.music))
            BeginMusicFade(current, 0, duration, true);
    } else {
        for (auto& voice : current.voices) {
            if (!voice.active || !voice.loaded)
                continue;
            if (duration <= 0) {
                StopSound(voice.sound);
                voice.active = false;
                voice.fade = {};
            } else {
                voice.fade = {voice.volume, 0, 0, duration, true};
            }
        }
    }
}

void SoundManager::StopAll()
{
    if (IsAudioDeviceReady()) {
        for (auto& entry : playback) {
            auto& current = entry.second;
            if (current.musicLoaded)
                StopMusicStream(current.music);
            current.musicVolume = 0;
            current.musicFade = {};
            for (auto& voice : current.voices) {
                if (voice.active && voice.loaded)
                    StopSound(voice.sound);
                voice.active = false;
                voice.fade = {};
            }
        }
    }
}

bool SoundManager::IsFading(int id) const
{
    const auto state = playback.find(id);
    if (state == playback.end())
        return false;
    if (state->second.musicFade.duration > 0)
        return true;
    return std::any_of(state->second.voices.begin(), state->second.voices.end(),
                       [](const Voice& voice) { return voice.active && voice.fade.duration > 0; });
}

void SoundManager::Update(float dt)
{
    if (!IsAudioDeviceReady())
        return;
    for (auto& entry : playback) {
        auto& current = entry.second;
        if (current.musicLoaded && IsMusicStreamPlaying(current.music)) {
            UpdateMusicStream(current.music);
            if (current.musicFade.duration > 0) {
                auto& fade = current.musicFade;
                fade.elapsed = std::min(fade.elapsed + dt, fade.duration);
                const float progress = fade.elapsed / fade.duration;
                current.musicVolume = fade.start + (fade.target - fade.start) * progress;
                SetMusicVolume(current.music, current.musicVolume);
                if (progress >= 1) {
                    const bool stop = fade.stopAtEnd;
                    current.musicFade = {};
                    if (stop) {
                        StopMusicStream(current.music);
                        current.musicVolume = 0;
                    }
                }
            }
        }
        if (!current.soundLoaded)
            continue;
        for (auto& voice : current.voices) {
            if (!voice.active)
                continue;
            if (!IsSoundPlaying(voice.sound)) {
                voice.active = false;
                voice.fade = {};
                continue;
            }
            if (voice.fade.duration <= 0)
                continue;
            voice.fade.elapsed = std::min(voice.fade.elapsed + dt, voice.fade.duration);
            const float progress = voice.fade.elapsed / voice.fade.duration;
            voice.volume = voice.fade.start + (voice.fade.target - voice.fade.start) * progress;
            SetSoundVolume(voice.sound, voice.volume);
            if (progress >= 1) {
                const bool stop = voice.fade.stopAtEnd;
                voice.fade = {};
                if (stop) {
                    StopSound(voice.sound);
                    voice.active = false;
                    voice.volume = 0;
                }
            }
        }
    }
}

void SoundManager::Unload()
{
    StopAll();
    const bool audioReady = IsAudioDeviceReady();
    for (auto& entry : playback) {
        auto& current = entry.second;
        if (audioReady && current.musicLoaded)
            UnloadMusicStream(current.music);
        if (audioReady)
            for (auto& voice : current.voices)
                if (voice.loaded)
                    UnloadSoundAlias(voice.sound);
        if (audioReady && current.soundLoaded)
            UnloadSound(current.sound);
    }
    playback.clear();
}
