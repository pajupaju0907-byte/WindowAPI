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
    // TODO: activeBlocks 순회하며 ApplyGravity + Integrate 호출 직접 구현
    (void)deltaTime;
}

void PhysicsManager::ApplyGravity(Block* block)
{
    // TODO: 중력 가속도를 블럭에 힘으로 적용하는 로직 직접 구현
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
