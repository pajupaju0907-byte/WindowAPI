#include "Block.h"
#include "../collision/Collider.h"

Block::Block() = default;
Block::~Block() = default;

void Block::ApplyForce(Vector2 force)
{
    // TODO: 질량을 고려한 힘 적용(가속도 계산) 직접 구현
    (void)force;
}

void Block::Integrate(float deltaTime)
{
    // TODO: 속도/각속도를 위치/각도에 누적하는 물리 적분 직접 구현
    (void)deltaTime;
}

void Block::WakeUp()
{
    // TODO: Sleeping -> Awake 전환 및 관련 상태 갱신 직접 구현
    m_physicsState = PhysicsState::Awake;
}
