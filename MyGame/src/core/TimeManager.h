#pragma once

// 프레임 간 델타타임을 관리하는 싱글톤
class TimeManager
{
public:
    static TimeManager& GetInstance();

    // 이전 프레임과의 시간 차이를 갱신
    void Update();

    float GetDeltaTime() const;

    // 디버그 표시용 FPS. Constants::FPS_SMOOTHING_INTERVAL_SECONDS 구간 동안 모은 프레임 수의
    // 평균이라, 매 프레임 순간값보다 표시가 안정적이다.
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

    // 표시용 FPS 평균 집계 상태
    float m_fpsAccumulatedTime = 0.0f;
    int m_fpsFrameCount = 0;
    float m_smoothedFps = 0.0f;
};
