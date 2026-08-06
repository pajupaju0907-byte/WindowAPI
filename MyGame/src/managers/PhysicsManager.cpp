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
            if (IsCellSupported(block->GetCellRenderPosition(i), block))
            {
                hasSupport = true;
            }
        }

        if (hasSupport)
        {
            block->DampVelocity(Constants::GROUNDED_VELOCITY_DAMPING);
        }
    }

    // 3. 밸런스 체크 (Awake + Sleeping 블록 전부)
    // [연쇄 붕괴 사각지대] Sleeping 블록은 WakeAll(새 블록 착지) 또는 충돌 접촉이 있어야만 다시 깨어난다.
    // 그 사이 주변 블록이 밀리거나 무게가 옮겨가면서 뒤늦게 불안정해질 수 있는데, 그런 경우 아무도
    // 다시 깨워주지 않으면 실제로는 무게중심이 지지 범위를 벗어났는데도 영원히 잠든 채로 방치된다.
    // 블록 수가 적어 매 프레임 전부 검사해도 비용 부담은 없다.
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        PhysicsState state = block->GetPhysicsState();
        if (state != PhysicsState::Awake && state != PhysicsState::Sleeping) continue;

        ResolveBalance(block, deltaTime);

        // ResolveBalance 외부적인 요인(충돌 등)으로 각도가 꺾인 경우의 안전장치
        if (block->GetPhysicsState() != PhysicsState::Toppling && std::fabs(block->GetAngle()) >= Constants::MAX_TOPPLE_ANGLE)
        {
            block->BeginToppling();
        }
    }

    RemoveToppledBlocks();
    SettleToppledBlocks();
    ForceSleepStuckBlocks();
    TrySleepAll();
}
void PhysicsManager::ApplyGravity(Block* block)
{
    // 질량과 무관하게 항상 같은 가속도(GRAVITY)로 떨어지게 하려고, 힘 = 질량 * 중력가속도로 건다
    // (Integrate가 힘을 다시 질량으로 나누므로 결국 가속도는 GRAVITY로 통일됨)
    block->ApplyForce({ 0.0f, block->GetMass() * Constants::GRAVITY });
}

void PhysicsManager::ResolveFloorCollision(Block* block)
{
    // [접촉 다각화] 칸마다 회전이 반영된 네 모서리를 전부 검사해서, 바닥을 파고든 모서리 중 가장 깊은
    // 2개(Contact Manifold)를 찾는다. 하나만 쓰면 평평하게 앉은 블럭도 바늘 위에 선 것처럼 계산되어
    // 계속 미세하게 회전하며 불안정해지기 때문. 안 기울어져 있으면 자연히 "아래쪽 두 모서리"가 뽑힌다.
    Vector2 deepestCorner = { 0.0f, 0.0f };
    Vector2 secondCorner = { 0.0f, 0.0f };
    float deepestPenetration = -999999.0f;
    float secondPenetration = -999999.0f;

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
            if (penetration > deepestPenetration)
            {
                secondPenetration = deepestPenetration;
                secondCorner = deepestCorner;
                deepestPenetration = penetration;
                deepestCorner = corners[c];
            }
            else if (penetration > secondPenetration)
            {
                secondPenetration = penetration;
                secondCorner = corners[c];
            }
        }
    }

    if (deepestPenetration > 0.0f)
    {
        // 바닥은 항상 수평이라 노멀(밀어내는 방향)은 늘 "위쪽" 고정.
        // 첫 번째 점은 위치 보정 + 속도/토크 반응을 다 하고, 두 번째 점은 이미 밀어낸 뒤라
        // penetration을 0으로 넘겨서 위치는 안 건드리고 속도/토크 반응만 추가로 준다
        Vector2 floorNormal = { 0.0f, -1.0f };
        block->ResolveRigidCollision(deepestCorner, floorNormal, deepestPenetration);

        if (secondPenetration > 0.0f)
        {
            block->ResolveRigidCollision(secondCorner, floorNormal, 0.0f);
        }
    }
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
    bool blockMovable = block->GetPhysicsState() == PhysicsState::Awake || block->GetPhysicsState() == PhysicsState::Toppling;
    bool otherMovable = other->GetPhysicsState() == PhysicsState::Awake || other->GetPhysicsState() == PhysicsState::Toppling;

    CellCollisionResult bestCollision;
    bestCollision.penetration = -999999.0f;

    for (int i = 0; i < block->GetCellCount(); ++i)
    {
        Vector2 cornersA[4];
        block->GetCellRotatedCorners(i, cornersA);

        for (int j = 0; j < other->GetCellCount(); ++j)
        {
            Vector2 cornersB[4];
            other->GetCellRotatedCorners(j, cornersB);

            CellCollisionResult collision = TestCellCollision(cornersA, cornersB);
            if (collision.collided && collision.penetration > bestCollision.penetration)
            {
                bestCollision = collision;
            }
        }
    }

    if (!bestCollision.collided)
    {
        return;
    }
    if (blockMovable && other->GetPhysicsState() == PhysicsState::Sleeping)
    {
        other->WakeUp();
        otherMovable = true; // 이제 other도 움직일 수 있게 되었으므로 true로 바꿔줌
    }
    else if (otherMovable && block->GetPhysicsState() == PhysicsState::Sleeping)
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
        block->ResolveRigidCollisionWithBlock(other, bestCollision.contactPoints[0], bestCollision.normal, bestCollision.penetration);
        block->ResolveRigidCollisionWithBlock(other, bestCollision.contactPoints[1], bestCollision.normal, 0.0f);
    }
    else if (blockMovable)
    {
        // other는 Sleeping(고정) — block만 밀려남. normal이 이미 "other->block" 방향이라 그대로 씀
        block->ResolveRigidCollision(bestCollision.contactPoints[0], bestCollision.normal, bestCollision.penetration);
        block->ResolveRigidCollision(bestCollision.contactPoints[1], bestCollision.normal, 0.0f);
    }
    else if (otherMovable)
    {
        // block이 Sleeping(고정) — other만 밀려남. other 입장에선 반대(block->other) 방향이 필요해서 뒤집는다
        other->ResolveRigidCollision(bestCollision.contactPoints[0], bestCollision.normal * -1.0f, bestCollision.penetration);
        other->ResolveRigidCollision(bestCollision.contactPoints[1], bestCollision.normal * -1.0f, 0.0f);
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

bool PhysicsManager::IsCellSupported(Vector2 cellPosition, Block* self) const
{
    float cellBottomY = cellPosition.y + Constants::TILE_SIZE;
    float cellCenterX = cellPosition.x + Constants::TILE_SIZE * 0.5f;

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
        bool otherCanSupport = other != self &&
            other->GetPhysicsState() != PhysicsState::Airborne &&
            other->GetPhysicsState() != PhysicsState::Toppling;
        if (!otherCanSupport)
        {
            continue;
        }

        for (int j = 0; j < other->GetCellCount(); ++j)
        {
            Vector2 otherCellPos = other->GetCellRenderPosition(j);

            bool centerOverlapsOther = cellCenterX > otherCellPos.x && cellCenterX < otherCellPos.x + Constants::TILE_SIZE;
            bool isRestingOnOther = cellBottomY >= otherCellPos.y - Constants::SUPPORT_CHECK_TOLERANCE &&
                cellBottomY <= otherCellPos.y + Constants::SUPPORT_CHECK_TOLERANCE;

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
    for (int i = 0; i < upper->GetCellCount(); ++i)
    {
        Vector2 cellPos = upper->GetCellRenderPosition(i);
        float cellBottomY = cellPos.y + Constants::TILE_SIZE;
        float cellCenterX = cellPos.x + Constants::TILE_SIZE * 0.5f;

        for (int j = 0; j < lower->GetCellCount(); ++j)
        {
            Vector2 lowerCellPos = lower->GetCellRenderPosition(j);

            bool centerOverlapsLower = cellCenterX > lowerCellPos.x && cellCenterX < lowerCellPos.x + Constants::TILE_SIZE;
            bool isRestingOnLower = cellBottomY >= lowerCellPos.y - Constants::SUPPORT_CHECK_TOLERANCE &&
                cellBottomY <= lowerCellPos.y + Constants::SUPPORT_CHECK_TOLERANCE;

            if (centerOverlapsLower && isRestingOnLower)
            {
                return true;
            }
        }
    }

    return false;
}

void PhysicsManager::AccumulateSupportedMass(Block* base, std::vector<Block*>& visited, float& outTotalMass, float& outWeightedX) const
{
    outTotalMass += base->GetMass();
    outWeightedX += base->GetMass() * (base->GetRenderPosition() + base->GetCenterOfMassLocal() * Constants::TILE_SIZE).x;

    for (Block* other : BlockManager::GetInstance().GetAllBlocks())
    {
        bool alreadyVisited = std::find(visited.begin(), visited.end(), other) != visited.end();
        // Toppling(이미 무너지는 중)인 블럭은 더 이상 자기 무게를 아래로 전달하지 않는다고 취급 —
        // 넘어지고 있는 조각까지 계속 하중으로 잡으면, 그 조각이 떨어져 나가는 동안 아래쪽이 계속 불안정하다고 오판한다
        bool canRestOnBase = !alreadyVisited &&
            other->GetPhysicsState() != PhysicsState::Airborne &&
            other->GetPhysicsState() != PhysicsState::Toppling;

        if (canRestOnBase && RestsOnBlock(other, base))
        {
            visited.push_back(other);
            AccumulateSupportedMass(other, visited, outTotalMass, outWeightedX);
        }
    }
}

bool PhysicsManager::ComputeSupportDebugInfo(Block* block, float& outMinX, float& outMaxX, float& outCombinedComX) const
{
    bool hasSupport = false;

    for (int i = 0; i < block->GetCellCount(); ++i)
    {
        Vector2 cellPos = block->GetCellRenderPosition(i);

        if (!IsCellSupported(cellPos, block))
        {
            continue;
        }

        if (!hasSupport)
        {
            outMinX = cellPos.x;
            outMaxX = cellPos.x + Constants::TILE_SIZE;
            hasSupport = true;
        }
        else
        {
            if (cellPos.x < outMinX) outMinX = cellPos.x;
            if (cellPos.x + Constants::TILE_SIZE > outMaxX) outMaxX = cellPos.x + Constants::TILE_SIZE;
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
    AccumulateSupportedMass(block, visited, totalMass, weightedX);
    outCombinedComX = weightedX / totalMass;
    return true;
}

void PhysicsManager::ResolveBalance(Block* block, float deltaTime)
{
    float supportMinX = 0.0f;
    float supportMaxX = 0.0f;
    float centerOfMassX = 0.0f;

    if (!ComputeSupportDebugInfo(block, supportMinX, supportMaxX, centerOfMassX))
    {
        return;
    }

    // [트리키 타워 붕괴 판정]
    // 서서히 각도를 기울이는 기존 ApplyBalanceTorque 방식 대신,
    // 무게중심이 지지대를 벗어나는 '즉시' Toppling 상태로 만들고 강한 회전력을 줍니다.
    // IMBALANCE_DEADZONE만큼은 그냥 넘어가서, 부동소수점/물리 계산 오차 수준의 미세한 초과로는
    // 안정적인 구조까지 하드 스핀이 걸려 떨리며 무너지는 일이 없게 한다.
    if (centerOfMassX < supportMinX - Constants::IMBALANCE_DEADZONE || centerOfMassX > supportMaxX + Constants::IMBALANCE_DEADZONE)
    {
        // 무게중심이 빠진 방향으로 순간적인 강한 각속도 부여 (휙! 넘어감)
        // 200.0f 부분은 테스트해보시면서 원하는 붕괴 속도에 맞춰 조절하세요.
        float tumbleSpin = (centerOfMassX < supportMinX) ? 200.0f : -200.0f;

        // [연쇄 붕괴] block만 넘어뜨리면, 위에 얹혀 있던 블럭들은 여전히 자기 자신의 낡은(개별) 판정으로
        // 따로 넘어지려 하면서 서로 다른 타이밍/크기로 부딪혀 진동(트레블링)한다. block이 넘어가기로
        // 결정됐다면, block이 떠받치던 덩어리(visited) 전체를 같은 회전으로 같이 넘어뜨려서
        // 한 덩어리처럼 자연스럽게 무너지게 한다. (ComputeSupportDebugInfo는 float만 돌려주므로 여기서
        // 떠받치는 덩어리를 다시 한번 구한다 — 블럭 수가 적어 비용은 무시할 만하다)
        std::vector<Block*> visited = { block };
        float totalMassUnused = 0.0f;
        float weightedXUnused = 0.0f;
        AccumulateSupportedMass(block, visited, totalMassUnused, weightedXUnused);

        for (Block* fallingBlock : visited)
        {
            if (fallingBlock->GetPhysicsState() == PhysicsState::Toppling)
            {
                continue;
            }

            fallingBlock->BeginToppling();
            fallingBlock->AddAngularVelocity(tumbleSpin);
        }
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
            if (IsCellSupported(block->GetCellRenderPosition(i), block))
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
            if (IsCellSupported(block->GetCellRenderPosition(i), block))
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

void PhysicsManager::ForceSleepStuckBlocks()
{
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        bool isActive = block->GetPhysicsState() == PhysicsState::Awake || block->GetPhysicsState() == PhysicsState::Toppling;
        if (!isActive)
        {
            continue;
        }

        if (block->GetActiveTimer() < Constants::FORCE_SLEEP_TIMEOUT)
        {
            continue;
        }

        // 지지대가 없으면(진짜로 낙하/이동 중이면) 강제로 재우면 안 된다 — 지지대가 있을 때만 적용
        bool hasSupport = false;
        for (int i = 0; i < block->GetCellCount() && !hasSupport; ++i)
        {
            if (IsCellSupported(block->GetCellRenderPosition(i), block))
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

void PhysicsManager::TrySleepAll()
{
	if (!CheckGlobalStability())
	{
		return;
	}

	for (Block* block : BlockManager::GetInstance().GetAllBlocks())
	{
		if (block->GetPhysicsState() != PhysicsState::Awake)
		{
            continue;
		}
		block->Sleep();
    }

}

void PhysicsManager::WakeAll()
{
	for (Block* block : BlockManager::GetInstance().GetAllBlocks())
	{
		if (block->GetPhysicsState() == PhysicsState::Sleeping)
		{
			block->WakeUp();
		}
	}
}
