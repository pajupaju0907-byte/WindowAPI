#pragma once

#include <string>

// 플레이어 닉네임을 관리하는 싱글톤. 파일에 저장해서 다음 실행 때도 기억한다.
class PlayerManager
{
public:
    static PlayerManager& GetInstance();

    // 현재 닉네임을 반환한다. 한 번도 정한 적 없으면 빈 문자열.
    const std::string& GetNickname() const;

    // 닉네임을 정하고 파일에 저장한다.
    void SetNickname(const std::string& nickname);

    // 이 컴퓨터를 식별하는 고유 ID(최초 실행 시 자동 생성되어 파일에 저장됨).
    // 온라인 랭킹은 닉네임 대신 이 값을 DB 키로 쓴다 — 서로 다른 컴퓨터가 같은 닉네임을
    // 쓰더라도 랭킹 기록이 서로 덮어써지지 않게 하기 위함.
    const std::string& GetDeviceId() const;

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
    void LoadOrCreateDeviceId();

    std::string m_nickname;

    std::string m_deviceId;

    float m_bestScore = 0.0f;
};