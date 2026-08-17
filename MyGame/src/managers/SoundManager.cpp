#include "pch.h"

#include "SoundManager.h"
#include <algorithm>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

namespace
{
    // MCI 명령에서 파일 경로 대신 쓰는 BGM 장치 별칭
    const wchar_t* BGM_ALIAS = L"bgm";
    // MCI setaudio volume 명령의 범위(0~1000). m_volume(0~1)에 이 값을 곱해서 넘긴다
    constexpr int MCI_VOLUME_MAX = 1000;

    // [효과음 채널] 짧게 겹쳐 재생될 수 있는 효과음(버튼 연타, 블럭 연속 착지 등)을 몇 개까지
    // 동시에 허용할지. 이 수를 넘는 겹침이 생기면 가장 오래(먼저) 쓴 채널을 재활용한다.
    constexpr int SFX_CHANNEL_COUNT = 4;
}

SoundManager& SoundManager::GetInstance()
{
    static SoundManager instance;
    return instance;
}

void SoundManager::PlayBgm(const string& path)
{
    StopBgm();

    wstring widePath(path.begin(), path.end());
    wstring openCommand = L"open \"" + widePath + L"\" type mpegvideo alias " + BGM_ALIAS;
    if (mciSendString(openCommand.c_str(), nullptr, 0, nullptr) != 0)
    {
        OutputDebugStringA(("SoundManager::PlayBgm 열기 실패: " + path + "\n").c_str());
        return;
    }

    m_isPlaying = true;
    ApplyVolume();

    // repeat: 재생이 끝에 도달하면 MCI가 자동으로 처음부터 다시 재생한다 (루프)
    mciSendString((wstring(L"play ") + BGM_ALIAS + L" repeat").c_str(), nullptr, 0, nullptr);
}

void SoundManager::StopBgm()
{
    if (!m_isPlaying)
    {
        return;
    }

    mciSendString((wstring(L"close ") + BGM_ALIAS).c_str(), nullptr, 0, nullptr);
    m_isPlaying = false;
}

void SoundManager::SetVolume(float volume)
{
    m_volume = std::clamp(volume, 0.0f, 1.0f);
    ApplyVolume();
}

float SoundManager::GetVolume() const
{
    return m_volume;
}

void SoundManager::ApplyVolume()
{
    if (!m_isPlaying)
    {
        return;
    }

    int mciVolume = static_cast<int>(m_volume * MCI_VOLUME_MAX);
    wstring command = wstring(L"setaudio ") + BGM_ALIAS + L" volume to " + std::to_wstring(mciVolume);
    mciSendString(command.c_str(), nullptr, 0, nullptr);
}

void SoundManager::PlaySfx(const string& path)
{
    wstring alias = L"sfx" + std::to_wstring(m_nextSfxChannel);
    m_nextSfxChannel = (m_nextSfxChannel + 1) % SFX_CHANNEL_COUNT;

    // 이 채널을 이전에 쓰고 있었다면 먼저 정리한다. 안 열려 있으면 이 호출은 실패하지만
    // 반환값을 쓰지 않으니 그냥 넘어가도 된다
    mciSendString((wstring(L"close ") + alias).c_str(), nullptr, 0, nullptr);

    wstring widePath(path.begin(), path.end());
    wstring openCommand = L"open \"" + widePath + L"\" type mpegvideo alias " + alias;
    if (mciSendString(openCommand.c_str(), nullptr, 0, nullptr) != 0)
    {
        OutputDebugStringA(("SoundManager::PlaySfx 열기 실패: " + path + "\n").c_str());
        return;
    }

    int mciVolume = static_cast<int>(m_volume * MCI_VOLUME_MAX);
    wstring volumeCommand = wstring(L"setaudio ") + alias + L" volume to " + std::to_wstring(mciVolume);
    mciSendString(volumeCommand.c_str(), nullptr, 0, nullptr);

    // repeat 없이 한 번만 재생 (반복 재생하는 PlayBgm과의 차이점)
    mciSendString((wstring(L"play ") + alias).c_str(), nullptr, 0, nullptr);
}

void SoundManager::Shutdown()
{
    StopBgm();
}
