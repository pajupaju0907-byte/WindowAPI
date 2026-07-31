#include "pch.h"

#include "TimeManager.h"

TimeManager& TimeManager::GetInstance()
{
    static TimeManager instance;
    return instance;
}

void TimeManager::Update()
{
    LARGE_INTEGER currentCounter;
    QueryPerformanceCounter(&currentCounter);

    if (!m_initialized)
    {
        QueryPerformanceFrequency(&m_frequency);
        m_lastCounter = currentCounter;
        m_initialized = true;
    }

    m_deltaTime = static_cast<float>(currentCounter.QuadPart - m_lastCounter.QuadPart) / static_cast<float>(m_frequency.QuadPart);
    m_lastCounter = currentCounter;
}

float TimeManager::GetDeltaTime() const
{
    return m_deltaTime;
}

float TimeManager::GetFPS() const
{
    return m_deltaTime > 0.0f ? 1.0f / m_deltaTime : 0.0f;
}
