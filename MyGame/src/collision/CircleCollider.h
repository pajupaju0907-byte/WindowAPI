#pragma once

#include "Collider.h"
#include "../util/Types.h"

// 원형 충돌체
class CircleCollider : public Collider
{
public:
    CircleCollider(Vector2 center, float radius);

    bool CheckCollision(const Collider& other) const override;
    AABB GetBounds() const override;

private:
    Vector2 m_center;
    float m_radius = 0.0f;
};
