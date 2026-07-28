#pragma once

#include "Collider.h"
#include "../util/Types.h"

// 사각형(축 정렬 바운딩 박스) 충돌체
class AABBCollider : public Collider
{
public:
    AABBCollider(Vector2 min, Vector2 max);

    bool CheckCollision(const Collider& other) const override;
    AABB GetBounds() const override;

private:
    Vector2 m_min;
    Vector2 m_max;
};
