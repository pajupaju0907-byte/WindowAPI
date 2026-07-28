#pragma once

#include <vector>

class Block;

// 낙하 중인 블럭들의 물리 갱신과 전체 안정성 판단을 담당하는 싱글톤
class PhysicsManager
{
public:
    static PhysicsManager& GetInstance();

    void Update(float deltaTime);
    void ApplyGravity(Block* block);

    // 개별 블럭이 아니라 탑 전체가 안정적인지 판단 (전체 판단은 매니저의 책임)
    bool CheckGlobalStability() const;

    void TrySleepAll();

private:
    PhysicsManager() = default;
    ~PhysicsManager();
    PhysicsManager(const PhysicsManager&) = delete;
    PhysicsManager& operator=(const PhysicsManager&) = delete;

    // Block의 소유권은 BlockManager에 있음. 여기서는 참조만 빌려온다.
    std::vector<Block*> m_activeBlocks;
    std::vector<Block*> m_sleepingBlocks;
};
