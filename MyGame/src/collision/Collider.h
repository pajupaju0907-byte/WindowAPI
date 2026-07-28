#pragma once

struct AABB;

// 모든 충돌체(사각형/원)가 구현해야 하는 인터페이스
class Collider
{
public:
    virtual ~Collider() = default;

    virtual bool CheckCollision(const Collider& other) const = 0;
    virtual AABB GetBounds() const = 0;
};
