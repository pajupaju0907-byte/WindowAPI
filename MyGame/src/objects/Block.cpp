#include "pch.h"

#include "Block.h"
#include "../collision/Collider.h"
#include "../collision/OBBCollider.h"
#include "../managers/BlockManager.h"
#include "../managers/PhysicsManager.h"
#include "../util/Constants.h"
#include "../util/MathUtil.h"
#include <cmath>

namespace
{
	// [지지 없이 얼어붙는 버그 수정 — 마찰에도 같은 기준 적용] normal(접촉 법선)이 수직에서
	// Constants::SUPPORT_SURFACE_MAX_TILT_DEGREES 이내로 벗어났는지. 기울어진 블록 위/옆면에 닿으면
	// TestOBBCollision의 SAT 법선도 그 기울기를 그대로 따라 나온다 — 즉 법선이 수직에서 많이 벗어났다는
	// 건 접촉면 자체가 기울어져 있다는 뜻이고, 이건 PhysicsManager::IsNearAxisAlignedAngle이 "이 표면은
	// 지지로 못 믿는다"고 판단하는 것과 정확히 같은 기하학적 상황이다. 이 조건 없이 마찰(쿨롱, 계수 0.7)을
	// 그대로 걸면, 경사가 완만해서(예: 17도) 마찰이 실제로 붙잡을 수 있는 범위 안이면 물리적으로는 "정상"
	// 이지만 — 공식 지지 판정(GetCellSupportRange)은 이미 "지지 없음"이라고 봐서 다음 순간 떨어지거나
	// 미끄러져야 하는데, 마찰이 그 아주 작은 접선 속도를 매 프레임 흡수해버려 영원히 그 자리에 붙박인다
	// ("지지가 없다는데도 얼어붙는" 버그의 실제 원인 — 실전 확인). 법선 방향의 침투 보정/충돌 임펄스
	// (아래 1~5단계, 폭주 이력이 있어 손대면 위험한 부분)는 그대로 두고, 마찰(6단계)만 이 조건으로 좁힌다.
	bool IsContactNormalNearVertical(Vector2 normal)
	{
		float cosMaxTilt = static_cast<float>(std::cos(MathUtil::DegreesToRadians(Constants::SUPPORT_SURFACE_MAX_TILT_DEGREES)));
		return std::fabs(normal.y) >= cosMaxTilt;
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

	m_angle += m_angularVelocity * deltaTime;
	m_position += m_velocity * deltaTime;

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

	float radians = MathUtil::DegreesToRadians(m_angle);
	float cosAngle = static_cast<float>(std::cos(radians));
	float sinAngle = static_cast<float>(std::sin(radians));

	Vector2 rotatedOffset
	{
		offset.x * cosAngle - offset.y * sinAngle,
		offset.x * sinAngle + offset.y * cosAngle
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

	// [충돌 반응과 분리 — 실전에서 발견된 과도한 스핀 버그 수정] "넘어지기 쉽게" 관성을 깎는 튜닝
	// (Constants::TOPPLE_INERTIA_SCALE)은 여기가 아니라 ApplyGravityTorque 호출부에서만 곱한다.
	// 예전엔 여기서 곧바로 0.2배 해서 돌려줬는데, 이 함수를 ResolveRigidCollision/
	// ResolveRigidCollisionWithBlock의 충돌 임펄스 계산도 그대로 가져다 써서, 가만히 쌓여 쉬는
	// 블럭들의 사소한 접촉까지 실제보다 5배 더 세게 회전으로 새어나가 영원히 안 재워지는 원인이 됐다.
	// 여기서는 물리적으로 정확한(축소 안 한) 값을 그대로 돌려준다.
	return totalInertia;
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

	// 4) 강체 충돌 임펄스 공식 (반발 없음, 상대는 안 움직이는 바닥/무한 질량이라고 가정한 버전):
	//        j = -v_n / (1/m + (r×n)^2 / I)
	//    분모의 (r×n)^2/I 항이 핵심 — contactPoint가 무게중심에서 멀수록(=더 잘 회전시키는 위치일수록)
	//    이 항이 커져서 결과적으로 튕기는 힘(선속도 증가분) 중 더 많은 몫이 회전으로 빠져나간다.
	//    r×n, r×impulse는 2차원 벡터의 외적(스칼라값): a×b = a.x*b.y - a.y*b.x
	float rCrossNormal = r.x * normal.y - r.y * normal.x;
	float momentOfInertia = GetMomentOfInertia();
	float inverseMassTerm = (1.0f / m_mass) + (rCrossNormal * rCrossNormal) / momentOfInertia;

	// [Toppling만 반발] 평소 얹혀서 쉬는 충돌(반발 0, 비탄성)은 그대로 두고, 넘어지는 중(Toppling)인
	// 블록이 실제 낙하 속도로 부딪힐 때만 TOPPLE_RESTITUTION만큼 튕겨준다 — 이유는 Constants.h의
	// TOPPLE_RESTITUTION 주석 참고.
	float restitution = (m_physicsState == PhysicsState::Toppling) ? Constants::TOPPLE_RESTITUTION : 0.0f;
	float impulseMagnitude = -(1.0f + restitution) * velocityAlongNormal / inverseMassTerm;

	// 5) 구한 임펄스를 선속도/각속도 양쪽에 나눠 반영한다
	Vector2 impulse = normal * impulseMagnitude;
	m_velocity += impulse * (1.0f / m_mass);

	float angularImpulse = r.x * impulse.y - r.y * impulse.x; // r × impulse
	float newAngularVelocityRad = angularVelocityRad + angularImpulse / momentOfInertia;

	// 6) [마찰] normal에 수직인 방향(tangent = 접촉면과 나란한 방향)으로의 미끄러짐을 붙잡는다.
	//    계산 방식은 normal 임펄스랑 똑같은데(그냥 축만 tangent로 바꾼 것), 대신 "수직으로 누르는
	//    힘(=방금 구한 impulseMagnitude)의 FRICTION_COEFFICIENT배"를 못 넘게 clamp한다 —
	//    실제 마찰도 세게 눌려 있을수록(수직항력이 클수록) 더 세게 붙잡을 수 있는 것과 같은 원리
	// [넘어지는 중엔 마찰 생략 — 실전 로그로 확인된 "절벽 모서리에 코너가 낀 채 못 빠져나감" 버그 수정]
	// Toppling 블록의 회전하는 코너가 바닥/블록 모서리에 걸친 채로 계속 회전해서 빠져나가야 하는 상황에서,
	// 매 스텝 이 마찰이 그 회전(접선 방향 미끄러짐)을 붙잡아버리면 중력 토크가 계속 걸리는데도 각속도가
	// 오히려 줄어든다 — 위치는 거의 그대로, 각도만 아주 느리게 진동하다가 결국 TOPPLE_STUCK_TIMEOUT에
	// 걸려 공중에 뜬 채로 ForceStabilize되는 것으로 physics_debug.log에서 실전 확인됨. 넘어지는 도중엔
	// 마찰로 붙들 "정지 자세"가 아니므로 생략하고, 중력+수직 임펄스만으로 코너를 타고 계속 회전해
	// 빠져나가게 한다.
	// [지지 없이 얼어붙는 버그 수정] IsContactNormalNearVertical 조건 추가 이유는 이 파일 상단 헬퍼 주석 참고.
	if (m_physicsState != PhysicsState::Toppling && IsContactNormalNearVertical(normal))
	{
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
	}

	m_angularVelocity = MathUtil::RadiansToDegrees(newAngularVelocityRad);
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

	// 4) 강체 충돌 임펄스 공식 (반발 없음, 양쪽 다 움직이는 버전. ResolveRigidCollision의 분모에 other 항이 추가된 것뿐):
	//        j = -v_n / (1/mA + 1/mB + (rA×n)^2/IA + (rB×n)^2/IB)
	float rSelfCrossNormal = rSelf.x * normal.y - rSelf.y * normal.x;
	float rOtherCrossNormal = rOther.x * normal.y - rOther.y * normal.x;
	float momentOfInertiaSelf = GetMomentOfInertia();
	float momentOfInertiaOther = other->GetMomentOfInertia();

	float inverseMassTerm = (1.0f / m_mass) + (1.0f / other->m_mass) +
		(rSelfCrossNormal * rSelfCrossNormal) / momentOfInertiaSelf +
		(rOtherCrossNormal * rOtherCrossNormal) / momentOfInertiaOther;

	// [Toppling만 반발] ResolveRigidCollision과 같은 이유 — this/other 둘 중 하나라도 Toppling(실제
	// 낙하 속도로 부딪히는 중)이면 반발을 준다. 둘 다 그냥 얹혀 쉬는 중이면 예전처럼 반발 0.
	bool eitherToppling = m_physicsState == PhysicsState::Toppling || other->m_physicsState == PhysicsState::Toppling;
	float restitution = eitherToppling ? Constants::TOPPLE_RESTITUTION : 0.0f;
	float impulseMagnitude = -(1.0f + restitution) * velocityAlongNormal / inverseMassTerm;
	Vector2 impulse = normal * impulseMagnitude;

	// 5) 뉴턴의 3법칙: this는 +impulse(=other에게서 멀어지는 방향), other는 -impulse(=this에게서 멀어지는 방향)
	m_velocity += impulse * (1.0f / m_mass);
	other->m_velocity -= impulse * (1.0f / other->m_mass);

	float angularImpulseSelf = rSelf.x * impulse.y - rSelf.y * impulse.x;
	float angularImpulseOther = rOther.x * impulse.y - rOther.y * impulse.x;

	float newAngularVelocitySelfRad = angularVelocitySelfRad + angularImpulseSelf / momentOfInertiaSelf;
	float newAngularVelocityOtherRad = angularVelocityOtherRad - angularImpulseOther / momentOfInertiaOther;

	// 6) [마찰] ResolveRigidCollision과 같은 방식, 양쪽(this/other) 다 관여하는 버전으로 확장한 것뿐이다.
	//    normal에 수직인 tangent 방향으로의 상대 미끄러짐을 normal 임펄스 크기에 비례해서만 붙잡는다
	// [넘어지는 중엔 마찰 생략] ResolveRigidCollision과 같은 이유 — this/other 중 하나라도 Toppling이면
	// (다른 블록 모서리에 걸친 채 회전하며 빠져나가는 코너에) 마찰이 그 회전을 붙잡아 각속도를 죽이는 걸
	// 막는다.
	// [지지 없이 얼어붙는 버그 수정] IsContactNormalNearVertical 조건 추가 이유는 이 파일 상단 헬퍼 주석 참고.
	if (m_physicsState != PhysicsState::Toppling && other->m_physicsState != PhysicsState::Toppling && IsContactNormalNearVertical(normal))
	{
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
	}

	m_angularVelocity = MathUtil::RadiansToDegrees(newAngularVelocitySelfRad);
	other->m_angularVelocity = MathUtil::RadiansToDegrees(newAngularVelocityOtherRad);
}

void Block::ApplyGravityTorque(Vector2 pivotWorld, float combinedComX, float combinedMass, float deltaTime)
{
	// 무게중심에서 pivot까지의 벡터 r. 중력(0, mass*GRAVITY)이 이 지렛대에 만들어내는 토크는
	// r.x * gravityForce.y - r.y * gravityForce.x인데, gravityForce.x가 항상 0이라 뒷항은 사라지고
	// 결국 "수평 거리(r.x) * 중력 크기"만 남는다 — 기울수록(r.x가 커질수록) 더 세게 돌아가는 이유.
	// [위에 얹힌 무게 반영] r.x(지렛대 길이)는 이 블록 혼자만의 무게중심이 아니라 combinedComX(자신 +
	// 위에 얹힌 전체의 결합 무게중심)를 쓴다 — pivot 기준 판단(ResolveBalance의 isImbalanced)도 이미
	// combinedComX로 하므로, 방향이 항상 일치한다(자기 자신만의 무게중심을 쓰면 pivot 반대쪽에 있어서
	// 오히려 반대 방향 토크가 나올 수 있었다 — 실전 확인). r.y(높이)는 계산이 복잡해지는 걸 피하려고
	// 이 블록 자신의 무게중심 높이를 그대로 쓴다(근사) — 토크 자체는 r.y와 무관(gravityForce.x=0)하고
	// 아래 관성 계산에서만 근사치로 쓰인다.
	Vector2 centerOfMassWorld = GetRenderPosition() + GetCenterOfMassLocal() * Constants::TILE_SIZE;
	Vector2 r = { combinedComX - pivotWorld.x, centerOfMassWorld.y - pivotWorld.y };
	float torqueAboutPivot = r.x * (combinedMass * Constants::GRAVITY);

	// [평행축 정리] 지금 실제로 도는 축은 무게중심이 아니라 pivot이므로, 무게중심 기준 관성모멘트
	// (GetMomentOfInertia)에 m*거리^2를 더해 pivot 기준 관성모멘트로 바꿔줘야 한다. 이걸 빼먹으면
	// 많이 기울어질수록(거리가 멀어질수록) 실제보다 비현실적으로 빨리 가속돼버린다.
	// [트리키 타워 핵심 수정] "넘어지기 쉽게" 관성을 깎는 튜닝은 GetMomentOfInertia() 자체가 아니라
	// 여기서만 TOPPLE_INERTIA_SCALE로 곱한다 — 충돌 임펄스 계산과 공유하는 값을 깎으면 안 되는
	// 이유는 GetMomentOfInertia() 주석 참고.
	// [위에 얹힌 무게 반영] 평행축 항(m*거리^2)의 질량도 combinedMass로 바꿔서 토크 쪽과 일관되게
	// 맞춘다. GetMomentOfInertia() 자체(자기 회전 저항)는 이 블록 혼자만의 값을 그대로 쓴다 — 위에
	// 얹힌 블럭들의 자체 관성모멘트까지 재귀로 정확히 합산하는 건 훨씬 큰 변경이라 일부러 안 했다(근사).
	float distanceSquared = r.x * r.x + r.y * r.y;
	float inertiaAboutPivot = GetMomentOfInertia() * Constants::TOPPLE_INERTIA_SCALE + combinedMass * distanceSquared;

	float angularAccelerationRad = torqueAboutPivot / inertiaAboutPivot;
	m_angularVelocity += MathUtil::RadiansToDegrees(angularAccelerationRad) * deltaTime;
}

void Block::BeginToppling()
{
	// 속도/각속도를 인위적으로 바꾸지 않는다 — 그 순간의 실제 속도를 그대로 이어받아야 자연스럽다.
	// 여기서 바뀌는 건 상태뿐이다. 이후로는 순수하게 진짜 물리(중력 + 바닥/블럭 충돌)만으로 움직인다
	m_physicsState = PhysicsState::Toppling;
	m_hasToppled = true;

	// [강제 취침 타임아웃] 여기서부터가 진짜 새로운 불안정 사건의 시작이므로, 이전에 쌓여있던
	// 활동 시간과 섞이지 않도록 여기서 새로 잰다
	m_activeTimer = 0.0f;
	m_imbalanceTimer = 0.0f;
	m_isWedged = false;

	// [무한 재넘어짐 방지] Sleep() 없이 다시 여기로 왔다는 뜻이므로 1 늘린다. Sleep()에서 0으로 리셋된다.
	++m_consecutiveToppleCount;

	// [Sleep 직후 즉시 재불안정 반복 방지] 진짜로 넘어지기 시작했다는 건 확실히 새로운 사건이므로,
	// m_activeTimer와 같은 이유로 여기서 0으로 리셋한다.
	m_sleepReawakenCount = 0;

	// [Sleep->Awake 완충 — 연쇄 전파 감쇠] 스스로 무게중심이 무너져 넘어지기 시작한 것도 진짜 새로운
	// 에너지원이므로, 이전에 연쇄로 전달받았던 깊이는 여기서 끊는다.
	m_wakeChainDepth = 0;
}

int Block::GetConsecutiveToppleCount() const
{
	return m_consecutiveToppleCount;
}

void Block::ResetConsecutiveToppleCount()
{
	m_consecutiveToppleCount = 0;
}

int Block::GetSleepReawakenCount() const
{
	return m_sleepReawakenCount;
}

void Block::AdvanceSleepReawakenCount()
{
	++m_sleepReawakenCount;
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

void Block::DecayImbalanceTimer(float deltaTime)
{
	m_imbalanceTimer -= deltaTime;
	if (m_imbalanceTimer <= 0.0f)
	{
		// 진짜로 다 풀렸다 — 끼임 표시까지 포함해서 완전히 리셋한다.
		ResetImbalanceTimer();
	}
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
	// [강제 취침 타임아웃] m_activeTimer는 여기서 리셋하지 않는다 — Sleep()과 동일한 이유:
	// Sleep<->Wake를 반복하는 동안 m_activeTimer가 끊기지 않고 누적돼야 ForceSleepStuckBlocks가
	// 3초를 채울 수 있다.
	//
	// [지지 없이 얼어붙는 버그 수정 — rest timer는 반대로 여기서 리셋해야 한다] m_restTimer는
	// "지지대가 있고 저속인 상태가 얼마나 *끊기지 않고* 지속됐는지"를 재는 타이머라(m_restTimer
	// 선언부 주석: "조건이 깨지면(지지 소실/충돌/속도 초과) 즉시 0으로 리셋된다"), 깨어나는 것 자체가
	// 바로 그 "조건이 깨짐"이다. 예전엔 여기서 안 지워서, 충돌로 잠깐 깨어나도 그 결과 속도가
	// SLEEP_LINEAR_THRESHOLD보다만 낮으면 TrySleepAll이 예전에 쌓아둔 낡은 rest timer를 그대로 이어받아
	// 거의 같은 스텝에 곧바로 재취침시켰다. 그런데 위치/각도를 실제로 갱신하는 Integrate()는 Awake 상태로
	// "다음 스텝의 1단계"에 살아있을 때만 도므로, 이렇게 한 스텝 만에 도로 잠들면 방금 충돌로 받은
	// 속도/각속도가 반영될 기회 자체가 없어(Sleep()이 각속도를 0으로 지워버림) — 실제로는 매 프레임
	// Wake/Sleep을 반복하며 속도만 쌓였다 지워지길 반복할 뿐, 위치/각도는 전혀 안 바뀌어서 겉보기엔
	// 완전히 얼어붙은 것처럼 보였다(physics_debug.log로 실전 확인 — 같은 블록이 20스텝 동안 매번
	// WAKE 직후 SLEEP을 반복하는데 angle이 단 한 번도 안 바뀜). 깨어날 때마다 새로 0.3초(SLEEP_DELAY)를
	// 다시 채우게 해야, 그 사이 최소 한 번은 Integrate()가 돌아 실제로 움직일 기회를 갖는다.
	ResetRestTimer();

	// [Sleep->Awake 완충] 갓 깨어난 시점부터 다시 재므로, WakeUp()마다 0으로 리셋한다.
	m_wakeTimer = 0.0f;

	// [Sleep->Awake 완충 — 연쇄 전파 감쇠] 기본값은 "새로운 에너지원"인 0이다. 이 블록이 이미 연쇄로
	// 충격을 전달받은 다른 블록에게 맞아서 깨어나는 특수한 경우(PhysicsManager::ResolveBlockPairCollision)
	// 에만, WakeUp() 호출 바로 뒤에서 SetWakeChainDepth()로 이 기본값을 덮어써서 실제 연쇄 깊이를
	// 반영한다 — ResolveBalance의 "지지 소실" 경로처럼 연쇄와 무관하게 깨는 경우까지 여기서 항상
	// 0으로 리셋해둬야, 예전 Sleep 이전에 남아있던 낡은 깊이가 무관한 다음 각성에 잘못 새어 들어가는
	// 걸 막는다(그래야 Sleep()/BeginToppling()에서 따로 리셋할 필요도 없다 — 다음 WakeUp()이 항상 0부터 시작).
	m_wakeChainDepth = 0;

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
	// [오뚜기처럼 도로 일어서는 버그 수정] m_isWedged(ForceStabilize로 강제 정지된 경우)일 땐 이 스냅을
	// 건너뛴다 — ForceStabilize가 부르는 Sleep()은 "정상적으로 조용히 멈춤"이 아니라 "넘어지다 만
	// 진짜 물리적 각도에서 강제로 중단된 것"이라, 그 각도가 우연히 90도 배수 12도 이내(예: 9도)에
	// 있다는 이유만으로 0도로 되돌리면 안 된다 — 실제로 안 일어난 블록이 시각적으로만 벌떡 일어나 보인다.
	// [다른 블록의 지지 기반이 영구히 망가지는 버그 수정] 다만 무조건 건너뛰면 안 된다 — wedge 각도가
	// SUPPORT_SURFACE_MAX_TILT_DEGREES(5도) 이내면, 그건 눈에 거의 안 보일 만큼 작은 기울기라 스냅해도
	// "벌떡 일어나는" 것처럼 안 보이는데, 스냅을 안 하면 이 블록은 지지 판정(IsNearAxisAlignedAngle)
	// 기준으로 축 정렬을 영원히 살짝 못 넘겨서, 이 위에 뭘 얹어도 "공식적으로 지지 없음"이 되어 아무리
	// 모양이 잘 맞아도 계속 넘어가 버린다(실전 확인 — 겉보기엔 멀쩡해 보이는 발판인데 그 위에 얹은 블록이
	// 계속 넘어짐). 그래서 "지지 판정 기준으로도 이미 못 믿을 정도로" 기운 경우에만 스냅을 건너뛴다.
	float nearestRightAngle = std::round(m_angle / 90.0f) * 90.0f;
	float angleFromRightAngle = std::fabs(m_angle - nearestRightAngle);
	bool isVisiblyTiltedWedge = m_isWedged && angleFromRightAngle > Constants::SUPPORT_SURFACE_MAX_TILT_DEGREES;
	if (!isVisiblyTiltedWedge && angleFromRightAngle <= Constants::ANGLE_SNAP_TOLERANCE)
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

void Block::AdvanceWakeTimer(float deltaTime)
{
	m_wakeTimer += deltaTime;
}

float Block::GetWakeTimer() const
{
	return m_wakeTimer;
}

int Block::GetWakeChainDepth() const
{
	return m_wakeChainDepth;
}

void Block::SetWakeChainDepth(int depth)
{
	m_wakeChainDepth = depth;
}

void Block::Land()
{
	m_position = { m_gridX * Constants::SUBCELL_SIZE, m_gridY * Constants::SUBCELL_SIZE };
	m_physicsState = PhysicsState::Awake;
	m_activeTimer = 0.0f;
	m_imbalanceTimer = 0.0f;
	m_isWedged = false;
	m_restAngleReference = 0.0f;
	m_sleepReawakenCount = 0;
	m_wakeTimer = 0.0f;
	// [Sleep->Awake 완충 — 연쇄 전파 감쇠] Land()는 WakeUp()을 거치지 않고 직접 상태를 바꾸므로,
	// WakeUp()의 기본 리셋을 여기서도 똑같이 해줘야 한다 — 새로 착지한 블록은 항상 깊이 0인 새 에너지원이다.
	m_wakeChainDepth = 0;
}
