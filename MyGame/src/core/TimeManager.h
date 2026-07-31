#pragma once

// 프레임 간 델타타임을 관리하는 싱글톤
class TimeManager
{
public:
    static TimeManager& GetInstance();

    // 이전 프레임과의 시간 차이를 갱신
    void Update();

    float GetDeltaTime() const;

    // 디버그 표시용 순간 FPS (1 / 델타타임)
    float GetFPS() const;

private:
    TimeManager() = default;
    ~TimeManager() = default;
    TimeManager(const TimeManager&) = delete;
    TimeManager& operator=(const TimeManager&) = delete;

    float m_deltaTime = 0.0f;

    // QueryPerformanceCounter 기반 델타타임 계산용 상태
    LARGE_INTEGER m_frequency{};
    LARGE_INTEGER m_lastCounter{};
    bool m_initialized = false;
};
