#include "pch.h"

#include "PhysicsManager.h"
#include "../objects/Block.h"
#include "../managers/BlockManager.h"

PhysicsManager& PhysicsManager::GetInstance()
{
    static PhysicsManager instance;
    return instance;
}

PhysicsManager::~PhysicsManager() = default;

void PhysicsManager::Update(float deltaTime)
{
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        if (block->GetPhysicsState() != PhysicsState::Awake)
        {
            continue;
        }

        ApplyGravity(block);
        block->Integrate(deltaTime);
        ResolveFloorCollision(block);
    }
    ResolveBlockCollisions();
}

void PhysicsManager::ApplyGravity(Block* block)
{
    // 질량과 무관하게 항상 같은 가속도(GRAVITY)로 떨어지게 하려고, 힘 = 질량 * 중력가속도로 건다
    // (Integrate가 힘을 다시 질량으로 나누므로 결국 가속도는 GRAVITY로 통일됨)
    block->ApplyForce({ 0.0f, block->GetMass() * Constants::GRAVITY });
}

void PhysicsManager::ResolveFloorCollision(Block* block)
{
    // 이 블럭이 차지하는 칸들 중 바닥을 가장 깊이 파고든 정도를 찾는다
    float deepestPenetration = 0.0f;
    for (int i = 0; i < block->GetCellCount(); ++i)
    {
        float cellBottomY = block->GetCellRenderPosition(i).y + Constants::TILE_SIZE;
        float penetration = cellBottomY - Constants::FLOOR_TOP_Y;
        if (penetration > deepestPenetration)
        {
            deepestPenetration = penetration;
        }
    }

    if (deepestPenetration > 0.0f)
    {
        block->ResolveVerticalPenetration(deepestPenetration);
    }
}

void PhysicsManager::ResolveBlockPairCollision(Block* block, Block* other)
{
    // 전체를 감싸는 사각형(바운딩 박스)끼리만 비교하면, L자처럼 옴폭 들어간 모양의 빈 홈을
    // "꽉 찬 것"처럼 취급해버려서 실제로는 안 겹치는데도 밀어올리는 문제가 생긴다.
    // 그래서 두 블럭의 실제 칸(최대 4x4=16쌍)끼리 하나씩 겹치는지 확인한다.
    float deepestPenetration = 0.0f;

    for (int i = 0; i < block->GetCellCount(); ++i)
    {
        Vector2 blockCellPos = block->GetCellRenderPosition(i);

        for (int j = 0; j < other->GetCellCount(); ++j)
        {
            Vector2 otherCellPos = other->GetCellRenderPosition(j);

            bool overlapsHorizontally = blockCellPos.x < otherCellPos.x + Constants::TILE_SIZE &&
                blockCellPos.x + Constants::TILE_SIZE > otherCellPos.x;
            if (!overlapsHorizontally)
            {
                continue;
            }

            // block의 이 칸이 other의 이 칸보다 위에 있으면서(작은 y) 겹치는 경우만 후보로 삼음.
            // 반대 방향(other가 block 위)은 other 입장에서 이 함수가 호출될 때 처리되므로 여기선 신경 안 씀
            float penetration = (blockCellPos.y + Constants::TILE_SIZE) - otherCellPos.y;
            if (penetration > deepestPenetration && blockCellPos.y < otherCellPos.y)
            {
                deepestPenetration = penetration;
            }
        }
    }

    if (deepestPenetration > 0.0f)
    {
        block->ResolveVerticalPenetration(deepestPenetration);
    }
}

void PhysicsManager::ResolveBlockCollisions()
{
    std::vector<Block*> allBlocks = BlockManager::GetInstance().GetAllBlocks();

    for (Block* block : allBlocks)
    {
        if (block->GetPhysicsState() != PhysicsState::Awake)
        {
            continue;
        }

        // Airborne(아직 공중에 있는) 블럭은 여기서 다룰 대상이 아니라 건너뜀
        for (Block* other : allBlocks)
        {
            if (block == other || other->GetPhysicsState() == PhysicsState::Airborne)
            {
                continue;
            }

            ResolveBlockPairCollision(block, other);
        }
    }
}

bool PhysicsManager::CheckGlobalStability() const
{
    // 최소 버전: Awake 블럭 중 하나라도 너무 빠르게 움직이면 불안정으로 판단.
    // 무게중심/지지 기반 등 더 정교한 판정은 나중에 직접 설계할 것
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        if (block->GetPhysicsState() != PhysicsState::Awake)
        {
            continue;
        }

        if (block->GetSpeedSquared() > Constants::UNSTABLE_SPEED_THRESHOLD * Constants::UNSTABLE_SPEED_THRESHOLD)
        {
            return false;
        }
    }

    return true;
}
void PhysicsManager::TrySleepAll()
{
    // TODO: 안정 상태인 블럭들을 activeBlocks -> sleepingBlocks로 이동시키는 로직 직접 구현
}
