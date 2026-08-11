#pragma once

#include <memory>
#include <vector>
#include "../util/Types.h"

class Block;

// 블럭의 생성과 소유, 낙하 중인 블럭의 고정(Lock)을 담당하는 싱글톤
class BlockManager
{
public:
    static BlockManager& GetInstance();

    void SpawnBlock(BlockType type);

    // 공중 상태(Airborne)인 현재 블럭의 낙하 타이머 갱신. 시간이 되면 Block::StepDown() 호출.
    void UpdateFalling(float deltaTime);

    // 공중 상태인 현재 블럭에 좌우 이동(서브셀 단위)을 요청. Block::MoveHorizontal()로 전달.
    void MoveCurrentBlock(int subCellDelta);

    // 좌우 방향키 입력을 매 프레임 확인해 MoveCurrentBlock을 호출.
    // 누른 순간 즉시 1회 이동하고, 계속 누르고 있으면 MOVE_REPEAT_INTERVAL마다 반복 이동.
    void UpdateMovementInput(float deltaTime);

    void UpdateRotationInput();

    // 착지 처리: Block::Land() 호출 및 전체 깨우기
    void LockBlock(Block* block);

    // block을 소유 목록(m_blocks)에서 제거해 파괴한다. PhysicsManager가 Toppling 블럭이
    // 화면 밖으로 나갔을 때 호출한다
    void RemoveBlock(Block* block);

    // 현재 공중(Airborne)에서 낙하 중인 블럭 조회. 없으면 nullptr
    Block* GetCurrentFallingBlock() const;

    // 착지한 블럭까지 포함해 지금까지 스폰된 모든 블럭 조회 (렌더링 등에서 전체 순회용)
    std::vector<Block*> GetAllBlocks() const;

    // [데스 판정] 블럭이 데스존(CheckDeathZone)에 닿았으면 true. 한 번 true가 되면 이 씬이 끝날 때까지 유지된다.
    bool IsGameOver() const;

    // [데스 판정] 매 프레임 호출: 지금 존재하는 모든 블럭의 모든 칸을 훑어서, 하나라도 데스존
    // (Constants::FLOOR_LEFT_X/FLOOR_RIGHT_X 바깥, GetDeathZoneTopY() 아래 — 밟을 땅이 없는 영역)에
    // 닿아있으면 즉시 게임오버 상태로 만든다.
    void CheckDeathZone();

    // [데스 판정] 데스존이 시작되는 월드 Y좌표. 카메라가 올라간 만큼 같이 올라가서 항상 "지금 보이는
    // 화면의 바닥"에 붙어있다 — PlayScene의 시각화(RenderDeathZones)도 이 값을 그대로 써서 어긋나지 않게 한다.
    float GetDeathZoneTopY() const;
    // 지금 쌓여있는 블럭 중 가장 높은 지점의 높이를 m 단위로 계산
    float GetTallestHeightMeters() const;
private:
    // worldPoint가 데스존 안에 있는지 판정
    bool IsPointInDeathZone(Vector2 worldPoint) const;

    BlockManager() = default;
    ~BlockManager();
    BlockManager(const BlockManager&) = delete;
    BlockManager& operator=(const BlockManager&) = delete;

    // 블럭의 생명주기를 책임지는 유일한 소유자
    std::vector<std::unique_ptr<Block>> m_blocks;
    Block* m_currentFallingBlock = nullptr;

    // 다음 그리드 낙하 스텝(StepDown)까지 남은 시간
    float m_fallTimer = 0.0f;

    // [락 딜레이] 더 못 내려가는 상태가 지속된 시간. LOCK_DELAY_DURATION을 넘기면 그때 Lock한다 —
    // 막힌 그 순간 바로 Lock하면, 옆으로 비켜서 좁은 틈에 끼워 넣으려는 도중에도 락이 걸려버린다.
    // 다시 내려갈 수 있게 되면(옆으로 비켜서 틈이 열리면) 0으로 리셋된다.
    float m_lockDelayTimer = 0.0f;

    // 좌우 이동키를 누르고 있을 때, 다음 반복 이동까지 남은 시간
    float m_moveTimer = 0.0f;

    // [데스 판정] IsGameOver() 참고
    bool m_isGameOver = false;
};
