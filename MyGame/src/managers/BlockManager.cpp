#include "pch.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "BlockManager.h"
#include "../objects/Block.h"
#include "../objects/TetrominoBlock.h"
#include "../objects/GiantTetrominoBlock.h"
#include "../objects/HeavyBlock.h"
#include "../util/Constants.h"
#include "../core/InputManager.h"
#include "CameraManager.h"
#include "SoundManager.h"
#include "PhysicsManager.h"
BlockManager& BlockManager::GetInstance()
{
    static BlockManager instance;
    return instance;
}

BlockManager::~BlockManager() = default;

std::unique_ptr<Block> BlockManager::CreateRandomTetromino()
{
    TetrominoShape randomShape = static_cast<TetrominoShape>(rand() % 7);
    return std::make_unique<TetrominoBlock>(randomShape);
}

void BlockManager::SpawnBlock(BlockType type)
{
    std::unique_ptr<Block> spawnedBlock;
    switch (type)
    {
    case BlockType::Tetromino:
        // 새로 랜덤을 뽑지 않고, Reset()/직전 SpawnBlock() 호출에서 미리 뽑아둔 m_nextBlockPreview를
        // 그대로 이번에 낙하시킬 블럭으로 쓴다. 그리고 빠진 자리를 곧바로 다시 채워둬야 다음 번에도
        // "미리 뽑아둔 다음 블럭"이 항상 존재한다.
        spawnedBlock = std::move(m_nextBlockPreview);
        m_nextBlockPreview = CreateRandomTetromino();
        break;
    case BlockType::GiantTetromino:
        spawnedBlock = std::make_unique<GiantTetrominoBlock>();
        break;
    case BlockType::Heavy:
        spawnedBlock = std::make_unique<HeavyBlock>();
        break;
    }

    // 그리드 가로 중앙에서 스폰. 세로는 고정된 0번 줄이 아니라 카메라가 보고 있는 화면 맨 위 지점
    // (world y = 카메라 위치)에 맞춰 스폰한다 — 카메라가 위로 올라간 만큼 스폰 위치도 같이 올라가서,
    // 항상 화면 맨 위에서 블럭이 등장하는 것처럼 보이게 한다. 카메라가 안 움직인 초반엔 위치가 0이라
    // 기존과 동일하게 맨 위 줄에서 스폰된다.
    float cameraTopWorldY = CameraManager::GetInstance().GetPosition().y;
    int spawnGridY = static_cast<int>(std::round(cameraTopWorldY / Constants::SUBCELL_SIZE));
    spawnedBlock->SetGridPosition(Constants::GRID_WIDTH_SUBCELLS / 2, spawnGridY);

    m_currentFallingBlock = spawnedBlock.get();
    m_blocks.push_back(std::move(spawnedBlock));
    m_fallTimer = GetCurrentFallInterval();
}

void BlockManager::UpdateFalling(float deltaTime)
{
    // [시간에 따른 가속] 낙하 중인 블럭이 없는 찰나(락 직후~재스폰 직전)에도 시간은 계속 흘러야
    // 하니, 아래 nullptr 얼리 리턴보다 먼저 누적한다.
    m_elapsedPlayTime += deltaTime;

    if (m_currentFallingBlock == nullptr) return;

    // [락 딜레이] 낙하 타이머(m_fallTimer)와는 별개로, 매 프레임 "지금 내려갈 수 있는 상태인지"를
    // 확인한다. 내려갈 수 있으면 딜레이를 취소하고, 못 내려가는 상태가 LOCK_DELAY_DURATION만큼
    // 이어지면 그때 착지를 확정한다 — 그래야 좁은 틈으로 옆으로 비켜 넣는 동안은 락이 안 걸린다.
    if (m_currentFallingBlock->CanStepDown())
    {
        m_lockDelayTimer = 0.0f;
    }
    else
    {
        m_lockDelayTimer += deltaTime;
        if (m_lockDelayTimer >= Constants::LOCK_DELAY_DURATION)
        {
            LockBlock(m_currentFallingBlock);
            m_currentFallingBlock = nullptr;
            m_lockDelayTimer = 0.0f;
            SpawnBlock(BlockType::Tetromino);
            return;
        }
    }

	float fallSpeedMultiplier = InputManager::GetInstance().IsKeyDown(VK_DOWN) ? Constants::SOFT_DROP_MULTIPLIER : 1.0f;
	m_fallTimer -= deltaTime * fallSpeedMultiplier;

    if (m_fallTimer <= 0.0f)
    {
		m_currentFallingBlock->StepDown(); // 실패(더 못 내려감)해도 락 판단은 위에서 이미 처리했으니 무시
		m_fallTimer += GetCurrentFallInterval();
    }
}

float BlockManager::GetCurrentFallInterval() const
{
    // 30초(SPEED_LEVEL_UP_INTERVAL_SECONDS)마다 레벨 1씩, 레벨당 FALL_INTERVAL_DECREASE_PER_LEVEL(초)만큼
    // 줄인다. static_cast<int>로 내림해서 "딱 30초가 지나야" 다음 레벨이 되게 한다(계단식).
    int speedLevel = static_cast<int>(m_elapsedPlayTime / Constants::SPEED_LEVEL_UP_INTERVAL_SECONDS);
    float interval = Constants::FALL_STEP_INTERVAL - speedLevel * Constants::FALL_INTERVAL_DECREASE_PER_LEVEL;
    return std::max(interval, Constants::MIN_FALL_STEP_INTERVAL);
}

void BlockManager::MoveCurrentBlock(int subCellDelta)
{
    if (m_currentFallingBlock == nullptr) return;

    m_currentFallingBlock->MoveHorizontal(subCellDelta);
}

void BlockManager::UpdateMovementInput(float deltaTime)
{
    InputManager& input = InputManager::GetInstance();

    int direction = 0;
    if (input.IsKeyDown(VK_LEFT)) direction = -1;
    else if (input.IsKeyDown(VK_RIGHT)) direction = 1;

    if (direction == 0)
    {
        return;
    }

    // 새로 누른 순간이면 타이머를 0으로 만들어서 이번 프레임에 바로 이동하게 한다
    if (input.IsKeyPressed(VK_LEFT) || input.IsKeyPressed(VK_RIGHT))
    {
        m_moveTimer = 0.0f;
    }

    m_moveTimer -= deltaTime;
    if (m_moveTimer <= 0.0f)
    {
        MoveCurrentBlock(direction * Constants::MOVE_STEP_SUBCELLS);
        m_moveTimer += Constants::MOVE_REPEAT_INTERVAL;
    }
}

void BlockManager::UpdateRotationInput()
{
    if (m_currentFallingBlock == nullptr) return;

    InputManager& input = InputManager::GetInstance();

    if (input.IsKeyPressed('Z'))
    {
        m_currentFallingBlock->Rotate(-1);
    }
    else if (input.IsKeyPressed('X'))
    {
        m_currentFallingBlock->Rotate(1);
    }
}

void BlockManager::LockBlock(Block* block)
{
    if (block == nullptr) return;

    block->Land();
    SoundManager::GetInstance().PlaySfx("assets/Sound/Pong.mp3");
}

bool BlockManager::IsGameOver() const
{
    return m_isGameOver;
}

void BlockManager::Reset()
{
    m_blocks.clear(), m_currentFallingBlock = nullptr, m_isGameOver = false, m_fallTimer = m_lockDelayTimer = m_moveTimer = m_elapsedPlayTime = 0.0f;

    // 첫 SpawnBlock(BlockType::Tetromino) 호출 전에 "다음 블럭"이 미리 준비되어 있어야
    // 미리보기가 게임 시작 직후부터 빈 채로 있지 않는다.
    m_nextBlockPreview = CreateRandomTetromino();
}

float BlockManager::GetDeathZoneTopY() const
{
    // WorldToScreen이 worldPos - cameraPosition이므로, "지금 화면 맨 아래(스크린 y = WINDOW_HEIGHT)"에
    // 해당하는 월드 y는 그 역산인 cameraPosition + WINDOW_HEIGHT다. 카메라가 위로 올라갈수록(음수로
    // 커질수록) 이 값도 같이 작아져서, 데스존이 항상 지금 보이는 화면 바닥에 붙어 따라 올라간다.
    return CameraManager::GetInstance().GetPosition().y + Constants::WINDOW_HEIGHT + Constants::DEATH_ZONE_MARGIN_BELOW_SCREEN;
}

float BlockManager::GetTallestHeightMeters() const
{
    bool hasStackedBlock = false;
	float highestBlockY = 0.0f;

	for (Block* block : GetAllBlocks())
	{
		if (block->GetPhysicsState() == PhysicsState::Airborne)
		{
			continue;
		}
		float blockY = block->GetWorldBounds().min.y;
		if (!hasStackedBlock || blockY < highestBlockY)
		{
			highestBlockY = blockY;
			hasStackedBlock = true;
		}
	}
	if (!hasStackedBlock)
	{
		return 0.0f;
	}
	float heightInPixels = Constants::FLOOR_TOP_Y - highestBlockY;
	float heightInTiles = heightInPixels / Constants::TILE_SIZE;
    return heightInTiles * Constants::METERS_PER_TILE;
}

bool BlockManager::IsPointInDeathZone(Vector2 worldPoint) const
{
    bool isBelowDeathZoneTop = worldPoint.y >= GetDeathZoneTopY();
    bool isOutsideFloorWidth = worldPoint.x < Constants::FLOOR_LEFT_X || worldPoint.x > Constants::FLOOR_RIGHT_X;
    return isBelowDeathZoneTop && isOutsideFloorWidth;
}

void BlockManager::CheckDeathZone()
{


    if (m_isGameOver)
    {
        return;
    }

    for (Block* block : GetAllBlocks())
    {
        PhysicsState state = block->GetPhysicsState();
        bool isAirborneOrToppling = state == PhysicsState::Airborne || state == PhysicsState::Toppling;

        // [미끄러지며 떨어지는 경우 데스존 누락 방지] MAX_TOPPLE_ANGLE(40도)까지 회전하지 않고도, 거의
        // 안 기운 채로 옆으로 미끄러져 바닥 옆 허공으로 떨어지는 경우가 있다 — 물리적으로는 Toppling과
        // 똑같은 완전 자유낙하인데 각도만 안 넘겼을 뿐이라 Awake로 남아있고, 위 조건에 안 걸려서 죽음
        // 판정이 누락됐다. 그렇다고 Awake를 무조건 다 검사하면, 아직 뭔가에 걸쳐 미끄러지는 중(지지는
        // 있음)인 정상적인 블록까지 가장자리를 살짝 스치기만 해도 바로 게임오버가 되어버린다. 그래서
        // Awake인데 칸이 하나도 지지를 못 받는 "완전히 붕 뜬" 경우만 추가로 포함시킨다 — 조금이라도
        // 뭔가에 얹혀있으면(지지 있음) 정상 물리로 붙잡힐 수 있으니 제외한다.
        bool isFreeFallingAwake = false;
        if (state == PhysicsState::Awake)
        {
            isFreeFallingAwake = true;
            for (int i = 0; i < block->GetCellCount() && isFreeFallingAwake; ++i)
            {
                if (PhysicsManager::GetInstance().IsCellSupported(block, i))
                {
                    isFreeFallingAwake = false;
                }
            }
        }

        if (!isAirborneOrToppling && !isFreeFallingAwake)
        {
            continue;
        }
        for (int i = 0; i < block->GetCellCount(); ++i)
        {
            Vector2 corners[4];
            block->GetCellRotatedCorners(i, corners);

            for (int c = 0; c < 4; ++c)
            {
                if (IsPointInDeathZone(corners[c]))
                {
                    m_isGameOver = true;
                    return;
                }
            }
        }
    }
}

void BlockManager::RemoveBlock(Block* block)
{
    for (size_t i = 0; i < m_blocks.size(); ++i)
    {
        if (m_blocks[i].get() == block)
        {
            m_blocks.erase(m_blocks.begin() + i);
            return;
        }
    }
}

Block* BlockManager::GetCurrentFallingBlock() const
{
    return m_currentFallingBlock;
}

const Block* BlockManager::GetNextBlockPreview() const
{
    return m_nextBlockPreview.get();
}

std::vector<Block*> BlockManager::GetAllBlocks() const
{
    std::vector<Block*> allBlocks;
    allBlocks.reserve(m_blocks.size());
    for (const std::unique_ptr<Block>& block : m_blocks)
    {
        allBlocks.push_back(block.get());
    }
    return allBlocks;
}
