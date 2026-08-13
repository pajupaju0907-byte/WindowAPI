#include "pch.h"

#include "PhysicsManager.h"
#include "../objects/Block.h"
#include "../managers/BlockManager.h"
#include "../managers/CollisionManager.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
	// [임시 디버그 - 왜 지지를 못 받는지 조사용] SettleToppledBlocks가 저속인 Toppling 블록을
	// 평가할 때마다 칸별 지지 판정 근거를 파일로 남긴다. 원인을 찾으면 지워도 된다.
	void LogSettleAttempt(Block* block, bool hasSupport)
	{
		FILE* file = nullptr;
		fopen_s(&file, "C:\\Users\\inha\\Desktop\\WobbleBlock\\WindowAPI\\physics_debug.log", "a");
		if (file == nullptr)
		{
			return;
		}

		Vector2 pos = block->GetRenderPosition();
		std::fprintf(file, "[SettleToppledBlocks] block=%p pos=(%.1f,%.1f) angle=%.2f speed=%.1f angVel=%.2f hasSupport=%d\n",
			block, pos.x, pos.y, block->GetAngle(), std::sqrt(block->GetSpeedSquared()), block->GetAngularVelocity(), hasSupport ? 1 : 0);

		for (int i = 0; i < block->GetCellCount(); ++i)
		{
			Vector2 corners[4];
			block->GetCellRotatedCorners(i, corners);
			float minX = corners[0].x, maxX = corners[0].x, bottomY = corners[0].y;
			for (int c = 1; c < 4; ++c)
			{
				if (corners[c].x < minX) minX = corners[c].x;
				if (corners[c].x > maxX) maxX = corners[c].x;
				if (corners[c].y > bottomY) bottomY = corners[c].y;
			}
			float centerX = (minX + maxX) * 0.5f;
			bool isOverFloorPlatform = centerX > Constants::FLOOR_LEFT_X && centerX < Constants::FLOOR_RIGHT_X;
			bool isNearFloorHeight = bottomY >= Constants::FLOOR_TOP_Y - Constants::SUPPORT_CHECK_TOLERANCE &&
				bottomY <= Constants::FLOOR_TOP_Y + Constants::SUPPORT_CHECK_TOLERANCE;
			std::fprintf(file, "  cell%d minX=%.1f maxX=%.1f centerX=%.1f bottomY=%.1f floorTopY=%.1f floorX=[%.1f,%.1f] overFloorX=%d nearFloorY=%d\n",
				i, minX, maxX, centerX, bottomY, Constants::FLOOR_TOP_Y, Constants::FLOOR_LEFT_X, Constants::FLOOR_RIGHT_X,
				isOverFloorPlatform ? 1 : 0, isNearFloorHeight ? 1 : 0);
		}
		std::fclose(file);
	}
}

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

    // 2.5. [바닥 마찰 근사] 지지대가 있는 Awake 블럭은 매 프레임 속도를 감쇠시켜서 미끄러짐을 잡는다.
    // ResolveRigidCollision의 임펄스 마찰은 얹혀서 미끄러지는 동안(수직 속도 거의 0)엔 안 걸리기 때문에 필요하다.
    // [넘어지는 중엔 제외] Toppling은 일부러 뺐다 — 넘어지는 도중엔 피벗 모서리가 거의 계속 바닥/다른
    // 블럭에 닿아 있어서(hasSupport=true) 이 감쇠가 매초 60번씩 각속도를 깎았다. "가만히 얹힌 블록이
    // 안 미끄러지게" 하려고 만든 감쇠인데, 지금 활발히 회전하며 넘어지는 블록에도 똑같이 걸려서 초기
    // 회전력이 거의 즉시 죽어버리고, 재넘어짐마다 그 죽은 속도에서 다시 시작해 넘어짐이 부자연스럽게
    // 느려 보이는 원인이었다. 넘어지는 중엔 실제 중력+충돌 임펄스만으로 가속되게 둔다.
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        bool isActive = block->GetPhysicsState() == PhysicsState::Awake;
        if (!isActive)
        {
            continue;
        }

        // [마찰 사각지대 방지] 여기는 일부러 엄격한 IsCellSupported 대신 느슨한 IsCellTouchingAnySupport를
        // 쓴다 — 모서리로만 살짝 걸친 상태도 마찰을 받아야, 중력에 살짝 밀렸다가 충돌에 다시 튕기는 게
        // 감쇠 없이 반복되며 영원히 떠는 사각지대가 안 생긴다. "진짜 안정적인지"는 아래 ResolveBalance가
        // IsCellSupported(엄격)로 따로 판단하니, 여기서 느슨하게 봐준다고 넘어져야 할 게 안 넘어지진 않는다.
        bool isTouchingAnySupport = false;
        for (int i = 0; i < block->GetCellCount() && !isTouchingAnySupport; ++i)
        {
            if (IsCellTouchingAnySupport(block, i))
            {
                isTouchingAnySupport = true;
            }
        }

        if (isTouchingAnySupport)
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
    SettleToppledBlocks(restingChildren);
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
    // [바닥 SAT 통합] 바닥을 하나의 정적 OBB로 취급해서, 블럭-블럭과 똑같이 CollisionManager가
    // 칸 단위 SAT + 접촉 매니폴드(접촉점 2개) 판정을 전담한다. 예전엔 여기서 직접 칸의 네 모서리를
    // 스캔하며 법선을 항상 (0,-1)로 고정하고 왼쪽/오른쪽 끝 모서리를 손으로 골랐는데, 그 판정 로직이
    // 이제 CollisionManager::DetectFloorCollision 하나로 모였다 — 여기서는 결과를 받아 반응만 한다.
    CollisionPair pair;
    if (!CollisionManager::GetInstance().DetectFloorCollision(block, pair))
    {
        return;
    }

    // [접촉 다각화] 첫 번째 점만 위치 보정을 하고(penetration 그대로), 두 번째 점은 이미 밀어낸 뒤라
    // penetration을 0으로 넘겨서 속도/토크 반응만 추가로 준다 — 면과 면이 맞닿았을 때 양 끝을 동시에
    // 붙잡아서 미세 회전(떨림)이 사라진다.
    block->ResolveRigidCollision(pair.contactPoints[0], pair.normal, pair.penetration);
    block->ResolveRigidCollision(pair.contactPoints[1], pair.normal, 0.0f);
}

void PhysicsManager::ResolveBlockPairCollision(Block* block, Block* other)
{
    // [책임 분리] "겹쳤는지/어디를/얼마나 겹쳤는지"(SAT, 접촉 매니폴드)는 CollisionManager가 전담한다.
    // 여기서는 그 판정 결과를 가지고 "그래서 어떻게 반응할지"(WakeUp 여부, 강체 충돌 임펄스)만 담당한다.
    CollisionPair pair;
    if (!CollisionManager::GetInstance().DetectPairCollision(block, other, pair))
    {
        return;
    }

    bool blockMovable = block->GetPhysicsState() == PhysicsState::Awake || block->GetPhysicsState() == PhysicsState::Toppling;
    bool otherMovable = other->GetPhysicsState() == PhysicsState::Awake || other->GetPhysicsState() == PhysicsState::Toppling;

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

    // [접촉 다각화] CollisionManager가 넘겨준 접촉점 2개에 대해서만 반응을 준다. 첫 번째 점만 위치
    // 보정을 하고(penetration 그대로), 두 번째 점은 이미 밀어낸 뒤라 penetration을 0으로 넘겨서
    // 속도/토크 반응만 추가로 준다 — 면과 면이 맞닿았을 때 양 끝을 동시에 붙잡아서 미세 회전(떨림)이 사라진다
    if (blockMovable && otherMovable)
    {
        // 둘 다 움직일 수 있으면 진짜 쌍방향 충돌
        block->ResolveRigidCollisionWithBlock(other, pair.contactPoints[0], pair.normal, pair.penetration);
        block->ResolveRigidCollisionWithBlock(other, pair.contactPoints[1], pair.normal, 0.0f);
    }
    else if (blockMovable)
    {
        // other는 Sleeping(고정) — block만 밀려남. normal이 이미 "other->block" 방향이라 그대로 씀
        block->ResolveRigidCollision(pair.contactPoints[0], pair.normal, pair.penetration);
        block->ResolveRigidCollision(pair.contactPoints[1], pair.normal, 0.0f);
    }
    else if (otherMovable)
    {
        // block이 Sleeping(고정) — other만 밀려남. other 입장에선 반대(block->other) 방향이 필요해서 뒤집는다
        other->ResolveRigidCollision(pair.contactPoints[0], pair.normal * -1.0f, pair.penetration);
        other->ResolveRigidCollision(pair.contactPoints[1], pair.normal * -1.0f, 0.0f);
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
        // [임시 디버그 - 왜 허공에서 Sleeping인지 조사용]
        if (block->GetPhysicsState() == PhysicsState::Sleeping)
        {
            static int floorLogCount = 0;
            if (floorLogCount < 50)
            {
                FILE* file = nullptr;
                fopen_s(&file, "C:\\Users\\inha\\Desktop\\WobbleBlock\\WindowAPI\\physics_debug.log", "a");
                if (file != nullptr)
                {
                    std::fprintf(file, "[IsCellSupported/FLOOR] block=%p cell=%d cellBottomY=%.1f cellCenterX=%.1f floorTopY=%.1f\n",
                        block, cellIndex, cellBottomY, cellCenterX, Constants::FLOOR_TOP_Y);
                    std::fclose(file);
                }
                ++floorLogCount;
            }
        }
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
                // [임시 디버그 - 왜 허공에서 Sleeping인지 조사용]
                if (block->GetPhysicsState() == PhysicsState::Sleeping)
                {
                    static int blockLogCount = 0;
                    if (blockLogCount < 50)
                    {
                        FILE* file = nullptr;
                        fopen_s(&file, "C:\\Users\\inha\\Desktop\\WobbleBlock\\WindowAPI\\physics_debug.log", "a");
                        if (file != nullptr)
                        {
                            std::fprintf(file,
                                "[IsCellSupported/BLOCK] block=%p cell=%d cellBottomY=%.1f cellCenterX=%.1f "
                                "other=%p otherState=%d otherCell=%d otherTopY=%.1f otherMinX=%.1f otherMaxX=%.1f\n",
                                block, cellIndex, cellBottomY, cellCenterX,
                                other, static_cast<int>(other->GetPhysicsState()), j, otherTopY, otherMinX, otherMaxX);
                            std::fclose(file);
                        }
                        ++blockLogCount;
                    }
                }
                return true;
            }
        }
    }

    return false;
}

bool PhysicsManager::IsCellTouchingAnySupport(Block* block, int cellIndex) const
{
    float cellTopY = 0.0f;
    float cellBottomY = 0.0f;
    float cellMinX = 0.0f;
    float cellMaxX = 0.0f;
    GetCellSupportBounds(block, cellIndex, cellTopY, cellBottomY, cellMinX, cellMaxX);

    // [마찰 사각지대 방지] IsCellSupported와 달리 "중심이 그 위에 있는지"가 아니라 "칸의 X범위가
    // 조금이라도 겹치는지"만 본다 — 모서리로만 걸친 접촉에도 마찰이 걸리게 하려는 것.
    bool isNearFloorHeight = cellBottomY >= Constants::FLOOR_TOP_Y - Constants::SUPPORT_CHECK_TOLERANCE &&
        cellBottomY <= Constants::FLOOR_TOP_Y + Constants::SUPPORT_CHECK_TOLERANCE;
    bool overlapsFloorPlatform = cellMaxX > Constants::FLOOR_LEFT_X && cellMinX < Constants::FLOOR_RIGHT_X;
    if (isNearFloorHeight && overlapsFloorPlatform)
    {
        return true;
    }

    for (Block* other : BlockManager::GetInstance().GetAllBlocks())
    {
        bool otherCanSupport = other != block &&
            other->GetPhysicsState() != PhysicsState::Airborne &&
            other->GetPhysicsState() != PhysicsState::Toppling;
        if (!otherCanSupport)
        {
            continue;
        }

        if (std::fabs(other->GetRenderPosition().y - cellBottomY) > Constants::SUPPORT_BROADPHASE_MAX_BLOCK_EXTENT)
        {
            continue;
        }

        for (int j = 0; j < other->GetCellCount(); ++j)
        {
            float otherTopY = 0.0f, otherBottomY = 0.0f, otherMinX = 0.0f, otherMaxX = 0.0f;
            GetCellSupportBounds(other, j, otherTopY, otherBottomY, otherMinX, otherMaxX);

            bool overlapsOther = cellMaxX > otherMinX && cellMinX < otherMaxX;
            bool isTouchingOther = cellBottomY >= otherTopY - Constants::SUPPORT_CHECK_TOLERANCE &&
                cellBottomY <= otherTopY + Constants::SUPPORT_CHECK_TOLERANCE;

            if (overlapsOther && isTouchingOther)
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

    // [임시 디버그 - 왜 안 넘어지는지 조사용] 처음 300번 호출까지만 supportMinX/MaxX/centerOfMassX를 파일로 남긴다.
    {
        static int logCount = 0;
        if (logCount < 300)
        {
            FILE* file = nullptr;
            fopen_s(&file, "C:\\Users\\inha\\Desktop\\WobbleBlock\\WindowAPI\\physics_debug.log", "a");
            if (file != nullptr)
            {
                std::fprintf(file, "[ResolveBalance] block=%p supportMinX=%.1f supportMaxX=%.1f centerOfMassX=%.1f imbalance=%.1f\n",
                    block, supportMinX, supportMaxX, centerOfMassX,
                    centerOfMassX < supportMinX ? centerOfMassX - supportMinX : (centerOfMassX > supportMaxX ? centerOfMassX - supportMaxX : 0.0f));
                std::fclose(file);
            }
            ++logCount;
        }
    }

    // [트리키 타워 붕괴 판정]
    // 서서히 각도를 기울이는 기존 ApplyBalanceTorque 방식 대신,
    // 무게중심이 지지대를 벗어나는 '즉시' Toppling 상태로 만들고 강한 회전력을 줍니다.
    // [칼날 위 균형 방지] 가장자리를 넘어야(구식) 불안정으로 보는 대신, 가장자리에서 안쪽으로
    // IMBALANCE_DEADZONE만큼도 못 들어와 있으면(가장자리에 걸치기만 해도) 불안정으로 본다 —
    // 지지 칸이 하나뿐이고 무게중심이 그 가장자리에 정확히 걸리는 칼날 위 균형(예: S자를 세로로 세워
    // 모서리 하나로만 받치는 경우)까지 "가장자리를 안 넘었으니 안정"으로 영원히 통과하는 걸 막는다.
    if (centerOfMassX < supportMinX + Constants::IMBALANCE_DEADZONE || centerOfMassX > supportMaxX - Constants::IMBALANCE_DEADZONE)
    {
        // [무한 재넘어짐 방지] 옆 블록 등에 막혀서 실제로는 못 넘어가는 블록은, SettleToppledBlocks가
        // Awake로 돌려보내자마자 여기서 곧바로 다시 imbalance로 판정돼 BeginToppling이 반복된다 —
        // 매번 새 TUMBLE_ANGULAR_VELOCITY와 새 피벗 고정이 걸리면서 제자리에서 계속 진동(퉁퉁 튀는
        // 것처럼 보임)한다. Sleep 없이 이 재넘어짐이 MAX_CONSECUTIVE_TOPPLE_COUNT번을 넘으면, 물리적으로
        // 완전히 안정된 상태가 아니어도 그 자리에서 강제로 멈춰서 무한 진동을 끊는다.
        if (block->GetConsecutiveToppleCount() >= Constants::MAX_CONSECUTIVE_TOPPLE_COUNT)
        {
            block->ForceStabilize();
            return;
        }

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
        // [재넘어짐 킥 중복 방지] 옆 블록 등에 막혀서 완전히 못 넘어가고 "살짝 넘어짐 → 피벗 고정으로
        // 거의 원위치 복귀 → 그런데 무게중심은 여전히 살짝 벗어나 있어서 다음 프레임에 또 imbalance 판정"이
        // 반복될 수 있다. 이때마다 TUMBLE_ANGULAR_VELOCITY를 처음부터 다시 꽂아 넣으면, 이미 있던(자연스러운)
        // 각속도 위에 매번 인위적인 순간 가속이 덧붙어서 화면에는 계속 "팍! 팍!" 튀는 것처럼 보인다.
        // 첫 넘어짐(GetConsecutiveToppleCount()==0, 이번 BeginToppling 호출로 1이 됨)에만 킥을 주고,
        // 재시도부터는 이미 진행 중인 회전에 맡긴다 — 실제로 넘어가는 중이면 그대로 이어지고, 막혀서
        // 멈춰 있으면(각속도 0에 가까움) MAX_CONSECUTIVE_TOPPLE_COUNT에 도달해 ForceStabilize가 정리한다.
        bool isFirstTopple = block->GetConsecutiveToppleCount() == 0;

        block->BeginToppling();
        if (isFirstTopple)
        {
            block->AddAngularVelocity(tumbleSpin);
        }
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
void PhysicsManager::SettleToppledBlocks(const std::unordered_map<Block*, std::vector<Block*>>& childrenOf)
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

        LogSettleAttempt(block, hasSupport); // [임시 디버그]

        if (!hasSupport)
        {
            continue;
        }

        // [진짜 원인 수정] "칸 하나라도 지지됨"만으로 Awake로 돌려보내면, 회전하며 넘어지는 도중 다른
        // 모서리가 바닥에 잠깐 스치기만 해도 매번 여기 걸려서 Awake로 튕겨나갔다가, 바로 다음 스텝
        // ResolveBalance가 결합 무게중심을 보고 다시 넘어뜨리는 게 반복된다 — 진짜 넘어지는 중인 블록의
        // 낙하가 계속 끊기고(눈에는 "재넘어짐"으로 보임), 결국 MAX_CONSECUTIVE_TOPPLE_COUNT에 걸려
        // 넘어져야 할 자세 그대로 ForceStabilize에 얼어붙는 원인이었다. 그래서 ResolveBalance와 완전히
        // 같은 기준(결합 무게중심이 지지 범위 안쪽으로 IMBALANCE_DEADZONE만큼 여유가 있는지)까지 확인해서,
        // 진짜로 안정된 경우에만 Awake로 돌려보낸다 — 아직 불안정하면 Toppling 상태를 그대로 유지해서
        // 실제 물리(중력+충돌)로 자연스럽게 계속 넘어지게 둔다.
        float supportMinX = 0.0f;
        float supportMaxX = 0.0f;
        float centerOfMassX = 0.0f;
        if (!ComputeSupportDebugInfo(block, childrenOf, supportMinX, supportMaxX, centerOfMassX))
        {
            continue;
        }

        bool isBalanced = centerOfMassX >= supportMinX + Constants::IMBALANCE_DEADZONE &&
            centerOfMassX <= supportMaxX - Constants::IMBALANCE_DEADZONE;
        if (isBalanced)
        {
            block->WakeUp();
            continue;
        }

        // [끼임 방지] 균형을 못 찾았어도, 옆 블럭이나 바닥 모서리에 끼어서 실제로는 더 못 넘어가는
        // 채로 TOPPLE_STUCK_TIMEOUT을 넘겨 계속 Toppling에 머물러 있으면 강제로 멈춘다 — 안 그러면
        // 위 hasSupport/isBalanced 두 탈출 조건을 영원히 못 만족해서 그 자세 그대로 무한정 얼어붙는다.
        if (block->GetActiveTimer() >= Constants::TOPPLE_STUCK_TIMEOUT)
        {
            block->ForceStabilize();
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

        // [임시 디버그 - 중력이 안 먹는 것처럼 보이는 Awake 블록 조사용] 지지대가 없는데(=중력을 받아
        // 계속 떨어지고 있어야 함) 거의 안 움직이는 Awake 블록이 있으면, 매 프레임 위치/속도를 남겨서
        // Integrate/ApplyGravity가 실제로 도는지, 뭔가 매 프레임 속도를 도로 0으로 죽이는지 확인한다.
        // 원인을 찾으면 지워도 된다.
        if (!hasSupport && block->GetSpeedSquared() < 1.0f)
        {
            FILE* file = nullptr;
            fopen_s(&file, "C:\\Users\\inha\\Desktop\\WobbleBlock\\WindowAPI\\physics_debug.log", "a");
            if (file != nullptr)
            {
                Vector2 pos = block->GetRenderPosition();
                std::fprintf(file, "[TrySleepAll/STUCK-NO-SUPPORT] block=%p pos=(%.1f,%.1f) angle=%.2f speed=%.2f angVel=%.2f activeTimer=%.2f\n",
                    block, pos.x, pos.y, block->GetAngle(), std::sqrt(block->GetSpeedSquared()), block->GetAngularVelocity(), block->GetActiveTimer());
                std::fclose(file);
            }
        }

        bool canRest = hasSupport && !tooFast && !spinningTooFast;

        if (!canRest)
        {
            block->ResetRestTimer();

            // [강제 취침 타임아웃] 지지대는 있는데 미세한 진동 때문에 속도가 SLEEP 임계값을 계속
            // 살짝살짝 넘나들면, 위의 정상 경로(rest timer가 SLEEP_DELAY만큼 안 끊기고 유지)로는
            // 영원히 잠들 수 없다 — 눈에는 거의 멈춘 것처럼 보이는데 계속 Awake로 남는 경우가 이거다.
            // m_activeTimer(Land()/BeginToppling() 이후 경과 시간, Sleep<->Wake 반복에도 안 끊김)가
            // FORCE_SLEEP_TIMEOUT을 넘으면 속도와 무관하게 강제로 안정화한다.
            if (hasSupport && block->GetActiveTimer() >= Constants::FORCE_SLEEP_TIMEOUT)
            {
                block->ForceStabilize();
            }

            continue;
        }

        block->AdvanceRestTimer(deltaTime);

        if (block->GetRestTimer() >= Constants::SLEEP_DELAY)
        {
            // [무한 재넘어짐 방지] 여기가 "진짜로 안정돼서 스스로 잠들었다"고 확인된 유일한 경로다
            // (지지대 있음 + 저속 상태가 SLEEP_DELAY 동안 유지됨) — 재넘어짐 카운트를 여기서만 리셋한다.
            block->ResetConsecutiveToppleCount();
            block->Sleep();
        }
    }
}

