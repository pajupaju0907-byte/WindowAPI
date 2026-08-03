#include "pch.h"

#include "Block.h"
#include "../collision/Collider.h"
#include "../managers/GridManager.h"
#include "../util/Constants.h"

Block::Block() = default;
Block::~Block() = default;

bool Block::CanOccupy(int originGridX, int originGridY) const
{
	for (int i = 0; i < CELL_COUNT; ++i)
	{
		int cellSubCellX = originGridX + static_cast<int>(m_cellShape[i].x) * Constants::GRID_SUBCELL_SCALE;
		int cellSubCellY = originGridY + static_cast<int>(m_cellShape[i].y) * Constants::GRID_SUBCELL_SCALE;
		for (int offsetY = 0; offsetY < Constants::GRID_SUBCELL_SCALE; ++offsetY)
		{
			for (int offsetX = 0; offsetX < Constants::GRID_SUBCELL_SCALE; ++offsetX)
			{
				if (GridManager::GetInstance().GetCellState(cellSubCellX + offsetX, cellSubCellY + offsetY) != 0)
				{
					return false;
				}
			}
		}
	}
	return true;
}

void Block::MarkOccupiedCells() const
{
	for (int i = 0; i < CELL_COUNT; ++i)
	{
		int cellSubCellX = m_gridX + static_cast<int>(m_cellShape[i].x) * Constants::GRID_SUBCELL_SCALE;
		int cellSubCellY = m_gridY + static_cast<int>(m_cellShape[i].y) * Constants::GRID_SUBCELL_SCALE;
		for (int offsetY = 0; offsetY < Constants::GRID_SUBCELL_SCALE; ++offsetY)
		{
			for (int offsetX = 0; offsetX < Constants::GRID_SUBCELL_SCALE; ++offsetX)
			{
				GridManager::GetInstance().SetCellOccupied(cellSubCellX + offsetX, cellSubCellY + offsetY);
			}
		}
	}
}

void Block::MoveHorizontal(int subCellDelta)
{
	int nextGridX = m_gridX + subCellDelta;

	if (!CanOccupy(nextGridX, m_gridY))
	{
		return;
	}

	m_gridX = nextGridX;
}

bool Block::StepDown()
{
	int nextGridY = m_gridY + Constants::FALL_STEP_SUBCELLS;

	if (!CanOccupy(m_gridX, nextGridY))
	{
		return false; // 막혔음 = 착지해야 함
	}

	m_gridY = nextGridY;
	return true;
}

void Block::Rotate(int direction)
{
	if (!m_canRotate) return;
	Vector2 originalShape[CELL_COUNT];
	for (int i = 0; i < CELL_COUNT; ++i)
	{
		originalShape[i] = m_cellShape[i];
	}
	for (int i = 0; i < CELL_COUNT; ++i)
	{
		float x = m_cellShape[i].x - m_pivot.x;
		float y = m_cellShape[i].y - m_pivot.y;

		if (direction > 0)
		{
			m_cellShape[i] = { m_pivot.x - y, m_pivot.y + x };
		}
		else
		{
			m_cellShape[i] = { m_pivot.x + y, m_pivot.y - x };
		}
	}
	if (!CanOccupy(m_gridX, m_gridY))
	{
		for (int i = 0; i < CELL_COUNT; ++i)
		{
			m_cellShape[i] = originalShape[i];
		}
	}
}
// 여러 힘(중력, 충돌 반발력 등)이 한 프레임에 겹칠 수 있어서, 바로 적용하지 않고 일단 모아둔다
void Block::ApplyForce(Vector2 force)
{
	m_accumulatedForce += force;
}
// F=ma 기반 물리 적분: 누적된 힘 -> 가속도 -> 속도 -> 위치 순서로 반영.
// 매 프레임 새로 힘을 받아야 하므로 마지막에 누적값을 리셋한다.
void Block::Integrate(float deltaTime)
{
	Vector2 acceleration = m_accumulatedForce * (1.0f / m_mass);
	m_velocity += acceleration * deltaTime;
	m_position += m_velocity * deltaTime;

	m_angle += m_angularVelocity * deltaTime;

	m_accumulatedForce = { 0.0f, 0.0f };
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
int Block::GetCellCount() const
{
	return CELL_COUNT;
}

Vector2 Block::GetCellRenderPosition(int cellIndex) const
{
	return GetRenderPosition() + m_cellShape[cellIndex] * Constants::TILE_SIZE;
}

AABB Block::GetWorldBounds() const
{
	Vector2 minPos = GetCellRenderPosition(0);
	Vector2 maxPos = { minPos.x + Constants::TILE_SIZE, minPos.y + Constants::TILE_SIZE };

	// 나머지 칸들을 돌면서 최소/최대 좌표를 넓혀간다
	for (int i = 1; i < GetCellCount(); ++i)
	{
		Vector2 cellPos = GetCellRenderPosition(i);

		if (cellPos.x < minPos.x) minPos.x = cellPos.x;
		if (cellPos.y < minPos.y) minPos.y = cellPos.y;
		if (cellPos.x + Constants::TILE_SIZE > maxPos.x) maxPos.x = cellPos.x + Constants::TILE_SIZE;
		if (cellPos.y + Constants::TILE_SIZE > maxPos.y) maxPos.y = cellPos.y + Constants::TILE_SIZE;
	}

	return { minPos, maxPos };
}

const std::string& Block::GetSpriteId() const
{
	return m_spriteId;
}

float Block::GetMass() const
{
	return m_mass;
}

float Block::GetSpeedSquared() const
{
	return m_velocity.x * m_velocity.x + m_velocity.y * m_velocity.y;
}

PhysicsState Block::GetPhysicsState() const
{
	return m_physicsState;
}

void Block::ResolveVerticalPenetration(float penetration)
{
	// 파고든 만큼 위로 밀어내되, 속도를 0으로 죽이지 않고 반대 방향으로 감쇠시켜 튕겨낸다.
	// 매 프레임 반복되면서 튕기는 폭이 점점 줄어들어 서서히 멈추는 흔들림(wobble)이 된다
	m_position.y -= penetration;
	m_velocity.y = -m_velocity.y * Constants::BOUNCE_RESTITUTION;
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
