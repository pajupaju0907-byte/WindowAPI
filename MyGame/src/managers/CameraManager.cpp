#include "pch.h"

#include "CameraManager.h"

CameraManager& CameraManager::GetInstance()
{
    static CameraManager instance;
    return instance;
}

void CameraManager::UpdateTarget(const std::vector<Block*>& blocks)
{
    // TODO: 블럭들의 최고 높이를 기준으로 targetY 계산하는 로직 직접 구현
    (void)blocks;
}

void CameraManager::Update(float deltaTime)
{
    // TODO: position을 targetY로 부드럽게 이동시키는 로직 직접 구현 (MathUtil::Lerp 활용 가능)
    (void)deltaTime;
}

Vector2 CameraManager::WorldToScreen(Vector2 worldPos) const
{

  
    return worldPos - m_position;
}
