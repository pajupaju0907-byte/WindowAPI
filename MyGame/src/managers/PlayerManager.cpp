#include "pch.h"

#include "PlayerManager.h"
#include <fstream>

namespace
{
    const char* PLAYER_SAVE_FILE_PATH = "nickname.txt";
    const char* PLAYER_BEST_SCORE_FILE_PATH = "bestscore.txt";
}

PlayerManager::PlayerManager()
{
    LoadFromFile();
    LoadBestScore();
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
    m_hasNickname = !m_nickname.empty();
}
bool PlayerManager::HasNickname() const
{
    return m_hasNickname;
}

const std::string& PlayerManager::GetNickname() const
{
    return m_nickname;
}
void PlayerManager::SetNickname(const std::string& nickname)
{
    m_nickname = nickname;
    m_hasNickname = true;
    SaveToFile();
}

void PlayerManager::SaveToFile() const
{
    std::ofstream file(PLAYER_SAVE_FILE_PATH);
    file << m_nickname;
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