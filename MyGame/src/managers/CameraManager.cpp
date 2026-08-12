#include "pch.h"

#include <cmath>

#include "CameraManager.h"
#include "../objects/Block.h"
#include "../util/Constants.h"
#include "../util/MathUtil.h"

CameraManager& CameraManager::GetInstance()
{
    static CameraManager instance;
    return instance;
}

// 쌓인(착지한) 블럭들 중 가장 높은(=world Y가 가장 작은) 지점을 찾아 카메라가 따라갈 목표 y값을 정한다
void CameraManager::UpdateTarget(const std::vector<Block*>& blocks)
{
    bool hasStackedBlock = false;
    float highestBlockY = 0.0f;

    for (Block* block : blocks)
    {
        // Airborne = 플레이어가 조작해서 떨어뜨리는 중인 블럭. 아직 "쌓인" 게 아니므로 기준에서 제외한다
        if (block->GetPhysicsState() == PhysicsState::Airborne)
        {
            continue;
        }

        float blockY = block->GetWorldBounds().min.y;
        if (!hasStackedBlock || blockY < highestBlockY)
        {
            highestBlockY = blockY;
            hasStackedBlock = true;
        }
    }

    if (!hasStackedBlock)
    {
        // 쌓인 블럭이 아직 하나도 없으면 카메라를 기본 위치(0)에 그대로 둔다
        m_targetY = 0.0f;
        return;
    }

    // 화면 상단에 CAMERA_TOP_MARGIN만큼 여유를 남겨두고, 탑이 그 여유를 넘어섰을 때만 카메라를 올린다.
    // desiredTargetY가 0보다 작다는 건 "가장 높은 블럭이 여유 구간 위로 넘어갔다"는 뜻이다
    float desiredTargetY = highestBlockY - Constants::CAMERA_TOP_MARGIN;
    m_targetY = (desiredTargetY < 0.0f) ? desiredTargetY : 0.0f;
}

// position을 targetY로 매 프레임 조금씩 보간해서 "점점 따라 올라가는" 느낌을 만든다
void CameraManager::Update(float deltaTime)
{
    // deltaTime을 지수 감쇠 형태로 반영해야 프레임레이트가 달라져도(30fps든 144fps든) 같은 체감 속도로 따라간다.
    // 단순히 매 프레임 고정 비율로 lerp하면 프레임이 자주 돌수록 더 빨리 따라붙는 문제가 생긴다
    float lerpFactor = 1.0f - std::exp(-Constants::CAMERA_FOLLOW_SPEED * deltaTime);
    m_position.y = MathUtil::Lerp(m_position.y, m_targetY, lerpFactor);
}

Vector2 CameraManager::WorldToScreen(Vector2 worldPos) const
{
    return worldPos - m_position;
}

Vector2 CameraManager::GetPosition() const
{
    return m_position;
}

void CameraManager::ResetCamera()
{
    m_position.y = 0.0f;
    m_targetY = 0.0f;
}
