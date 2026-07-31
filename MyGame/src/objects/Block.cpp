#include "pch.h"

#include "Block.h"
#include "../collision/Collider.h"
#include "../managers/GridManager.h"
#include "../util/Constants.h"

Block::Block() = default;
Block::~Block() = default;

void Block::MoveHorizontal(int subCellDelta)
{
    int nextGridX = m_gridX + subCellDelta;

    // TODO: 지금은 테스트용 1칸(=GRID_SUBCELL_SCALE 서브셀 너비)짜리 블럭 기준으로 그 너비만큼만 확인.
    // 실제 테트로미노 모양(cellShape)이 생기면 모양 전체 칸을 확인하도록 확장할 것.
    // 너비 전체(2칸)를 확인해야 하는 이유: 이동은 서브셀 1칸씩인데 블럭은 2칸 너비라서,
    // 원점 칸 하나만 보면 반대쪽 끝이 이미 그리드를 벗어나도 못 잡아낼 수 있음
    for (int offset = 0; offset < Constants::GRID_SUBCELL_SCALE; ++offset)
    {
        if (GridManager::GetInstance().GetCellState(nextGridX + offset, m_gridY) != 0)
        {
            return;
        }
    }

    m_gridX = nextGridX;
}

bool Block::StepDown()
{
    int nextGridY = m_gridY + Constants::FALL_STEP_SUBCELLS;

    // TODO: 지금은 테스트용 1칸(=GRID_SUBCELL_SCALE 서브셀 높이)짜리 블럭 기준으로 그 높이만큼만 확인.
    // 실제 테트로미노 모양(cellShape)이 생기면 모양 전체 칸을 확인하도록 확장할 것.
    // 높이 전체(2칸)를 확인해야 하는 이유: 낙하가 서브셀 1칸씩인데 블럭은 2칸 높이라서,
    // 원점 칸 하나만 보면 바닥 쪽 끝이 이미 바닥을 파고들어도 못 잡아낼 수 있음 (MoveHorizontal 오른쪽 경계 버그와 같은 원인)
    for (int offset = 0; offset < Constants::GRID_SUBCELL_SCALE; ++offset)
    {
        if (GridManager::GetInstance().GetCellState(m_gridX, nextGridY + offset) != 0)
        {
            return false; // 막혔음 = 착지해야 함
        }
    }

    m_gridY = nextGridY;
    return true;
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

void Block::SetGridPosition(int gridX, int gridY)
{
    m_gridX = gridX;
    m_gridY = gridY;
}

Vector2 Block::GetRenderPosition() const
{
    if (m_physicsState == PhysicsState::Airborne)
    {
        return { m_gridX * Constants::SUBCELL_SIZE, m_gridY * Constants::SUBCELL_SIZE };
    }
    return m_position;
}

const std::string& Block::GetSpriteId() const
{
    return m_spriteId;
}

void Block::WakeUp()
{
    // TODO: Sleeping -> Awake 전환 및 관련 상태 갱신 직접 구현
    m_physicsState = PhysicsState::Awake;
}

void Block::Land()
{
    m_position = { m_gridX * Constants::SUBCELL_SIZE, m_gridY * Constants::SUBCELL_SIZE };
    m_physicsState = PhysicsState::Awake;
}
