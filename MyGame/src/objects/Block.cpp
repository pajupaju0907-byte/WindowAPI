#include "pch.h"

#include "Block.h"
#include "../collision/Collider.h"
#include "../collision/OBBCollider.h"
#include "../managers/BlockManager.h"
#include "../managers/PhysicsManager.h"
#include "../util/Constants.h"
#include "../util/MathUtil.h"
#include <cmath>
#include <cstdio>

namespace
{
	// [관찰 가능성] 물리 상태 전이를 Visual Studio 출력 창 + physics_debug.log 양쪽에 남긴다. Sleep<->Wake가
	// 얼마나 자주 반복되는지, 어느 지점(WakeUp/Sleep/Land/BeginToppling)에서 전이가 일어났는지 코드 안 보고
	// 바로 확인용. [기존엔 OutputDebugStringA만 썼는데, 그러면 ResolveBalance의 imbalance 로그(파일)와
	// 상태 전이 시점을 나란히 놓고 비교할 방법이 없어서 "왜 이 블럭이 이 순간 깨어났는지"를 못 봤다 —
	// 그래서 파일에도 같이 남기고, 그 순간의 위치/각도/속도까지 같이 찍어서 원인 추적이 되게 한다]
	void LogStateChange(const Block* block, const char* reason)
	{
		char text[96];
		std::snprintf(text, sizeof(text), "[Physics] Block %p: %s\n", block, reason);
		OutputDebugStringA(text);

		FILE* file = nullptr;
		fopen_s(&file, "C:\\Users\\inha\\Desktop\\WobbleBlock\\WindowAPI\\physics_debug.log", "a");
		if (file == nullptr)
		{
			return;
		}

		Vector2 pos = block->GetRenderPosition();
		std::fprintf(file, "[StateChange] block=%p reason=%s pos=(%.1f,%.1f) angle=%.2f speed=%.1f angVel=%.2f\n",
			block, reason, pos.x, pos.y, block->GetAngle(), std::sqrt(block->GetSpeedSquared()), block->GetAngularVelocity());
		std::fclose(file);
	}

	// [임시 디버그 - 위로 튐 원인 조사용] 충돌 반응 한 번으로 위쪽(-y) 속도가 비정상적으로 커지면
	// 그 순간의 전후 상태를 파일로 남긴다. 원인을 찾으면 이 함수와 호출부는 지워도 된다.
	void LogIfSuspiciousUpwardVelocity(const char* where, const void* block, Vector2 contactPoint, Vector2 normal,
		float penetration, float velocityAlongNormalBefore, float restitution, float impulseMagnitude,
		Vector2 velocityBefore, Vector2 velocityAfter, float angularVelocityBefore, float angularVelocityAfter,
		int physicsState, float pivotLockTimeRemaining)
	{
		if (velocityAfter.y > -20.0f)
		{
			return;
		}

		FILE* file = nullptr;
		fopen_s(&file, "C:\\Users\\inha\\Desktop\\WobbleBlock\\WindowAPI\\physics_debug.log", "a");
		if (file == nullptr)
		{
			return;
		}

		std::fprintf(file,
			"[%s] block=%p state=%d pivotLock=%.3f contact=(%.1f,%.1f) normal=(%.2f,%.2f) pen=%.2f vAlongNormalBefore=%.1f "
			"restitution=%.3f impulse=%.1f vBefore=(%.1f,%.1f) vAfter=(%.1f,%.1f) wBefore=%.1f wAfter=%.1f\n",
			where, block, physicsState, pivotLockTimeRemaining, contactPoint.x, contactPoint.y, normal.x, normal.y, penetration,
			velocityAlongNormalBefore, restitution, impulseMagnitude, velocityBefore.x, velocityBefore.y,
			velocityAfter.x, velocityAfter.y, angularVelocityBefore, angularVelocityAfter);
		std::fclose(file);
	}
}

Block::Block() = default;
Block::~Block() = default;

bool Block::CanOccupy(int originGridX, int originGridY) const
{
	for (int i = 0; i < CELL_COUNT; ++i)
	{
		int cellSubCellX = originGridX + static_cast<int>(m_cellShape[i].x) * Constants::GRID_SUBCELL_SCALE;
		int cellSubCellY = originGridY + static_cast<int>(m_cellShape[i].y) * Constants::GRID_SUBCELL_SCALE;

		// 가로 범위와 바닥 쪽 하한선(GRID_HEIGHT_SUBCELLS)은 그대로 막되, 위쪽(y가 작아지는 방향)은
		// 일부러 제한을 두지 않는다 — 카메라가 계속 따라 올라가며 탑이 원래 창 높이 위로도 끝없이 쌓일 수
		// 있어야 하기 때문. GridManager는 이 판정에 관여하지 않아서(다른 블럭과의 실제 좌표 SAT 비교로
		// 충돌을 막기 때문에) 이 한 줄만 풀어도 안전하다.
		if (cellSubCellX < 0 || cellSubCellX + Constants::GRID_SUBCELL_SCALE > Constants::GRID_WIDTH_SUBCELLS ||
			cellSubCellY + Constants::GRID_SUBCELL_SCALE > Constants::GRID_HEIGHT_SUBCELLS)
		{
			return false;
		}

		Vector2 cellMin = { cellSubCellX * Constants::SUBCELL_SIZE, cellSubCellY * Constants::SUBCELL_SIZE };
		Vector2 cellMax = { cellMin.x + Constants::TILE_SIZE, cellMin.y + Constants::TILE_SIZE };

		// 바닥(발판) 범위와 겹치면 막힘 — Constants::FLOOR_LEFT_X/RIGHT_X/TOP_Y를 직접 본다
		bool overlapsFloor = cellMax.y > Constants::FLOOR_TOP_Y &&
			cellMax.x > Constants::FLOOR_LEFT_X && cellMin.x < Constants::FLOOR_RIGHT_X;
		if (overlapsFloor)
		{
			return false;
		}

		// 후보 칸(항상 축 정렬된 사각형 — 낙하 중인 조각은 90도 스냅 회전만 하므로 회전 불필요)의 네 꼭짓점
		Vector2 candidateCorners[4] =
		{
			{ cellMin.x, cellMin.y },
			{ cellMax.x, cellMin.y },
			{ cellMax.x, cellMax.y },
			{ cellMin.x, cellMax.y },
		};

		// 이미 착지한(Airborne이 아닌) 다른 블럭의 "지금 실제(회전 반영) 위치"와 겹치면 막힘.
		// GetCellRenderPosition(회전 안 된 좌표)으로 비교하면, 넘어져서 비스듬히 누운 블럭은 실제
		// 겹치는 자리와 전혀 다른 곳을 막힌 걸로 오판해서, 떨어지던 조각이 빈 허공에서 얼어붙어버린다 —
		// 그래서 PhysicsManager가 충돌 판정에 쓰는 것과 같은 SAT(회전 반영) 검사를 그대로 재사용한다.
		for (Block* other : BlockManager::GetInstance().GetAllBlocks())
		{
			bool otherIsLanded = other != this && other->GetPhysicsState() != PhysicsState::Airborne;
			if (!otherIsLanded)
			{
				continue;
			}

			for (int j = 0; j < other->GetCellCount(); ++j)
			{
				Vector2 otherCorners[4];
				other->GetCellRotatedCorners(j, otherCorners);

				if (TestOBBCollision(candidateCorners, otherCorners).collided)
				{
					return false;
				}
			}
		}
	}
	return true;
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

bool Block::CanStepDown() const
{
	int nextGridY = m_gridY + Constants::FALL_STEP_SUBCELLS;
	return CanOccupy(m_gridX, nextGridY);
}

float Block::ComputeContinuousDropOffset() const
{
	Vector2 gridPixelPos = { m_gridX * Constants::SUBCELL_SIZE, m_gridY * Constants::SUBCELL_SIZE };

	// 다음 서브셀로는 못 간다는 것만 확정됐을 뿐이라, 최대로 내려갈 수 있는 한도는 일단 한 서브셀
	// 미만으로 잡아둔다 — 그 이상 여유가 있었다면애초에 CanStepDown()이 true였을 것이다.
	float maxDrop = Constants::SUBCELL_SIZE;

	for (int i = 0; i < CELL_COUNT; ++i)
	{
		Vector2 cellMin = gridPixelPos + m_cellShape[i] * Constants::TILE_SIZE;
		Vector2 cellMax = cellMin + Vector2{ Constants::TILE_SIZE, Constants::TILE_SIZE };

		// 바닥(발판)까지 남은 세로 여유
		bool overlapsFloorX = cellMax.x > Constants::FLOOR_LEFT_X && cellMin.x < Constants::FLOOR_RIGHT_X;
		if (overlapsFloorX)
		{
			float distanceToFloor = Constants::FLOOR_TOP_Y - cellMax.y;
			if (distanceToFloor < maxDrop) maxDrop = distanceToFloor;
		}

		// 이미 착지한 다른 블럭까지 남은 세로 여유. 회전된(GetCellRotatedCorners) 좌표를 써서, 넘어져서
		// 기운 블럭 위에 떨어지는 경우도 실제 기운 모양을 기준으로 정확히 계산한다
		for (Block* other : BlockManager::GetInstance().GetAllBlocks())
		{
			if (other == this || other->GetPhysicsState() == PhysicsState::Airborne)
			{
				continue;
			}

			for (int j = 0; j < other->GetCellCount(); ++j)
			{
				Vector2 otherCorners[4];
				other->GetCellRotatedCorners(j, otherCorners);

				float otherMinX = otherCorners[0].x, otherMaxX = otherCorners[0].x, otherTopY = otherCorners[0].y;
				for (int c = 1; c < 4; ++c)
				{
					if (otherCorners[c].x < otherMinX) otherMinX = otherCorners[c].x;
					if (otherCorners[c].x > otherMaxX) otherMaxX = otherCorners[c].x;
					if (otherCorners[c].y < otherTopY) otherTopY = otherCorners[c].y;
				}

				bool overlapsX = cellMax.x > otherMinX && cellMin.x < otherMaxX;
				if (overlapsX)
				{
					float distanceToOther = otherTopY - cellMax.y;
					if (distanceToOther < maxDrop) maxDrop = distanceToOther;
				}
			}
		}
	}

	// 이론상 항상 >= 0이어야 하지만(현재 위치는 이미 안 겹치는 상태), 부동소수점 오차 방지용 clamp
	if (maxDrop < 0.0f) maxDrop = 0.0f;
	return maxDrop;
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
	// [피벗 고정 중 속도 누수 방지] 잠겨 있는 동안은 m_velocity가 위치에 전혀 반영되지 않는데(아래
	// pivotLockTimeRemaining>0 분기 참고), 그렇다고 중력 가속도 누적까지 막지 않으면 잠긴 시간(최대
	// TOPPLE_PIVOT_LOCK_DURATION초) 내내 m_velocity.y가 화면엔 안 보이는 채로 계속 커진다. 이게
	// 잠금이 풀리는 순간 한꺼번에 위치에 반영되면서, 그리고 그 부풀려진 속도로 다음 바닥/블럭
	// 충돌이 "매우 빠른 충돌"로 오판되어 큰 반발 임펄스를 만들면서 위로 튀는 원인이 된다.
	// 잠긴 동안은 힘을 그냥 버린다 — 그 시간 동안의 중력은 피벗이 대신 떠받치고 있다고 보는 것.
	bool isPivotLocked = m_pivotLockTimeRemaining > 0.0f;

	if (!isPivotLocked)
	{
		Vector2 acceleration = m_accumulatedForce * (1.0f / m_mass);
		m_velocity += acceleration * deltaTime;
	}

	m_angle += m_angularVelocity * deltaTime;

	if (isPivotLocked)
	{
		// [넘어짐 피벗 고정] 위치를 그냥 적분하는 대신, 캡처 시점 이후 회전한 만큼(deltaAngle)만큼
		// m_pivotOffsetAtCapture(무게중심->피벗 오프셋)를 같이 돌려서, 그 결과가 여전히
		// m_pivotWorldTarget을 가리키도록 무게중심 위치를 역산한다. RotateLocalPointToWorld와
		// 같은 회전 공식이지만, 로컬 셀 좌표가 아니라 이미 월드 단위인 오프셋 벡터에 바로 적용한다.
		float deltaAngle = m_angle - m_pivotCaptureAngle;
		float radians = MathUtil::DegreesToRadians(deltaAngle);
		float cosAngle = static_cast<float>(std::cos(radians));
		float sinAngle = static_cast<float>(std::sin(radians));
		Vector2 rotatedOffset
		{
			m_pivotOffsetAtCapture.x * cosAngle - m_pivotOffsetAtCapture.y * sinAngle,
			m_pivotOffsetAtCapture.x * sinAngle + m_pivotOffsetAtCapture.y * cosAngle
		};

		Vector2 centerOfMassWorldTarget = m_pivotWorldTarget - rotatedOffset;
		m_position = centerOfMassWorldTarget - GetCenterOfMassLocal() * Constants::TILE_SIZE;

		m_pivotLockTimeRemaining -= deltaTime;

		// [핸드오프] 잠금이 이 프레임에 끝나면, 다음 프레임부터는 m_velocity로 실제 위치를 적분하게 된다.
		// 잠긴 동안 쌓였을 수 있는(충돌 반응 등에서 온) 남은 속도를 그대로 이어받는 대신, 지금 이 순간
		// 실제로 보이던 회전 운동(무게중심이 피벗을 축으로 각속도 ω로 돌 때의 접선 속도)으로 다시
		// 계산해서 덮어쓴다 — 회전으로 눈에 보이던 움직임과 정확히 이어지는 속도라, 딱 끊거나(0으로 리셋)
		// 갑자기 튀지 않고 부드럽게 일반 물리로 넘어간다.
		if (m_pivotLockTimeRemaining <= 0.0f)
		{
			Vector2 centerOfMassWorld = m_position + GetCenterOfMassLocal() * Constants::TILE_SIZE;
			Vector2 pivotToCenter = centerOfMassWorld - m_pivotWorldTarget;
			float angularVelocityRad = MathUtil::DegreesToRadians(m_angularVelocity);
			m_velocity = { -pivotToCenter.y * angularVelocityRad, pivotToCenter.x * angularVelocityRad };
		}
	}
	else
	{
		m_position += m_velocity * deltaTime;
	}

	m_accumulatedForce = { 0.0f, 0.0f };
}

void Block::SetTopplePivot(Vector2 pivotWorld)
{
	Vector2 centerOfMassWorld = GetRenderPosition() + GetCenterOfMassLocal() * Constants::TILE_SIZE;
	m_pivotWorldTarget = pivotWorld;
	m_pivotOffsetAtCapture = pivotWorld - centerOfMassWorld;
	m_pivotCaptureAngle = m_angle;
	m_pivotLockTimeRemaining = Constants::TOPPLE_PIVOT_LOCK_DURATION;
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

Vector2 Block::GetCellCenterRotated(int cellIndex) const
{
	return RotateLocalPointToWorld(m_cellShape[cellIndex] + Vector2{ 0.5f, 0.5f });
}

Vector2 Block::RotateLocalPointToWorld(Vector2 localPoint) const
{
	// 무게중심에서 이 점까지의 상대 오프셋(픽셀 단위) - 회전은 이 오프셋에만 적용하고 무게중심 자체는 고정.
	// [강체물리 1단계] 예전엔 90도 스냅 회전용 m_pivot을 그대로 재사용했는데, 그건 진짜 무게중심이 아니라서
	// (모양에 따라 어긋남) 정확한 물리를 위해 GetCenterOfMassLocal()로 바꿨다
	Vector2 centerOfMassLocal = GetCenterOfMassLocal();
	Vector2 offset = (localPoint - centerOfMassLocal) * Constants::TILE_SIZE;

	// [성능] m_angle이 지난 호출 때와 같으면 sin/cos를 다시 계산하지 않고 캐시를 재사용한다
	if (!m_trigCacheValid || m_cachedTrigAngle != m_angle)
	{
		float radians = MathUtil::DegreesToRadians(m_angle);
		m_cachedCosAngle = static_cast<float>(std::cos(radians));
		m_cachedSinAngle = static_cast<float>(std::sin(radians));
		m_cachedTrigAngle = m_angle;
		m_trigCacheValid = true;
	}

	Vector2 rotatedOffset
	{
		offset.x * m_cachedCosAngle - offset.y * m_cachedSinAngle,
		offset.x * m_cachedSinAngle + offset.y * m_cachedCosAngle
	};

	Vector2 centerOfMassWorld = GetRenderPosition() + centerOfMassLocal * Constants::TILE_SIZE;
	return centerOfMassWorld + rotatedOffset;
}

void Block::GetCellRotatedCorners(int cellIndex, Vector2 outCorners[4]) const
{
	// cellIndex번 칸은 로컬 좌표계에서 m_cellShape[cellIndex]를 왼쪽 위 꼭짓점으로 하는 1x1 정사각형이다.
	// 네 꼭짓점 전부를 무게중심 기준으로 회전시켜 월드 좌표로 바꿔준다
	// [강체물리 3단계] SAT(분리축) 충돌 검사가 "변"을 순서대로 훑어야 해서, 네 꼭짓점을 시계방향으로
	// 반환하도록 순서를 맞췄다 (왼쪽위 -> 오른쪽위 -> 오른쪽아래 -> 왼쪽아래)
	Vector2 topLeft = m_cellShape[cellIndex];
	outCorners[0] = RotateLocalPointToWorld(topLeft + Vector2{ 0.0f, 0.0f }); // 왼쪽 위
	outCorners[1] = RotateLocalPointToWorld(topLeft + Vector2{ 1.0f, 0.0f }); // 오른쪽 위
	outCorners[2] = RotateLocalPointToWorld(topLeft + Vector2{ 1.0f, 1.0f }); // 오른쪽 아래
	outCorners[3] = RotateLocalPointToWorld(topLeft + Vector2{ 0.0f, 1.0f }); // 왼쪽 아래
}

OBBCollider Block::GetCellCollider(int cellIndex) const
{
	Vector2 halfExtents = { Constants::TILE_SIZE * 0.5f, Constants::TILE_SIZE * 0.5f };
	return OBBCollider(GetCellCenterRotated(cellIndex), halfExtents, m_angle);
}

Vector2 Block::GetCenterOfMassLocal() const
{
	// 칸 하나의 "중심"은 모서리(m_cellShape[i])에서 반 칸(0.5, 0.5)만큼 안쪽이다.
	// 칸마다 무게(m_cellWeight)가 다를 수 있으므로 무게 가중 평균을 낸다 — 전부 1.0이면(기본값)
	// 기존과 동일하게 4칸 중심의 단순 평균이 되고, 특정 칸이 더 무거우면 무게중심이 그쪽으로 쏠린다.
	Vector2 weightedSum = { 0.0f, 0.0f };
	float totalWeight = 0.0f;
	for (int i = 0; i < CELL_COUNT; ++i)
	{
		weightedSum += (m_cellShape[i] + Vector2{ 0.5f, 0.5f }) * m_cellWeight[i];
		totalWeight += m_cellWeight[i];
	}
	return weightedSum * (1.0f / totalWeight);
}

float Block::GetMomentOfInertia() const
{
	// 칸별 질량 = 총 질량(m_mass)을 m_cellWeight 비율대로 나눈 값. 전부 1.0이면 기존처럼 균등 분배.
	float totalWeight = 0.0f;
	for (int i = 0; i < CELL_COUNT; ++i)
	{
		totalWeight += m_cellWeight[i];
	}

	Vector2 centerOfMassLocal = GetCenterOfMassLocal();
	float totalInertia = 0.0f;

	for (int i = 0; i < CELL_COUNT; ++i)
	{
		float cellMass = m_mass * (m_cellWeight[i] / totalWeight);
		float selfInertiaPerCell = (1.0f / 6.0f) * cellMass * Constants::TILE_SIZE * Constants::TILE_SIZE;

		Vector2 cellCenterLocal = m_cellShape[i] + Vector2{ 0.5f, 0.5f };
		Vector2 offsetPx = (cellCenterLocal - centerOfMassLocal) * Constants::TILE_SIZE;
		float distanceSquared = offsetPx.x * offsetPx.x + offsetPx.y * offsetPx.y;

		totalInertia += selfInertiaPerCell + cellMass * distanceSquared;
	}

	// [트리키 타워 핵심 수정]
	// 관성 모멘트가 크면 회전하기(넘어지기) 힘듭니다.
	// 기존 값의 20% 수준(0.2f)으로 확 낮춰서, 살짝만 밀려도 시원하게 휙휙 넘어가게 만듭니다.
	return totalInertia * 0.2f;
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

int Block::GetColorSlotIndex() const
{
	return m_colorSlotIndex;
}

float Block::GetMass() const
{
	return m_mass;
}

float Block::GetSpeedSquared() const
{
	return m_velocity.x * m_velocity.x + m_velocity.y * m_velocity.y;
}

float Block::GetAngle() const
{
	return m_angle;
}

float Block::GetAngularVelocity() const
{
	return m_angularVelocity;
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

// [강체물리 2단계] 회전을 반영한 진짜 강체 충돌.
// 핵심 아이디어: 무게중심 정중앙을 맞은 게 아니라 한쪽 구석(접촉점)을 맞았기 때문에,
// 그 충격의 일부가 "밀림(선속도)"이 아니라 "회전(각속도)"으로 새어나간다.
// 이게 바로 팽이가 옆을 치면 넘어지고 가운데를 누르면 안 넘어지는 이유와 같은 원리다.
//
// [되돌림 — "접촉점 동시 처리"는 더 심각한 버그였다] 한때 이 함수를 접촉점 2개를 한 번에 받아서
// "이번 호출 시작 시점의 속도 하나를 기준으로 각 점의 임펄스를 독립적으로 계산한 뒤 더하는" 방식으로
// 바꿨었다 — 얹혀서 쉬는(속도 거의 0인) 상태의 미세한 회전 편향은 잡았지만, 진짜 충돌(속도가 큰 경우)
// 에서는 두 점이 사실상 "같은 움직임"을 각자 독립적으로 다 멈추려고 계산한 걸 그대로 더해버려서 실제
// 필요한 것보다 훨씬 큰 임펄스가 나왔다 — 각속도가 한 번의 충돌 처리에서 500도/초대에서 -700도/초대로
// 튀는 등 블록이 통째로 날아가는 폭주로 이어졌다(physics_debug.log로 실전 확인됨). 제대로 된 동시-접촉
// 처리는 두 점을 연립해서 풀어야 하는데, 그 대신 원래 방식(접촉점마다 순차 처리 + Step()의
// COLLISION_SOLVER_ITERATIONS(4회) 반복으로 수렴)으로 되돌린다 — 이건 실전에서 오래 쓰인 검증된 방식이고,
// 순차 처리의 미세한 회전 편향은 폭주보다 훨씬 덜 치명적이다.
void Block::ResolveRigidCollision(Vector2 contactPoint, Vector2 normal, float penetration)
{
	// 1) 위치 보정: 파고든 만큼 노멀 방향으로 밀어낸다 (평행이동이라 회전 상태는 그대로 유지됨).
	//    단, 아주 살짝 파고든 것(POSITION_CORRECTION_SLOP 이하)은 무시하고, 나머지도 한 번에 100%가 아니라
	//    POSITION_CORRECTION_PERCENT만큼만 민다 — 매 프레임 완벽하게 0으로 만들려고 하면 그 반동으로
	//    오히려 계속 미세하게 떨리기 때문에, 여러 프레임(솔버 반복)에 걸쳐 서서히 빠져나오게 하는 것
	float correctedPenetration = penetration - Constants::POSITION_CORRECTION_SLOP;
	if (correctedPenetration < 0.0f) correctedPenetration = 0.0f;

	// [순간이동 방지] 착지 판정 경계선의 미세한 오차 등으로 아주 드물게 비정상적으로 깊은 파고듦이
	// 생겼을 때도, 한 프레임에 전부 밀어내면 순간이동처럼 보인다 — 한 프레임당 최대 이동 거리를
	// 제한해서 나머지는 다음 프레임들에 걸쳐 서서히 빠져나오게 한다.
	float moveDistance = correctedPenetration * Constants::POSITION_CORRECTION_PERCENT;
	if (moveDistance > Constants::MAX_POSITION_CORRECTION_PER_STEP)
	{
		moveDistance = Constants::MAX_POSITION_CORRECTION_PER_STEP;
	}

	m_position += normal * moveDistance;

	// 2) 충돌 지점의 실제 속도 = 무게중심 속도 + 회전 때문에 생기는 접선 속도.
	//    회전하는 물체 위의 한 점은 무게중심과 속도가 다르다 (팽이 끝이 중심보다 훨씬 빠르게 도는 것과 같은 이유).
	//    무게중심에서 contactPoint로 향하는 벡터를 r이라 하면, r이 각속도 ω(라디안/초)로 돌 때
	//    r의 속도는 r에 수직인 방향으로 (-r.y, r.x) * ω 이다 (원운동의 속도는 항상 반지름에 수직)
	Vector2 centerOfMassWorld = GetRenderPosition() + GetCenterOfMassLocal() * Constants::TILE_SIZE;
	Vector2 r = contactPoint - centerOfMassWorld;
	float angularVelocityRad = MathUtil::DegreesToRadians(m_angularVelocity);
	Vector2 tangentialVelocity = { -r.y * angularVelocityRad, r.x * angularVelocityRad };
	Vector2 contactVelocity = m_velocity + tangentialVelocity;

	// 3) 노멀 방향 상대속도. 이미 노멀 방향으로 멀어지는 중이면(0 이상) 튕겨낼 필요가 없다 —
	//    위치 보정만 하고 속도는 그대로 둔다 (매 프레임 살짝 파고드는 걸 계속 밀어내는 정도로 충분)
	float velocityAlongNormal = contactVelocity.x * normal.x + contactVelocity.y * normal.y;
	if (velocityAlongNormal >= 0.0f)
	{
		return;
	}

	// 4) 강체 충돌 임펄스 공식 (상대는 안 움직이는 바닥/무한 질량이라고 가정한 버전):
	//        j = -(1+e) * v_n / (1/m + (r×n)^2 / I)
	//    분모의 (r×n)^2/I 항이 핵심 — contactPoint가 무게중심에서 멀수록(=더 잘 회전시키는 위치일수록)
	//    이 항이 커져서 결과적으로 튕기는 힘(선속도 증가분) 중 더 많은 몫이 회전으로 빠져나간다.
	//    r×n, r×impulse는 2차원 벡터의 외적(스칼라값): a×b = a.x*b.y - a.y*b.x
	float rCrossNormal = r.x * normal.y - r.y * normal.x;
	float momentOfInertia = GetMomentOfInertia();
	float inverseMassTerm = (1.0f / m_mass) + (rCrossNormal * rCrossNormal) / momentOfInertia;

	// [Resting contact] 접촉점 속도(무게중심 속도 + 회전 접선 속도)가 WAKE_IMPACT_SPEED_THRESHOLD보다
	// 작으면 "진짜 충돌"이 아니라 얹혀서 미세하게 흔들리거나 회전 중인 상태로 본다 — restitution을 0으로
	// 눌러서 안 튕기게 한다. 안 그러면 기울어진 블록의 회전 접선 속도가 노멀 성분으로 새어들어와
	// 매 프레임 "가짜 충돌"처럼 반복 튕기며 안정 접촉 떨림과, 심하면 위로 튀는 현상까지 만든다.
	float restitution = (-velocityAlongNormal < Constants::WAKE_IMPACT_SPEED_THRESHOLD) ? 0.0f : Constants::BOUNCE_RESTITUTION;
	float impulseMagnitude = -(1.0f + restitution) * velocityAlongNormal / inverseMassTerm;

	Vector2 velocityBeforeImpulse = m_velocity; // [임시 디버그]

	// 5) 구한 임펄스를 선속도/각속도 양쪽에 나눠 반영한다
	Vector2 impulse = normal * impulseMagnitude;
	m_velocity += impulse * (1.0f / m_mass);

	float angularImpulse = r.x * impulse.y - r.y * impulse.x; // r × impulse
	float newAngularVelocityRad = angularVelocityRad + angularImpulse / momentOfInertia;

	// 6) [마찰] normal에 수직인 방향(tangent = 접촉면과 나란한 방향)으로의 미끄러짐을 붙잡는다.
	//    계산 방식은 normal 임펄스랑 똑같은데(그냥 축만 tangent로 바꾼 것), 반발(튕김)은 없어서 계수에 (1+e)가 안 붙고,
	//    대신 "수직으로 누르는 힘(=방금 구한 impulseMagnitude)의 FRICTION_COEFFICIENT배"를 못 넘게 clamp한다 —
	//    실제 마찰도 세게 눌려 있을수록(수직항력이 클수록) 더 세게 붙잡을 수 있는 것과 같은 원리
	Vector2 tangent = { -normal.y, normal.x };
	float velocityAlongTangent = contactVelocity.x * tangent.x + contactVelocity.y * tangent.y;
	float rCrossTangent = r.x * tangent.y - r.y * tangent.x;
	float tangentInverseMassTerm = (1.0f / m_mass) + (rCrossTangent * rCrossTangent) / momentOfInertia;
	float frictionImpulseMagnitude = -velocityAlongTangent / tangentInverseMassTerm;

	float maxFriction = Constants::FRICTION_COEFFICIENT * std::fabs(impulseMagnitude);
	if (frictionImpulseMagnitude > maxFriction) frictionImpulseMagnitude = maxFriction;
	if (frictionImpulseMagnitude < -maxFriction) frictionImpulseMagnitude = -maxFriction;

	Vector2 frictionImpulse = tangent * frictionImpulseMagnitude;
	m_velocity += frictionImpulse * (1.0f / m_mass);
	float frictionAngularImpulse = r.x * frictionImpulse.y - r.y * frictionImpulse.x;
	newAngularVelocityRad += frictionAngularImpulse / momentOfInertia;

	m_angularVelocity = MathUtil::RadiansToDegrees(newAngularVelocityRad);

	LogIfSuspiciousUpwardVelocity("ResolveRigidCollision", this, contactPoint, normal, penetration,
		velocityAlongNormal, restitution, impulseMagnitude, velocityBeforeImpulse, m_velocity,
		MathUtil::RadiansToDegrees(angularVelocityRad), m_angularVelocity,
		static_cast<int>(m_physicsState), m_pivotLockTimeRemaining); // [임시 디버그]
}

// [강체물리 3단계] ResolveRigidCollision과 구조는 같지만, "상대가 안 움직인다"는 가정이 없어서
// other의 위치/속도/각속도도 함께 갱신한다 (뉴턴 3법칙: this가 받는 힘과 other가 받는 힘은 크기 같고 방향 반대).
// normal은 반드시 "other -> this" 방향이어야 한다 (TestOBBCollision/CollisionManager가 그렇게 맞춰서 준다).
// [되돌림] "접촉점 동시 처리"를 되돌린 이유는 ResolveRigidCollision 주석 참고 — 두 점을 독립적으로 계산해
// 더하면 진짜 충돌에서 임펄스가 과도하게 커져 블록이 날아가는 폭주가 생겼다. 원래의 접촉점별 순차 처리
// (+ Step()의 4회 반복 수렴)로 되돌린다.
void Block::ResolveRigidCollisionWithBlock(Block* other, Vector2 contactPoint, Vector2 normal, float penetration)
{
	// 1) 위치 보정: 겹친 만큼을 질량 비율대로 나눠서 서로 반대 방향으로 밀어낸다 (무거운 쪽이 덜 밀림).
	//    ResolveRigidCollision과 마찬가지로 slop은 무시하고 나머지도 일부(POSITION_CORRECTION_PERCENT)만 민다
	float correctedPenetration = penetration - Constants::POSITION_CORRECTION_SLOP;
	if (correctedPenetration < 0.0f) correctedPenetration = 0.0f;
	correctedPenetration *= Constants::POSITION_CORRECTION_PERCENT;

	// [순간이동 방지] ResolveRigidCollision과 같은 이유로, 한 프레임에 밀어낼 수 있는 최대 거리를 제한한다
	if (correctedPenetration > Constants::MAX_POSITION_CORRECTION_PER_STEP)
	{
		correctedPenetration = Constants::MAX_POSITION_CORRECTION_PER_STEP;
	}

	float totalMass = m_mass + other->m_mass;
	float pushRatioSelf = other->m_mass / totalMass;
	float pushRatioOther = m_mass / totalMass;

	m_position += normal * (correctedPenetration * pushRatioSelf);
	other->m_position -= normal * (correctedPenetration * pushRatioOther);

	// 2) 양쪽 접촉점의 실제 속도(무게중심 속도 + 회전 접선 속도)를 각각 구한다
	Vector2 centerOfMassWorldSelf = GetRenderPosition() + GetCenterOfMassLocal() * Constants::TILE_SIZE;
	Vector2 centerOfMassWorldOther = other->GetRenderPosition() + other->GetCenterOfMassLocal() * Constants::TILE_SIZE;
	Vector2 rSelf = contactPoint - centerOfMassWorldSelf;
	Vector2 rOther = contactPoint - centerOfMassWorldOther;

	float angularVelocitySelfRad = MathUtil::DegreesToRadians(m_angularVelocity);
	float angularVelocityOtherRad = MathUtil::DegreesToRadians(other->m_angularVelocity);

	Vector2 contactVelocitySelf = m_velocity + Vector2{ -rSelf.y * angularVelocitySelfRad, rSelf.x * angularVelocitySelfRad };
	Vector2 contactVelocityOther = other->m_velocity + Vector2{ -rOther.y * angularVelocityOtherRad, rOther.x * angularVelocityOtherRad };

	// 3) normal이 other->this 방향이므로, "this속도 - other속도"가 접근 중일 때 음수가 된다
	//    (this가 그 방향으로 다가가고 있으면, this 속도의 normal 성분이 other보다 더 작다/음수 쪽이라서)
	Vector2 relativeVelocity = contactVelocitySelf - contactVelocityOther;
	float velocityAlongNormal = relativeVelocity.x * normal.x + relativeVelocity.y * normal.y;
	if (velocityAlongNormal >= 0.0f)
	{
		// 이미 서로 멀어지는 중이면 위치 보정만 하고 끝
		return;
	}

	// 4) 강체 충돌 임펄스 공식 (양쪽 다 움직이는 버전. ResolveRigidCollision의 분모에 other 항이 추가된 것뿐):
	//        j = -(1+e) * v_n / (1/mA + 1/mB + (rA×n)^2/IA + (rB×n)^2/IB)
	float rSelfCrossNormal = rSelf.x * normal.y - rSelf.y * normal.x;
	float rOtherCrossNormal = rOther.x * normal.y - rOther.y * normal.x;
	float momentOfInertiaSelf = GetMomentOfInertia();
	float momentOfInertiaOther = other->GetMomentOfInertia();

	float inverseMassTerm = (1.0f / m_mass) + (1.0f / other->m_mass) +
		(rSelfCrossNormal * rSelfCrossNormal) / momentOfInertiaSelf +
		(rOtherCrossNormal * rOtherCrossNormal) / momentOfInertiaOther;

	// [Resting contact] ResolveRigidCollision과 같은 이유로, 상대 접근 속도가 작으면(진짜 충돌이 아니면)
	// restitution을 0으로 눌러서 안 튕기게 한다.
	float restitution = (-velocityAlongNormal < Constants::WAKE_IMPACT_SPEED_THRESHOLD) ? 0.0f : Constants::BOUNCE_RESTITUTION;
	float impulseMagnitude = -(1.0f + restitution) * velocityAlongNormal / inverseMassTerm;
	Vector2 impulse = normal * impulseMagnitude;

	Vector2 selfVelocityBeforeImpulse = m_velocity; // [임시 디버그]
	Vector2 otherVelocityBeforeImpulse = other->m_velocity; // [임시 디버그]

	// 5) 뉴턴의 3법칙: this는 +impulse(=other에게서 멀어지는 방향), other는 -impulse(=this에게서 멀어지는 방향)
	m_velocity += impulse * (1.0f / m_mass);
	other->m_velocity -= impulse * (1.0f / other->m_mass);

	float angularImpulseSelf = rSelf.x * impulse.y - rSelf.y * impulse.x;
	float angularImpulseOther = rOther.x * impulse.y - rOther.y * impulse.x;

	float newAngularVelocitySelfRad = angularVelocitySelfRad + angularImpulseSelf / momentOfInertiaSelf;
	float newAngularVelocityOtherRad = angularVelocityOtherRad - angularImpulseOther / momentOfInertiaOther;

	// 6) [마찰] ResolveRigidCollision과 같은 방식, 양쪽(this/other) 다 관여하는 버전으로 확장한 것뿐이다.
	//    normal에 수직인 tangent 방향으로의 상대 미끄러짐을 normal 임펄스 크기에 비례해서만 붙잡는다
	Vector2 tangent = { -normal.y, normal.x };
	float velocityAlongTangent = relativeVelocity.x * tangent.x + relativeVelocity.y * tangent.y;
	float rSelfCrossTangent = rSelf.x * tangent.y - rSelf.y * tangent.x;
	float rOtherCrossTangent = rOther.x * tangent.y - rOther.y * tangent.x;

	float tangentInverseMassTerm = (1.0f / m_mass) + (1.0f / other->m_mass) +
		(rSelfCrossTangent * rSelfCrossTangent) / momentOfInertiaSelf +
		(rOtherCrossTangent * rOtherCrossTangent) / momentOfInertiaOther;

	float frictionImpulseMagnitude = -velocityAlongTangent / tangentInverseMassTerm;
	float maxFriction = Constants::FRICTION_COEFFICIENT * std::fabs(impulseMagnitude);
	if (frictionImpulseMagnitude > maxFriction) frictionImpulseMagnitude = maxFriction;
	if (frictionImpulseMagnitude < -maxFriction) frictionImpulseMagnitude = -maxFriction;

	Vector2 frictionImpulse = tangent * frictionImpulseMagnitude;
	m_velocity += frictionImpulse * (1.0f / m_mass);
	other->m_velocity -= frictionImpulse * (1.0f / other->m_mass);

	float frictionAngularImpulseSelf = rSelf.x * frictionImpulse.y - rSelf.y * frictionImpulse.x;
	float frictionAngularImpulseOther = rOther.x * frictionImpulse.y - rOther.y * frictionImpulse.x;

	newAngularVelocitySelfRad += frictionAngularImpulseSelf / momentOfInertiaSelf;
	newAngularVelocityOtherRad -= frictionAngularImpulseOther / momentOfInertiaOther;

	m_angularVelocity = MathUtil::RadiansToDegrees(newAngularVelocitySelfRad);
	other->m_angularVelocity = MathUtil::RadiansToDegrees(newAngularVelocityOtherRad);

	// [임시 디버그]
	LogIfSuspiciousUpwardVelocity("ResolveRigidCollisionWithBlock/self", this, contactPoint, normal, penetration,
		velocityAlongNormal, restitution, impulseMagnitude, selfVelocityBeforeImpulse, m_velocity,
		MathUtil::RadiansToDegrees(angularVelocitySelfRad), m_angularVelocity,
		static_cast<int>(m_physicsState), m_pivotLockTimeRemaining);
	LogIfSuspiciousUpwardVelocity("ResolveRigidCollisionWithBlock/other", other, contactPoint, normal * -1.0f, penetration,
		-velocityAlongNormal, restitution, -impulseMagnitude, otherVelocityBeforeImpulse, other->m_velocity,
		MathUtil::RadiansToDegrees(angularVelocityOtherRad), other->m_angularVelocity,
		static_cast<int>(other->m_physicsState), other->m_pivotLockTimeRemaining);
}

void Block::ApplyGravityTorque(Vector2 pivotWorld, float deltaTime)
{
	// 무게중심에서 pivot까지의 벡터 r. 중력(0, mass*GRAVITY)이 이 지렛대에 만들어내는 토크는
	// r.x * gravityForce.y - r.y * gravityForce.x인데, gravityForce.x가 항상 0이라 뒷항은 사라지고
	// 결국 "수평 거리(r.x) * 중력 크기"만 남는다 — 기울수록(r.x가 커질수록) 더 세게 돌아가는 이유.
	Vector2 centerOfMassWorld = GetRenderPosition() + GetCenterOfMassLocal() * Constants::TILE_SIZE;
	Vector2 r = centerOfMassWorld - pivotWorld;
	float torqueAboutPivot = r.x * (m_mass * Constants::GRAVITY);

	// [평행축 정리] 지금 실제로 도는 축은 무게중심이 아니라 pivot이므로, 무게중심 기준 관성모멘트
	// (GetMomentOfInertia)에 m*거리^2를 더해 pivot 기준 관성모멘트로 바꿔줘야 한다. 이걸 빼먹으면
	// 많이 기울어질수록(거리가 멀어질수록) 실제보다 비현실적으로 빨리 가속돼버린다.
	float distanceSquared = r.x * r.x + r.y * r.y;
	float inertiaAboutPivot = GetMomentOfInertia() + m_mass * distanceSquared;

	float angularAccelerationRad = torqueAboutPivot / inertiaAboutPivot;
	m_angularVelocity += MathUtil::RadiansToDegrees(angularAccelerationRad) * deltaTime;
}

void Block::BeginToppling()
{
	// 속도/각속도를 인위적으로 바꾸지 않는다 — 그 순간의 실제 속도를 그대로 이어받아야 자연스럽다.
	// 여기서 바뀌는 건 상태뿐이다. 이후로는 순수하게 진짜 물리(중력 + 바닥/블럭 충돌)만으로 움직인다
	LogStateChange(this, "-> Toppling");

	m_physicsState = PhysicsState::Toppling;
	m_hasToppled = true;

	// [강제 취침 타임아웃] 여기서부터가 진짜 새로운 불안정 사건의 시작이므로, 이전에 쌓여있던
	// 활동 시간과 섞이지 않도록 여기서 새로 잰다
	m_activeTimer = 0.0f;
	m_imbalanceTimer = 0.0f;
	m_isWedged = false;

	// [무한 재넘어짐 방지] Sleep() 없이 다시 여기로 왔다는 뜻이므로 1 늘린다. Sleep()에서 0으로 리셋된다.
	++m_consecutiveToppleCount;
}

int Block::GetConsecutiveToppleCount() const
{
	return m_consecutiveToppleCount;
}

void Block::ResetConsecutiveToppleCount()
{
	m_consecutiveToppleCount = 0;
}

void Block::ConfirmRestingAngle()
{
	m_restAngleReference = m_angle;
}

float Block::GetRestAngleReference() const
{
	return m_restAngleReference;
}

void Block::AdvanceImbalanceTimer(float deltaTime)
{
	m_imbalanceTimer += deltaTime;
}

void Block::ResetImbalanceTimer()
{
	m_imbalanceTimer = 0.0f;
	m_isWedged = false;
}

float Block::GetImbalanceTimer() const
{
	return m_imbalanceTimer;
}

void Block::SetImbalancePivot(Vector2 pivot)
{
	m_imbalancePivot = pivot;
	m_imbalanceCenterOfMassAtStart = GetRenderPosition() + GetCenterOfMassLocal() * Constants::TILE_SIZE;
	m_imbalanceAngleAtStart = m_angle;
}

Vector2 Block::GetImbalancePivot() const
{
	return m_imbalancePivot;
}

Vector2 Block::GetEffectiveWeightPositionWorld() const
{
	if (m_imbalanceTimer <= 0.0f)
	{
		// 불균형 에피소드 중이 아니면(평범하게 안정된 상태) 그냥 진짜 무게중심을 쓴다.
		return GetRenderPosition() + GetCenterOfMassLocal() * Constants::TILE_SIZE;
	}

	// 에피소드 시작 시점의 (무게중심-피벗) 오프셋을, 그 이후 실제로 더 돈 각도만큼 회전시켜서
	// "pivot을 축으로 진짜 swing했다면 지금 무게중심이 어디 있을지"를 계산한다. m_position은 안 건드린다.
	Vector2 offsetAtStart = m_imbalanceCenterOfMassAtStart - m_imbalancePivot;
	float deltaAngle = m_angle - m_imbalanceAngleAtStart;
	float radians = MathUtil::DegreesToRadians(deltaAngle);
	float cosAngle = static_cast<float>(std::cos(radians));
	float sinAngle = static_cast<float>(std::sin(radians));
	Vector2 rotatedOffset
	{
		offsetAtStart.x * cosAngle - offsetAtStart.y * sinAngle,
		offsetAtStart.x * sinAngle + offsetAtStart.y * cosAngle
	};

	return m_imbalancePivot + rotatedOffset;
}

bool Block::IsWedged() const
{
	return m_isWedged;
}

void Block::ForceStabilize()
{
	LogStateChange(this, "-> ForceStabilize (stuck toppling loop)");
	m_velocity = { 0.0f, 0.0f };
	m_angularVelocity = 0.0f;

	// [무한 재시도 방지 — 실전에서 발견된 2차 폭주 버그] m_activeTimer는 Sleep<->WakeUp을 오가도 일부러
	// 리셋 안 되게 설계됐다(FORCE_SLEEP_TIMEOUT 안전장치가 반복되는 미세 진동도 결국 잡아내야 하니까) —
	// 그런데 ForceStabilize가 "이미 한 번 안전장치에 걸려 강제로 재운" 뒤에도 이 타이머를 그대로 두면,
	// 나중에 (옆 블록 충돌 등) 다른 이유로 다시 깨어나는 순간 activeTimer가 이미 FORCE_SLEEP_TIMEOUT을
	// 훌쩍 넘은 상태라 TrySleepAll/ResolveBalance가 그 즉시 또 ForceStabilize를 불러버린다 — 강제 정지
	// →깨어남→강제 정지가 사실상 매 프레임 무한 반복되는 걸 physics_debug.log로 실전 확인했다(각도/위치가
	// 고정된 채 수백 번 반복). "이미 포기했다"는 사실 자체가 새로운 사건이므로, 여기서부터 다시 새로
	// 3초를 재도록 완전히 리셋한다.
	m_activeTimer = 0.0f;

	// 같은 이유로, ResolveBalance도 같은(안 풀린) 불균형을 곧바로 다시 재시도하지 않도록 "포기" 표시를
	// 해둔다 — 어느 ForceStabilize 호출 경로(끼임 판정/재넘어짐 한도/전역 안전장치)에서 왔든 동일하게
	// 적용되도록 호출부가 아니라 여기서 직접 처리한다. 균형이 실제로 돌아오면 ResetImbalanceTimer()에서
	// 자연히 풀린다.
	m_imbalanceTimer = 0.0f;
	m_isWedged = true;

	Sleep();
}

void Block::DampVelocity(float dampingFactor)
{
	m_velocity = m_velocity * dampingFactor;
}

void Block::DampAngularVelocity(float dampingFactor)
{
	m_angularVelocity *= dampingFactor;
}

void Block::WakeUp()
{
	LogStateChange(this, "-> Awake (WakeUp)");

	// [강제 취침 타임아웃] 여기서도 타이머를 리셋하지 않는다 — Sleep()과 동일한 이유:
	// Sleep<->Wake를 반복하는 동안 m_activeTimer가 끊기지 않고 누적돼야 ForceSleepStuckBlocks가
	// 3초를 채울 수 있다.
	m_physicsState = PhysicsState::Awake;
}
void Block::Sleep()
{
	// [그리드 스냅] 한 번도 안 넘어지고(Toppling 없이) 정상 착지해서 흔들리다 멈춘 블럭은, 물리 오차로
	// 살짝 어긋난 위치/각도 대신 진짜 그리드에 딱 맞춘 좌표로 스냅해서 테트리스처럼 꽉 들어맞아 보이게 한다.
	// 넘어진 블럭은 착지 위치까지는 물리가 정한 그대로 두지만(그리드에 맞을 이유가 없다), 각도는
	// 90도 배수에서 ANGLE_SNAP_TOLERANCE 이내로 "거의" 다 정렬됐을 때만 마저 반올림한다 — 다른 블럭에
	// 기대서 진짜로 비스듬히 안정된 블럭까지 강제로 우뚝 세우면 안 되고, 물리 오차 수준의 미세한
	// 잔여 기울기만 깔끔하게 정리한다.
	float nearestRightAngle = std::round(m_angle / 90.0f) * 90.0f;
	if (std::fabs(m_angle - nearestRightAngle) <= Constants::ANGLE_SNAP_TOLERANCE)
	{
		m_angle = nearestRightAngle;
	}
	m_angularVelocity = 0.0f;

	// [넘어짐 재발 판정 기준각] 지금 이 각도(0도든, 완전히 넘어져서 90/135도든)를 "확실히 안정된" 기준으로
	// 새로 잡는다 — MAX_TOPPLE_ANGLE 비교가 절대각이 아니라 이 기준에서 얼마나 더 돌았는지를 봐야
	// 완전히 넘어져서 옆으로 누운 채 잠든 블록이 또 안전장치에 걸려 무한 반복하지 않는다.
	ConfirmRestingAngle();

	if (!m_hasToppled)
	{
		// [Sleep 스냅] 각도 스냅과 대칭되는 안전장치 — 허용 오차 없이 무조건 반올림하면 물리 솔버가
		// 침투 없이 정착시킨 위치를 다시 밀어넣어 옆 블록과 겹치게 만들 수 있다. 이미 격자에
		// POSITION_SNAP_TOLERANCE 이내로 가까울 때만, 그 축만 스냅한다.
		float snappedX = std::round(m_position.x / Constants::SUBCELL_SIZE) * Constants::SUBCELL_SIZE;
		float snappedY = std::round(m_position.y / Constants::SUBCELL_SIZE) * Constants::SUBCELL_SIZE;

		if (std::fabs(m_position.x - snappedX) <= Constants::POSITION_SNAP_TOLERANCE)
		{
			m_position.x = snappedX;
		}
		if (std::fabs(m_position.y - snappedY) <= Constants::POSITION_SNAP_TOLERANCE)
		{
			m_position.y = snappedY;
		}
		m_velocity = { 0.0f, 0.0f };
	}

	// [강제 취침 타임아웃] 여기서 타이머를 리셋하면 안 된다 — 미세하게 떨리는 블럭은 TrySleepAll이
	// "잠깐 느려졌다"고 착각해서 Sleep 시켰다가 바로 다음 프레임 충돌로 다시 WakeUp되는 걸 반복하는데,
	// 그때마다 리셋되면 3초를 채울 기회가 영영 없다. 진짜로 새 불안정 사건이 시작되는 Land()/BeginToppling()
	// 에서만 리셋해서, 이런 Sleep<->Wake 반복 중에도 누적 시간이 끊기지 않게 한다.
	m_physicsState = PhysicsState::Sleeping;

	// [무한 재넘어짐 방지] 재넘어짐 카운트는 여기서 리셋하지 않는다 — ForceStabilize()도 이 Sleep()을
	// 거치는데, 만약 여기서 리셋해버리면 "반칸 걸침"처럼 진짜 애매한 경계에서는 강제로 재운 바로 다음
	// 프레임에 다시 불안정 판정 → 카운트 0부터 5번 재시도 → 또 강제로 재움... 묶음이 영원히 반복된다.
	// 그래서 "진짜로 안정돼서 스스로 잠든" 경로(TrySleepAll)에서만 리셋하고, ForceStabilize가 억지로
	// 재운 경우엔 카운트를 그대로 유지해서 다음 재판정에도 곧바로 다시 ForceStabilize로 조용히 멈추게 한다.

	LogStateChange(this, "-> Sleeping");
}
void Block::AdvanceRestTimer(float deltaTime)
{
	m_restTimer += deltaTime;
}

void Block::ResetRestTimer()
{
	m_restTimer = 0.0f;
}

float Block::GetRestTimer() const
{
	return m_restTimer;
}
void Block::AdvanceActiveTimer(float deltaTime)
{
	m_activeTimer += deltaTime;
}

float Block::GetActiveTimer() const
{
	return m_activeTimer;
}

void Block::Land()
{
	LogStateChange(this, "-> Awake (Land)");

	// [그리드-연속 경계] 그리드 칸에 딱 맞춰 배치하는 대신, 실제 장애물까지 남은 여유(dropOffset)만큼
	// 더 내려간 위치에 배치한다 — 최대 24px(한 서브셀)까지 남아있던 "허공에 뜬 채로 락" 간격이 사라져서,
	// 착지 직후 잠깐 멈췄다가 다시 떨어지는 어색함 없이 바로 실제 접촉 위치에서 물리가 시작된다.
	float dropOffset = ComputeContinuousDropOffset();
	m_position = { m_gridX * Constants::SUBCELL_SIZE, m_gridY * Constants::SUBCELL_SIZE + dropOffset };
	m_physicsState = PhysicsState::Awake;
	m_activeTimer = 0.0f;
	m_imbalanceTimer = 0.0f;
	m_isWedged = false;
	m_restAngleReference = 0.0f;
}
