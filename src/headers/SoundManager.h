#pragma once

#include "raylib.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

enum class ManagedSoundType { BGM, SFX };

struct ManagedSound
{
    int id = 0;
    std::string name;
    std::string asset;
    ManagedSoundType type = ManagedSoundType::SFX;
    float volume = 1.0f;
    bool loop = false;
};

class SoundManager
{
public:
    SoundManager() = default;
    ~SoundManager();
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

    void Load(const std::filesystem::path& projectDirectory);
    void Save() const;
    std::vector<ManagedSound>& Sounds() { return sounds; }
    const std::vector<ManagedSound>& Sounds() const { return sounds; }
    ManagedSound* Find(int id);
    const ManagedSound* Find(int id) const;
    int AddSound(ManagedSoundType type);
    bool RemoveSound(int id);
    bool HasSound(int id) const;

    void On(int id);
    void Off(int id);
    void FadeIn(int id, float duration);
    void FadeOut(int id, float duration);
    void StopAll();
    bool IsFading(int id) const;
    void Update(float dt);
    void Unload();

private:
    struct Fade
    {
        float start = 0;
        float target = 0;
        float elapsed = 0;
        float duration = 0;
        bool stopAtEnd = false;
    };

    struct Voice
    {
        Sound sound{};
        bool loaded = false;
        bool active = false;
        float volume = 0;
        Fade fade;
    };

    struct Playback
    {
        Music music{};
        bool musicLoaded = false;
        float musicVolume = 0;
        Fade musicFade;
        Sound sound{};
        bool soundLoaded = false;
        std::vector<Voice> voices;
    };

    ManagedSound& RequireSound(int id);
    const ManagedSound& RequireSound(int id) const;
    Playback& State(int id);
    void EnsureMusic(const ManagedSound& sound, Playback& playback);
    void EnsureSound(const ManagedSound& sound, Playback& playback);
    void StartVoice(const ManagedSound& sound, Playback& playback,
                    bool fadeIn, float duration);
    void BeginMusicFade(Playback& playback, float target, float duration,
                        bool stopAtEnd);
    void StopBGMExcept(int id);

    std::filesystem::path filePath;
    std::vector<ManagedSound> sounds;
    std::map<int, Playback> playback;
};
