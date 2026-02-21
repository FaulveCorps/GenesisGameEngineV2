#include "engine/IAudio.h"
#include "engine/SubsystemRegistry.h"

#include <vector>
#include <mutex>
#include <iostream>
#include <algorithm>

#ifdef HAVE_MINIAUDIO
#include "miniaudio.h"
#endif

namespace Genesis::Engine {

class MiniaudioAudio : public IAudio {
public:
    MiniaudioAudio() {}
    bool Init() override {
#ifdef HAVE_MINIAUDIO
        ma_result r = ma_engine_init(NULL, &m_engine);
        if (r != MA_SUCCESS) {
            std::cerr << "MiniaudioAudio: failed to init ma_engine (" << r << ")" << std::endl;
            return false;
        }
        ma_engine_set_volume(&m_engine, m_masterVolume);
        m_initialized = true;
        return true;
#else
        std::cerr << "MiniaudioAudio: miniaudio not available at compile time" << std::endl;
        return false;
#endif
    }

    void Shutdown() override {
#ifdef HAVE_MINIAUDIO
        if (!m_initialized) return;
        std::lock_guard<std::mutex> lock(m_lock);
        // ma_engine_uninit stops and frees resources
        ma_engine_uninit(&m_engine);
        m_active.clear();
        m_initialized = false;
#endif
    }

    void Update(double /*dt*/) override {
#ifdef HAVE_MINIAUDIO
        if (!m_initialized) return;
        std::lock_guard<std::mutex> lock(m_lock);
        for (auto it = m_active.begin(); it != m_active.end();) {
            ma_sound* s = *it;
            if (!s) { it = m_active.erase(it); continue; }
            if (ma_sound_at_end(s) || ma_sound_is_playing(s) == MA_FALSE) {
                ma_sound_uninit(s);
                ma_free(s, NULL);
                it = m_active.erase(it);
            } else {
                ++it;
            }
        }
#endif
    }

    std::string Name() const override { return "miniaudio"; }

    bool PlayOneShot(const std::string& assetPath, float volume = 1.0f) override {
        AudioPlayParams params;
        params.volume = volume;
        return PlayOneShot(assetPath, params);
    }

    bool PlayOneShot(const std::string& assetPath, const AudioPlayParams& params) override {
#ifdef HAVE_MINIAUDIO
        if (!m_initialized) return false;
        if (assetPath.empty()) return false;
        ma_sound* sound = (ma_sound*)ma_malloc(sizeof(ma_sound), NULL);
        if (!sound) {
            std::cerr << "MiniaudioAudio: allocation failed" << std::endl;
            return false;
        }
        ma_result r = ma_sound_init_from_file(&m_engine, assetPath.c_str(), 0, NULL, NULL, sound);
        if (r != MA_SUCCESS) {
            std::cerr << "MiniaudioAudio: failed to init sound '" << assetPath << "' (" << r << ")" << std::endl;
            ma_free(sound, NULL);
            return false;
        }
        const float safeVolume = std::max(0.0f, params.volume);
        const float safePitch = std::max(0.01f, params.pitch);
        const float minDist = std::max(0.0f, params.minDistance);
        const float maxDist = std::max(minDist, params.maxDistance);

        ma_sound_set_volume(sound, safeVolume);
        ma_sound_set_pitch(sound, safePitch);
        ma_sound_set_looping(sound, params.loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_spatialization_enabled(sound, params.spatial ? MA_TRUE : MA_FALSE);
        if (params.spatial) {
            ma_sound_set_positioning(sound, ma_positioning_absolute);
            ma_sound_set_position(sound, params.position[0], params.position[1], params.position[2]);
            ma_sound_set_min_distance(sound, minDist);
            ma_sound_set_max_distance(sound, maxDist);
        }
        r = ma_sound_start(sound);
        if (r != MA_SUCCESS) {
            std::cerr << "MiniaudioAudio: failed to start sound '" << assetPath << "' (" << r << ")" << std::endl;
            ma_sound_uninit(sound);
            ma_free(sound, NULL);
            return false;
        }
        std::lock_guard<std::mutex> lock(m_lock);
        m_active.push_back(sound);
        return true;
#else
        (void)assetPath; (void)params;
        return false;
#endif
    }

    void StopAll() override {
#ifdef HAVE_MINIAUDIO
        if (!m_initialized) return;
        std::lock_guard<std::mutex> lock(m_lock);
        for (auto s : m_active) {
            if (s) {
                ma_sound_stop(s);
                ma_sound_uninit(s);
                ma_free(s, NULL);
            }
        }
        m_active.clear();
#endif
    }

    void SetListener(const AudioListener& listener) override {
#ifdef HAVE_MINIAUDIO
        if (!m_initialized) return;
        std::lock_guard<std::mutex> lock(m_lock);
        ma_engine_listener_set_position(&m_engine, 0, listener.position[0], listener.position[1], listener.position[2]);
        ma_engine_listener_set_direction(&m_engine, 0, listener.forward[0], listener.forward[1], listener.forward[2]);
        ma_engine_listener_set_world_up(&m_engine, 0, listener.up[0], listener.up[1], listener.up[2]);
#else
        (void)listener;
#endif
    }

    void SetMasterVolume(float volume) override {
        IAudio::SetMasterVolume(volume);
#ifdef HAVE_MINIAUDIO
        if (!m_initialized) return;
        ma_engine_set_volume(&m_engine, m_masterVolume);
#endif
    }

    float GetMasterVolume() const override {
        return IAudio::GetMasterVolume();
    }

private:
#ifdef HAVE_MINIAUDIO
    ma_engine m_engine;
    std::vector<ma_sound*> m_active;
    std::mutex m_lock;
    bool m_initialized = false;
#endif
};

static bool register_miniaudio = []() {
#ifdef HAVE_MINIAUDIO
    SubsystemRegistry::Instance().RegisterFactory("Audio", "miniaudio", []() {
        return std::make_unique<MiniaudioAudio>();
    });
#endif
    return true;
}();

void RegisterMiniaudioFactory() {
#ifdef HAVE_MINIAUDIO
    SubsystemRegistry::Instance().RegisterFactory("Audio", "miniaudio", []() {
        return std::make_unique<MiniaudioAudio>();
    });
#endif
}

} // namespace Genesis::Engine
