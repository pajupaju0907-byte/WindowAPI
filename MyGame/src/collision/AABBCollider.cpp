#include "AABBCollider.h"

AABBCollider::AABBCollider(Vector2 min, Vector2 max)
    : m_min(min)
    , m_max(max)
{
}

bool AABBCollider::CheckCollision(const Collider& other) const
{
    // TODO: AABB-AABB, AABB-Circle 충돌 판정 직접 구현
    (void)other;
    return false;
}

AABB AABBCollider::GetBounds() const
{
    return { m_min, m_max };
}
