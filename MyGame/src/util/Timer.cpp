#include "Timer.h"

void Timer::Start()
{
    m_startTime = std::chrono::steady_clock::now();
}

void Timer::Reset()
{
    m_startTime = std::chrono::steady_clock::now();
}

float Timer::GetElapsedSeconds() const
{
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(now - m_startTime).count();
}
