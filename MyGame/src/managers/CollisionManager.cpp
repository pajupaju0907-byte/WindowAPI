#include "pch.h"

#include "CollisionManager.h"

CollisionManager& CollisionManager::GetInstance()
{
    static CollisionManager instance;
    return instance;
}

std::vector<CollisionPair> CollisionManager::DetectCollisions(const std::vector<Block*>& blocks) const
{
    // TODO: 블럭 목록을 순회하며 충돌 쌍을 찾는 로직 직접 구현
    (void)blocks;
    return {};
}

void CollisionManager::ResolveCollision(const CollisionPair& pair)
{
    // TODO: 충돌 쌍의 위치/속도를 보정하는 해소 로직 직접 구현
    (void)pair;
}
