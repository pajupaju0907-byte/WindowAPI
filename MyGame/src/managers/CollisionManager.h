#pragma once

#include "../util/Types.h"
#include "../collision/OBBCollider.h"
#include <vector>

class Block;

// [충돌 판정 책임 분리] 블럭 간 충돌 "판정" 결과 — 겹친 블럭 쌍 + 대표 접촉 정보(노멀/침투깊이/접촉점 2개).
// normal은 "second에서 first를 향하는 방향"이다. 실제 반응(힘/속도 변화, WakeUp 판단 등)은
// 여기 담지 않는다 — 그건 PhysicsManager 책임이라, 판정 결과만 그대로 넘겨준다.
struct CollisionPair
{
    Block* first = nullptr;
    Block* second = nullptr;
    Vector2 normal = { 0.0f, 0.0f };
    float penetration = 0.0f;
    Vector2 contactPoints[2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
};

// 블럭 간 충돌 "판정"만 담당하는 싱글톤. OBBCollider(회전 사각형) + SAT로 칸 단위까지 정확히 검사해서
// 겹친 블럭 쌍과 접촉 매니폴드(방향/깊이/접촉점)를 만들어낸다.
// 판정 결과를 가지고 어떻게 반응할지(임펄스, WakeUp 등)는 PhysicsManager가 담당한다 — 여기서는 안 다룬다.
class CollisionManager
{
public:
    static CollisionManager& GetInstance();

    // blocks 목록에서 실제로 겹친 블럭 쌍을 전부 찾아 반환한다 (중복 없이 한 번씩, i<j로 순회)
    std::vector<CollisionPair> DetectCollisions(const std::vector<Block*>& blocks) const;

    // block과 other 한 쌍만 검사한다. 겹쳤으면 true를 반환하고 outPair를 채운다 (겹치지 않았으면 outPair는 안 건드림)
    bool DetectPairCollision(Block* block, Block* other, CollisionPair& outPair) const;

    // [바닥 SAT 통합] block과 바닥(정적인 큰 OBB로 취급)을 블럭-블럭과 동일한 SAT+매니폴드 경로로 검사한다.
    // 겹쳤으면 true를 반환하고 outPair를 채운다. outPair.second는 항상 nullptr(바닥은 Block이 아니므로)이고,
    // normal은 기존 관례대로 "바닥에서 block을 향하는 방향"이다.
    bool DetectFloorCollision(Block* block, CollisionPair& outPair) const;

private:
    CollisionManager() = default;
    ~CollisionManager() = default;
    CollisionManager(const CollisionManager&) = delete;
    CollisionManager& operator=(const CollisionManager&) = delete;

    // 겹친 칸 쌍 후보들(candidates)에서 "가장 깊이 겹친 대표 충돌"과 "접촉면을 따라 가장 멀리 떨어진
    // 접촉점 2개"를 뽑아낸다. DetectPairCollision/DetectFloorCollision이 공통으로 쓰는 매니폴드 병합 로직.
    void BuildManifold(const std::vector<OBBCollisionResult>& candidates, OBBCollisionResult& outBest, Vector2 outManifoldPoints[2]) const;

    // 바닥(FLOOR_LEFT_X~FLOOR_RIGHT_X, FLOOR_TOP_Y 아래)을 감싸는 큰 정적 OBB를 만든다.
    // 화면 아래로 충분히 두껍게 잡아서, 블럭이 아무리 깊이 파고들어도 바닥의 "밑면"은 절대 안 닿게 한다.
    OBBCollider MakeFloorCollider() const;
};
