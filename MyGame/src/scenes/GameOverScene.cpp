#include "pch.h"

#include "GameOverScene.h"
#include <algorithm>
#include <cmath>


#include "../managers/ResourceManager.h"
#include "../managers/RenderManager.h"
#include "../core/InputManager.h"
#include "../core/SceneManager.h"
#include "../core/WindowManager.h"
#include "../util/Constants.h"
#include "../util/Types.h"
#include "../managers/BlockManager.h"

namespace
{
    constexpr int BUTTON_SLOT_RETRY = 0;
    constexpr int BUTTON_SLOT_TITLE = 1;
}

void GameOverScene::Enter()
{
    ResourceManager::GetInstance().LoadSprite("assets/GameOverBG.png");
    ResourceManager::GetInstance().LoadSprite("assets/GameOverText.png");
    ResourceManager::GetInstance().LoadSprite("assets/Button2.png");

    m_gameOverDropOffsetY = Constants::GAME_OVER_DROP_START_OFFSET_Y;
    m_gameOverDropVelocityY = 0.0f;
    m_gameOverDropSettled = false;
}

void GameOverScene::Exit()
{
    // TODO: 게임오버 씬 정리 로직 직접 구현
}

void GameOverScene::Update(float deltaTime)
{
    if (!m_gameOverDropSettled)
    {
        m_gameOverDropVelocityY += Constants::GAME_OVER_DROP_GRAVITY * deltaTime;
        m_gameOverDropOffsetY += m_gameOverDropVelocityY * deltaTime;

        if (m_gameOverDropOffsetY >= 0.0f)
        {
            m_gameOverDropOffsetY = 0.0f;

            if (std::fabs(m_gameOverDropVelocityY) < Constants::GAME_OVER_DROP_SETTLE_SPEED)
            {
                m_gameOverDropVelocityY = 0.0f;
                m_gameOverDropSettled = true;
            }
            else
            {
                m_gameOverDropVelocityY = -m_gameOverDropVelocityY * Constants::GAME_OVER_DROP_BOUNCE_RESTITUTION;
            }
        }
    }
    Vector2 mousePos = InputManager::GetInstance().GetMousePosition();
    bool isLeftClick = InputManager::GetInstance().IsKeyPressed(VK_LBUTTON);

    bool isMouseOverTitle = IsPointInRect(mousePos, GetButtonHitboxCenter(BUTTON_SLOT_TITLE), GetButtonHitboxSize(BUTTON_SLOT_TITLE));
    if (isMouseOverTitle && isLeftClick)
    {
        SceneManager::GetInstance().ChangeScene(SceneType::Title);
    }
    bool isMouseOverRetry = IsPointInRect(mousePos, GetButtonHitboxCenter(BUTTON_SLOT_RETRY), GetButtonHitboxSize(BUTTON_SLOT_RETRY ));
    if (isMouseOverRetry && isLeftClick)
    {
        SceneManager::GetInstance().ChangeScene(SceneType::Play);
    }
    if (InputManager::GetInstance().IsKeyPressed(VK_F1))
    {
        m_showDebug = !m_showDebug;
    }
}

void GameOverScene::Render(ID2D1RenderTarget* renderTarget)
{
    const SpriteInfo& backgroundSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/GameOverBG.png");
    if (backgroundSprite.bitmap)
    {
        D2D1_SIZE_F nativeSize = backgroundSprite.bitmap->GetSize();
        float scale = std::max(Constants::WINDOW_WIDTH / nativeSize.width, Constants::WINDOW_HEIGHT / nativeSize.height);
        Vector2 backgroundSize = { nativeSize.width * scale, nativeSize.height * scale };
        Vector2 backgroundCenter = { Constants::WINDOW_WIDTH / 2.0f, Constants::WINDOW_HEIGHT - backgroundSize.y / 2.0f };
        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, backgroundSprite, backgroundCenter, backgroundSize, 0.0f, 0);
    }

    const SpriteInfo& GameoverSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/GameOverText.png");
    if (GameoverSprite.bitmap)
    {
        D2D1_SIZE_F nativeSize = GameoverSprite.bitmap->GetSize();
        float scale = std::min(Constants::WINDOW_WIDTH / nativeSize.width, Constants::WINDOW_HEIGHT / nativeSize.height) * Constants::GAME_OVER_IMAGE_SCALE;
        Vector2 GameoverSize = { nativeSize.width * scale, nativeSize.height * scale };
        Vector2 GameoverCenter = { Constants::WINDOW_WIDTH / 2.0f, Constants::GAME_OVER_IMAGE_CENTER_Y + m_gameOverDropOffsetY };
        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, GameoverSprite, GameoverCenter, GameoverSize, 0.0f, 0);
    }
    RenderManager::GetInstance().DrawScorePanel(
        renderTarget,
        { Constants::WINDOW_WIDTH / 2.0f, Constants::GAME_OVER_SCORE_CENTER_Y },
        BlockManager::GetInstance().GetTallestHeightMeters());

    const SpriteInfo& buttonSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/Button2.png");
    if (buttonSprite.bitmap)
    {
        D2D1_RECT_F TitleRect = GetButtonSlotSourceRect(BUTTON_SLOT_TITLE);
        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, buttonSprite, GetButtonSlotCenter(BUTTON_SLOT_TITLE), GetButtonSize(), 0.0f, 0, 1.0f, &TitleRect);

        D2D1_RECT_F RetryRect = GetButtonSlotSourceRect(BUTTON_SLOT_RETRY);
        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, buttonSprite, GetButtonSlotCenter(BUTTON_SLOT_RETRY), GetButtonSize(), 0.0f, 0, 1.0f, &RetryRect);



    }
    if (m_showDebug)
    {
        RenderManager::GetInstance().DrawDebugRect(renderTarget, GetButtonHitboxCenter(BUTTON_SLOT_TITLE), GetButtonHitboxSize(BUTTON_SLOT_TITLE), Constants::COLLIDER_DEBUG_COLOR);
        RenderManager::GetInstance().DrawDebugRect(renderTarget, GetButtonHitboxCenter(BUTTON_SLOT_RETRY), GetButtonHitboxSize(BUTTON_SLOT_RETRY), Constants::COLLIDER_DEBUG_COLOR);
    }
}
Vector2 GameOverScene::GetButtonSlotCenter(int slotIndex) const
{
    // slotIndex가 클수록 Start 자리 밑으로 버튼 한 칸 높이씩(GetButtonSize().y) 이어 붙는다
    float stepY = GetButtonSize().y;
    return { Constants::WINDOW_WIDTH / 2.0f, Constants::GAME_OVER_BUTTON_CENTER_Y + stepY * static_cast<float>(slotIndex) };
}

Vector2 GameOverScene::GetButtonSize() const
{
    // 실제로 그리는 건 한 칸(원본의 1/3)뿐이니, 그 부분의 가로세로 비율을 기준으로 계산해야
    // 이미지 전체 비율로 계산했을 때처럼 세로로 길쭉하게 늘어나 보이지 않는다.
    D2D1_RECT_F sourceRect = GetButtonSlotSourceRect(BUTTON_SLOT_TITLE);
    float croppedWidth = sourceRect.right - sourceRect.left;
    float croppedHeight = sourceRect.bottom - sourceRect.top;
    if (croppedWidth <= 0.0f)
    {
        return { 0.0f, 0.0f };
    }

    float scale = Constants::TITLE_BUTTON_TARGET_WIDTH / croppedWidth;
    return { croppedWidth * scale, croppedHeight * scale };
}

Vector2 GameOverScene::GetButtonHitboxCenter(int slotIndex) const
{
    Vector2 center = GetButtonSlotCenter(slotIndex );
    center.y += (slotIndex == BUTTON_SLOT_TITLE) ? Constants::GAME_OVER_TITLE_HITBOX_OFFSET_Y : Constants::GAME_OVER_RETRY_HITBOX_OFFSET_Y;
    return center;
}

Vector2 GameOverScene::GetButtonHitboxSize(int slotIndex) const
{
    // 그리는 크기보다 좁게(또는 넓게) 잡아서, 옆 칸(Option/Ranking)까지 눌리지 않으면서도 실제
    // 버튼 그림 모양(가로로 넓고 얇은 형태 등)에 맞출 수 있게 가로/세로를 따로 조절한다
    Vector2 drawnSize = GetButtonSize();
    return { drawnSize.x * Constants::GAME_OVER_BUTTON_HITBOX_SCALE_X, drawnSize.y * Constants::GAME_OVER_BUTTON_HITBOX_SCALE_Y };
}


bool GameOverScene::IsPointInRect(Vector2 point, Vector2 rectCenter, Vector2 rectSize)
{
    return point.x >= rectCenter.x - rectSize.x / 2.0f && point.x <= rectCenter.x + rectSize.x / 2.0f &&
        point.y >= rectCenter.y - rectSize.y / 2.0f && point.y <= rectCenter.y + rectSize.y / 2.0f;
}
D2D1_RECT_F GameOverScene::GetButtonSlotSourceRect(int slotIndex) const
{
    const SpriteInfo& buttonSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/Button2.png");
    if (!buttonSprite.bitmap)
    {
        return D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    }

    D2D1_SIZE_F nativeSize = buttonSprite.bitmap->GetSize();
    float slotHeight = nativeSize.height / 2.0f;
    float top = slotHeight * static_cast<float>(slotIndex);
    return D2D1::RectF(0.0f, top, nativeSize.width, top + slotHeight);
}
