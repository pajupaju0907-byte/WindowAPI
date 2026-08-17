#pragma once

#include <string>

// 플레이어 닉네임을 관리하는 싱글톤. 파일에 저장해서 다음 실행 때도 기억한다.
class PlayerManager
{
public:
    static PlayerManager& GetInstance();

    // 저장된 닉네임이 있는지 (없으면 최초 실행 등, 입력을 받아야 함)
    bool HasNickname() const;

    // 현재 닉네임을 반환한다. HasNickname()이 false일 때는 빈 문자열.
    const std::string& GetNickname() const;

    // 닉네임을 정하고 파일에 저장한다.
    void SetNickname(const std::string& nickname);

    // 로컬에 저장된 개인 최고 기록(m). 한 번도 기록이 없으면 0.
    float GetBestScore() const;

    // newScore가 기존 최고 기록보다 높으면 갱신하고 파일에 저장한 뒤 true를 반환한다.
    // 그렇지 않으면(신기록이 아니면) 아무것도 바꾸지 않고 false를 반환한다.
    bool TryUpdateBestScore(float newScore);

private:
    PlayerManager();
    ~PlayerManager() = default;
    PlayerManager(const PlayerManager&) = delete;
    PlayerManager& operator=(const PlayerManager&) = delete;

    void LoadFromFile();
    void SaveToFile() const;
    void LoadBestScore();
    void SaveBestScore() const;

    std::string m_nickname;
    bool m_hasNickname = false;

    float m_bestScore = 0.0f;
};