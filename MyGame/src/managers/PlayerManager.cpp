#include "pch.h"

#include "PlayerManager.h"
#include <fstream>
#include <cstdio>
#include <objbase.h>
#pragma comment(lib, "ole32.lib")

namespace
{
    const char* PLAYER_SAVE_FILE_PATH = "nickname.txt";
    const char* PLAYER_BEST_SCORE_FILE_PATH = "bestscore.txt";
    const char* PLAYER_DEVICE_ID_FILE_PATH = "deviceid.txt";

    // 이 컴퓨터를 식별할 고유 ID를 새로 하나 만든다. GUID를 그대로(하이픈/중괄호 포함) 쓸 이유가
    // 없어서, 온라인 랭킹 DB 경로에 바로 넣기 좋게 16진수 32자리만 이어붙인 문자열로 뽑는다.
    std::string GenerateDeviceId()
    {
        GUID guid = {};
        CoCreateGuid(&guid);

        char buffer[64];
        std::snprintf(buffer, sizeof(buffer),
            "%08lX%04hX%04hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX%02hX",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

        return std::string(buffer);
    }
}

PlayerManager::PlayerManager()
{
    LoadFromFile();
    LoadBestScore();
    LoadOrCreateDeviceId();
}

PlayerManager& PlayerManager::GetInstance()
{
    static PlayerManager instance;
    return instance;

}

void PlayerManager::LoadFromFile()
{
    std::ifstream file(PLAYER_SAVE_FILE_PATH);
    if (!file.is_open())
    {
        return;
    }

    std::getline(file, m_nickname);
}

const std::string& PlayerManager::GetNickname() const
{
    return m_nickname;
}
void PlayerManager::SetNickname(const std::string& nickname)
{
    m_nickname = nickname;
    SaveToFile();
}

void PlayerManager::SaveToFile() const
{
    std::ofstream file(PLAYER_SAVE_FILE_PATH);
    file << m_nickname;
}

const std::string& PlayerManager::GetDeviceId() const
{
    return m_deviceId;
}

void PlayerManager::LoadOrCreateDeviceId()
{
    std::ifstream file(PLAYER_DEVICE_ID_FILE_PATH);
    if (file.is_open())
    {
        std::getline(file, m_deviceId);
    }

    if (!m_deviceId.empty())
    {
        return;
    }

    // 파일이 없거나(최초 실행) 비어 있으면 이번에 새로 만들어서 저장 — 다음 실행부터는 위에서 읽힌다
    m_deviceId = GenerateDeviceId();
    std::ofstream outFile(PLAYER_DEVICE_ID_FILE_PATH);
    outFile << m_deviceId;
}

float PlayerManager::GetBestScore() const
{
    return m_bestScore;
}

bool PlayerManager::TryUpdateBestScore(float newScore)
{
    if (newScore <= m_bestScore)
    {
        return false;
    }

    m_bestScore = newScore;
    SaveBestScore();
    return true;
}

void PlayerManager::LoadBestScore()
{
    std::ifstream file(PLAYER_BEST_SCORE_FILE_PATH);
    if (!file.is_open())
    {
        return;
    }

    file >> m_bestScore;
}

void PlayerManager::SaveBestScore() const
{
    std::ofstream file(PLAYER_BEST_SCORE_FILE_PATH);
    file << m_bestScore;
}