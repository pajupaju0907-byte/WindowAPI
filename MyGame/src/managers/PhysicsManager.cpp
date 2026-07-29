#include "pch.h"

#include "PhysicsManager.h"
#include "../objects/Block.h"

PhysicsManager& PhysicsManager::GetInstance()
{
    static PhysicsManager instance;
    return instance;
}

PhysicsManager::~PhysicsManager() = default;

void PhysicsManager::Update(float deltaTime)
{
    // TODO: m_activeBlocks(Awake 상태 블럭만) 순회하며 ApplyGravity + Integrate 호출 직접 구현.
    // Airborne 블럭은 대상이 아님 (BlockManager::UpdateFalling에서 별도 처리)
    (void)deltaTime;
}

void PhysicsManager::ApplyGravity(Block* block)
{
    // TODO: 바닥 상태(Awake)의 흔들림 표현을 위한 중력 가속도를 힘으로 적용하는 로직 직접 구현
    (void)block;
}

bool PhysicsManager::CheckGlobalStability() const
{
    // TODO: 탑 전체의 흔들림/안정성 판단 알고리즘 직접 설계 및 구현
    return false;
}

void PhysicsManager::TrySleepAll()
{
    // TODO: 안정 상태인 블럭들을 activeBlocks -> sleepingBlocks로 이동시키는 로직 직접 구현
}
