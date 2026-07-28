#include "TimeManager.h"

TimeManager& TimeManager::GetInstance()
{
    static TimeManager instance;
    return instance;
}

void TimeManager::Update()
{
    // TODO: QueryPerformanceCounter 등을 이용한 실제 델타타임 계산 직접 구현
}

float TimeManager::GetDeltaTime() const
{
    return m_deltaTime;
}
