#include "engine/IAudio.h"
#include "engine/SubsystemRegistry.h"

#include <vector>
#include <mutex>
#include <iostream>

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
        return true;
#else
        std::cerr << "MiniaudioAudio: miniaudio not available at compile time" << std::endl;
        return false;
#endif
    }

    void Shutdown() override {
#ifdef HAVE_MINIAUDIO
        std::lock_guard<std::mutex> lock(m_lock);
        // ma_engine_uninit stops and frees resources
        ma_engine_uninit(&m_engine);
        m_active.clear();
#endif
    }

    void Update(double /*dt*/) override {
#ifdef HAVE_MINIAUDIO
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
#ifdef HAVE_MINIAUDIO
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
        ma_sound_set_volume(sound, volume);
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
        (void)assetPath; (void)volume;
        return false;
#endif
    }

    void StopAll() override {
#ifdef HAVE_MINIAUDIO
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

private:
#ifdef HAVE_MINIAUDIO
    ma_engine m_engine;
    std::vector<ma_sound*> m_active;
    std::mutex m_lock;
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
