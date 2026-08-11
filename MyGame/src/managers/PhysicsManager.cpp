#include "pch.h"

#include "PhysicsManager.h"
#include "../objects/Block.h"
#include "../managers/BlockManager.h"
#include <algorithm>
#include <cmath>

PhysicsManager& PhysicsManager::GetInstance()
{
    static PhysicsManager instance;
    return instance;
}

PhysicsManager::~PhysicsManager() = default;

void PhysicsManager::Update(float deltaTime)
{
    // [고정 timestep] 렌더 프레임이 밀려서 deltaTime이 순간적으로 커져도(디버거로 멈췄다 풀거나 랙),
    // 그걸 그대로 누적하면 다음 프레임에 수십 스텝을 몰아서 처리하려다 오히려 더 느려진다
    // ("Spiral of Death") — 그래서 누적 전에 한 프레임 최대치를 잘라낸다.
    float clampedDeltaTime = std::min(deltaTime, Constants::PHYSICS_FIXED_TIMESTEP * Constants::PHYSICS_MAX_SUBSTEPS);
    m_accumulator += clampedDeltaTime;

    int substeps = 0;
    while (m_accumulator >= Constants::PHYSICS_FIXED_TIMESTEP && substeps < Constants::PHYSICS_MAX_SUBSTEPS)
    {
        Step(Constants::PHYSICS_FIXED_TIMESTEP);
        m_accumulator -= Constants::PHYSICS_FIXED_TIMESTEP;
        ++substeps;
    }
}

void PhysicsManager::Step(float deltaTime)
{
    // 1. 상태 업데이트 및 중력 적용
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        bool isAwake = block->GetPhysicsState() == PhysicsState::Awake;
        bool isToppling = block->GetPhysicsState() == PhysicsState::Toppling;

        if (!isAwake && !isToppling) continue;

        block->AdvanceActiveTimer(deltaTime);
        ApplyGravity(block);
        block->Integrate(deltaTime);
    }

    // 2. 충돌 해결 (Solver Iterations)
    for (int iteration = 0; iteration < Constants::COLLISION_SOLVER_ITERATIONS; ++iteration)
    {
        for (Block* block : BlockManager::GetInstance().GetAllBlocks())
        {
            if (block->GetPhysicsState() == PhysicsState::Awake || block->GetPhysicsState() == PhysicsState::Toppling)
            {
                ResolveFloorCollision(block);
            }
        }
        ResolveBlockCollisions();
    }

    // 2.5. [바닥 마찰 근사] 지지대가 있는 블럭은 매 프레임 속도를 감쇠시켜서 미끄러짐을 잡는다.
    // ResolveRigidCollision의 임펄스 마찰은 얹혀서 미끄러지는 동안(수직 속도 거의 0)엔 안 걸리기 때문에 필요하다
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        bool isActive = block->GetPhysicsState() == PhysicsState::Awake || block->GetPhysicsState() == PhysicsState::Toppling;
        if (!isActive)
        {
            continue;
        }

        bool hasSupport = false;
        for (int i = 0; i < block->GetCellCount() && !hasSupport; ++i)
        {
            if (IsCellSupported(block, i))
            {
                hasSupport = true;
            }
        }

        if (hasSupport)
        {
            block->DampVelocity(Constants::GROUNDED_VELOCITY_DAMPING);
            block->DampAngularVelocity(Constants::GROUNDED_ANGULAR_DAMPING);
        }
    }

    // 3. 밸런스 체크 (Awake + Sleeping 블록 전부)
    // [연쇄 붕괴 사각지대] Sleeping 블록은 충돌 접촉이 있어야만 다시 깨어난다.
    // 그 사이 주변 블록이 밀리거나 무게가 옮겨가면서 뒤늦게 불안정해질 수 있는데, 그런 경우 아무도
    // 다시 깨워주지 않으면 실제로는 무게중심이 지지 범위를 벗어났는데도 영원히 잠든 채로 방치된다.
    // [성능] "누가 누구 위에 얹혀 있는지"를 이 스텝에서 한 번만 계산해서 모든 블록의 ResolveBalance가
    // 재사용한다 — 예전엔 블록마다(그리고 그 안에서 재귀할 때마다) 전체 블록을 매번 다시 스캔해서,
    // 탑이 커질수록 비용이 세제곱에 가깝게 늘어났다.
    std::unordered_map<Block*, std::vector<Block*>> restingChildren = BuildRestingChildrenMap();
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        PhysicsState state = block->GetPhysicsState();
        if (state != PhysicsState::Awake && state != PhysicsState::Sleeping) continue;

        ResolveBalance(block, deltaTime, restingChildren);

        // ResolveBalance 외부적인 요인(충돌 등)으로 각도가 꺾인 경우의 안전장치
        if (block->GetPhysicsState() != PhysicsState::Toppling && std::fabs(block->GetAngle()) >= Constants::MAX_TOPPLE_ANGLE)
        {
            block->BeginToppling();
        }
    }

    RemoveToppledBlocks();
    SettleToppledBlocks();
    TrySleepAll(deltaTime);
}
void PhysicsManager::ApplyGravity(Block* block)
{
    // 질량과 무관하게 항상 같은 가속도(GRAVITY)로 떨어지게 하려고, 힘 = 질량 * 중력가속도로 건다
    // (Integrate가 힘을 다시 질량으로 나누므로 결국 가속도는 GRAVITY로 통일됨)
    block->ApplyForce({ 0.0f, block->GetMass() * Constants::GRAVITY });
}

void PhysicsManager::ResolveFloorCollision(Block* block)
{
    // [Contact Manifold] 칸마다 회전이 반영된 네 모서리를 전부 검사해서, 바닥에 닿은(파고든) 모서리를
    // 모아 "가장 깊이 파고든 정도"(위치 보정용)와 "가로로 가장 멀리 떨어진 두 점"(왼쪽 끝/오른쪽 끝,
    // 접촉점으로 사용)을 구한다. 예전엔 "가장 깊은 것 + 그다음 깊은 것"을 발견 순서대로 골랐는데,
    // I자처럼 셀이 여러 개 나란히 붙어 평평하게 누우면 닿은 모서리가 전부 같은 깊이라서 맨 처음 셀
    // 하나의 두 모서리(48px 간격)만 우연히 뽑히고 반대쪽 끝은 접촉점으로 안 잡혀 시소처럼 계속
    // 흔들렸다. 바닥 노멀은 항상 수평 위쪽 고정이라, 가로(X) 극값 두 개만 찾으면 충분하다.
    float deepestPenetration = -999999.0f;
    float leftmostX = 999999.0f;
    float rightmostX = -999999.0f;
    Vector2 leftmostCorner = { 0.0f, 0.0f };
    Vector2 rightmostCorner = { 0.0f, 0.0f };
    bool touchedAny = false;

    for (int i = 0; i < block->GetCellCount(); ++i)
    {
        Vector2 corners[4];
        block->GetCellRotatedCorners(i, corners);

        for (int c = 0; c < 4; ++c)
        {
            // 발판 옆은 빈 공간이라, y좌표가 바닥 높이여도 x좌표가 발판 범위 밖이면 충돌이 아니다
            bool isOverFloorPlatform = corners[c].x > Constants::FLOOR_LEFT_X && corners[c].x < Constants::FLOOR_RIGHT_X;
            if (!isOverFloorPlatform)
            {
                continue;
            }

            float penetration = corners[c].y - Constants::FLOOR_TOP_Y;
            if (penetration <= 0.0f)
            {
                continue; // 안 닿음
            }

            touchedAny = true;
            if (penetration > deepestPenetration)
            {
                deepestPenetration = penetration;
            }
            if (corners[c].x < leftmostX)
            {
                leftmostX = corners[c].x;
                leftmostCorner = corners[c];
            }
            if (corners[c].x > rightmostX)
            {
                rightmostX = corners[c].x;
                rightmostCorner = corners[c];
            }
        }
    }

    if (!touchedAny)
    {
        return;
    }

    // 바닥은 항상 수평이라 노멀(밀어내는 방향)은 늘 "위쪽" 고정.
    // 왼쪽 끝 점은 위치 보정 + 속도/토크 반응을 다 하고, 오른쪽 끝 점은 이미 밀어낸 뒤라
    // penetration을 0으로 넘겨서 위치는 안 건드리고 속도/토크 반응만 추가로 준다.
    // (닿은 모서리가 1개뿐인 코너 접촉이면 leftmostCorner==rightmostCorner가 되어 자연히 1점 충돌과 동일하게 동작한다)
    Vector2 floorNormal = { 0.0f, -1.0f };
    block->ResolveRigidCollision(leftmostCorner, floorNormal, deepestPenetration);
    block->ResolveRigidCollision(rightmostCorner, floorNormal, 0.0f);
}

void PhysicsManager::ResolveBlockPairCollision(Block* block, Block* other)
{
    // [강체물리 3단계] SAT로 "진짜" 겹침 여부/깊이/방향/접촉점을 구하고 강체 충돌로 반응한다.
    // 두 블럭 실제 칸(최대 4x4=16쌍)끼리 하나씩 검사 — 바운딩 박스만 보면 L자 같은 오목한 모양의 빈 홈을
    // "꽉 찬 것"처럼 취급해버려서 실제로는 안 겹치는데도 반응하는 문제가 생기기 때문.
    //
    // [Over-solving 방지] 예전엔 겹치는 칸 쌍을 찾을 때마다 그 자리에서 바로 충돌 반응을 적용했는데,
    // 두 블럭이 여러 칸에 걸쳐 동시에 닿으면(예: T자가 일자 위에 반듯하게 얹히는 경우 3칸이 한 번에 겹침)
    // 위치 보정/임펄스가 겹친 칸 개수만큼 중첩되어 실제보다 훨씬 세게 튕겨나가는 문제가 있었다.
    // 그래서 16쌍을 다 검사하되 반응은 적용하지 않고, 그중 "가장 깊이 겹친" 충돌 하나만 골라서
    // 블럭 쌍(block, other)당 딱 한 번만 반응한다.
    // [성능] 이 함수는 솔버 반복(최대 4회) x 물리 서브스텝(최대 5회)마다 "움직이는 쪽이 하나라도 있는"
    // 모든 블록 쌍에 대해 불린다. 실제 칸 모서리(회전 포함)를 계산해 16쌍 SAT 검사를 하기 전에, 두 블록
    // 원점이 서로 닿을 수 있는 범위(SUPPORT_BROADPHASE_MAX_BLOCK_EXTENT, IsCellSupported와 같은 이유로
    // 유도한 값) 밖이면 곧바로 건너뛴다 — 탑이 커질수록 대부분의 쌍이 여기서 걸러진다.
    Vector2 blockOrigin = block->GetRenderPosition();
    Vector2 otherOrigin = other->GetRenderPosition();
    if (std::fabs(blockOrigin.x - otherOrigin.x) > Constants::SUPPORT_BROADPHASE_MAX_BLOCK_EXTENT ||
        std::fabs(blockOrigin.y - otherOrigin.y) > Constants::SUPPORT_BROADPHASE_MAX_BLOCK_EXTENT)
    {
        return;
    }

    bool blockMovable = block->GetPhysicsState() == PhysicsState::Awake || block->GetPhysicsState() == PhysicsState::Toppling;
    bool otherMovable = other->GetPhysicsState() == PhysicsState::Awake || other->GetPhysicsState() == PhysicsState::Toppling;

    // [Contact Manifold] 겹친 셀 쌍을 전부 모아둔다. 넓은 면 접촉에서는 여러 셀 쌍이 동시에 겹치는데,
    // "가장 깊이 겹친 것" 하나만 쓰면 접촉면의 좌우 양 끝을 표현하지 못해 한쪽으로 치우친 토크가 생기고
    // 그게 미세 회전(떨림)으로 반복된다. 일단 전부 모아뒀다가 아래에서 같은 방향(노멀)끼리 병합한다.
    std::vector<CellCollisionResult> candidates;

    for (int i = 0; i < block->GetCellCount(); ++i)
    {
        Vector2 cornersA[4];
        block->GetCellRotatedCorners(i, cornersA);

        for (int j = 0; j < other->GetCellCount(); ++j)
        {
            Vector2 cornersB[4];
            other->GetCellRotatedCorners(j, cornersB);

            CellCollisionResult collision = TestCellCollision(cornersA, cornersB);
            if (collision.collided)
            {
                candidates.push_back(collision);
            }
        }
    }

    if (candidates.empty())
    {
        return;
    }

    // 가장 깊이 겹친 후보를 "대표 충돌"로 삼는다 — 위치 보정에 쓸 침투 깊이와, 아래 병합 기준이 될 노멀을 여기서 얻는다
    CellCollisionResult bestCollision = candidates[0];
    for (const CellCollisionResult& candidate : candidates)
    {
        if (candidate.penetration > bestCollision.penetration)
        {
            bestCollision = candidate;
        }
    }

    // [Contact Manifold] 대표 노멀과 방향이 비슷한(거의 같은 면에서 생긴) 후보들의 접촉점만 모아서,
    // 그중 접촉면을 따라(접선 방향으로) 가장 멀리 떨어진 두 점을 최종 접촉점 2개로 쓴다.
    // 후보가 1개뿐(점 하나짜리 접촉)이면 min/max가 같은 셀의 두 점이 되어 예전과 동일하게 동작한다.
    Vector2 tangent = { -bestCollision.normal.y, bestCollision.normal.x };
    float minProjection = 999999.0f;
    float maxProjection = -999999.0f;
    Vector2 manifoldPoints[2] = { bestCollision.contactPoints[0], bestCollision.contactPoints[1] };

    for (const CellCollisionResult& candidate : candidates)
    {
        float normalAlignment = candidate.normal.x * bestCollision.normal.x + candidate.normal.y * bestCollision.normal.y;
        if (normalAlignment < 0.9f)
        {
            continue; // 방향이 많이 다른 접촉(예: 옆면과 밑면이 동시에 겹친 코너 케이스)은 이 manifold에 안 섞는다
        }

        for (int p = 0; p < 2; ++p)
        {
            Vector2 point = candidate.contactPoints[p];
            float projection = point.x * tangent.x + point.y * tangent.y;

            if (projection < minProjection)
            {
                minProjection = projection;
                manifoldPoints[0] = point;
            }
            if (projection > maxProjection)
            {
                maxProjection = projection;
                manifoldPoints[1] = point;
            }
        }
    }

    bool blockIsRealImpact = block->GetSpeedSquared() > Constants::WAKE_IMPACT_SPEED_THRESHOLD * Constants::WAKE_IMPACT_SPEED_THRESHOLD;
    bool otherIsRealImpact = other->GetSpeedSquared() > Constants::WAKE_IMPACT_SPEED_THRESHOLD * Constants::WAKE_IMPACT_SPEED_THRESHOLD;

    if (blockMovable && blockIsRealImpact && other->GetPhysicsState() == PhysicsState::Sleeping)
    {
        other->WakeUp();
        otherMovable = true; // 이제 other도 움직일 수 있게 되었으므로 true로 바꿔줌
    }
    else if (otherMovable && otherIsRealImpact && block->GetPhysicsState() == PhysicsState::Sleeping)
    {
        block->WakeUp();
        blockMovable = true; // 이제 block도 움직일 수 있게 되었으므로 true로 바꿔줌
    }
    // [접촉 다각화] 가장 깊게 겹친 그 칸 쌍의 접촉점 2개에 대해서만 반응을 준다. 첫 번째 점만 위치
    // 보정을 하고(penetration 그대로), 두 번째 점은 이미 밀어낸 뒤라 penetration을 0으로 넘겨서
    // 속도/토크 반응만 추가로 준다 — 면과 면이 맞닿았을 때 양 끝을 동시에 붙잡아서 미세 회전(떨림)이 사라진다
    if (blockMovable && otherMovable)
    {
        // 둘 다 움직일 수 있으면 진짜 쌍방향 충돌
        block->ResolveRigidCollisionWithBlock(other, manifoldPoints[0], bestCollision.normal, bestCollision.penetration);
        block->ResolveRigidCollisionWithBlock(other, manifoldPoints[1], bestCollision.normal, 0.0f);
    }
    else if (blockMovable)
    {
        // other는 Sleeping(고정) — block만 밀려남. normal이 이미 "other->block" 방향이라 그대로 씀
        block->ResolveRigidCollision(manifoldPoints[0], bestCollision.normal, bestCollision.penetration);
        block->ResolveRigidCollision(manifoldPoints[1], bestCollision.normal, 0.0f);
    }
    else if (otherMovable)
    {
        // block이 Sleeping(고정) — other만 밀려남. other 입장에선 반대(block->other) 방향이 필요해서 뒤집는다
        other->ResolveRigidCollision(manifoldPoints[0], bestCollision.normal * -1.0f, bestCollision.penetration);
        other->ResolveRigidCollision(manifoldPoints[1], bestCollision.normal * -1.0f, 0.0f);
    }
}

void PhysicsManager::ResolveBlockCollisions()
{
    std::vector<Block*> allBlocks = BlockManager::GetInstance().GetAllBlocks();

    // [강체물리 3단계] 이제 충돌 반응이 양쪽 다 반응하는 진짜 쌍방향이라, 같은 쌍을 두 번 풀면
    // ((X,Y)와 (Y,X) 양쪽에서 각각) 임펄스를 두 번 주는 꼴이 되어버린다. 그래서 i<j로만 순회해서
    // 정확히 한 쌍당 한 번만 처리한다.
    for (size_t i = 0; i < allBlocks.size(); ++i)
    {
        Block* blockA = allBlocks[i];
        bool aIsSolid = blockA->GetPhysicsState() == PhysicsState::Awake ||
            blockA->GetPhysicsState() == PhysicsState::Sleeping ||
            blockA->GetPhysicsState() == PhysicsState::Toppling;
        if (!aIsSolid)
        {
            continue;
        }

        for (size_t j = i + 1; j < allBlocks.size(); ++j)
        {
            Block* blockB = allBlocks[j];
            bool bIsSolid = blockB->GetPhysicsState() == PhysicsState::Awake ||
                blockB->GetPhysicsState() == PhysicsState::Sleeping ||
                blockB->GetPhysicsState() == PhysicsState::Toppling;
            bool blockAMovable = blockA->GetPhysicsState() == PhysicsState::Awake || blockA->GetPhysicsState() == PhysicsState::Toppling;
            bool blockBMovable = blockB->GetPhysicsState() == PhysicsState::Awake || blockB->GetPhysicsState() == PhysicsState::Toppling;
            bool eitherMovable = blockAMovable || blockBMovable;

            // 둘 다 안 움직이는 상태(Sleeping끼리)면 부딪혀도 반응할 게 없으니 건너뜀
            if (!bIsSolid || !eitherMovable)
            {
                continue;
            }

            ResolveBlockPairCollision(blockA, blockB);
        }
    }
}

CellCollisionResult PhysicsManager::TestCellCollision(const Vector2 cornersA[4], const Vector2 cornersB[4]) const
{
    // SAT(분리축 정리): 두 볼록 도형이 안 겹친다면, 두 도형의 변 중 적어도 하나에 수직인 방향(축)으로
    // 투영했을 때 두 도형의 투영 구간이 겹치지 않는 축이 반드시 존재한다.
    // 반대로 모든 후보 축에서 투영이 겹친다면 두 도형은 실제로 겹쳐 있다는 뜻이다.
    // 사각형은 마주보는 변끼리 평행이라 축은 실질적으로 2개씩(총 4개)이면 충분하지만,
    // 코드를 단순하게 유지하려고 그냥 8개(양쪽 다 4변씩) 다 검사한다.
    CellCollisionResult result;
    float smallestOverlap = 999999.0f;
    Vector2 smallestAxis = { 0.0f, 0.0f };

    for (int shapeIndex = 0; shapeIndex < 2; ++shapeIndex)
    {
        const Vector2* corners = (shapeIndex == 0) ? cornersA : cornersB;

        for (int edgeIndex = 0; edgeIndex < 4; ++edgeIndex)
        {
            Vector2 edgeStart = corners[edgeIndex];
            Vector2 edgeEnd = corners[(edgeIndex + 1) % 4];
            Vector2 edge = edgeEnd - edgeStart;

            // 변을 90도 돌리면 그 변에 수직인 방향(분리축 후보)이 된다
            Vector2 axis = { edge.y, -edge.x };
            float axisLength = std::sqrt(axis.x * axis.x + axis.y * axis.y);
            if (axisLength < 0.0001f)
            {
                continue;
            }
            axis = axis * (1.0f / axisLength);

            float minA, maxA, minB, maxB;
            ProjectCornersOntoAxis(cornersA, axis, minA, maxA);
            ProjectCornersOntoAxis(cornersB, axis, minB, maxB);

            float overlapEnd = maxA;
            if (maxB < overlapEnd) overlapEnd = maxB;
            float overlapStart = minA;
            if (minB > overlapStart) overlapStart = minB;
            float overlap = overlapEnd - overlapStart;

            if (overlap <= 0.0f)
            {
                // 이 축에서 안 겹친다 = 분리축 발견 = 두 사각형은 충돌하지 않는다 (result.collided는 기본값 false)
                return result;
            }

            if (overlap < smallestOverlap)
            {
                smallestOverlap = overlap;
                smallestAxis = axis;
            }
        }
    }

    // 모든 축에서 겹쳤다 = 충돌. 겹침이 가장 작은 축이 "밀어내야 할 방향"(최소 이동 벡터)이 된다
    result.collided = true;
    result.penetration = smallestOverlap;

    // normal은 항상 "B에서 A를 향하는 방향"으로 통일한다 (Block::ResolveRigidCollision 계열이 이 규약을 전제로 함)
    Vector2 centerA = ComputeQuadCenter(cornersA);
    Vector2 centerB = ComputeQuadCenter(cornersB);
    Vector2 aFromB = centerA - centerB;
    float dot = aFromB.x * smallestAxis.x + aFromB.y * smallestAxis.y;
    result.normal = (dot < 0.0f) ? smallestAxis * -1.0f : smallestAxis;

    // [접촉 다각화] B의 꼭짓점 4개를 normal 방향(=A가 있는 쪽)으로 얼마나 깊이 들어갔는지 기준으로 정렬해서,
    // 가장 깊은 2개를 접촉점으로 삼는다. 면과 면이 맞닿은 경우 이 2개가 정확히 겹치는 변의 양 끝이 되고,
    // 꼭짓점 하나만 닿는 경우엔 자연히 2개가 (거의) 같은 자리를 가리키게 된다
    float bestProjection = -999999.0f;
    float secondProjection = -999999.0f;
    int bestIndex = 0;
    int secondIndex = 0;

    for (int i = 0; i < 4; ++i)
    {
        float projection = cornersB[i].x * result.normal.x + cornersB[i].y * result.normal.y;
        if (projection > bestProjection)
        {
            secondProjection = bestProjection;
            secondIndex = bestIndex;
            bestProjection = projection;
            bestIndex = i;
        }
        else if (projection > secondProjection)
        {
            secondProjection = projection;
            secondIndex = i;
        }
    }

    result.contactPoints[0] = cornersB[bestIndex];
    result.contactPoints[1] = cornersB[secondIndex];

    return result;
}

void PhysicsManager::ProjectCornersOntoAxis(const Vector2 corners[4], Vector2 axis, float& outMin, float& outMax) const
{
    outMin = corners[0].x * axis.x + corners[0].y * axis.y;
    outMax = outMin;
    for (int i = 1; i < 4; ++i)
    {
        float projection = corners[i].x * axis.x + corners[i].y * axis.y;
        if (projection < outMin) outMin = projection;
        if (projection > outMax) outMax = projection;
    }
}

Vector2 PhysicsManager::ComputeQuadCenter(const Vector2 corners[4]) const
{
    Vector2 sum = { 0.0f, 0.0f };
    for (int i = 0; i < 4; ++i)
    {
        sum += corners[i];
    }
    return sum * 0.25f;
}
void PhysicsManager::GetCellSupportBounds(Block* block, int cellIndex, float& outTopY, float& outBottomY, float& outMinX, float& outMaxX) const
{
    Vector2 corners[4];
    block->GetCellRotatedCorners(cellIndex, corners);

    outTopY = corners[0].y;
    outBottomY = corners[0].y;
    outMinX = corners[0].x;
    outMaxX = corners[0].x;

    for (int i = 1; i < 4; ++i)
    {
        if (corners[i].y < outTopY) outTopY = corners[i].y;
        if (corners[i].y > outBottomY) outBottomY = corners[i].y;
        if (corners[i].x < outMinX) outMinX = corners[i].x;
        if (corners[i].x > outMaxX) outMaxX = corners[i].x;
    }
}
bool PhysicsManager::IsCellSupported(Block* block, int cellIndex) const
{
    float cellTopY = 0.0f;
    float cellBottomY = 0.0f;
    float cellMinX = 0.0f;
    float cellMaxX = 0.0f;
    GetCellSupportBounds(block, cellIndex, cellTopY, cellBottomY, cellMinX, cellMaxX);
    float cellCenterX = (cellMinX + cellMaxX) * 0.5f;

    // 칸이 조금이라도 걸치는지가 아니라, 칸의 중심(cellCenterX)이 실제로 그 위에 있는지를 본다
    // (=칸의 절반 넘게 걸쳐야 지지된다고 인정) — 살짝만 걸쳐도 지지된다고 치면 지지 범위가 실제보다
    // 넓게 잡혀서 명백히 넘어져야 할 상황에서도 안 넘어지게 된다
    bool isNearFloorHeight = cellBottomY >= Constants::FLOOR_TOP_Y - Constants::SUPPORT_CHECK_TOLERANCE &&
        cellBottomY <= Constants::FLOOR_TOP_Y + Constants::SUPPORT_CHECK_TOLERANCE;
    bool isOverFloorPlatform = cellCenterX > Constants::FLOOR_LEFT_X && cellCenterX < Constants::FLOOR_RIGHT_X;
    if (isNearFloorHeight && isOverFloorPlatform)
    {
        return true;
    }

    for (Block* other : BlockManager::GetInstance().GetAllBlocks())
    {
        // Toppling(무너져서 낙하 중)인 블럭은 그 자체가 안 안정적인 상태라, 다른 블럭이 그 위에
        // 안정적으로 얹혀 있다고 볼 수 없다 — 지지 제공자로 인정하지 않는다
        bool otherCanSupport = other != block &&
            other->GetPhysicsState() != PhysicsState::Airborne &&
            other->GetPhysicsState() != PhysicsState::Toppling;
        if (!otherCanSupport)
        {
            continue;
        }

        // [성능] 실제 모서리(회전 반영, sin/cos 포함) 계산은 비싸다. 블록 원점 Y가 이 칸의 바닥과
        // SUPPORT_BROADPHASE_MAX_BLOCK_EXTENT보다 멀리 떨어져 있으면, 그 블록의 어떤 칸도 이 칸에
        // 닿을 수 없다는 뜻이니 모서리 계산 없이 곧바로 건너뛴다.
        if (std::fabs(other->GetRenderPosition().y - cellBottomY) > Constants::SUPPORT_BROADPHASE_MAX_BLOCK_EXTENT)
        {
            continue;
        }

        for (int j = 0; j < other->GetCellCount(); ++j)
        {
            float otherTopY = 0.0f, otherBottomY = 0.0f, otherMinX = 0.0f, otherMaxX = 0.0f;
            GetCellSupportBounds(other, j, otherTopY, otherBottomY, otherMinX, otherMaxX);

            // 내 칸의 바닥이 맞닿아야 하는 건 상대 칸의 "바닥"이 아니라 "윗면"이다 — 반대로 비교하면
            // 다른 블록 위에 쌓인 블록은 지지 판정이 늘 실패해서 절대 Sleep에 못 들어간다
            bool centerOverlapsOther = cellCenterX > otherMinX && cellCenterX < otherMaxX;
            bool isRestingOnOther = cellBottomY >= otherTopY - Constants::SUPPORT_CHECK_TOLERANCE &&
                cellBottomY <= otherTopY + Constants::SUPPORT_CHECK_TOLERANCE;

            if (centerOverlapsOther && isRestingOnOther)
            {
                return true;
            }
        }
    }

    return false;
}

bool PhysicsManager::RestsOnBlock(Block* upper, Block* lower) const
{
    // [성능] BuildRestingChildrenMap이 모든 블록 쌍(n²)에 대해 이 함수를 부르므로, 명백히 멀리 떨어진
    // 쌍은 칸 단위 모서리 계산 없이 블록 원점 Y거리만으로 먼저 걸러낸다 (IsCellSupported와 같은 이유)
    if (std::fabs(upper->GetRenderPosition().y - lower->GetRenderPosition().y) > Constants::SUPPORT_BROADPHASE_MAX_BLOCK_EXTENT)
    {
        return false;
    }

    for (int i = 0; i < upper->GetCellCount(); ++i)
    {
        float cellTopY = 0.0f, cellBottomY = 0.0f, cellMinX = 0.0f, cellMaxX = 0.0f;
        GetCellSupportBounds(upper, i, cellTopY, cellBottomY, cellMinX, cellMaxX);
        float cellCenterX = (cellMinX + cellMaxX) * 0.5f;

        for (int j = 0; j < lower->GetCellCount(); ++j)
        {
            float lowerTopY = 0.0f, lowerBottomY = 0.0f, lowerMinX = 0.0f, lowerMaxX = 0.0f;
            GetCellSupportBounds(lower, j, lowerTopY, lowerBottomY, lowerMinX, lowerMaxX);

            // 여기도 마찬가지로 upper의 바닥은 lower의 "윗면"과 비교해야 한다
            bool centerOverlapsLower = cellCenterX > lowerMinX && cellCenterX < lowerMaxX;
            bool isRestingOnLower = cellBottomY >= lowerTopY - Constants::SUPPORT_CHECK_TOLERANCE &&
                cellBottomY <= lowerTopY + Constants::SUPPORT_CHECK_TOLERANCE;

            if (centerOverlapsLower && isRestingOnLower)
            {
                return true;
            }
        }
    }

    return false;
}

std::unordered_map<Block*, std::vector<Block*>> PhysicsManager::BuildRestingChildrenMap() const
{
    std::unordered_map<Block*, std::vector<Block*>> childrenOf;
    std::vector<Block*> allBlocks = BlockManager::GetInstance().GetAllBlocks();

    for (Block* child : allBlocks)
    {
        // Airborne 블럭은 애초에 아무 위에도 "얹혀" 있는 상태가 아니라 제외한다.
        // Toppling은 여기서 안 거른다 — 재귀 도중 BeginToppling으로 상태가 바뀔 수 있어서,
        // 그 필터는 AccumulateSupportedMass가 순회할 때 그 시점의 상태로 판단해야 한다.
        if (child->GetPhysicsState() == PhysicsState::Airborne)
        {
            continue;
        }

        for (Block* base : allBlocks)
        {
            if (base != child && RestsOnBlock(child, base))
            {
                childrenOf[base].push_back(child);
            }
        }
    }

    return childrenOf;
}

void PhysicsManager::AccumulateSupportedMass(Block* base, std::vector<Block*>& visited,
    const std::unordered_map<Block*, std::vector<Block*>>& childrenOf,
    float& outTotalMass, float& outWeightedX) const
{
    outTotalMass += base->GetMass();
    outWeightedX += base->GetMass() * (base->GetRenderPosition() + base->GetCenterOfMassLocal() * Constants::TILE_SIZE).x;

    auto it = childrenOf.find(base);
    if (it == childrenOf.end())
    {
        return;
    }

    for (Block* child : it->second)
    {
        bool alreadyVisited = std::find(visited.begin(), visited.end(), child) != visited.end();
        // Toppling(이미 무너지는 중)인 블럭은 더 이상 자기 무게를 아래로 전달하지 않는다고 취급 —
        // 넘어지고 있는 조각까지 계속 하중으로 잡으면, 그 조각이 떨어져 나가는 동안 아래쪽이 계속 불안정하다고 오판한다
        bool canRestOnBase = !alreadyVisited && child->GetPhysicsState() != PhysicsState::Toppling;

        if (canRestOnBase)
        {
            visited.push_back(child);
            AccumulateSupportedMass(child, visited, childrenOf, outTotalMass, outWeightedX);
        }
    }
}

bool PhysicsManager::ComputeSupportDebugInfo(Block* block, const std::unordered_map<Block*, std::vector<Block*>>& childrenOf, float& outMinX, float& outMaxX, float& outCombinedComX) const
{
    bool hasSupport = false;

    for (int i = 0; i < block->GetCellCount(); ++i)
    {
        if (!IsCellSupported(block, i))
        {
            continue;
        }

        float cellTopY = 0.0f, cellBottomY = 0.0f, cellMinX = 0.0f, cellMaxX = 0.0f;
        GetCellSupportBounds(block, i, cellTopY, cellBottomY, cellMinX, cellMaxX);

        if (!hasSupport)
        {
            outMinX = cellMinX;
            outMaxX = cellMaxX;
            hasSupport = true;
        }
        else
        {
            if (cellMinX < outMinX) outMinX = cellMinX;
            if (cellMaxX > outMaxX) outMaxX = cellMaxX;
        }
    }

    if (!hasSupport)
    {
        return false;
    }

    // [연쇄 붕괴] block 자신의 무게중심이 아니라, block이 떠받치고 있는 전체(자신 + 위에 얹힌 모든 블럭)의
    // 결합 무게중심을 block 자신의 지지 범위와 비교한다. 이래야 위에 삐딱하게 얹힌 블럭 하나 때문에
    // 아래 블럭까지 실제로 넘어져야 하는 상황이 정확히 반영된다.
    std::vector<Block*> visited = { block };
    float totalMass = 0.0f;
    float weightedX = 0.0f;
    AccumulateSupportedMass(block, visited, childrenOf, totalMass, weightedX);
    outCombinedComX = weightedX / totalMass;
    return true;
}

bool PhysicsManager::ComputeSupportDebugInfo(Block* block, float& outMinX, float& outMaxX, float& outCombinedComX) const
{
    // [디버그 전용] F1 오버레이에서 블록 하나씩 개별 호출되는 드문 경로라, 매번 새로 계산해도 괜찮다.
    // 매 프레임 도는 물리 스텝(ResolveBalance)은 이 버전이 아니라 아래의 childrenOf를 받는 버전을 쓴다.
    std::unordered_map<Block*, std::vector<Block*>> childrenOf = BuildRestingChildrenMap();
    return ComputeSupportDebugInfo(block, childrenOf, outMinX, outMaxX, outCombinedComX);
}

void PhysicsManager::ResolveBalance(Block* block, float deltaTime, const std::unordered_map<Block*, std::vector<Block*>>& childrenOf)
{
    float supportMinX = 0.0f;
    float supportMaxX = 0.0f;
    float centerOfMassX = 0.0f;

    if (!ComputeSupportDebugInfo(block, childrenOf, supportMinX, supportMaxX, centerOfMassX))
    {
		if (block->GetPhysicsState() == PhysicsState::Sleeping)
		{
			block->WakeUp();
		}
        
        return;
    }

    // [트리키 타워 붕괴 판정]
    // 서서히 각도를 기울이는 기존 ApplyBalanceTorque 방식 대신,
    // 무게중심이 지지대를 벗어나는 '즉시' Toppling 상태로 만들고 강한 회전력을 줍니다.
    // IMBALANCE_DEADZONE만큼은 그냥 넘어가서, 부동소수점/물리 계산 오차 수준의 미세한 초과로는
    // 안정적인 구조까지 하드 스핀이 걸려 떨리며 무너지는 일이 없게 한다.
    if (centerOfMassX < supportMinX - Constants::IMBALANCE_DEADZONE || centerOfMassX > supportMaxX + Constants::IMBALANCE_DEADZONE)
    {
        // 무게중심이 빠진 방향으로 순간적인 각속도 부여 (휙! 넘어감)
        // Constants::TUMBLE_ANGULAR_VELOCITY로 세기를 조절할 수 있다.
        // [화면 좌표계(Y 아래로 증가) 기준] RotateLocalPointToWorld의 회전 공식으로는 양수 각도가
        // 시계방향이라, 무게중심이 오른쪽으로 벗어났을 때(오른쪽으로 넘어가야 함) 양수를 줘야
        // 오른쪽이 아래로 내려가는 방향으로 돈다. 반대로 왼쪽으로 벗어났으면 음수(반시계).
        float tumbleSpin = (centerOfMassX < supportMinX) ? -Constants::TUMBLE_ANGULAR_VELOCITY : Constants::TUMBLE_ANGULAR_VELOCITY;

        // [넘어짐 피벗 고정] 무게중심이 벗어난 방향(넘어가는 방향)의 반대쪽, 즉 지지 범위에서 그 방향의
        // 가장자리를 축으로 삼는다. 그 가장자리를 실제로 담당하는 지지 칸을 찾아서 그 칸의 바닥 Y를
        // 피벗의 Y로 쓴다 — 이걸 SetTopplePivot에 넘겨서, 넘어가기 시작한 직후 잠깐은 이 모서리가
        // 허공으로 붕 뜨지 않고 그 자리에 고정된 채로 회전하게 한다.
        float pivotX = (tumbleSpin > 0.0f) ? supportMaxX : supportMinX;
        float pivotY = Constants::FLOOR_TOP_Y;
        for (int i = 0; i < block->GetCellCount(); ++i)
        {
            if (!IsCellSupported(block, i))
            {
                continue;
            }
            float cellTopY = 0.0f, cellBottomY = 0.0f, cellMinX = 0.0f, cellMaxX = 0.0f;
            GetCellSupportBounds(block, i, cellTopY, cellBottomY, cellMinX, cellMaxX);
            float edgeX = (tumbleSpin > 0.0f) ? cellMaxX : cellMinX;
            if (std::fabs(edgeX - pivotX) < 0.01f)
            {
                pivotY = cellBottomY;
                break;
            }
        }

        // [연쇄 붕괴 전파] block 하나만 넘어뜨린다. 예전엔 block이 떠받치던 덩어리 전체에 똑같은 각속도를
        // 강제로 줬는데, 블록마다 무게중심/관성모멘트가 달라서 같은 각속도를 받아도 다르게 움직여야
        // 정상이다 — 억지로 맞추려니 서로 침투했다 밀려나며 새 떨림이 생겼다. 이제는 위에 얹힌 블록들을
        // 직접 건드리지 않는다: IsCellSupported가 Toppling 블록을 지지 제공자로 인정하지 않으므로, 다음
        // 물리 스텝에 그 블록들이 각자 자기 지지 판정에서 "지지를 잃었다"를 스스로 감지해 개별적으로
        // 깨어나고(Sleeping->Awake) 자연스러운 충돌로 무너진다.
        block->BeginToppling();
        block->AddAngularVelocity(tumbleSpin);
        block->SetTopplePivot({ pivotX, pivotY });
    }
}

bool PhysicsManager::CheckGlobalStability() const
{
    // Toppling(무너져서 이탈 중)인 블럭은 일부러 검사하지 않는다 — 무너지는 조각 하나 때문에 이미
    // 안정적인 나머지 블럭들까지 계속 깨어있게 붙잡아두면, 그 사이 다른 곳에서도 자잘한 흔들림이
    // 누적될 기회가 생겨서 "그 부분만 무너짐"이 아니라 "탑 전체가 무너짐"처럼 보이게 된다
    // Awake 블럭 중 하나라도 너무 빠르게 움직이거나(직선) 너무 빠르게 돌면(회전) 불안정으로 판단
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        if (block->GetPhysicsState() != PhysicsState::Awake)
        {
            continue;
        }

        bool tooFast = block->GetSpeedSquared() > Constants::UNSTABLE_SPEED_THRESHOLD * Constants::UNSTABLE_SPEED_THRESHOLD;
        bool spinningTooFast = std::fabs(block->GetAngularVelocity()) > Constants::UNSTABLE_ANGULAR_SPEED_THRESHOLD;

        if (tooFast || spinningTooFast)
        {
            return false;
        }

        // [자유낙하 오판 방지] 막 Land()됐거나 지지를 잃은 블럭은 중력을 받기 시작한 첫 프레임엔
        // 속도가 거의 0이라(가속이 막 시작됐으니) 위 속도 검사를 통과해버린다. 지지대가 전혀 없는데도
        // "안정적"이라고 오판해서 재워버리면, Sleeping은 중력 계산에서 아예 빠지니까 그 자리에
        // 영원히 멈춰(공중에 뜬 채로) 버린다 — 지지대가 없으면 무조건 아직 불안정한 것으로 본다
        bool hasSupport = false;
        for (int i = 0; i < block->GetCellCount() && !hasSupport; ++i)
        {
            if (IsCellSupported(block, i))
            {
                hasSupport = true;
            }
        }

        if (!hasSupport)
        {
            return false;
        }
    }

    return true;
}

void PhysicsManager::RemoveToppledBlocks()
{
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        if (block->GetPhysicsState() != PhysicsState::Toppling)
        {
            continue;
        }

        // 블록이 화면 아래(바닥)로 완전히 벗어났는지 체크
        bool isBelowWindow = block->GetRenderPosition().y > Constants::WINDOW_HEIGHT;

        // [원인 제거]
        // 순간적으로 마찰에 걸려 속도가 0이 되었을 때 삭제되는 일이 없도록
        // hasStoppedMoving 조건을 과감히 지웁니다.

        if (isBelowWindow)
        {
            BlockManager::GetInstance().RemoveBlock(block);
        }
    }
}
void PhysicsManager::SettleToppledBlocks()
{
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        if (block->GetPhysicsState() != PhysicsState::Toppling)
        {
            continue;
        }

        // 화면 밖으로 떨어지는 중이면 계속 Toppling으로 둬야 RemoveToppledBlocks가 나중에 치운다
        bool isBelowWindow = block->GetRenderPosition().y > Constants::WINDOW_HEIGHT;
        if (isBelowWindow)
        {
            continue;
        }

        bool tooFast = block->GetSpeedSquared() > Constants::UNSTABLE_SPEED_THRESHOLD * Constants::UNSTABLE_SPEED_THRESHOLD;
        bool spinningTooFast = std::fabs(block->GetAngularVelocity()) > Constants::UNSTABLE_ANGULAR_SPEED_THRESHOLD;
        if (tooFast || spinningTooFast)
        {
            continue;
        }

        // 속도가 느려도 포물선 꼭대기(수직 속도가 잠깐 0에 가까워지는 순간)에 우연히 걸릴 수 있어서,
        // 실제로 바닥/다른 블럭에 닿아 있는지(지지 여부)까지 확인해야 공중에서 얼어붙는 걸 막을 수 있다
        bool hasSupport = false;
        for (int i = 0; i < block->GetCellCount() && !hasSupport; ++i)
        {
            if (IsCellSupported(block, i))
            {
                hasSupport = true;
            }
        }

        if (hasSupport)
        {
            block->Sleep();
        }
    }
}

void PhysicsManager::TrySleepAll(float deltaTime)
{
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        if (block->GetPhysicsState() != PhysicsState::Awake)
        {
            continue;
        }

        bool tooFast = block->GetSpeedSquared() > Constants::SLEEP_LINEAR_THRESHOLD * Constants::SLEEP_LINEAR_THRESHOLD;
        bool spinningTooFast = std::fabs(block->GetAngularVelocity()) > Constants::SLEEP_ANGULAR_THRESHOLD;

        bool hasSupport = false;
        for (int i = 0; i < block->GetCellCount() && !hasSupport; ++i)
        {
            if (IsCellSupported(block, i))
            {
                hasSupport = true;
            }
        }

        bool canRest = hasSupport && !tooFast && !spinningTooFast;

        if (!canRest)
        {
            block->ResetRestTimer();
            continue;
        }

        block->AdvanceRestTimer(deltaTime);

        if (block->GetRestTimer() >= Constants::SLEEP_DELAY)
        {
            block->Sleep();
        }
    }
}

