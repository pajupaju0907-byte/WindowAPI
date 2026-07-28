#pragma once

// 프레임 간 델타타임을 관리하는 싱글톤
class TimeManager
{
public:
    static TimeManager& GetInstance();

    // 이전 프레임과의 시간 차이를 갱신
    void Update();

    float GetDeltaTime() const;

private:
    TimeManager() = default;
    ~TimeManager() = default;
    TimeManager(const TimeManager&) = delete;
    TimeManager& operator=(const TimeManager&) = delete;

    float m_deltaTime = 0.0f;
};
