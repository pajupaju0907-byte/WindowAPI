#pragma once

#include <chrono>

// 특정 시점 이후 경과 시간을 재는 범용 타이머 (Sleep 판정, 연출 타이밍 등에 사용)
class Timer
{
public:
    void Start();
    void Reset();
    float GetElapsedSeconds() const;

private:
    std::chrono::steady_clock::time_point m_startTime;
};
