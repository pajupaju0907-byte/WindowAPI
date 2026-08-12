#include "pch.h"

#include "CollisionManager.h"
#include "../objects/Block.h"
#include "../collision/OBBCollider.h"
#include "../util/Constants.h"
#include <cmath>

CollisionManager& CollisionManager::GetInstance()
{
    static CollisionManager instance;
    return instance;
}

bool CollisionManager::DetectPairCollision(Block* block, Block* other, CollisionPair& outPair) const
{
    // [성능] 실제 칸 모서리(회전 포함, sin/cos 포함)를 계산하기 전에, 두 블록 원점이 서로 닿을 수 있는
    // 범위(SUPPORT_BROADPHASE_MAX_BLOCK_EXTENT) 밖이면 곧바로 건너뛴다 — 탑이 커질수록 대부분의
    // 블록 쌍이 여기서 걸러진다 (PhysicsManager::IsCellSupported 등과 같은 이유로 유도된 값).
    Vector2 blockOrigin = block->GetRenderPosition();
    Vector2 otherOrigin = other->GetRenderPosition();
    if (std::fabs(blockOrigin.x - otherOrigin.x) > Constants::SUPPORT_BROADPHASE_MAX_BLOCK_EXTENT ||
        std::fabs(blockOrigin.y - otherOrigin.y) > Constants::SUPPORT_BROADPHASE_MAX_BLOCK_EXTENT)
    {
        return false;
    }

    // [Contact Manifold] 두 블럭의 실제 칸(최대 4x4=16쌍)끼리 OBB SAT 검사를 해서 겹친 쌍을 전부 모아둔다.
    // 넓은 면 접촉(예: T자가 일자 위에 반듯하게 얹히는 경우)에서는 여러 칸 쌍이 동시에 겹치는데,
    // "가장 깊이 겹친 것" 하나만 쓰면 접촉면의 좌우 양 끝을 표현하지 못해 한쪽으로 치우친 토크가 생기고
    // 그게 미세 회전(떨림)으로 반복된다.
    std::vector<OBBCollisionResult> candidates;

    for (int i = 0; i < block->GetCellCount(); ++i)
    {
        OBBCollider colliderA = block->GetCellCollider(i);

        for (int j = 0; j < other->GetCellCount(); ++j)
        {
            OBBCollider colliderB = other->GetCellCollider(j);

            OBBCollisionResult collision = colliderA.Intersect(colliderB);
            if (collision.collided)
            {
                candidates.push_back(collision);
            }
        }
    }

    if (candidates.empty())
    {
        return false;
    }

    OBBCollisionResult bestCollision;
    Vector2 manifoldPoints[2];
    BuildManifold(candidates, bestCollision, manifoldPoints);

    outPair.first = block;
    outPair.second = other;
    outPair.normal = bestCollision.normal;
    outPair.penetration = bestCollision.penetration;
    outPair.contactPoints[0] = manifoldPoints[0];
    outPair.contactPoints[1] = manifoldPoints[1];
    return true;
}

bool CollisionManager::DetectFloorCollision(Block* block, CollisionPair& outPair) const
{
    // [바닥 SAT 통합] 예전엔 바닥만 별도의 손으로 짠 로직(항상 법선 (0,-1) 고정, 칸을 하나하나 스캔해서
    // 가장 왼쪽/오른쪽에서 닿은 모서리를 억지로 골라내는 방식)으로 처리했다. 블럭-블럭과 완전히 다른
    // 경로라서, 기울어진 채 모서리로 걸쳐 있는 경우 등에 실제 접촉 형상과 어긋난 판단을 할 여지가 있었다.
    // 이제는 바닥도 하나의 (안 움직이는) 큰 OBB로 취급해서, 블럭끼리와 똑같이 칸 단위 SAT + 매니폴드
    // 병합을 거치게 한다 — "블럭끼리 붙는 것과 동일한" 판정 로직이 된다.
    OBBCollider floorCollider = MakeFloorCollider();

    std::vector<OBBCollisionResult> candidates;

    for (int i = 0; i < block->GetCellCount(); ++i)
    {
        OBBCollider cellCollider = block->GetCellCollider(i);

        // [접촉점 선택 순서 주의] Intersect(A.Intersect(B))는 항상 "B(뒤에 넘긴 도형)의 꼭짓점"을
        // 접촉점 후보로 쓴다 — 블럭끼리는 둘 다 칸 하나 크기(48px)라 어느 쪽을 써도 실제 접촉 근처였지만,
        // 바닥은 발판 전체를 덮는 훨씬 큰 OBB라서 floorCollider.Intersect(cellCollider)로 순서를 바꾸지
        // 않으면 접촉점이 발판 양 끝 꼭짓점(블럭과 무관하게 수백 px 떨어진 곳)으로 잡혀서, 그 지점을
        // 기준으로 한 회전 토크가 실제보다 훨씬 크게 계산되는 버그가 있었다(밑면이 좁은 모양일수록 두드러짐).
        // 여기선 cellCollider를 B로 넘겨서 항상 "칸의" 꼭짓점이 접촉점이 되게 한다.
        OBBCollisionResult collision = floorCollider.Intersect(cellCollider);
        if (collision.collided)
        {
            // Intersect(A=floor, B=cell)의 normal은 "B(cell)에서 A(floor)를 향하는 방향" = 아래쪽이다.
            // 이 함수가 약속하는 "바닥 -> block(위쪽)" 방향과 반대라서 뒤집어준다.
            collision.normal = collision.normal * -1.0f;
            candidates.push_back(collision);
        }
    }

    if (candidates.empty())
    {
        return false;
    }

    OBBCollisionResult bestCollision;
    Vector2 manifoldPoints[2];
    BuildManifold(candidates, bestCollision, manifoldPoints);

    outPair.first = block;
    outPair.second = nullptr;
    outPair.normal = bestCollision.normal;
    outPair.penetration = bestCollision.penetration;
    outPair.contactPoints[0] = manifoldPoints[0];
    outPair.contactPoints[1] = manifoldPoints[1];
    return true;
}

void CollisionManager::BuildManifold(const std::vector<OBBCollisionResult>& candidates, OBBCollisionResult& outBest, Vector2 outManifoldPoints[2]) const
{
    // 가장 깊이 겹친 후보를 "대표 충돌"로 삼는다 — 위치 보정에 쓸 침투 깊이와, 아래 병합 기준이 될 노멀을 여기서 얻는다
    outBest = candidates[0];
    for (const OBBCollisionResult& candidate : candidates)
    {
        if (candidate.penetration > outBest.penetration)
        {
            outBest = candidate;
        }
    }

    // [Contact Manifold] 대표 노멀과 방향이 비슷한(거의 같은 면에서 생긴) 후보들의 접촉점만 모아서,
    // 그중 접촉면을 따라(접선 방향으로) 가장 멀리 떨어진 두 점을 최종 접촉점 2개로 쓴다.
    // 후보가 1개뿐(점 하나짜리 접촉)이면 min/max가 같은 칸의 두 점이 되어 자연히 1점 충돌과 동일하게 동작한다.
    Vector2 tangent = { -outBest.normal.y, outBest.normal.x };
    float minProjection = 999999.0f;
    float maxProjection = -999999.0f;
    outManifoldPoints[0] = outBest.contactPoints[0];
    outManifoldPoints[1] = outBest.contactPoints[1];

    for (const OBBCollisionResult& candidate : candidates)
    {
        float normalAlignment = candidate.normal.x * outBest.normal.x + candidate.normal.y * outBest.normal.y;
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
                outManifoldPoints[0] = point;
            }
            if (projection > maxProjection)
            {
                maxProjection = projection;
                outManifoldPoints[1] = point;
            }
        }
    }
}

OBBCollider CollisionManager::MakeFloorCollider() const
{
    // 가로는 발판 범위(FLOOR_LEFT_X~FLOOR_RIGHT_X) 그대로, 세로는 FLOOR_TOP_Y부터 화면 아래로 충분히
    // 두껍게(WINDOW_HEIGHT의 몇 배) 잡아서 블럭이 아무리 깊이 파고들어도 "밑면"에는 절대 안 닿게 한다.
    float floorWidth = Constants::FLOOR_RIGHT_X - Constants::FLOOR_LEFT_X;
    float floorDepth = Constants::WINDOW_HEIGHT * 4.0f;
    Vector2 center =
    {
        (Constants::FLOOR_LEFT_X + Constants::FLOOR_RIGHT_X) * 0.5f,
        Constants::FLOOR_TOP_Y + floorDepth * 0.5f
    };
    Vector2 halfExtents = { floorWidth * 0.5f, floorDepth * 0.5f };
    return OBBCollider(center, halfExtents, 0.0f);
}

std::vector<CollisionPair> CollisionManager::DetectCollisions(const std::vector<Block*>& blocks) const
{
    std::vector<CollisionPair> pairs;

    for (size_t i = 0; i < blocks.size(); ++i)
    {
        for (size_t j = i + 1; j < blocks.size(); ++j)
        {
            CollisionPair pair;
            if (DetectPairCollision(blocks[i], blocks[j], pair))
            {
                pairs.push_back(pair);
            }
        }
    }

    return pairs;
}
