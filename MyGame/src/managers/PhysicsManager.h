#pragma once

#include <vector>

class Block;

// 바닥 상태(Awake)인 블럭들의 물리 갱신과 전체 안정성 판단을 담당하는 싱글톤.
// 공중 상태(Airborne) 블럭의 낙하는 그리드 스텝 방식이라 BlockManager가 처리하며, 여기서는 다루지 않는다.
class PhysicsManager
{
public:
    static PhysicsManager& GetInstance();

    // m_activeBlocks(Awake 상태) 순회하며 ApplyGravity + Integrate 호출
    void Update(float deltaTime);

    // 바닥 상태에서의 흔들림 표현을 위한 중력 적용. 공중 낙하 속도와는 무관하다.
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
