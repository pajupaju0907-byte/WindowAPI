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

void SoundManager::Shutdown()
{
    StopBgm();
}
