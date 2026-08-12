#include "pch.h"

#include "TimeManager.h"

#include "../util/Constants.h"

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

    // 표시용 FPS는 매 프레임 순간값 대신 일정 구간(FPS_SMOOTHING_INTERVAL_SECONDS) 평균으로 갱신한다.
    // 순간값은 프레임 페이싱의 미세한 지터까지 그대로 드러내서 숫자가 실제 체감보다 심하게 깜빡여 보인다.
    m_fpsAccumulatedTime += m_deltaTime;
    ++m_fpsFrameCount;
    if (m_fpsAccumulatedTime >= Constants::FPS_SMOOTHING_INTERVAL_SECONDS)
    {
        m_smoothedFps = static_cast<float>(m_fpsFrameCount) / m_fpsAccumulatedTime;
        m_fpsAccumulatedTime = 0.0f;
        m_fpsFrameCount = 0;
    }
}

float TimeManager::GetDeltaTime() const
{
    return m_deltaTime;
}

float TimeManager::GetFPS() const
{
    return m_smoothedFps;
}
