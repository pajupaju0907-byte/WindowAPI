#include "CircleCollider.h"

CircleCollider::CircleCollider(Vector2 center, float radius)
    : m_center(center)
    , m_radius(radius)
{
}

bool CircleCollider::CheckCollision(const Collider& other) const
{
    // TODO: Circle-Circle, Circle-AABB 충돌 판정 직접 구현
    (void)other;
    return false;
}

AABB CircleCollider::GetBounds() const
{
    // TODO: 반지름 기반 바운딩 박스 계산 직접 구현
    return { m_center, m_center };
}
