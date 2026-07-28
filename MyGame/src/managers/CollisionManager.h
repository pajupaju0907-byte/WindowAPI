#pragma once

#include <vector>

class Block;

// 충돌 판정 대상이 되는 블럭 한 쌍.
// TODO: 실제로 필요한 부가 정보(충돌 깊이, 법선 벡터 등)가 있다면 직접 추가할 것
struct CollisionPair
{
    Block* first = nullptr;
    Block* second = nullptr;
};

// 블럭 간 충돌 감지와 해소를 담당하는 싱글톤
class CollisionManager
{
public:
    static CollisionManager& GetInstance();

    std::vector<CollisionPair> DetectCollisions(const std::vector<Block*>& blocks) const;
    void ResolveCollision(const CollisionPair& pair);

private:
    CollisionManager() = default;
    ~CollisionManager() = default;
    CollisionManager(const CollisionManager&) = delete;
    CollisionManager& operator=(const CollisionManager&) = delete;
};
