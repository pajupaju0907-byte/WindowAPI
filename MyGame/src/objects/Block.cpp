#include "pch.h"

#include "Block.h"
#include "../collision/Collider.h"

Block::Block() = default;
Block::~Block() = default;

void Block::MoveHorizontal(int subCellDelta)
{
    // TODO: m_gridX를 subCellDelta(서브셀 단위)만큼 이동시키되,
    // 그리드 경계 및 GridManager 점유 상태를 확인해 이동 가능 여부를 판단하는 로직 직접 구현
    (void)subCellDelta;
}

void Block::StepDown()
{
    // TODO: m_gridY를 Constants::FALL_STEP_SUBCELLS만큼 내리되,
    // 바닥/다른 블럭에 막히면 낙하 대신 착지(Land) 흐름으로 이어지도록 BlockManager와 협력해 직접 구현
}

void Block::ApplyForce(Vector2 force)
{
    // TODO: 질량을 고려한 힘 적용(가속도 계산) 직접 구현. Awake 상태에서만 의미 있음
    (void)force;
}

void Block::Integrate(float deltaTime)
{
    // TODO: 속도/각속도를 위치/각도에 누적하는 물리 적분 직접 구현.
    // Awake 상태의 블럭에만 호출되어야 함 (Airborne 낙하는 StepDown으로 처리)
    (void)deltaTime;
}

void Block::WakeUp()
{
    // TODO: Sleeping -> Awake 전환 및 관련 상태 갱신 직접 구현
    m_physicsState = PhysicsState::Awake;
}

void Block::Land()
{
    // TODO: 그리드 좌표(m_gridX, m_gridY)를 서브셀 크기(Constants::SUBCELL_SIZE) 기준으로
    // 월드 좌표(m_position)에 대입하고, m_physicsState를 Awake로 전환하는 로직 직접 구현
}
