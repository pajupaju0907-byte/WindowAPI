#pragma once

#include "Collider.h"
#include "../util/Types.h"

// [충돌 판정 책임 분리] SAT(분리축 정리) 충돌 검사 결과.
// normal은 항상 "TestOBBCollision(cornersA, cornersB) 호출 기준으로 B에서 A를 향하는 방향"으로 통일한다 —
// Block::ResolveRigidCollision(WithBlock)이 전부 이 방향 규약("other -> this")을 전제로 짜여 있기 때문.
//
// [접촉 다각화] 접촉점을 1개만 쓰면, 면과 면이 맞닿는 상황(블럭이 평평하게 얹힌 경우)에서도 마치
// 뾰족한 점 하나로 균형을 잡고 있는 것처럼 계산되어 계속 미세하게 회전하며 불안정해진다.
// 그래서 B의 꼭짓점 중 겹침이 깊은 2개(contactPoints[0], [1])를 같이 돌려준다.
struct OBBCollisionResult
{
    bool collided = false;
    Vector2 normal = { 0.0f, 0.0f };
    float penetration = 0.0f;
    Vector2 contactPoints[2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
};

// 두 사각형(각각 회전 반영된 네 꼭짓점, 시계방향)을 SAT로 검사한다.
// OBBCollider 없이 원시 코너 배열만으로도 쓸 수 있게 자유 함수로 둔다 — Block::CanOccupy처럼
// 아직 블럭에 실제로 붙지 않은 후보 칸(회전 없는 사각형)을 검사할 때는 OBBCollider를 새로 만들 필요 없이 바로 쓴다.
OBBCollisionResult TestOBBCollision(const Vector2 cornersA[4], const Vector2 cornersB[4]);

// 회전된 사각형(Oriented Bounding Box) 콜라이더. 블럭이 차지하는 칸 하나(1x1 타일)를 표현하는 데 쓰인다 —
// 블럭 전체(최대 4칸)는 하나의 볼록 도형이 아니라서, 칸 단위로 쪼개 각각을 OBB로 다룬다.
class OBBCollider : public Collider
{
public:
    OBBCollider(Vector2 center, Vector2 halfExtents, float angleDegrees);

    // Collider 인터페이스: 겹쳤는지 여부만 필요할 때. other가 OBBCollider가 아니면 항상 false.
    bool CheckCollision(const Collider& other) const override;
    AABB GetBounds() const override;

    // 네 꼭짓점(시계방향: 왼쪽위->오른쪽위->오른쪽아래->왼쪽아래)을 회전 반영해서 채운다
    void GetCorners(Vector2 outCorners[4]) const;

    // TestOBBCollision을 감싸서, other와의 정밀 SAT 결과(방향/깊이/접촉점)를 돌려준다
    OBBCollisionResult Intersect(const OBBCollider& other) const;

private:
    Vector2 m_center;
    Vector2 m_halfExtents;
    float m_angleDegrees;
};
