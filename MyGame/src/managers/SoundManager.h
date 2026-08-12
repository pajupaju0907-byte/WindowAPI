#pragma once

#include <string>

// 배경음악(BGM) 재생을 담당하는 싱글톤. Windows MCI(winmm)로 mp3를 반복 재생한다.
class SoundManager
{
public:
    static SoundManager& GetInstance();

    // path의 mp3를 배경음악으로 반복 재생한다. 이미 다른 BGM이 재생 중이면 먼저 정지하고 바꿔 튼다.
    void PlayBgm(const std::string& path);

    // 현재 재생 중인 BGM을 정지한다.
    void StopBgm();

    // 0.0(무음)~1.0(최대) 사이로 볼륨을 설정한다. 재생 중이면 즉시 반영되고, 이후 PlayBgm으로 곡이
    // 바뀌어도 이 값이 유지된다.
    void SetVolume(float volume);
    float GetVolume() const;

    // 프로그램 종료 시 MCI 장치를 정리한다.
    void Shutdown();

private:
    SoundManager() = default;
    ~SoundManager() = default;
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

    // 열려있는 MCI 장치에 현재 m_volume을 적용한다(setaudio 명령)
    void ApplyVolume();

    bool m_isPlaying = false;
    float m_volume = 1.0f;
};
